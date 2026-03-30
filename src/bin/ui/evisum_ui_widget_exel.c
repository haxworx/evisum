#include "evisum_ui_widget_exel.h"
#include "evisum_ui_util.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define EVISUM_UI_WIDGET_EXEL_FIELDS_MAX 64
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_FIRST 0
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_MAX   32
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_HIT_WIDTH   8
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_CACHE_SIZE  30
#define EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD      6

typedef struct {
    int id;
    Evas_Object *btn;
    const char *desc;
    Evisum_Ui_Widget_Exel_Item_Cell_Def row_def;
    int default_width;
    int min_width;
    Eina_Bool always_visible;
    Eina_Bool enabled;
    Eina_Bool row_def_set;
} Exel_Field;

typedef struct {
    Evas_Object *win;
    Evas_Object *parent;
    int field_first;
    int field_max;
    int resize_hit_width;
    unsigned int *fields_mask;
    int *field_widths;
    int *field_order;
    Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb reference_mask_get_cb;
    Evisum_Ui_Widget_Exel_Fields_Changed_Cb fields_changed_cb;
    Evisum_Ui_Widget_Exel_Fields_Applied_Cb fields_applied_cb;
    Evisum_Ui_Widget_Exel_Resize_Live_Cb resize_live_cb;
    Evisum_Ui_Widget_Exel_Resize_Done_Cb resize_done_cb;
    Evisum_Ui_Widget_Exel_Fields_Reordered_Cb fields_reordered_cb;
    void *data;
} Evisum_Ui_Widget_Exel_Params;

struct _Evisum_Ui_Widget_Exel {
    Evisum_Ui_Widget_Exel_Params p;
    unsigned int fields_mask_state;
    int field_widths_state[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX];
    int field_order_state[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX];
    Exel_Field fields[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX];
    Evisum_Ui_Cache *cache;
    Evas_Object *root;
    Evas_Object *header_tb;
    Evas_Object *glist;

    Evas_Object *fields_menu;
    Evas_Object *resize_btn;
    Ecore_Timer *deferred_timer;
    void (*deferred_cb)(void *data);
    void *deferred_data;

    int resize_field;
    Evas_Coord resize_start_x;
    Evas_Coord resize_start_w;
    int resize_dir;
    int drag_field;
    Evas_Coord drag_start_x;
    Evas_Coord drag_start_y;
    Evas_Coord drag_offset_x;
    Evas_Coord drag_offset_y;
    Evas_Object *drag_proxy;
    Eina_Bool reorder_enabled;
    Eina_Bool dragging;
    Eina_Bool drag_pending;

    Eina_Bool resizing;
    Eina_Bool ignore_sort_click;
    Eina_Bool fields_changed;
    Eina_Bool owns_state;
    Eina_Bool win_mouse_move_registered;
    Eina_Bool unrealized_release_enabled;
    Eina_Bool unrealized_delete_unhandled;
};

static Exel_Field *
_evisum_ui_widget_exel_field_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    if (!wx) return NULL;
    if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return NULL;
    if (wx->fields[id].id != id) return NULL;
    return (Exel_Field *) &wx->fields[id];
}

static void
_evisum_ui_widget_exel_field_order_sanitize(Evisum_Ui_Widget_Exel *wx) {
    unsigned char seen[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX] = { 0 };
    int out[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX] = { 0 };
    int pos;

    if (!wx || !wx->p.field_order) return;

    /* Keep the first field pinned (e.g. command column). */
    out[wx->p.field_first] = wx->p.field_first;
    seen[wx->p.field_first] = 1;
    pos = wx->p.field_first + 1;
    for (int i = wx->p.field_first; i < wx->p.field_max; i++) {
        int id = wx->p.field_order[i];
        if (id < wx->p.field_first || id >= wx->p.field_max) continue;
        if (seen[id]) continue;
        out[pos++] = id;
        seen[id] = 1;
    }

    for (int id = wx->p.field_first; id < wx->p.field_max; id++) {
        if (seen[id]) continue;
        out[pos++] = id;
    }

    for (int i = wx->p.field_first; i < wx->p.field_max; i++) wx->p.field_order[i] = out[i];
}

static int
_evisum_ui_widget_exel_field_order_pos_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    if (!wx || !wx->p.field_order) return -1;
    for (int i = wx->p.field_first; i < wx->p.field_max; i++) {
        if (wx->p.field_order[i] == id) return i;
    }
    return -1;
}

static Eina_Bool
_evisum_ui_widget_exel_field_order_move_to_pos(Evisum_Ui_Widget_Exel *wx, int id, int to_pos) {
    int from_pos;
    int value;
    int reorder_min_pos;

    if (!wx || !wx->p.field_order) return EINA_FALSE;
    if (id < wx->p.field_first || id >= wx->p.field_max) return EINA_FALSE;
    reorder_min_pos = wx->p.field_first + 1;
    if (reorder_min_pos >= wx->p.field_max) reorder_min_pos = wx->p.field_first;
    if (to_pos < reorder_min_pos) to_pos = reorder_min_pos;
    if (to_pos >= wx->p.field_max) to_pos = wx->p.field_max - 1;

    from_pos = _evisum_ui_widget_exel_field_order_pos_get(wx, id);
    if (from_pos < 0 || from_pos == to_pos) return EINA_FALSE;

    value = wx->p.field_order[from_pos];
    if (from_pos < to_pos) {
        for (int i = from_pos; i < to_pos; i++) wx->p.field_order[i] = wx->p.field_order[i + 1];
    } else {
        for (int i = from_pos; i > to_pos; i--) wx->p.field_order[i] = wx->p.field_order[i - 1];
    }
    wx->p.field_order[to_pos] = value;

    return EINA_TRUE;
}

static Eina_Bool
_evisum_ui_widget_exel_field_id_valid(const Evisum_Ui_Widget_Exel *wx, int id) {
    if (!wx) return EINA_FALSE;
    if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return EINA_FALSE;
    return EINA_TRUE;
}

static int
_evisum_ui_widget_exel_field_prev_visible_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    int pos;

    if (!wx || !wx->p.field_order) return 0;

    pos = _evisum_ui_widget_exel_field_order_pos_get(wx, id);
    if (pos < 0) return 0;

    for (int i = pos - 1; i >= wx->p.field_first; i--) {
        int prev_id = wx->p.field_order[i];
        if (evisum_ui_widget_exel_field_enabled_get(wx, prev_id)) return prev_id;
    }

    return 0;
}

static int
_evisum_ui_widget_exel_field_min_width_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
    if (!f) return ELM_SCALE_SIZE(48);
    return f->min_width;
}

static int
_evisum_ui_widget_exel_field_default_width_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
    if (!f) return ELM_SCALE_SIZE(80);
    return f->default_width;
}

static void
_evisum_ui_widget_exel_fields_sync(Evisum_Ui_Widget_Exel *wx) {
    int i;

    if (!wx || !wx->p.fields_mask) return;
    _evisum_ui_widget_exel_field_order_sanitize(wx);

    for (i = wx->p.field_first; i < wx->p.field_max; i++) {
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, i);
        if (!f) continue;

        f->enabled = 1;
        if (!f->always_visible && !(*(wx->p.fields_mask) & (1U << i))) f->enabled = 0;
    }
}

static Eina_Bool
_evisum_ui_widget_exel_deferred_timer_cb(void *data) {
    Evisum_Ui_Widget_Exel *wx = data;
    void (*cb)(void *data);
    void *cb_data;

    if (!wx) return ECORE_CALLBACK_CANCEL;

    cb = wx->deferred_cb;
    cb_data = wx->deferred_data;
    wx->deferred_timer = NULL;
    wx->deferred_cb = NULL;
    wx->deferred_data = NULL;

    if (cb) cb(cb_data);

    return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_evisum_ui_widget_exel_dirty_get(Evisum_Ui_Widget_Exel *wx) {
    unsigned int ref;

    if (!wx->p.reference_mask_get_cb) return wx->fields_changed;
    if (!wx->p.fields_mask) return wx->fields_changed;

    ref = wx->p.reference_mask_get_cb(wx->p.data);
    return ref != *(wx->p.fields_mask);
}

static void
_evisum_ui_widget_exel_fields_changed_notify(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return;
    wx->fields_changed = _evisum_ui_widget_exel_dirty_get(wx);
    if (wx->p.fields_changed_cb) wx->p.fields_changed_cb(wx->p.data, wx->fields_changed);
}

static void
_evisum_ui_widget_exel_fields_icon_mouse_in_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj,
                                               void *event_info EINA_UNUSED) {
    evas_object_color_set(obj, 128, 128, 128, 255);
}

static void
_evisum_ui_widget_exel_fields_icon_mouse_out_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj,
                                                void *event_info EINA_UNUSED) {
    evas_object_color_set(obj, 255, 255, 255, 255);
}

static void
_evisum_ui_widget_exel_fields_menu_close_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                    void *event_info EINA_UNUSED) {
    Evisum_Ui_Widget_Exel *wx = data;
    evisum_ui_widget_exel_fields_menu_dismiss(wx);
}

static void
_evisum_ui_widget_exel_fields_menu_apply_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                    void *event_info EINA_UNUSED) {
    Evisum_Ui_Widget_Exel *wx = data;
    Eina_Bool changed;

    changed = wx->fields_changed;
    evisum_ui_widget_exel_fields_menu_dismiss(wx);

    if (changed && wx->owns_state) {
        evisum_ui_widget_exel_fields_apply(wx);
        if (wx->cache) evisum_ui_item_cache_reset(wx->cache, NULL, NULL);
        if (wx->glist) elm_genlist_realized_items_update(wx->glist);
    }

    if (wx->p.fields_applied_cb) wx->p.fields_applied_cb(wx->p.data, changed);
}

static void
_evisum_ui_widget_exel_fields_menu_check_changed_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Exel_Field *f = data;
    Evisum_Ui_Widget_Exel *wx;
    Evas_Object *ic;

    wx = evas_object_data_get(obj, "wx");
    if (!wx || !f || !wx->p.fields_mask) return;

    *(wx->p.fields_mask) ^= (1U << f->id);
    _evisum_ui_widget_exel_fields_sync(wx);
    _evisum_ui_widget_exel_fields_changed_notify(wx);

    ic = evas_object_data_get(obj, "apply_icon");
    if (!ic) return;
    if (!wx->fields_changed) evas_object_hide(ic);
    else evas_object_show(ic);
}

static Evas_Object *
_evisum_ui_widget_exel_fields_menu_create(Evisum_Ui_Widget_Exel *wx) {
    Evas_Object *o, *fr, *hbx, *pad, *ic, *ic2, *bx, *ck;
    int i;

    fr = elm_frame_add(wx->p.parent);
    elm_object_style_set(fr, "pad_small");

    bx = elm_box_add(wx->p.parent);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    evas_object_show(bx);
    elm_object_content_set(fr, bx);
    evas_object_show(fr);

    hbx = elm_box_add(wx->p.parent);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);
    evas_object_size_hint_align_set(hbx, FILL, FILL);

    pad = elm_box_add(wx->p.parent);
    evas_object_size_hint_weight_set(pad, EXPAND, 0);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    elm_box_pack_end(hbx, pad);
    evas_object_show(pad);

    ic = elm_icon_add(wx->p.parent);
    evisum_ui_icon_set(ic, "apply");
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    elm_box_pack_end(hbx, ic);
    evas_object_smart_callback_add(ic, "clicked", _evisum_ui_widget_exel_fields_menu_apply_clicked_cb, wx);
    evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_IN, _evisum_ui_widget_exel_fields_icon_mouse_in_cb, NULL);
    evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_OUT, _evisum_ui_widget_exel_fields_icon_mouse_out_cb, NULL);

    ic2 = elm_icon_add(wx->p.parent);
    evisum_ui_icon_set(ic2, "exit");
    evas_object_size_hint_min_set(ic2, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    evas_object_show(ic2);
    elm_box_pack_end(hbx, ic2);
    evas_object_smart_callback_add(ic2, "clicked", _evisum_ui_widget_exel_fields_menu_close_clicked_cb, wx);
    evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_IN, _evisum_ui_widget_exel_fields_icon_mouse_in_cb, NULL);
    evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_OUT, _evisum_ui_widget_exel_fields_icon_mouse_out_cb, NULL);

    elm_box_pack_end(bx, hbx);
    evas_object_show(hbx);

    for (i = wx->p.field_first + 1; i < wx->p.field_max; i++) {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->btn || !f->desc) continue;

        ck = elm_check_add(wx->p.parent);
        evas_object_size_hint_weight_set(ck, EXPAND, EXPAND);
        evas_object_size_hint_align_set(ck, FILL, FILL);
        elm_object_text_set(ck, f->desc);
        elm_check_state_set(ck, f->enabled);
        evas_object_data_set(ck, "apply_icon", ic);
        evas_object_data_set(ck, "wx", wx);
        evas_object_smart_callback_add(ck, "changed", _evisum_ui_widget_exel_fields_menu_check_changed_cb, f);
        elm_box_pack_end(bx, ck);
        evas_object_show(ck);
    }

    o = elm_ctxpopup_add(wx->p.parent);
    evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
    evas_object_size_hint_align_set(o, FILL, FILL);
    elm_object_style_set(o, "noblock");

    elm_object_content_set(o, fr);

    if (!wx->fields_changed) evas_object_hide(ic);

    return o;
}

static int
_evisum_ui_widget_exel_drop_pos_get(const Evisum_Ui_Widget_Exel *wx, Evas_Coord x) {
    int reorder_min_pos;
    int last_visible;

    if (!wx || !wx->p.field_order) return -1;
    reorder_min_pos = wx->p.field_first + 1;
    if (reorder_min_pos >= wx->p.field_max) reorder_min_pos = wx->p.field_first;
    last_visible = reorder_min_pos;

    for (int i = reorder_min_pos; i < wx->p.field_max; i++) {
        int id = wx->p.field_order[i];
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        Evas_Coord ox = 0, ow = 0;

        if (!f || !f->btn) continue;
        if (!f->enabled) continue;
        if (id == wx->drag_field) continue;

        evas_object_geometry_get(f->btn, &ox, NULL, &ow, NULL);
        if (x < (ox + (ow / 2))) return i;
        last_visible = i;
    }

    return last_visible + 1;
}

static Eina_Bool
_evisum_ui_widget_exel_reorder_commit(Evisum_Ui_Widget_Exel *wx, Evas_Coord x) {
    int drop_pos;
    int from_pos;
    int to_pos;
    Eina_Bool changed;

    if (!wx || !wx->p.field_order || !wx->dragging) return EINA_FALSE;

    drop_pos = _evisum_ui_widget_exel_drop_pos_get(wx, x);
    if (drop_pos < wx->p.field_first) return EINA_FALSE;

    from_pos = _evisum_ui_widget_exel_field_order_pos_get(wx, wx->drag_field);
    if (from_pos < 0) return EINA_FALSE;

    if (drop_pos > from_pos) to_pos = drop_pos - 1;
    else to_pos = drop_pos;

    changed = _evisum_ui_widget_exel_field_order_move_to_pos(wx, wx->drag_field, to_pos);
    if (!changed) return EINA_FALSE;

    wx->ignore_sort_click = 1;
    if (!wx->p.fields_reordered_cb && wx->header_tb) {
        elm_table_clear(wx->header_tb, EINA_FALSE);
        evisum_ui_widget_exel_fields_pack_visible(wx, wx->header_tb, 0);
    }

    if (wx->p.fields_reordered_cb) wx->p.fields_reordered_cb(wx->p.data);

    return EINA_TRUE;
}

static void
_evisum_ui_widget_exel_drag_proxy_del(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return;
    if (!wx->drag_proxy) return;
    evas_object_del(wx->drag_proxy);
    wx->drag_proxy = NULL;
}

static void
_evisum_ui_widget_exel_drag_proxy_move(Evisum_Ui_Widget_Exel *wx, Evas_Coord x, Evas_Coord y) {
    if (!wx || !wx->drag_proxy) return;
    evas_object_move(wx->drag_proxy, x - wx->drag_offset_x, y - wx->drag_offset_y);
}

static void
_evisum_ui_widget_exel_drag_proxy_create(Evisum_Ui_Widget_Exel *wx) {
    Exel_Field *f;
    Evas_Object *src;
    Evas_Object *proxy;
    Evas_Coord ox = 0, oy = 0, ow = 0, oh = 0;
    const char *text;
    Eina_Bool reverse;

    if (!wx) return;
    if (wx->drag_proxy) return;
    if (!wx->dragging) return;

    f = _evisum_ui_widget_exel_field_get(wx, wx->drag_field);
    if (!f || !f->btn) return;
    src = f->btn;

    evas_object_geometry_get(src, &ox, &oy, &ow, &oh);
    text = elm_object_text_get(src);
    reverse = (Eina_Bool) (intptr_t) evas_object_data_get(src, "sort_reverse");

    proxy = evisum_ui_widget_exel_sort_header_button_add(wx->p.win, text ? text : "", reverse, 0.0, FILL, NULL, NULL);
    if (!proxy) return;

    evas_object_size_hint_min_set(proxy, ow, oh);
    evas_object_size_hint_max_set(proxy, ow, oh);
    evas_object_resize(proxy, ow, oh);
    evas_object_move(proxy, ox, oy);
    evas_object_layer_set(proxy, EVAS_LAYER_MAX);
    evas_object_show(proxy);
    wx->drag_proxy = proxy;

    wx->drag_offset_x = wx->drag_start_x - ox;
    wx->drag_offset_y = wx->drag_start_y - oy;
    if (wx->drag_offset_x < 0) wx->drag_offset_x = ow / 2;
    if (wx->drag_offset_x > ow) wx->drag_offset_x = ow / 2;
    if (wx->drag_offset_y < 0) wx->drag_offset_y = oh / 2;
    if (wx->drag_offset_y > oh) wx->drag_offset_y = oh / 2;
}

static void
_evisum_ui_widget_exel_field_header_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Widget_Exel *wx = data;
    Evas_Event_Mouse_Down *ev = event_info;
    Evas_Coord x, w;
    int field, prev;

    if (!wx || !obj || !ev) return;
    if (ev->button != 1) return;

    evas_object_geometry_get(obj, &x, NULL, &w, NULL);
    field = (int) (intptr_t) evas_object_data_get(obj, "field_id");
    if (field < wx->p.field_first || field >= wx->p.field_max) return;
    if (field == wx->p.field_first) return;

    if (ev->canvas.x >= (x + w - ELM_SCALE_SIZE(wx->p.resize_hit_width))) {
        _evisum_ui_widget_exel_drag_proxy_del(wx);
        wx->resizing = 1;
        wx->resize_btn = obj;
        wx->resize_field = field;
        wx->resize_start_x = ev->canvas.x;
        wx->resize_start_w = w;
        wx->resize_dir = 1;
        wx->drag_pending = 0;
        wx->dragging = 0;
        return;
    }

    if (ev->canvas.x <= (x + ELM_SCALE_SIZE(wx->p.resize_hit_width))) {
        prev = _evisum_ui_widget_exel_field_prev_visible_get(wx, field);
        if (!prev) return;

        wx->resize_btn
                = _evisum_ui_widget_exel_field_get(wx, prev) ? _evisum_ui_widget_exel_field_get(wx, prev)->btn : NULL;
        if (!wx->resize_btn) return;

        evas_object_geometry_get(wx->resize_btn, NULL, NULL, &wx->resize_start_w, NULL);
        wx->resizing = 1;
        wx->resize_field = prev;
        wx->resize_start_x = ev->canvas.x;
        wx->resize_dir = 1;
        wx->drag_pending = 0;
        wx->dragging = 0;
        return;
    }

    if (!wx->reorder_enabled) return;

    _evisum_ui_widget_exel_drag_proxy_del(wx);
    wx->drag_field = field;
    wx->drag_start_x = ev->canvas.x;
    wx->drag_start_y = ev->canvas.y;
    wx->drag_pending = 1;
    wx->dragging = 0;
}

static void
_evisum_ui_widget_exel_field_header_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Widget_Exel *wx = data;
    Evas_Event_Mouse_Up *ev = event_info;

    if (!wx || !ev) return;

    if (evisum_ui_widget_exel_field_reorder_mouse_up(wx, ev)) return;
    evisum_ui_widget_exel_field_resize_mouse_up(wx, ev);
    evisum_ui_widget_exel_fields_menu_show(wx, obj, ev);
}

static void
_evisum_ui_widget_exel_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                         void *event_info);

static void
_evisum_ui_widget_exel_genlist_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info);

Evisum_Ui_Widget_Exel *
evisum_ui_widget_exel_create(Evas_Object *parent) {
    Evisum_Ui_Widget_Exel_Params p;
    Evisum_Ui_Widget_Exel *wx;
    int i;

    if (!parent) return NULL;

    memset(&p, 0, sizeof(p));
    p.win = parent;
    p.parent = parent;
    p.field_first = EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_FIRST;
    p.field_max = EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_MAX;
    p.resize_hit_width = EVISUM_UI_WIDGET_EXEL_DEFAULT_HIT_WIDTH;

    wx = calloc(1, sizeof(Evisum_Ui_Widget_Exel));
    if (!wx) return NULL;

    wx->fields_mask_state = UINT_MAX;
    for (i = p.field_first; i < p.field_max; i++) {
        wx->field_widths_state[i] = 0;
        wx->field_order_state[i] = i;
    }

    p.fields_mask = &wx->fields_mask_state;
    p.field_widths = wx->field_widths_state;
    p.field_order = wx->field_order_state;
    p.data = wx;
    wx->p = p;
    wx->owns_state = EINA_TRUE;

    wx->root = elm_box_add(parent);
    evas_object_size_hint_weight_set(wx->root, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(wx->root, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(wx->root);

    wx->header_tb = elm_table_add(wx->root);
    evas_object_size_hint_weight_set(wx->header_tb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(wx->header_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(wx->header_tb);
    elm_box_pack_end(wx->root, wx->header_tb);

    evas_object_event_callback_add(p.win, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_widget_exel_win_mouse_move_cb, wx);
    wx->win_mouse_move_registered = EINA_TRUE;

    return wx;
}

Evas_Object *
evisum_ui_widget_exel_object_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return NULL;
    return wx->root;
}

void
evisum_ui_widget_exel_field_add(Evisum_Ui_Widget_Exel *wx, const Evisum_Ui_Widget_Exel_Field_Params *params) {
    Evas_Object *btn;
    Evisum_Ui_Widget_Exel_Item_Cell_Def def;

    if (!wx || !params || !wx->header_tb) return;

    btn = evisum_ui_widget_exel_sort_header_button_add(wx->header_tb, params->title, params->reverse, 1.0,
                                                        EVAS_HINT_FILL, params->clicked_cb, params->clicked_data);
    if (!btn) return;

    evisum_ui_widget_exel_field_register(wx, params->id, btn, params->desc, params->default_width, params->min_width,
                                         params->always_visible);
    def = (Evisum_Ui_Widget_Exel_Item_Cell_Def) {
        .type = params->type,
        .key = params->key,
        .aux_key = params->aux_key,
        .unit_format = params->unit_format,
        .align_x = params->align_x,
        .weight_x = params->weight_x,
        .boxed = params->boxed,
        .spacer = params->spacer,
        .icon_size = params->icon_size,
    };
    evisum_ui_widget_exel_field_row_def_set(wx, params->id, &def);
    evisum_ui_widget_exel_field_resize_attach(wx, btn, params->id);
    evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_UP, _evisum_ui_widget_exel_field_header_mouse_up_cb, wx);
}

void
evisum_ui_widget_exel_fields_apply(Evisum_Ui_Widget_Exel *wx) {
    int i;

    if (!wx || !wx->header_tb) return;

    for (i = wx->p.field_first; i < wx->p.field_max; i++) {
        evisum_ui_widget_exel_field_width_apply(wx, i);
    }
    evisum_ui_widget_exel_field_proportions_apply(wx);
    elm_table_clear(wx->header_tb, EINA_FALSE);
    evisum_ui_widget_exel_fields_pack_visible(wx, wx->header_tb, 0);
}

unsigned int
evisum_ui_widget_exel_fields_mask_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->p.fields_mask) return 0;
    return *(wx->p.fields_mask);
}

int
evisum_ui_widget_exel_field_width_state_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    if (!wx || !wx->p.field_widths) return 0;
    if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return 0;
    return wx->p.field_widths[id];
}

void
evisum_ui_widget_exel_state_bind(Evisum_Ui_Widget_Exel *wx, unsigned int *fields_mask, int *field_widths) {
    int i;

    if (!wx) return;

    if (fields_mask) {
        wx->p.fields_mask = fields_mask;
        wx->owns_state = EINA_FALSE;
    } else {
        wx->p.fields_mask = &wx->fields_mask_state;
        wx->owns_state = EINA_TRUE;
    }

    if (field_widths) {
        wx->p.field_widths = field_widths;
    } else {
        for (i = wx->p.field_first; i < wx->p.field_max; i++) {
            wx->field_widths_state[i] = 0;
        }
        wx->p.field_widths = wx->field_widths_state;
        if (!fields_mask) wx->owns_state = EINA_TRUE;
    }

    _evisum_ui_widget_exel_fields_sync(wx);
}

void
evisum_ui_widget_exel_field_bounds_set(Evisum_Ui_Widget_Exel *wx, int field_first, int field_max) {
    if (!wx) return;
    if (field_first < 0) field_first = 0;
    if (field_first >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) field_first = EVISUM_UI_WIDGET_EXEL_FIELDS_MAX - 1;
    if (field_max <= field_first) field_max = field_first + 1;
    if (field_max > EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) field_max = EVISUM_UI_WIDGET_EXEL_FIELDS_MAX;

    wx->p.field_first = field_first;
    wx->p.field_max = field_max;
    for (int i = wx->p.field_first; i < wx->p.field_max; i++) {
        if (!wx->p.field_order) wx->field_order_state[i] = i;
        else if ((wx->p.field_order[i] < wx->p.field_first) || (wx->p.field_order[i] >= wx->p.field_max))
            wx->p.field_order[i] = i;
    }
    _evisum_ui_widget_exel_field_order_sanitize(wx);
    _evisum_ui_widget_exel_fields_sync(wx);
}

void
evisum_ui_widget_exel_field_order_bind(Evisum_Ui_Widget_Exel *wx, int *field_order) {
    if (!wx) return;

    if (field_order) wx->p.field_order = field_order;
    else wx->p.field_order = wx->field_order_state;
    _evisum_ui_widget_exel_field_order_sanitize(wx);
}

void
evisum_ui_widget_exel_field_reorder_enabled_set(Evisum_Ui_Widget_Exel *wx, Eina_Bool enabled) {
    if (!wx) return;
    wx->reorder_enabled = enabled;
}

void
evisum_ui_widget_exel_resize_hit_width_set(Evisum_Ui_Widget_Exel *wx, int hit_width) {
    if (!wx) return;
    if (hit_width < 0) hit_width = 0;
    wx->p.resize_hit_width = hit_width;
}

void
evisum_ui_widget_exel_callbacks_set(Evisum_Ui_Widget_Exel *wx,
                                    Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb reference_mask_get_cb,
                                    Evisum_Ui_Widget_Exel_Fields_Changed_Cb fields_changed_cb,
                                    Evisum_Ui_Widget_Exel_Fields_Applied_Cb fields_applied_cb,
                                    Evisum_Ui_Widget_Exel_Resize_Live_Cb resize_live_cb,
                                    Evisum_Ui_Widget_Exel_Resize_Done_Cb resize_done_cb,
                                    Evisum_Ui_Widget_Exel_Fields_Reordered_Cb fields_reordered_cb, void *data) {
    if (!wx) return;
    wx->p.reference_mask_get_cb = reference_mask_get_cb;
    wx->p.fields_changed_cb = fields_changed_cb;
    wx->p.fields_applied_cb = fields_applied_cb;
    wx->p.resize_live_cb = resize_live_cb;
    wx->p.resize_done_cb = resize_done_cb;
    wx->p.fields_reordered_cb = fields_reordered_cb;
    wx->p.data = data;
}

void
evisum_ui_widget_exel_free(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return;
    if (wx->unrealized_release_enabled && wx->glist) {
        evas_object_smart_callback_del_full(wx->glist, "unrealized", _evisum_ui_widget_exel_genlist_unrealized_cb, wx);
    }
    if (wx->win_mouse_move_registered && wx->p.win) {
        evas_object_event_callback_del_full(wx->p.win, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_widget_exel_win_mouse_move_cb,
                                            wx);
    }
    evisum_ui_widget_exel_fields_menu_dismiss(wx);
    evisum_ui_widget_exel_deferred_call_cancel(wx);
    _evisum_ui_widget_exel_drag_proxy_del(wx);
    if (wx->cache) evisum_ui_item_cache_free(wx->cache);
    if (wx->root) evas_object_del(wx->root);
    wx->header_tb = NULL;
    wx->glist = NULL;
    free(wx);
}

void
evisum_ui_widget_exel_field_register(Evisum_Ui_Widget_Exel *wx, int id, Evas_Object *btn, const char *desc,
                                     int default_width, int min_width, Eina_Bool always_visible) {
    Exel_Field *f;

    if (!wx) return;
    if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return;

    f = &wx->fields[id];
    f->id = id;
    f->btn = btn;
    f->desc = desc;
    f->default_width = default_width;
    f->min_width = min_width;
    f->always_visible = always_visible;

    _evisum_ui_widget_exel_fields_sync(wx);
}

void
evisum_ui_widget_exel_field_row_def_set(Evisum_Ui_Widget_Exel *wx, int id,
                                        const Evisum_Ui_Widget_Exel_Item_Cell_Def *def) {
    Exel_Field *f;

    if (!wx || !def) return;
    if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return;

    f = &wx->fields[id];
    if (f->id != id) return;
    f->row_def = *def;
    f->row_def_set = EINA_TRUE;
}

Eina_Bool
evisum_ui_widget_exel_field_enabled_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
    if (!f) return 0;
    return f->enabled;
}

static int
_evisum_ui_widget_exel_last_visible_field_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return 0;

    for (int i = wx->p.field_max - 1; i >= wx->p.field_first; i--) {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        if (evisum_ui_widget_exel_field_enabled_get(wx, id)) return id;
    }

    return wx->p.field_first;
}

Eina_Bool
evisum_ui_widget_exel_field_is_last_visible(const Evisum_Ui_Widget_Exel *wx, int id) {
    if (!wx) return 0;
    return _evisum_ui_widget_exel_last_visible_field_get(wx) == id;
}

int
evisum_ui_widget_exel_fields_pack_visible(Evisum_Ui_Widget_Exel *wx, Evas_Object *tb, int col_start) {
    int col = col_start;

    if (!wx || !tb) return col_start;

    _evisum_ui_widget_exel_fields_sync(wx);

    for (int i = wx->p.field_first; i < wx->p.field_max; i++) {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->btn) continue;

        if (!f->enabled) {
            evas_object_hide(f->btn);
            continue;
        }

        elm_table_pack(tb, f->btn, col++, 0, 1, 1);
        evas_object_show(f->btn);
    }

    return col;
}

Evas_Coord
evisum_ui_widget_exel_field_width_get(const Evisum_Ui_Widget_Exel *wx, int id) {
    Exel_Field *f;
    Evas_Coord w = 0;

    if (!wx) return 0;

    f = _evisum_ui_widget_exel_field_get(wx, id);
    if (!f || !f->btn) return 0;

    evas_object_geometry_get(f->btn, NULL, NULL, &w, NULL);
    return w;
}

void
evisum_ui_widget_exel_field_min_width_set(Evisum_Ui_Widget_Exel *wx, int id, Evas_Coord width) {
    Exel_Field *f;

    if (!wx) return;

    f = _evisum_ui_widget_exel_field_get(wx, id);
    if (!f || !f->btn) return;

    evas_object_size_hint_min_set(f->btn, width, 1);
}

void
evisum_ui_widget_exel_field_width_apply(Evisum_Ui_Widget_Exel *wx, int id) {
    Evas_Object *btn;
    int width;

    if (!wx) return;
    if (!_evisum_ui_widget_exel_field_id_valid(wx, id) || !wx->p.field_widths) return;

    {
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        btn = f ? f->btn : NULL;
    }
    if (!btn) return;

    width = wx->p.field_widths[id];
    if (width < _evisum_ui_widget_exel_field_min_width_get(wx, id))
        width = _evisum_ui_widget_exel_field_default_width_get(wx, id);

    evas_object_size_hint_min_set(btn, width, 1);
}

void
evisum_ui_widget_exel_field_proportions_apply(Evisum_Ui_Widget_Exel *wx) {
    double total = 0.0;
    int i;

    if (!wx) return;

    _evisum_ui_widget_exel_fields_sync(wx);

    for (i = wx->p.field_first; i < wx->p.field_max; i++) {
        if (!evisum_ui_widget_exel_field_enabled_get(wx, i)) continue;
        if (!wx->p.field_widths[i]) wx->p.field_widths[i] = _evisum_ui_widget_exel_field_default_width_get(wx, i);
        total += wx->p.field_widths[i];
    }

    if (total <= 0.0) total = 1.0;

    for (i = wx->p.field_first; i < wx->p.field_max; i++) {
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, i);
        Evas_Object *btn = f ? f->btn : NULL;
        if (!btn) continue;

        if (evisum_ui_widget_exel_field_enabled_get(wx, i))
            evas_object_size_hint_weight_set(btn, wx->p.field_widths[i] / total, 0.0);
        else evas_object_size_hint_weight_set(btn, 0.0, 0.0);
    }
}

void
evisum_ui_widget_exel_cmd_width_sync(Evisum_Ui_Widget_Exel *wx, int field_cmd_id, Evas_Coord width) {
    int minw;

    if (!wx) return;
    if (!_evisum_ui_widget_exel_field_id_valid(wx, field_cmd_id) || !wx->p.field_widths) return;

    minw = _evisum_ui_widget_exel_field_min_width_get(wx, field_cmd_id);
    if (width < minw) width = minw;

    if (wx->p.field_widths[field_cmd_id] >= width) return;

    wx->p.field_widths[field_cmd_id] = width;
    evisum_ui_widget_exel_field_width_apply(wx, field_cmd_id);
    evisum_ui_widget_exel_field_proportions_apply(wx);
}

void
evisum_ui_widget_exel_field_resize_attach(Evisum_Ui_Widget_Exel *wx, Evas_Object *btn, int field_id) {
    if (!wx || !btn) return;

    evas_object_data_set(btn, "field_id", (void *) (intptr_t) field_id);
    evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_DOWN, _evisum_ui_widget_exel_field_header_mouse_down_cb,
                                   wx);
}

static void
_evisum_ui_widget_exel_field_resize_mouse_move(Evisum_Ui_Widget_Exel *wx, Evas_Event_Mouse_Move *ev) {
    Evas_Coord width;

    if (!wx || !ev) return;
    if (!wx->resizing || !wx->resize_btn) return;
    if (!_evisum_ui_widget_exel_field_id_valid(wx, wx->resize_field) || !wx->p.field_widths) return;

    width = wx->resize_start_w + (wx->resize_dir * (ev->cur.canvas.x - wx->resize_start_x));
    if (width < _evisum_ui_widget_exel_field_min_width_get(wx, wx->resize_field))
        width = _evisum_ui_widget_exel_field_min_width_get(wx, wx->resize_field);

    evas_object_size_hint_min_set(wx->resize_btn, width, 1);
    wx->p.field_widths[wx->resize_field] = width;
    evisum_ui_widget_exel_field_proportions_apply(wx);
    if (wx->glist) elm_genlist_realized_items_update(wx->glist);

    if (wx->p.resize_live_cb) wx->p.resize_live_cb(wx->p.data);
}

static void
_evisum_ui_widget_exel_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                         void *event_info) {
    Evisum_Ui_Widget_Exel *wx = data;
    Evas_Event_Mouse_Move *ev = event_info;

    if (!wx || !ev) return;

    if (wx->drag_pending) {
        Evas_Coord dx = ev->cur.canvas.x - wx->drag_start_x;
        Evas_Coord dy = ev->cur.canvas.y - wx->drag_start_y;
        if ((abs(dx) >= ELM_SCALE_SIZE(EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD))
            || (abs(dy) >= ELM_SCALE_SIZE(EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD))) {
            wx->drag_pending = 0;
            wx->dragging = 1;
            wx->ignore_sort_click = 1;
            _evisum_ui_widget_exel_drag_proxy_create(wx);
        }
    }
    if (wx->dragging && !wx->drag_proxy) _evisum_ui_widget_exel_drag_proxy_create(wx);
    if (wx->dragging) _evisum_ui_widget_exel_drag_proxy_move(wx, ev->cur.canvas.x, ev->cur.canvas.y);

    _evisum_ui_widget_exel_field_resize_mouse_move(wx, ev);
}

void
evisum_ui_widget_exel_field_resize_mouse_up(Evisum_Ui_Widget_Exel *wx, Evas_Event_Mouse_Up *ev) {
    Evas_Coord w;

    if (!wx || !ev) return;
    if (ev->button != 1) return;
    if (!wx->resizing || !wx->resize_btn) return;
    if (!_evisum_ui_widget_exel_field_id_valid(wx, wx->resize_field) || !wx->p.field_widths) {
        wx->resizing = 0;
        wx->resize_btn = NULL;
        return;
    }

    evas_object_geometry_get(wx->resize_btn, NULL, NULL, &w, NULL);
    wx->p.field_widths[wx->resize_field] = w;
    evisum_ui_widget_exel_field_proportions_apply(wx);
    if (wx->glist) elm_genlist_realized_items_update(wx->glist);

    wx->resizing = 0;
    wx->resize_btn = NULL;
    wx->ignore_sort_click = 1;

    if (wx->p.resize_done_cb) wx->p.resize_done_cb(wx->p.data);
}

Eina_Bool
evisum_ui_widget_exel_field_reorder_mouse_up(Evisum_Ui_Widget_Exel *wx, Evas_Event_Mouse_Up *ev) {
    Eina_Bool handled = EINA_FALSE;

    if (!wx || !ev) return EINA_FALSE;
    if (ev->button != 1) return EINA_FALSE;

    if (wx->dragging) handled = _evisum_ui_widget_exel_reorder_commit(wx, ev->canvas.x);
    else if (wx->drag_pending) handled = EINA_FALSE;

    _evisum_ui_widget_exel_drag_proxy_del(wx);
    wx->drag_pending = 0;
    wx->dragging = 0;
    wx->drag_field = 0;

    return handled;
}

Eina_Bool
evisum_ui_widget_exel_sort_ignore_consume(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return 0;
    if (!wx->ignore_sort_click) return 0;
    wx->ignore_sort_click = 0;
    return 1;
}

void
evisum_ui_widget_exel_fields_menu_show(Evisum_Ui_Widget_Exel *wx, Evas_Object *anchor_btn, Evas_Event_Mouse_Up *ev) {
    Evas_Coord ox, oy, ow, oh;
    Evas_Object *o;

    if (!wx || !anchor_btn || !ev) return;
    if (ev->button != 3) return;
    if (wx->fields_menu) return;

    _evisum_ui_widget_exel_fields_sync(wx);
    _evisum_ui_widget_exel_fields_changed_notify(wx);

    evas_object_geometry_get(anchor_btn, &ox, &oy, &ow, &oh);

    o = wx->fields_menu = _evisum_ui_widget_exel_fields_menu_create(wx);
    if (!o) {
        wx->fields_menu = NULL;
        return;
    }
    elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_DOWN, ELM_CTXPOPUP_DIRECTION_UP,
                                        ELM_CTXPOPUP_DIRECTION_LEFT, ELM_CTXPOPUP_DIRECTION_RIGHT);
    evas_object_move(o, ox + (ow / 2), oy + oh);
    evas_object_show(o);
}

void
evisum_ui_widget_exel_fields_menu_dismiss(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return;
    if (!wx->fields_menu) return;

    elm_ctxpopup_dismiss(wx->fields_menu);
    wx->fields_menu = NULL;
}

Eina_Bool
evisum_ui_widget_exel_fields_menu_visible_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return 0;
    return wx->fields_menu != NULL;
}

Evas_Object *
evisum_ui_widget_exel_item_column_add(Evas_Object *tb, const char *key, int col,
                                      const Evisum_Ui_Widget_Exel_Column_Params *params) {
    Evas_Object *lb, *rec;

    if (!params) return NULL;

    if (params->boxed) {
        Evas_Object *hbx;

        hbx = elm_box_add(tb);
        elm_box_horizontal_set(hbx, 1);
        evas_object_size_hint_align_set(hbx, params->align_x, params->align_y);
        evas_object_size_hint_weight_set(hbx, params->weight_x, params->weight_y);

        lb = elm_label_add(tb);
        evas_object_data_set(tb, key, lb);
        evas_object_size_hint_align_set(lb, params->align_x, params->align_y);
        evas_object_size_hint_weight_set(lb, params->weight_x, params->weight_y);
        elm_box_pack_end(hbx, lb);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, params->boxed_spacer, 1);
        elm_box_pack_end(hbx, rec);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_data_set(lb, "rec", rec);
        elm_table_pack(tb, rec, col, 0, 1, 1);
        elm_table_pack(tb, hbx, col, 0, 1, 1);
        evas_object_show(hbx);
    } else {
        lb = elm_label_add(tb);
        evas_object_size_hint_align_set(lb, params->align_x, params->align_y);
        evas_object_size_hint_weight_set(lb, params->weight_x, params->weight_y);
        evas_object_data_set(tb, key, lb);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_data_set(lb, "rec", rec);
        elm_table_pack(tb, lb, col, 0, 1, 1);
        elm_table_pack(tb, rec, col, 0, 1, 1);
    }

    evas_object_show(lb);

    return lb;
}

Evas_Object *
evisum_ui_widget_exel_item_row_add(Evas_Object *parent, const Evisum_Ui_Widget_Exel_Item_Cell_Def *defs,
                                   unsigned int count) {
    Evas_Object *tb, *cell;
    Evisum_Ui_Widget_Exel_Column_Params p;
    unsigned int i;

    if (!parent || !defs || !count) return NULL;

    tb = elm_table_add(parent);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_size_hint_weight_set(tb, EXPAND, 0);

    for (i = 0; i < count; i++) {
        const Evisum_Ui_Widget_Exel_Item_Cell_Def *d = &defs[i];

        switch (d->type) {
            case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_CMD:
                evisum_ui_widget_exel_item_cmd_column_add(tb, d->aux_key ? d->aux_key : "icon", d->key ? d->key : "cmd",
                                                          i, d->icon_size > 0 ? d->icon_size : 16,
                                                          d->spacer > 0 ? d->spacer : 4);
                break;
            case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS:
                evisum_ui_widget_exel_item_progress_column_add(tb, d->key, i, d->unit_format);
                break;
            case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT:
            default:
                p = (Evisum_Ui_Widget_Exel_Column_Params) {
                    .boxed = d->boxed,
                    .boxed_spacer = d->spacer,
                    .align_x = d->align_x,
                    .align_y = FILL,
                    .weight_x = d->weight_x,
                    .weight_y = EXPAND,
                };
                cell = evisum_ui_widget_exel_item_column_add(tb, d->key, i, &p);
                if (cell) evas_object_size_hint_align_set(cell, d->align_x, FILL);
                break;
        }
    }

    return tb;
}

Evas_Object *
evisum_ui_widget_exel_item_cmd_column_add(Evas_Object *tb, const char *icon_key, const char *label_key, int col,
                                          int icon_size, int spacer) {
    Evas_Object *hbx, *ibox, *ic, *lb, *rec;

    if (!tb || !icon_key || !label_key) return NULL;

    hbx = elm_box_add(tb);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_align_set(hbx, 0.0, FILL);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);

    ibox = elm_table_add(tb);
    evas_object_size_hint_weight_set(ibox, 0.0, 0.0);
    evas_object_size_hint_align_set(ibox, 0.5, 0.5);
    evas_object_size_hint_min_set(ibox, ELM_SCALE_SIZE(icon_size), ELM_SCALE_SIZE(icon_size));
    evas_object_size_hint_max_set(ibox, ELM_SCALE_SIZE(icon_size), ELM_SCALE_SIZE(icon_size));
    elm_box_pack_end(hbx, ibox);
    evas_object_show(ibox);

    /* Always keep a fixed icon slot so rows stay aligned when icon content is missing. */
    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_color_set(rec, 0, 0, 0, 0);
    evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
    evas_object_size_hint_align_set(rec, FILL, FILL);
    elm_table_pack(ibox, rec, 0, 0, 1, 1);
    evas_object_show(rec);

    ic = elm_icon_add(tb);
    evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
    evas_object_size_hint_align_set(ic, FILL, FILL);
    evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(icon_size), ELM_SCALE_SIZE(icon_size));
    evas_object_size_hint_max_set(ic, ELM_SCALE_SIZE(icon_size), ELM_SCALE_SIZE(icon_size));
    evas_object_data_set(tb, icon_key, ic);
    elm_table_pack(ibox, ic, 0, 0, 1, 1);
    evas_object_show(ic);

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_color_set(rec, 0, 0, 0, 0);
    evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(spacer), 1);
    elm_box_pack_end(hbx, rec);

    lb = elm_label_add(tb);
    evas_object_size_hint_weight_set(lb, 0, EXPAND);
    evas_object_data_set(tb, label_key, lb);
    elm_box_pack_end(hbx, lb);
    evas_object_show(lb);

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_color_set(rec, 0, 0, 0, 0);
    evas_object_data_set(ic, "rec", rec);
    elm_table_pack(tb, rec, col, 0, 1, 1);
    elm_table_pack(tb, hbx, col, 0, 1, 1);
    evas_object_show(hbx);

    return lb;
}

Evas_Object *
evisum_ui_widget_exel_item_progress_column_add(Evas_Object *tb, const char *key, int col, const char *unit_format) {
    Evas_Object *hbx, *pb, *rec;

    if (!tb || !key) return NULL;

    hbx = elm_box_add(tb);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_weight_set(hbx, 1.0, 1.0);
    evas_object_size_hint_align_set(hbx, FILL, FILL);

    pb = elm_progressbar_add(hbx);
    evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(pb, FILL, FILL);
    if (unit_format) elm_progressbar_unit_format_set(pb, unit_format);
    elm_box_pack_end(hbx, pb);
    evas_object_show(pb);
    evas_object_show(hbx);

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_color_set(rec, 0, 0, 0, 0);
    evas_object_data_set(pb, "rec", rec);
    elm_table_pack(tb, rec, col, 0, 1, 1);
    elm_table_pack(tb, hbx, col, 0, 1, 1);
    evas_object_data_set(tb, key, pb);

    return pb;
}

Eina_Bool
evisum_ui_widget_exel_item_text_object_if_changed_set(Evas_Object *obj, const char *text) {
    const char *cur;

    if (!obj || !text) return 0;

    cur = elm_object_text_get(obj);
    if (cur && !strcmp(cur, text)) return 0;

    elm_object_text_set(obj, text);
    return 1;
}

Eina_Bool
evisum_ui_widget_exel_item_progress_if_changed_set(Evas_Object *pb, double value, const char *status) {
    double cur;

    if (!pb) return 0;

    cur = elm_progressbar_value_get(pb);
    if (EINA_DBL_EQ(cur, value)) {
        if (status) {
            const char *old = elm_object_part_text_get(pb, "elm.text.status");
            if ((!old) || strcmp(old, status)) {
                elm_object_part_text_set(pb, "elm.text.status", status);
                return 1;
            }
        }
        return 0;
    }

    elm_progressbar_value_set(pb, value);
    if (status) elm_object_part_text_set(pb, "elm.text.status", status);

    return 1;
}

Evas_Object *
evisum_ui_widget_exel_item_object_get(Evas_Object *row, const char *key) {
    if (!row || !key) return NULL;
    return evas_object_data_get(row, key);
}

Evas_Object *
evisum_ui_widget_exel_item_field_cell_get(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id, const char *key) {
    Evas_Object *cell;
    Evas_Coord w;

    if (!wx || !row || !key) return NULL;

    cell = evisum_ui_widget_exel_item_object_get(row, key);
    if (!cell) return NULL;

    w = evisum_ui_widget_exel_field_width_get(wx, field_id);
    if (!w) return NULL;

    evisum_ui_widget_exel_item_column_width_apply(cell, w, evisum_ui_widget_exel_field_is_last_visible(wx, field_id));
    return cell;
}

Eina_Bool
evisum_ui_widget_exel_item_field_text_set(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id, const char *key,
                                          const char *text) {
    Evas_Object *cell;

    cell = evisum_ui_widget_exel_item_field_cell_get(wx, row, field_id, key);
    if (!cell) return 0;

    return evisum_ui_widget_exel_item_text_object_if_changed_set(cell, text);
}

Eina_Bool
evisum_ui_widget_exel_item_field_progress_set(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id,
                                              const char *key, double value, const char *status) {
    Evas_Object *cell;

    cell = evisum_ui_widget_exel_item_field_cell_get(wx, row, field_id, key);
    if (!cell) return 0;

    return evisum_ui_widget_exel_item_progress_if_changed_set(cell, value, status);
}

void
evisum_ui_widget_exel_item_column_width_apply(Evas_Object *obj, Evas_Coord width, Eina_Bool is_last) {
    Evas_Object *rec;

    if (!obj) return;

    rec = evas_object_data_get(obj, "rec");
    if (!rec) return;

    if (!is_last) evas_object_size_hint_min_set(rec, width, 1);
    else {
        evas_object_size_hint_min_set(rec, 1, 1);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
    }
}

static void
_evisum_ui_widget_exel_glist_ensure_n_items(Evas_Object *glist, unsigned int items, Elm_Genlist_Item_Class *itc) {
    if (!glist || !itc) return;
    Elm_Object_Item *it;
    unsigned int i, existing = elm_genlist_items_count(glist);

    if (items < existing) {
        for (i = existing - items; i > 0; i--) {
            it = elm_genlist_last_item_get(glist);
            if (it) elm_object_item_del(it);
        }
    }

    if (items == existing) return;

    for (i = existing; i < items; i++) {
        elm_genlist_item_append(glist, itc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
    }
}

Elm_Object_Item *
evisum_ui_widget_exel_menu_item_add(Evas_Object *menu, Elm_Object_Item *parent, const char *icon, const char *label,
                                    Evas_Smart_Cb func, const void *data) {
    Evas_Object *ic;
    Elm_Object_Item *it;

    if (!menu) return NULL;
    if (!icon) return elm_menu_item_add(menu, parent, NULL, label, func, data);

    it = elm_menu_item_add(menu, parent, NULL, label, func, data);
    if (!it) return NULL;

    ic = elm_icon_add(menu);
    {
        const char *theme_icon;

        theme_icon = evisum_ui_icon_name_get(icon);
        if ((!theme_icon) || strncmp(theme_icon, "evisum/icons/", 13))
            theme_icon = evisum_ui_icon_name_get("application");
        evisum_ui_icon_set(ic, theme_icon);
    }
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    evas_object_show(ic);

    elm_object_item_part_content_set(it, "elm.swallow.icon", ic);
    if (!elm_object_item_part_content_get(it, "elm.swallow.icon")) elm_object_item_part_content_set(it, "icon", ic);
    if (!elm_object_item_part_content_get(it, "icon")) elm_object_item_part_content_set(it, "elm.swallow.start", ic);

    return it;
}

void
evisum_ui_widget_exel_sort_header_button_state_set(Evas_Object *btn, Eina_Bool reverse) {
    Evas_Object *ic;

    if (!btn) return;

    ic = elm_icon_add(btn);
    if (reverse) elm_icon_standard_set(ic, "go-down");
    else elm_icon_standard_set(ic, "go-up");

    elm_object_part_content_set(btn, "icon", ic);
    evas_object_data_set(btn, "sort_reverse", (void *) (intptr_t) reverse);
    evas_object_show(ic);
}

Evas_Object *
evisum_ui_widget_exel_sort_header_button_add(Evas_Object *parent, const char *text, Eina_Bool reverse, double weight_x,
                                             double align_x, Evas_Smart_Cb clicked_cb, const void *data) {
    Evas_Object *btn;

    btn = elm_button_add(parent);
    evas_object_size_hint_weight_set(btn, weight_x, EXPAND);
    evas_object_size_hint_align_set(btn, align_x, FILL);
    elm_object_text_set(btn, text);
    evisum_ui_widget_exel_sort_header_button_state_set(btn, reverse);
    if (clicked_cb) evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

    return btn;
}

Evas_Object *
evisum_ui_widget_exel_icon_button_add(Evas_Object *parent, const char *icon, const char *tooltip, double weight_x,
                                      double align_x, Evas_Smart_Cb clicked_cb, const void *data) {
    Evas_Object *btn, *ic;

    btn = elm_button_add(parent);
    evas_object_size_hint_weight_set(btn, weight_x, EXPAND);
    evas_object_size_hint_align_set(btn, align_x, FILL);

    ic = elm_icon_add(btn);
    if (icon && !strcmp(icon, "menu")) elm_icon_standard_set(ic, "menu");
    else evisum_ui_icon_set(ic, icon);
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    elm_object_part_content_set(btn, "icon", ic);
    evas_object_show(ic);

    if (tooltip) elm_object_tooltip_text_set(btn, tooltip);
    if (clicked_cb) evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

    return btn;
}

static void
_evisum_ui_widget_exel_item_cache_add(Evisum_Ui_Widget_Exel *wx, Evas_Object *parent,
                                      Evas_Object *(*create_cb)(Evas_Object *), int size) {
    if (!wx) return;
    if (!parent || !create_cb || size <= 0) return;

    if (wx->cache) evisum_ui_item_cache_free(wx->cache);

    wx->cache = evisum_ui_item_cache_new(parent, create_cb, size);
}

static Evas_Object *
_evisum_ui_widget_exel_item_create_from_fields(Evas_Object *parent) {
    Evisum_Ui_Widget_Exel *wx = evas_object_data_get(parent, "widget_exel");
    Evisum_Ui_Widget_Exel_Item_Cell_Def defs[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX];
    unsigned int n = 0;

    if (!wx) return NULL;

    _evisum_ui_widget_exel_fields_sync(wx);

    for (int i = wx->p.field_first; i < wx->p.field_max; i++) {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->row_def_set || !f->enabled) continue;
        defs[n++] = f->row_def;
    }

    if (!n) return NULL;
    return evisum_ui_widget_exel_item_row_add(parent, defs, n);
}

static Eina_Bool
_evisum_ui_widget_exel_item_cache_item_release(Evisum_Ui_Widget_Exel *wx, Evas_Object *obj) {
    if (!wx || !wx->cache) return 0;
    return evisum_ui_item_cache_item_release(wx->cache, obj);
}

Evas_Object *
evisum_ui_widget_exel_item_cache_object_get(Evisum_Ui_Widget_Exel *wx) {
    Item_Cache *it;

    if (!wx) return NULL;
    if (!wx->cache && wx->glist) {
        _evisum_ui_widget_exel_item_cache_add(wx, wx->glist, _evisum_ui_widget_exel_item_create_from_fields,
                                              EVISUM_UI_WIDGET_EXEL_DEFAULT_CACHE_SIZE);
    }
    if (!wx->cache) return NULL;
    it = evisum_ui_item_cache_item_get(wx->cache);
    if (!it) return NULL;

    return it->obj;
}
void
evisum_ui_widget_exel_item_cache_reset(Evisum_Ui_Widget_Exel *wx, void (*done_cb)(void *data), void *data) {
    if (!wx || !wx->cache) return;
    evisum_ui_item_cache_reset(wx->cache, done_cb, data);
}

void
evisum_ui_widget_exel_item_cache_steal(Evisum_Ui_Widget_Exel *wx, Eina_List *objs) {
    if (!wx || !wx->cache) return;
    evisum_ui_item_cache_steal(wx->cache, objs);
}

unsigned int
evisum_ui_widget_exel_item_cache_active_count_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->cache) return 0;
    return eina_list_count(wx->cache->active);
}

static void
_evisum_ui_widget_exel_genlist_unrealized_release_enable(Evisum_Ui_Widget_Exel *wx, Eina_Bool delete_unhandled);

Evas_Object *
evisum_ui_widget_exel_genlist_add(Evisum_Ui_Widget_Exel *wx, Evas_Object *parent) {
    Evas_Object *glist;

    if (!wx || !parent) return NULL;

    glist = elm_genlist_add(parent);
    elm_genlist_homogeneous_set(glist, EINA_TRUE);
    elm_scroller_bounce_set(glist, EINA_FALSE, EINA_FALSE);
    elm_object_focus_allow_set(glist, EINA_TRUE);
    elm_scroller_policy_set(glist, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_AUTO);
    elm_genlist_multi_select_set(glist, EINA_FALSE);
    elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_DEFAULT);
    evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
    evas_object_size_hint_align_set(glist, FILL, FILL);
    evas_object_data_set(glist, "widget_exel", wx);
    if (wx->p.data) evas_object_data_set(glist, "widget_exel_data", wx->p.data);
    evas_object_show(glist);

    wx->glist = glist;
    _evisum_ui_widget_exel_genlist_unrealized_release_enable(wx, EINA_TRUE);
    return glist;
}

Evas_Object *
evisum_ui_widget_exel_genlist_obj_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return NULL;
    return wx->glist;
}

void
evisum_ui_widget_exel_genlist_event_callback_add(Evisum_Ui_Widget_Exel *wx, Evas_Callback_Type type,
                                                 Evas_Object_Event_Cb cb, const void *data) {
    if (!wx || !wx->glist || !cb) return;
    evas_object_event_callback_add(wx->glist, type, cb, data);
}

void
evisum_ui_widget_exel_genlist_clear(Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->glist) return;
    elm_genlist_clear(wx->glist);
}

void
evisum_ui_widget_exel_genlist_items_ensure(Evisum_Ui_Widget_Exel *wx, unsigned int items, Elm_Genlist_Item_Class *itc) {
    if (!wx || !wx->glist) return;
    _evisum_ui_widget_exel_glist_ensure_n_items(wx->glist, items, itc);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_first_item_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->glist) return NULL;
    return elm_genlist_first_item_get(wx->glist);
}

void
evisum_ui_widget_exel_genlist_realized_items_update(Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->glist) return;
    elm_genlist_realized_items_update(wx->glist);
}

Eina_List *
evisum_ui_widget_exel_genlist_realized_items_get(const Evisum_Ui_Widget_Exel *wx) {
    if (!wx || !wx->glist) return NULL;
    return elm_genlist_realized_items_get(wx->glist);
}

void
evisum_ui_widget_exel_genlist_page_bring_in(Evisum_Ui_Widget_Exel *wx, int h_pagenumber, int v_pagenumber) {
    if (!wx || !wx->glist) return;
    elm_scroller_page_bring_in(wx->glist, h_pagenumber, v_pagenumber);
}

void
evisum_ui_widget_exel_genlist_region_get(const Evisum_Ui_Widget_Exel *wx, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w,
                                         Evas_Coord *h) {
    if (!wx || !wx->glist) return;
    elm_scroller_region_get(wx->glist, x, y, w, h);
}

void
evisum_ui_widget_exel_genlist_region_bring_in(Evisum_Ui_Widget_Exel *wx, Evas_Coord x, Evas_Coord y, Evas_Coord w,
                                              Evas_Coord h) {
    if (!wx || !wx->glist) return;
    elm_scroller_region_bring_in(wx->glist, x, y, w, h);
}

void
evisum_ui_widget_exel_genlist_policy_set(Evisum_Ui_Widget_Exel *wx, Elm_Scroller_Policy policy_h,
                                         Elm_Scroller_Policy policy_v) {
    if (!wx || !wx->glist) return;
    elm_scroller_policy_set(wx->glist, policy_h, policy_v);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_at_xy_item_get(Evisum_Ui_Widget_Exel *wx, int x, int y, int *posret) {
    if (!wx || !wx->glist) return NULL;
    return elm_genlist_at_xy_item_get(wx->glist, x, y, posret);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_next_get(Elm_Object_Item *it) {
    if (!it) return NULL;
    return elm_genlist_item_next_get(it);
}

void
evisum_ui_widget_exel_genlist_item_selected_set(Elm_Object_Item *it, Eina_Bool selected) {
    if (!it) return;
    elm_genlist_item_selected_set(it, selected);
}

void
evisum_ui_widget_exel_genlist_item_update(Elm_Object_Item *it) {
    if (!it) return;
    elm_genlist_item_update(it);
}

static void
_evisum_ui_widget_exel_genlist_item_contents_release(Evisum_Ui_Widget_Exel *wx, Elm_Object_Item *it,
                                                     Eina_Bool delete_unhandled) {
    Eina_List *contents = NULL;
    Evas_Object *obj;

    if (!it) return;
    elm_genlist_item_all_contents_unset(it, &contents);

    EINA_LIST_FREE(contents, obj) {
        if (_evisum_ui_widget_exel_item_cache_item_release(wx, obj)) continue;
        if (delete_unhandled) evas_object_del(obj);
    }
}

static void
_evisum_ui_widget_exel_genlist_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Widget_Exel *wx = data;
    _evisum_ui_widget_exel_genlist_item_contents_release(wx, event_info, wx->unrealized_delete_unhandled);
}

static void
_evisum_ui_widget_exel_genlist_unrealized_release_enable(Evisum_Ui_Widget_Exel *wx, Eina_Bool delete_unhandled) {
    if (!wx || !wx->glist) return;

    if (!wx->unrealized_release_enabled) {
        evas_object_smart_callback_add(wx->glist, "unrealized", _evisum_ui_widget_exel_genlist_unrealized_cb, wx);
        wx->unrealized_release_enabled = EINA_TRUE;
    }
    wx->unrealized_delete_unhandled = delete_unhandled;
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_append(Evisum_Ui_Widget_Exel *wx, Elm_Genlist_Item_Class *itc, const void *data,
                                          Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Evas_Smart_Cb func,
                                          const void *func_data) {
    if (!wx || !wx->glist || !itc) return NULL;
    return elm_genlist_item_append(wx->glist, itc, data, parent, type, func, (void *) func_data);
}

void *
evisum_ui_widget_exel_object_item_data_get(Elm_Object_Item *it) {
    if (!it) return NULL;
    return elm_object_item_data_get(it);
}

void
evisum_ui_widget_exel_object_item_data_set(Elm_Object_Item *it, void *data) {
    if (!it) return;
    elm_object_item_data_set(it, data);
}

void
evisum_ui_widget_exel_deferred_call_schedule(Evisum_Ui_Widget_Exel *wx, double delay_seconds, void (*cb)(void *data),
                                             void *data) {
    if (!wx || !cb) return;

    if (wx->deferred_timer) ecore_timer_del(wx->deferred_timer);

    wx->deferred_cb = cb;
    wx->deferred_data = data;
    wx->deferred_timer = ecore_timer_add(delay_seconds, _evisum_ui_widget_exel_deferred_timer_cb, wx);
}

void
evisum_ui_widget_exel_deferred_call_cancel(Evisum_Ui_Widget_Exel *wx) {
    if (!wx) return;
    if (wx->deferred_timer) ecore_timer_del(wx->deferred_timer);
    wx->deferred_timer = NULL;
    wx->deferred_cb = NULL;
    wx->deferred_data = NULL;
}
