#include "evisum_ui_widget_exel.h"
#include "evisum_ui_util.h"
#include "evisum_ui_cache.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
   Eina_Bool boxed;
   Evas_Coord boxed_spacer;
   double align_x;
   double align_y;
   double weight_x;
   double weight_y;
} Evisum_Ui_Widget_Exel_Column_Params;

/* Implementation helpers; consumers use the smart-object API below. */
static void
_evisum_ui_widget_exel_field_register(Evas_Object *obj, int id, Evas_Object *btn, const char *desc,
                                     int default_width, int min_width, Eina_Bool always_visible);

static int
_evisum_ui_widget_exel_fields_pack_visible(Evas_Object *obj, Evas_Object *tb, int col_start);

static void
_evisum_ui_widget_exel_field_width_apply(Evas_Object *obj, int id);

static void
_evisum_ui_widget_exel_field_proportions_apply(Evas_Object *obj);

static void
_evisum_ui_widget_exel_field_resize_attach(Evas_Object *obj, Evas_Object *btn, int field_id);

static void
_evisum_ui_widget_exel_field_resize_mouse_up(Evas_Object *obj, Evas_Event_Mouse_Up *ev);

static Eina_Bool
_evisum_ui_widget_exel_field_reorder_mouse_up(Evas_Object *obj, Evas_Event_Mouse_Up *ev);

static Evas_Object *
_evisum_ui_widget_exel_item_column_add(Evas_Object *tb, const char *key, int col,
                                      const Evisum_Ui_Widget_Exel_Column_Params *params);

static Evas_Object *
_evisum_ui_widget_exel_item_row_add(Evas_Object *parent, const Evisum_Ui_Widget_Exel_Item_Cell_Def *defs,
                                   unsigned int count);

static Evas_Object *
_evisum_ui_widget_exel_item_cmd_column_add(Evas_Object *tb, const char *icon_key, const char *label_key, int col,
                                          int icon_size, int spacer);

static Evas_Object *
_evisum_ui_widget_exel_item_progress_column_add(Evas_Object *tb, const char *key, int col, const char *unit_format);

static Eina_Bool
_evisum_ui_widget_exel_item_progress_if_changed_set(Evas_Object *pb, double value, const char *status);

static Evas_Object *
_evisum_ui_widget_exel_sort_header_button_add(Evas_Object *parent, const char *text, Eina_Bool reverse, double weight_x,
                                             double align_x, Evas_Smart_Cb clicked_cb, const void *data);

static Evas_Object *
_evisum_ui_widget_exel_genlist_add(Evas_Object *obj, Evas_Object *parent);

#define EVISUM_UI_WIDGET_EXEL_FIELDS_MAX 64
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_FIRST 0
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_MAX   32
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_HIT_WIDTH   8
#define EVISUM_UI_WIDGET_EXEL_DEFAULT_CACHE_SIZE  30
#define EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD      6
#define EVISUM_UI_WIDGET_EXEL_CURSOR_RESIZE_H     "sb_h_double_arrow"

typedef struct
{
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

typedef struct
{
   Evas_Object *win;
   Evas_Object *parent;
   int field_first;
   int field_max;
   int resize_hit_width;
   unsigned int *fields_mask;
   int *field_widths;
   int *field_order;
   Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb reference_mask_get_cb;
   void *reference_mask_data;
} Evisum_Ui_Widget_Exel_Params;

typedef struct
{
   Evas_Object *obj;
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
   Eina_Bool fields_menu_apply_requested;
   Eina_Bool owns_state;
   Eina_Bool resize_cursor_active;
   Eina_Bool unrealized_release_enabled;
   Eina_Bool unrealized_delete_unhandled;
   unsigned int fields_menu_mask_snapshot;
   Evas_Object *header_content;
   Evas_Object *footer;
   Eina_Bool deleting;
} Exel_Data;

#define EXEL_SMART_NAME "evisum_ui_widget_exel"

static Exel_Data *
_exel_data_get(const Evas_Object *obj)
{
   if (!obj || !evas_object_smart_type_check(obj, EXEL_SMART_NAME)) return NULL;
   return evas_object_smart_data_get(obj);
}

static Exel_Field *
_evisum_ui_widget_exel_field_get(const Exel_Data *wx, int id)
{
   if (!wx) return NULL;
   if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return NULL;
   if (wx->fields[id].id != id) return NULL;
   return (Exel_Field *) &wx->fields[id];
}

static void
_evisum_ui_widget_exel_field_order_sanitize(Exel_Data *wx)
{
   unsigned char seen[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX] = { 0 };
   int out[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX] = { 0 };
   int pos;

   if (!wx || !wx->p.field_order) return;

   /* Keep the first field pinned (e.g. command column). */
   out[wx->p.field_first] = wx->p.field_first;
   seen[wx->p.field_first] = 1;
   pos = wx->p.field_first + 1;
   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        int id = wx->p.field_order[i];
        if (id < wx->p.field_first || id >= wx->p.field_max) continue;
        if (seen[id]) continue;
        out[pos++] = id;
        seen[id] = 1;
     }

   for (int id = wx->p.field_first; id < wx->p.field_max; id++)
     {
        if (seen[id]) continue;
        out[pos++] = id;
     }

   for (int i = wx->p.field_first; i < wx->p.field_max; i++) wx->p.field_order[i] = out[i];
}

static int
_evisum_ui_widget_exel_field_order_pos_get(const Exel_Data *wx, int id)
{
   if (!wx || !wx->p.field_order) return -1;
   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        if (wx->p.field_order[i] == id) return i;
     }
   return -1;
}

static Eina_Bool
_evisum_ui_widget_exel_field_order_move_to_pos(Exel_Data *wx, int id, int to_pos)
{
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
   if (from_pos < to_pos)
     {
        for (int i = from_pos; i < to_pos; i++) wx->p.field_order[i] = wx->p.field_order[i + 1];
     }
   else
     {
        for (int i = from_pos; i > to_pos; i--) wx->p.field_order[i] = wx->p.field_order[i - 1];
     }
   wx->p.field_order[to_pos] = value;

   return EINA_TRUE;
}

static Eina_Bool
_evisum_ui_widget_exel_field_id_valid(const Exel_Data *wx, int id)
{
   if (!wx) return EINA_FALSE;
   if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return EINA_FALSE;
   return EINA_TRUE;
}

static int
_evisum_ui_widget_exel_field_prev_visible_get(const Exel_Data *wx, int id)
{
   int pos;

   if (!wx || !wx->p.field_order) return 0;

   pos = _evisum_ui_widget_exel_field_order_pos_get(wx, id);
   if (pos < 0) return 0;

   for (int i = pos - 1; i >= wx->p.field_first; i--)
     {
        int prev_id = wx->p.field_order[i];
        if (evisum_ui_widget_exel_field_enabled_get(wx->obj, prev_id)) return prev_id;
     }

   return 0;
}

static int
_evisum_ui_widget_exel_field_min_width_get(const Exel_Data *wx, int id)
{
   Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
   if (!f) return ELM_SCALE_SIZE(48);
   return f->min_width;
}

static int
_evisum_ui_widget_exel_field_default_width_get(const Exel_Data *wx, int id)
{
   Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
   if (!f) return ELM_SCALE_SIZE(80);
   return f->default_width;
}

static void
_evisum_ui_widget_exel_fields_sync(Exel_Data *wx)
{
   int i;

   if (!wx || !wx->p.fields_mask) return;
   _evisum_ui_widget_exel_field_order_sanitize(wx);

   for (i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, i);
        if (!f) continue;

        f->enabled = 1;
        if (!f->always_visible && !(*(wx->p.fields_mask) & (1U << i))) f->enabled = 0;
     }
}

static Eina_Bool
_evisum_ui_widget_exel_deferred_timer_cb(void *data)
{
   Exel_Data *wx = data;
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
_evisum_ui_widget_exel_dirty_get(Exel_Data *wx)
{
   unsigned int ref;

   if (!wx->p.reference_mask_get_cb) return wx->fields_changed;
   if (!wx->p.fields_mask) return wx->fields_changed;

   ref = wx->p.reference_mask_get_cb(wx->p.reference_mask_data);
   return ref != *(wx->p.fields_mask);
}

static void
_evisum_ui_widget_exel_fields_changed_notify(Exel_Data *wx)
{
   if (!wx) return;
   wx->fields_changed = _evisum_ui_widget_exel_dirty_get(wx);
   if (!wx->deleting) evas_object_smart_callback_call(wx->obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_CHANGED, &wx->fields_changed);
}

static void
_evisum_ui_widget_exel_fields_icon_mouse_in_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj,
                                               void *event_info EINA_UNUSED)
{
   evas_object_color_set(obj, 128, 128, 128, 255);
}

static void
_evisum_ui_widget_exel_fields_icon_mouse_out_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj,
                                                void *event_info EINA_UNUSED)
{
   evas_object_color_set(obj, 255, 255, 255, 255);
}

static void
_evisum_ui_widget_exel_fields_menu_close_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                    void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   evisum_ui_widget_exel_fields_menu_dismiss(wx->obj);
}

static void
_evisum_ui_widget_exel_fields_menu_apply_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                    void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   Eina_Bool changed;

   changed = wx->fields_changed;
   wx->fields_menu_apply_requested = EINA_TRUE;
   evisum_ui_widget_exel_fields_menu_dismiss(wx->obj);

   if (changed && wx->owns_state)
     {
        evisum_ui_widget_exel_fields_apply(wx->obj);
        if (wx->cache) evisum_ui_item_cache_reset(wx->cache, NULL, NULL);
        if (wx->glist) elm_genlist_realized_items_update(wx->glist);
     }

   if (!wx->deleting) evas_object_smart_callback_call(wx->obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_APPLIED, &changed);
}

static void
_evisum_ui_widget_exel_fields_menu_check_changed_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Exel_Field *f = data;
   Exel_Data *wx;
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

static void
_evisum_ui_widget_exel_fields_menu_deleted_cb(void *data, Evas *e EINA_UNUSED,
                                             Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   if (wx->fields_menu == obj) wx->fields_menu = NULL;
}

static void
_evisum_ui_widget_exel_fields_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                               void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   evisum_ui_widget_exel_fields_menu_dismiss(wx->obj);
}

static Evas_Object *
_evisum_ui_widget_exel_fields_menu_create(Exel_Data *wx)
{
   Evas_Object *o, *fr, *hbx, *pad, *ic, *ic2, *bx, *ck;
   int i;

   o = elm_ctxpopup_add(wx->p.parent);
   if (!o) return NULL;
   evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
   evas_object_size_hint_align_set(o, FILL, FILL);
   elm_object_style_set(o, "noblock");
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                  _evisum_ui_widget_exel_fields_menu_deleted_cb, wx);
   evas_object_smart_callback_add(o, "dismissed",
                                  _evisum_ui_widget_exel_fields_menu_dismissed_cb, wx);

   fr = elm_frame_add(o);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(o);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(fr, bx);
   evas_object_show(fr);

   hbx = elm_box_add(o);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_align_set(hbx, FILL, FILL);

   pad = elm_box_add(o);
   evas_object_size_hint_weight_set(pad, EXPAND, 0);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   elm_box_pack_end(hbx, pad);
   evas_object_show(pad);

   ic = elm_icon_add(o);
   evisum_ui_icon_set(ic, "apply");
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   elm_box_pack_end(hbx, ic);
   evas_object_smart_callback_add(ic, "clicked", _evisum_ui_widget_exel_fields_menu_apply_clicked_cb, wx);
   evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_IN, _evisum_ui_widget_exel_fields_icon_mouse_in_cb, NULL);
   evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_OUT, _evisum_ui_widget_exel_fields_icon_mouse_out_cb, NULL);

   ic2 = elm_icon_add(o);
   evisum_ui_icon_set(ic2, "exit");
   evas_object_size_hint_min_set(ic2, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_show(ic2);
   elm_box_pack_end(hbx, ic2);
   evas_object_smart_callback_add(ic2, "clicked", _evisum_ui_widget_exel_fields_menu_close_clicked_cb, wx);
   evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_IN, _evisum_ui_widget_exel_fields_icon_mouse_in_cb, NULL);
   evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_OUT, _evisum_ui_widget_exel_fields_icon_mouse_out_cb, NULL);

   elm_box_pack_end(bx, hbx);
   evas_object_show(hbx);

   for (i = wx->p.field_first + 1; i < wx->p.field_max; i++)
     {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->btn || !f->desc) continue;

        ck = elm_check_add(o);
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

   elm_object_content_set(o, fr);

   if (!wx->fields_changed) evas_object_hide(ic);

   return o;
}

static int
_evisum_ui_widget_exel_drop_pos_get(const Exel_Data *wx, Evas_Coord x)
{
   int reorder_min_pos;
   int last_visible;

   if (!wx || !wx->p.field_order) return -1;
   reorder_min_pos = wx->p.field_first + 1;
   if (reorder_min_pos >= wx->p.field_max) reorder_min_pos = wx->p.field_first;
   last_visible = reorder_min_pos;

   for (int i = reorder_min_pos; i < wx->p.field_max; i++)
     {
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
_evisum_ui_widget_exel_reorder_commit(Exel_Data *wx, Evas_Coord x)
{
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
   if (wx->header_tb)
     {
        elm_table_clear(wx->header_tb, EINA_FALSE);
        if (wx->header_content) elm_table_pack(wx->header_tb, wx->header_content, 0, 0, 1, 1);
        _evisum_ui_widget_exel_fields_pack_visible(wx->obj, wx->header_tb, wx->header_content ? 1 : 0);
     }

   if (!wx->deleting) evas_object_smart_callback_call(wx->obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_REORDERED, NULL);

   return EINA_TRUE;
}

static void
_evisum_ui_widget_exel_drag_proxy_del(Exel_Data *wx)
{
   if (!wx) return;
   if (!wx->drag_proxy) return;
   evas_object_del(wx->drag_proxy);
   wx->drag_proxy = NULL;
}

static void
_evisum_ui_widget_exel_drag_proxy_move(Exel_Data *wx, Evas_Coord x, Evas_Coord y)
{
   if (!wx || !wx->drag_proxy) return;
   evas_object_move(wx->drag_proxy, x - wx->drag_offset_x, y - wx->drag_offset_y);
}

static void
_evisum_ui_widget_exel_drag_proxy_create(Exel_Data *wx)
{
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

   proxy = _evisum_ui_widget_exel_sort_header_button_add(wx->p.win, text ? text : "", reverse, 0.0, FILL, NULL, NULL);
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
_evisum_ui_widget_exel_field_header_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Exel_Data *wx = data;
   Evas_Event_Mouse_Down *ev = event_info;
   Evas_Coord x, w;
   int field, prev;

   if (!wx || !obj || !ev) return;
   if (ev->button != 1) return;

   evas_object_geometry_get(obj, &x, NULL, &w, NULL);
   field = (int) (intptr_t) evas_object_data_get(obj, "field_id");
   if (field < wx->p.field_first || field >= wx->p.field_max) return;
   if (field == wx->p.field_first) return;

   if (ev->canvas.x >= (x + w - ELM_SCALE_SIZE(wx->p.resize_hit_width)))
     {
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

   if (ev->canvas.x <= (x + ELM_SCALE_SIZE(wx->p.resize_hit_width)))
     {
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
_evisum_ui_widget_exel_field_header_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   Exel_Data *wx = data;
   Evas_Event_Mouse_Up *ev = event_info;

   if (!wx || !ev) return;

   if (_evisum_ui_widget_exel_field_reorder_mouse_up(wx->obj, ev)) return;
   _evisum_ui_widget_exel_field_resize_mouse_up(wx->obj, ev);
   evisum_ui_widget_exel_fields_menu_show(wx->obj, obj, ev);
}

static void
_evisum_ui_widget_exel_resize_cursor_update(Exel_Data *wx, Eina_Bool active)
{
   if (!wx || !wx->p.win) return;
   if (wx->resize_cursor_active == active) return;

   wx->resize_cursor_active = active;
   if (active) elm_object_cursor_set(wx->p.win, EVISUM_UI_WIDGET_EXEL_CURSOR_RESIZE_H);
   else elm_object_cursor_unset(wx->p.win);
}

static Eina_Bool
_evisum_ui_widget_exel_resize_handle_hit_test(const Exel_Data *wx, Evas_Coord px, Evas_Coord py)
{
   int hit_width;

   if (!wx) return EINA_FALSE;
   hit_width = ELM_SCALE_SIZE(wx->p.resize_hit_width);
   if (hit_width <= 0) return EINA_FALSE;

   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Evas_Coord x = 0, y = 0, w = 0, h = 0;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->btn || !f->enabled) continue;
        if (id == wx->p.field_first) continue;

        evas_object_geometry_get(f->btn, &x, &y, &w, &h);
        if (px < x || px > (x + w) || py < y || py > (y + h)) continue;

        if (px >= (x + w - hit_width)) return EINA_TRUE;
        if ((px <= (x + hit_width)) && (_evisum_ui_widget_exel_field_prev_visible_get(wx, id) != 0)) return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_exel_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                         void *event_info);

static void
_exel_mouse_out_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                        void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   if (!wx) return;
   _evisum_ui_widget_exel_resize_cursor_update(wx, EINA_FALSE);
}

static void
_evisum_ui_widget_exel_genlist_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info);

static void
_exel_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Exel_Data *wx = data;
   if (_evisum_ui_widget_exel_field_reorder_mouse_up(wx->obj, event_info)) return;
   _evisum_ui_widget_exel_field_resize_mouse_up(wx->obj, event_info);
}

static void _exel_smart_del(Evas_Object *obj);

static void
_exel_parent_free_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   wx->p.parent = NULL;
   wx->p.win = NULL;
   evas_object_del(wx->obj);
}

static void
_exel_root_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Exel_Data *wx = data;
   wx->root = NULL;
   if (!wx->deleting) evas_object_del(wx->obj);
}

static void
_exel_hints_changed_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *root, void *event_info EINA_UNUSED)
{
   Evas_Coord w, h;
   Exel_Data *wx = data;

   evas_object_size_hint_min_get(root, &w, &h);
   evas_object_size_hint_min_set(wx->obj, w, h);
}

static void
_exel_smart_add(Evas_Object *obj)
{
   Exel_Data *wx = calloc(1, sizeof(*wx));
   if (!wx) return;
   wx->obj = obj;
   evas_object_smart_data_set(obj, wx);
}

static void
_exel_smart_move(Evas_Object *obj, Evas_Coord x, Evas_Coord y)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_move(wx->root, x, y);
}

static void
_exel_smart_resize(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_resize(wx->root, w, h);
}

static void
_exel_smart_show(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_show(wx->root);
}

static void
_exel_smart_hide(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_hide(wx->root);
}

static void
_exel_smart_color_set(Evas_Object *obj, int r, int g, int b, int a)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_color_set(wx->root, r, g, b, a);
}

static void
_exel_smart_clip_set(Evas_Object *obj, Evas_Object *clip)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_clip_set(wx->root, clip);
}

static void
_exel_smart_clip_unset(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && wx->root) evas_object_clip_unset(wx->root);
}

static Evas_Smart *
_exel_smart_get(void)
{
   static Evas_Smart *smart;
   static const Evas_Smart_Cb_Description callbacks[] = {
      { EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_CHANGED, "Eina_Bool" },
      { EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_APPLIED, "Eina_Bool" },
      { EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_REORDERED, "" },
      { EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZE, "" },
      { EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZED, "" },
      { "selected", "Elm_Object_Item" },
      { "unselected", "Elm_Object_Item" },
      { "activated", "Elm_Object_Item" },
      { "expanded", "Elm_Object_Item" },
      { "contracted", "Elm_Object_Item" },
      { "scroll", "" },
      { "scroll,anim,stop", "" },
      { "scroll,drag,stop", "" },
      { NULL, NULL }
   };
   static const Evas_Smart_Class sc = {
      .name = EXEL_SMART_NAME,
      .version = EVAS_SMART_CLASS_VERSION,
      .add = _exel_smart_add,
      .del = _exel_smart_del,
      .move = _exel_smart_move,
      .resize = _exel_smart_resize,
      .show = _exel_smart_show,
      .hide = _exel_smart_hide,
      .color_set = _exel_smart_color_set,
      .clip_set = _exel_smart_clip_set,
      .clip_unset = _exel_smart_clip_unset,
      .callbacks = callbacks,
   };

   if (!smart) smart = evas_smart_class_new(&sc);
   return smart;
}

Evas_Object *
evisum_ui_widget_exel_add(Evas_Object *parent)
{
   Evas_Object *obj;
   Exel_Data *wx;

   if (!parent) return NULL;
   obj = evas_object_smart_add(evas_object_evas_get(parent), _exel_smart_get());
   if (!obj) return NULL;
   wx = _exel_data_get(obj);
   if (!wx)
     {
        evas_object_del(obj);
        return NULL;
     }

   wx->p.win = elm_object_top_widget_get(parent);
   wx->p.parent = parent;
   wx->p.field_first = EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_FIRST;
   wx->p.field_max = EVISUM_UI_WIDGET_EXEL_DEFAULT_FIELD_MAX;
   wx->p.resize_hit_width = EVISUM_UI_WIDGET_EXEL_DEFAULT_HIT_WIDTH;
   wx->fields_mask_state = UINT_MAX;
   for (int i = 0; i < EVISUM_UI_WIDGET_EXEL_FIELDS_MAX; i++) wx->field_order_state[i] = i;
   wx->p.fields_mask = &wx->fields_mask_state;
   wx->p.field_widths = wx->field_widths_state;
   wx->p.field_order = wx->field_order_state;
   wx->owns_state = EINA_TRUE;

   evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
   evas_object_size_hint_align_set(obj, FILL, FILL);
   wx->root = elm_box_add(parent);
   evas_object_smart_member_add(wx->root, obj);
   evas_object_size_hint_weight_set(wx->root, EXPAND, EXPAND);
   evas_object_size_hint_align_set(wx->root, FILL, FILL);
   evas_object_event_callback_add(wx->root, EVAS_CALLBACK_CHANGED_SIZE_HINTS, _exel_hints_changed_cb, wx);
   evas_object_event_callback_add(wx->root, EVAS_CALLBACK_DEL, _exel_root_del_cb, wx);
   evas_object_event_callback_add(parent, EVAS_CALLBACK_FREE, _exel_parent_free_cb, wx);

   wx->header_tb = elm_table_add(wx->root);
   evas_object_size_hint_weight_set(wx->header_tb, EXPAND, 0.0);
   evas_object_size_hint_align_set(wx->header_tb, FILL, FILL);
   evas_object_show(wx->header_tb);
   elm_box_pack_end(wx->root, wx->header_tb);
   _evisum_ui_widget_exel_genlist_add(obj, wx->root);

   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_MOVE, _exel_mouse_move_cb, wx);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_OUT, _exel_mouse_out_cb, wx);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_MOUSE_UP, _exel_mouse_up_cb, wx);
   return obj;
}

void
evisum_ui_widget_exel_header_content_set(Evas_Object *obj, Evas_Object *content)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || wx->header_content == content) return;
   if (wx->header_content) evas_object_del(wx->header_content);
   wx->header_content = content;
   if (content) evas_object_show(content);
   evisum_ui_widget_exel_fields_apply(obj);
}

void
evisum_ui_widget_exel_footer_set(Evas_Object *obj, Evas_Object *content)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || wx->footer == content) return;
   if (wx->footer) evas_object_del(wx->footer);
   wx->footer = content;
   if (content)
     {
        elm_box_pack_end(wx->root, content);
        evas_object_show(content);
     }
}

Evas_Object *
evisum_ui_widget_exel_field_add(Evas_Object *obj, const Evisum_Ui_Widget_Exel_Field_Params *params)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Object *btn;
   Evisum_Ui_Widget_Exel_Item_Cell_Def def;

   if (!wx || !params || !wx->header_tb) return NULL;
   if (params->id < wx->p.field_first || params->id >= wx->p.field_max) return NULL;

   btn = _evisum_ui_widget_exel_sort_header_button_add(wx->header_tb, params->title, params->reverse, 1.0,
                                                      EVAS_HINT_FILL, params->clicked_cb, params->clicked_data);
   if (!btn) return NULL;
   if (params->desc) elm_object_tooltip_text_set(btn, params->desc);

   _evisum_ui_widget_exel_field_register(wx->obj, params->id, btn, params->desc, params->default_width, params->min_width,
                                        params->always_visible);
   def = (Evisum_Ui_Widget_Exel_Item_Cell_Def){
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
   evisum_ui_widget_exel_field_row_def_set(wx->obj, params->id, &def);
   _evisum_ui_widget_exel_field_resize_attach(wx->obj, btn, params->id);
   evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_UP, _evisum_ui_widget_exel_field_header_mouse_up_cb, wx);
   evas_object_show(btn);
   return btn;
}

void
evisum_ui_widget_exel_fields_apply(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   int i;

   if (!wx || !wx->header_tb) return;

   for (i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        _evisum_ui_widget_exel_field_width_apply(wx->obj, i);
     }
   _evisum_ui_widget_exel_field_proportions_apply(wx->obj);
   elm_table_clear(wx->header_tb, EINA_FALSE);
   if (wx->header_content) elm_table_pack(wx->header_tb, wx->header_content, 0, 0, 1, 1);
   _evisum_ui_widget_exel_fields_pack_visible(wx->obj, wx->header_tb, wx->header_content ? 1 : 0);
}

unsigned int
evisum_ui_widget_exel_fields_mask_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->p.fields_mask) return 0;
   return *(wx->p.fields_mask);
}

void
evisum_ui_widget_exel_state_bind(Evas_Object *obj, unsigned int *fields_mask, int *field_widths)
{
   Exel_Data *wx = _exel_data_get(obj);
   int i;

   if (!wx) return;

   if (fields_mask)
     {
        wx->p.fields_mask = fields_mask;
        wx->owns_state = EINA_FALSE;
     }
   else
     {
        wx->p.fields_mask = &wx->fields_mask_state;
        wx->owns_state = EINA_TRUE;
     }

   if (field_widths)
     {
        wx->p.field_widths = field_widths;
     }
   else
     {
        for (i = wx->p.field_first; i < wx->p.field_max; i++)
          {
             wx->field_widths_state[i] = 0;
          }
        wx->p.field_widths = wx->field_widths_state;
        if (!fields_mask) wx->owns_state = EINA_TRUE;
     }

   _evisum_ui_widget_exel_fields_sync(wx);
}

void
evisum_ui_widget_exel_field_bounds_set(Evas_Object *obj, int field_first, int field_max)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   if (field_first < 0) field_first = 0;
   if (field_first >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) field_first = EVISUM_UI_WIDGET_EXEL_FIELDS_MAX - 1;
   if (field_max <= field_first) field_max = field_first + 1;
   if (field_max > EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) field_max = EVISUM_UI_WIDGET_EXEL_FIELDS_MAX;

   wx->p.field_first = field_first;
   wx->p.field_max = field_max;
   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        if (!wx->p.field_order) wx->field_order_state[i] = i;
        else if ((wx->p.field_order[i] < wx->p.field_first) || (wx->p.field_order[i] >= wx->p.field_max))
           wx->p.field_order[i] = i;
     }
   _evisum_ui_widget_exel_field_order_sanitize(wx);
   _evisum_ui_widget_exel_fields_sync(wx);
}

void
evisum_ui_widget_exel_field_order_bind(Evas_Object *obj, int *field_order)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;

   if (field_order) wx->p.field_order = field_order;
   else wx->p.field_order = wx->field_order_state;
   _evisum_ui_widget_exel_field_order_sanitize(wx);
}

void
evisum_ui_widget_exel_field_reorder_enabled_set(Evas_Object *obj, Eina_Bool enabled)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   wx->reorder_enabled = enabled;
}

void
evisum_ui_widget_exel_resize_hit_width_set(Evas_Object *obj, int hit_width)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   if (hit_width < 0) hit_width = 0;
   wx->p.resize_hit_width = hit_width;
}

void
evisum_ui_widget_exel_reference_mask_callback_set(Evas_Object *obj,
                                                 Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb cb, void *data)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   wx->p.reference_mask_get_cb = cb;
   wx->p.reference_mask_data = data;
}

static void
_exel_smart_del(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   wx->deleting = EINA_TRUE;
   if (wx->p.parent)
      evas_object_event_callback_del_full(wx->p.parent, EVAS_CALLBACK_FREE, _exel_parent_free_cb, wx);
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_MOUSE_MOVE, _exel_mouse_move_cb, wx);
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_MOUSE_OUT, _exel_mouse_out_cb, wx);
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_MOUSE_UP, _exel_mouse_up_cb, wx);
   _evisum_ui_widget_exel_resize_cursor_update(wx, EINA_FALSE);
   evisum_ui_widget_exel_fields_menu_dismiss(obj);
   evisum_ui_widget_exel_deferred_call_cancel(obj);
   _evisum_ui_widget_exel_drag_proxy_del(wx);
   /* Clearing first releases realized row contents back into the cache. */
   if (wx->glist) elm_genlist_clear(wx->glist);
   if (wx->unrealized_release_enabled && wx->glist)
      evas_object_smart_callback_del_full(wx->glist, "unrealized", _evisum_ui_widget_exel_genlist_unrealized_cb, wx);
   if (wx->cache) evisum_ui_item_cache_free(wx->cache);
   if (wx->root)
     {
        evas_object_event_callback_del_full(wx->root, EVAS_CALLBACK_DEL, _exel_root_del_cb, wx);
        evas_object_del(wx->root);
     }
   evas_object_smart_data_set(obj, NULL);
   free(wx);
}

static void
_evisum_ui_widget_exel_field_register(Evas_Object *obj, int id, Evas_Object *btn, const char *desc,
                                     int default_width, int min_width, Eina_Bool always_visible)
{
   Exel_Data *wx = _exel_data_get(obj);
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
evisum_ui_widget_exel_field_row_def_set(Evas_Object *obj, int id,
                                        const Evisum_Ui_Widget_Exel_Item_Cell_Def *def)
{
   Exel_Data *wx = _exel_data_get(obj);
   Exel_Field *f;

   if (!wx || !def) return;
   if (id < 0 || id >= EVISUM_UI_WIDGET_EXEL_FIELDS_MAX) return;

   f = &wx->fields[id];
   if (f->id != id) return;
   f->row_def = *def;
   f->row_def_set = EINA_TRUE;
}

Eina_Bool
evisum_ui_widget_exel_field_enabled_get(const Evas_Object *obj, int id)
{
   Exel_Data *wx = _exel_data_get(obj);
   Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
   if (!f) return 0;
   return f->enabled;
}

static int
_evisum_ui_widget_exel_last_visible_field_get(const Exel_Data *wx)
{
   if (!wx) return 0;

   for (int i = wx->p.field_max - 1; i >= wx->p.field_first; i--)
     {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        if (evisum_ui_widget_exel_field_enabled_get(wx->obj, id)) return id;
     }

   return wx->p.field_first;
}

Eina_Bool
evisum_ui_widget_exel_field_is_last_visible(const Evas_Object *obj, int id)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return 0;
   return _evisum_ui_widget_exel_last_visible_field_get(wx) == id;
}

static int
_evisum_ui_widget_exel_fields_pack_visible(Evas_Object *obj, Evas_Object *tb, int col_start)
{
   Exel_Data *wx = _exel_data_get(obj);
   int col = col_start;

   if (!wx || !tb) return col_start;

   _evisum_ui_widget_exel_fields_sync(wx);

   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->btn) continue;

        if (!f->enabled)
          {
             evas_object_hide(f->btn);
             continue;
          }

        elm_table_pack(tb, f->btn, col++, 0, 1, 1);
        evas_object_show(f->btn);
     }

   return col;
}

Evas_Coord
evisum_ui_widget_exel_field_width_get(const Evas_Object *obj, int id)
{
   Exel_Data *wx = _exel_data_get(obj);
   Exel_Field *f;
   Evas_Coord w = 0;

   if (!wx) return 0;

   f = _evisum_ui_widget_exel_field_get(wx, id);
   if (!f || !f->btn) return 0;

   evas_object_geometry_get(f->btn, NULL, NULL, &w, NULL);
   return w;
}

void
evisum_ui_widget_exel_field_min_width_set(Evas_Object *obj, int id, Evas_Coord width)
{
   Exel_Data *wx = _exel_data_get(obj);
   Exel_Field *f;

   if (!wx) return;

   f = _evisum_ui_widget_exel_field_get(wx, id);
   if (!f || !f->btn) return;

   evas_object_size_hint_min_set(f->btn, width, 1);
}

static void
_evisum_ui_widget_exel_field_width_apply(Evas_Object *obj, int id)
{
   Exel_Data *wx = _exel_data_get(obj);
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

static void
_evisum_ui_widget_exel_field_proportions_apply(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   double total = 0.0;
   int i;

   if (!wx) return;

   _evisum_ui_widget_exel_fields_sync(wx);

   for (i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        if (!evisum_ui_widget_exel_field_enabled_get(wx->obj, i)) continue;
        if (!wx->p.field_widths[i]) wx->p.field_widths[i] = _evisum_ui_widget_exel_field_default_width_get(wx, i);
        total += wx->p.field_widths[i];
     }

   if (total <= 0.0) total = 1.0;

   for (i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, i);
        Evas_Object *btn = f ? f->btn : NULL;
        if (!btn) continue;

        if (evisum_ui_widget_exel_field_enabled_get(wx->obj, i))
           evas_object_size_hint_weight_set(btn, wx->p.field_widths[i] / total, 0.0);
        else evas_object_size_hint_weight_set(btn, 0.0, 0.0);
     }
}

void
evisum_ui_widget_exel_cmd_width_sync(Evas_Object *obj, int field_cmd_id, Evas_Coord width)
{
   Exel_Data *wx = _exel_data_get(obj);
   int minw;

   if (!wx) return;
   if (!_evisum_ui_widget_exel_field_id_valid(wx, field_cmd_id) || !wx->p.field_widths) return;

   minw = _evisum_ui_widget_exel_field_min_width_get(wx, field_cmd_id);
   if (width < minw) width = minw;

   if (wx->p.field_widths[field_cmd_id] >= width) return;

   wx->p.field_widths[field_cmd_id] = width;
   _evisum_ui_widget_exel_field_width_apply(wx->obj, field_cmd_id);
   _evisum_ui_widget_exel_field_proportions_apply(wx->obj);
}

static void
_evisum_ui_widget_exel_field_resize_attach(Evas_Object *obj, Evas_Object *btn, int field_id)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !btn) return;

   evas_object_data_set(btn, "field_id", (void *) (intptr_t) field_id);
   evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_DOWN, _evisum_ui_widget_exel_field_header_mouse_down_cb, wx);
}

static void
_evisum_ui_widget_exel_field_resize_mouse_move(Exel_Data *wx, Evas_Event_Mouse_Move *ev)
{
   Evas_Coord width;

   if (!wx || !ev) return;
   if (!wx->resizing || !wx->resize_btn) return;
   if (!_evisum_ui_widget_exel_field_id_valid(wx, wx->resize_field) || !wx->p.field_widths) return;

   width = wx->resize_start_w + (wx->resize_dir * (ev->cur.canvas.x - wx->resize_start_x));
   if (width < _evisum_ui_widget_exel_field_min_width_get(wx, wx->resize_field))
      width = _evisum_ui_widget_exel_field_min_width_get(wx, wx->resize_field);

   evas_object_size_hint_min_set(wx->resize_btn, width, 1);
   wx->p.field_widths[wx->resize_field] = width;
   _evisum_ui_widget_exel_field_proportions_apply(wx->obj);
   if (wx->glist) elm_genlist_realized_items_update(wx->glist);

   if (!wx->deleting) evas_object_smart_callback_call(wx->obj, EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZE, NULL);
}

static void
_exel_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                         void *event_info)
{
   Exel_Data *wx = data;
   Evas_Event_Mouse_Move *ev = event_info;
   Eina_Bool on_resize_handle = EINA_FALSE;

   if (!wx || !ev) return;

   if (wx->drag_pending)
     {
        Evas_Coord dx = ev->cur.canvas.x - wx->drag_start_x;
        Evas_Coord dy = ev->cur.canvas.y - wx->drag_start_y;
        if ((abs(dx) >= ELM_SCALE_SIZE(EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD))
            || (abs(dy) >= ELM_SCALE_SIZE(EVISUM_UI_WIDGET_EXEL_DRAG_THRESHOLD)))
          {
             wx->drag_pending = 0;
             wx->dragging = 1;
             wx->ignore_sort_click = 1;
             _evisum_ui_widget_exel_drag_proxy_create(wx);
          }
     }
   if (wx->dragging && !wx->drag_proxy) _evisum_ui_widget_exel_drag_proxy_create(wx);
   if (wx->dragging) _evisum_ui_widget_exel_drag_proxy_move(wx, ev->cur.canvas.x, ev->cur.canvas.y);

   _evisum_ui_widget_exel_field_resize_mouse_move(wx, ev);

   if (wx->resizing) on_resize_handle = EINA_TRUE;
   else if (!wx->dragging && !wx->drag_pending)
      on_resize_handle = _evisum_ui_widget_exel_resize_handle_hit_test(wx, ev->cur.canvas.x, ev->cur.canvas.y);

   _evisum_ui_widget_exel_resize_cursor_update(wx, on_resize_handle);
}

static void
_evisum_ui_widget_exel_field_resize_mouse_up(Evas_Object *obj, Evas_Event_Mouse_Up *ev)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Coord w;

   if (!wx || !ev) return;
   if (ev->button != 1) return;
   if (!wx->resizing || !wx->resize_btn) return;
   if (!_evisum_ui_widget_exel_field_id_valid(wx, wx->resize_field) || !wx->p.field_widths)
     {
        wx->resizing = 0;
        wx->resize_btn = NULL;
        return;
     }

   evas_object_geometry_get(wx->resize_btn, NULL, NULL, &w, NULL);
   wx->p.field_widths[wx->resize_field] = w;
   _evisum_ui_widget_exel_field_proportions_apply(wx->obj);
   if (wx->glist) elm_genlist_realized_items_update(wx->glist);

   wx->resizing = 0;
   wx->resize_btn = NULL;
   wx->ignore_sort_click = 1;
   _evisum_ui_widget_exel_resize_cursor_update(wx, EINA_FALSE);

   if (!wx->deleting) evas_object_smart_callback_call(wx->obj, EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZED, NULL);
}

static Eina_Bool
_evisum_ui_widget_exel_field_reorder_mouse_up(Evas_Object *obj, Evas_Event_Mouse_Up *ev)
{
   Exel_Data *wx = _exel_data_get(obj);
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
evisum_ui_widget_exel_sort_ignore_consume(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return 0;
   if (!wx->ignore_sort_click) return 0;
   wx->ignore_sort_click = 0;
   return 1;
}

void
evisum_ui_widget_exel_fields_menu_show(Evas_Object *obj, Evas_Object *anchor_btn, Evas_Event_Mouse_Up *ev)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Coord ox, oy, ow, oh;
   Evas_Object *o;

   if (!wx || !anchor_btn || !ev) return;
   if (ev->button != 3) return;
   if (wx->fields_menu) return;

   wx->fields_menu_apply_requested = EINA_FALSE;
   wx->fields_menu_mask_snapshot = wx->p.fields_mask ? *(wx->p.fields_mask) : 0;

   _evisum_ui_widget_exel_fields_sync(wx);
   _evisum_ui_widget_exel_fields_changed_notify(wx);

   evas_object_geometry_get(anchor_btn, &ox, &oy, &ow, &oh);

   o = wx->fields_menu = _evisum_ui_widget_exel_fields_menu_create(wx);
   if (!o)
     {
        wx->fields_menu = NULL;
        return;
     }
   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_DOWN, ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_LEFT, ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy + oh);
   evas_object_show(o);
}

void
evisum_ui_widget_exel_fields_menu_dismiss(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   if (!wx->fields_menu) return;

   Evas_Object *menu = wx->fields_menu;
   wx->fields_menu = NULL;

   /* Restore focus before callbacks can rebuild or destroy the table. */
   evas_object_hide(menu);
   evas_object_del(menu);

   if (!wx->fields_menu_apply_requested && wx->p.fields_mask)
     {
        *(wx->p.fields_mask) = wx->fields_menu_mask_snapshot;
        _evisum_ui_widget_exel_fields_sync(wx);
        _evisum_ui_widget_exel_fields_changed_notify(wx);
     }

   wx->fields_menu_apply_requested = EINA_FALSE;
}

Eina_Bool
evisum_ui_widget_exel_fields_menu_visible_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return 0;
   return wx->fields_menu != NULL;
}

static Evas_Object *
_evisum_ui_widget_exel_item_column_add(Evas_Object *tb, const char *key, int col,
                                      const Evisum_Ui_Widget_Exel_Column_Params *params)
{
   Evas_Object *lb, *rec;

   if (!params) return NULL;

   if (params->boxed)
     {
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
     }
   else
     {
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

static Evas_Object *
_evisum_ui_widget_exel_item_row_add(Evas_Object *parent, const Evisum_Ui_Widget_Exel_Item_Cell_Def *defs,
                                   unsigned int count)
{
   Evas_Object *tb, *cell;
   Evisum_Ui_Widget_Exel_Column_Params p;
   unsigned int i;

   if (!parent || !defs || !count) return NULL;

   tb = elm_table_add(parent);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, 0);

   for (i = 0; i < count; i++)
     {
        const Evisum_Ui_Widget_Exel_Item_Cell_Def *d = &defs[i];

        switch (d->type)
          {
          case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_CMD:
             _evisum_ui_widget_exel_item_cmd_column_add(tb, d->aux_key ? d->aux_key : "icon", d->key ? d->key : "cmd",
                                                       i, d->icon_size > 0 ? d->icon_size : 16,
                                                       d->spacer > 0 ? d->spacer : 4);
             break;
          case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS:
             _evisum_ui_widget_exel_item_progress_column_add(tb, d->key, i, d->unit_format);
             break;
          case EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT:
          default:
             p = (Evisum_Ui_Widget_Exel_Column_Params){
                .boxed = d->boxed,
                .boxed_spacer = d->spacer,
                .align_x = d->align_x,
                .align_y = FILL,
                .weight_x = d->weight_x,
                .weight_y = EXPAND,
             };
             cell = _evisum_ui_widget_exel_item_column_add(tb, d->key, i, &p);
             if (cell) evas_object_size_hint_align_set(cell, d->align_x, FILL);
             break;
          }
     }

   return tb;
}

static Evas_Object *
_evisum_ui_widget_exel_item_cmd_column_add(Evas_Object *tb, const char *icon_key, const char *label_key, int col,
                                          int icon_size, int spacer)
{
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

static Evas_Object *
_evisum_ui_widget_exel_item_progress_column_add(Evas_Object *tb, const char *key, int col, const char *unit_format)
{
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
evisum_ui_widget_exel_item_text_object_if_changed_set(Evas_Object *obj, const char *text)
{
   const char *cur;

   if (!obj || !text) return 0;

   cur = elm_object_text_get(obj);
   if (cur && !strcmp(cur, text)) return 0;

   elm_object_text_set(obj, text);
   return 1;
}

static Eina_Bool
_evisum_ui_widget_exel_item_progress_if_changed_set(Evas_Object *pb, double value, const char *status)
{
   double cur;

   if (!pb) return 0;

   cur = elm_progressbar_value_get(pb);
   if (EINA_DBL_EQ(cur, value))
     {
        if (status)
          {
             const char *old = elm_object_part_text_get(pb, "elm.text.status");
             if ((!old) || strcmp(old, status))
               {
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
evisum_ui_widget_exel_item_object_get(Evas_Object *row, const char *key)
{
   if (!row || !key) return NULL;
   return evas_object_data_get(row, key);
}

Evas_Object *
evisum_ui_widget_exel_item_field_cell_get(Evas_Object *obj, Evas_Object *row, int field_id, const char *key)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Object *cell;
   Evas_Coord w;

   if (!wx || !row || !key) return NULL;

   cell = evisum_ui_widget_exel_item_object_get(row, key);
   if (!cell) return NULL;

   w = evisum_ui_widget_exel_field_width_get(wx->obj, field_id);
   if (!w) return NULL;

   evisum_ui_widget_exel_item_column_width_apply(cell, w, evisum_ui_widget_exel_field_is_last_visible(wx->obj, field_id));
   return cell;
}

Eina_Bool
evisum_ui_widget_exel_item_field_text_set(Evas_Object *obj, Evas_Object *row, int field_id, const char *key,
                                          const char *text)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Object *cell;

   cell = evisum_ui_widget_exel_item_field_cell_get(wx->obj, row, field_id, key);
   if (!cell) return 0;

   return evisum_ui_widget_exel_item_text_object_if_changed_set(cell, text);
}

Eina_Bool
evisum_ui_widget_exel_item_field_progress_set(Evas_Object *obj, Evas_Object *row, int field_id,
                                              const char *key, double value, const char *status)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Object *cell;

   cell = evisum_ui_widget_exel_item_field_cell_get(wx->obj, row, field_id, key);
   if (!cell) return 0;

   return _evisum_ui_widget_exel_item_progress_if_changed_set(cell, value, status);
}

void
evisum_ui_widget_exel_item_column_width_apply(Evas_Object *obj, Evas_Coord width, Eina_Bool is_last)
{
   Evas_Object *rec;

   if (!obj) return;

   rec = evas_object_data_get(obj, "rec");
   if (!rec) return;

   if (!is_last) evas_object_size_hint_min_set(rec, width, 1);
   else
     {
        evas_object_size_hint_min_set(rec, 1, 1);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
     }
}

static void
_evisum_ui_widget_exel_glist_ensure_n_items(Evas_Object *glist, unsigned int items, Elm_Genlist_Item_Class *itc)
{
   if (!glist || !itc) return;
   Elm_Object_Item *it;
   unsigned int i, existing = elm_genlist_items_count(glist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
          {
             it = elm_genlist_last_item_get(glist);
             if (it) elm_object_item_del(it);
          }
     }

   if (items == existing) return;

   for (i = existing; i < items; i++)
     {
        elm_genlist_item_append(glist, itc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }
}

Elm_Object_Item *
evisum_ui_widget_exel_menu_item_add(Evas_Object *menu, Elm_Object_Item *parent, const char *icon, const char *label,
                                    Evas_Smart_Cb func, const void *data)
{
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
evisum_ui_widget_exel_sort_header_button_state_set(Evas_Object *btn, Eina_Bool reverse)
{
   Evas_Object *ic;

   if (!btn) return;

   ic = elm_icon_add(btn);
   if (reverse) elm_icon_standard_set(ic, "go-down");
   else elm_icon_standard_set(ic, "go-up");

   elm_object_part_content_set(btn, "icon", ic);
   evas_object_data_set(btn, "sort_reverse", (void *) (intptr_t) reverse);
   evas_object_show(ic);
}

static Evas_Object *
_evisum_ui_widget_exel_sort_header_button_add(Evas_Object *parent, const char *text, Eina_Bool reverse, double weight_x,
                                             double align_x, Evas_Smart_Cb clicked_cb, const void *data)
{
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
                                      double align_x, Evas_Smart_Cb clicked_cb, const void *data)
{
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
_evisum_ui_widget_exel_item_cache_add(Exel_Data *wx, Evas_Object *parent,
                                      Evas_Object *(*create_cb)(Evas_Object *), int size)
{
   if (!wx) return;
   if (!parent || !create_cb || size <= 0) return;

   if (wx->cache) evisum_ui_item_cache_free(wx->cache);

   wx->cache = evisum_ui_item_cache_new(parent, create_cb, size);
}

static Evas_Object *
_evisum_ui_widget_exel_item_create_from_fields(Evas_Object *parent)
{
   Exel_Data *wx = _exel_data_get(evas_object_data_get(parent, "widget_exel"));
   Evisum_Ui_Widget_Exel_Item_Cell_Def defs[EVISUM_UI_WIDGET_EXEL_FIELDS_MAX];
   unsigned int n = 0;

   if (!wx) return NULL;

   _evisum_ui_widget_exel_fields_sync(wx);

   for (int i = wx->p.field_first; i < wx->p.field_max; i++)
     {
        int id = wx->p.field_order ? wx->p.field_order[i] : i;
        Exel_Field *f = _evisum_ui_widget_exel_field_get(wx, id);
        if (!f || !f->row_def_set || !f->enabled) continue;
        defs[n++] = f->row_def;
     }

   if (!n) return NULL;
   return _evisum_ui_widget_exel_item_row_add(parent, defs, n);
}

static Eina_Bool
_evisum_ui_widget_exel_item_cache_item_release(Exel_Data *wx, Evas_Object *obj)
{
   if (!wx || !wx->cache) return 0;
   return evisum_ui_item_cache_item_release(wx->cache, obj);
}

Evas_Object *
evisum_ui_widget_exel_item_cache_object_get(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   Item_Cache *it;

   if (!wx) return NULL;
   if (!wx->cache && wx->glist)
     {
        _evisum_ui_widget_exel_item_cache_add(wx, wx->glist, _evisum_ui_widget_exel_item_create_from_fields,
                                              EVISUM_UI_WIDGET_EXEL_DEFAULT_CACHE_SIZE);
     }
   if (!wx->cache) return NULL;
   it = evisum_ui_item_cache_item_get(wx->cache);
   if (!it) return NULL;

   return it->obj;
}
void
evisum_ui_widget_exel_item_cache_reset(Evas_Object *obj, void (*done_cb)(void *data), void *data)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   if (!wx->cache)
     {
        if (done_cb) done_cb(data);
        return;
     }
   evisum_ui_item_cache_reset(wx->cache, done_cb, data);
}

void
evisum_ui_widget_exel_item_cache_steal(Evas_Object *obj, Eina_List *items)
{
   Exel_Data *wx = _exel_data_get(obj);
   Eina_List *contents = NULL;
   Eina_List *l;
   Elm_Object_Item *it;
   Evas_Object *content;

   if (!wx || !wx->cache) return;

   EINA_LIST_FOREACH (items, l, it)
     {
        content = elm_object_item_part_content_get(it, "elm.swallow.content");
        if (content) contents = eina_list_append(contents, content);
     }

   evisum_ui_item_cache_steal(wx->cache, contents);
   eina_list_free(contents);
}

unsigned int
evisum_ui_widget_exel_item_cache_active_count_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->cache) return 0;
   return eina_list_count(wx->cache->active);
}

static void
_evisum_ui_widget_exel_genlist_unrealized_release_enable(Exel_Data *wx, Eina_Bool delete_unhandled);

static void
_exel_genlist_event_cb(void *data, Evas_Object *glist, void *event_info)
{
   Evas_Object *obj = evas_object_data_get(glist, "widget_exel");
   Exel_Data *wx = _exel_data_get(obj);
   if (wx && !wx->deleting) evas_object_smart_callback_call(obj, data, event_info);
}

static Evas_Object *
_evisum_ui_widget_exel_genlist_add(Evas_Object *obj, Evas_Object *parent)
{
   Exel_Data *wx = _exel_data_get(obj);
   Evas_Object *glist;

   if (!wx || !parent) return NULL;
   if (wx->glist) return wx->glist;

   glist = elm_genlist_add(wx->root);
   elm_genlist_homogeneous_set(glist, EINA_TRUE);
   elm_scroller_bounce_set(glist, EINA_FALSE, EINA_FALSE);
   elm_object_focus_allow_set(glist, EINA_TRUE);
   elm_scroller_policy_set(glist, ELM_SCROLLER_POLICY_AUTO, ELM_SCROLLER_POLICY_AUTO);
   elm_genlist_multi_select_set(glist, EINA_FALSE);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_DEFAULT);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   evas_object_data_set(glist, "widget_exel", obj);
   evas_object_show(glist);

   wx->glist = glist;
   /* Keep standard genlist events available on the public widget itself. */
   static const char *events[] = { "selected", "unselected", "activated", "expanded", "contracted",
                                   "scroll", "scroll,anim,stop", "scroll,drag,stop" };
   for (unsigned int i = 0; i < EINA_C_ARRAY_LENGTH(events); i++)
      evas_object_smart_callback_add(glist, events[i], _exel_genlist_event_cb, events[i]);
   elm_box_pack_end(wx->root, glist);
   _evisum_ui_widget_exel_genlist_unrealized_release_enable(wx, EINA_TRUE);
   return glist;
}

Evas_Object *
evisum_ui_widget_exel_genlist_obj_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return NULL;
   return wx->glist;
}

void
evisum_ui_widget_exel_genlist_event_callback_add(Evas_Object *obj, Evas_Callback_Type type,
                                                 Evas_Object_Event_Cb cb, const void *data)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist || !cb) return;
   evas_object_event_callback_add(wx->glist, type, cb, data);
}

void
evisum_ui_widget_exel_genlist_clear(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_genlist_clear(wx->glist);
}

void
evisum_ui_widget_exel_genlist_items_ensure(Evas_Object *obj, unsigned int items, Elm_Genlist_Item_Class *itc)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   _evisum_ui_widget_exel_glist_ensure_n_items(wx->glist, items, itc);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_first_item_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return NULL;
   return elm_genlist_first_item_get(wx->glist);
}

void
evisum_ui_widget_exel_genlist_realized_items_update(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_genlist_realized_items_update(wx->glist);
}

Eina_List *
evisum_ui_widget_exel_genlist_realized_items_get(const Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return NULL;
   return elm_genlist_realized_items_get(wx->glist);
}

void
evisum_ui_widget_exel_genlist_page_bring_in(Evas_Object *obj, int h_pagenumber, int v_pagenumber)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_scroller_page_bring_in(wx->glist, h_pagenumber, v_pagenumber);
}

void
evisum_ui_widget_exel_genlist_region_get(const Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w,
                                         Evas_Coord *h)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_scroller_region_get(wx->glist, x, y, w, h);
}

void
evisum_ui_widget_exel_genlist_region_bring_in(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w,
                                              Evas_Coord h)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_scroller_region_bring_in(wx->glist, x, y, w, h);
}

void
evisum_ui_widget_exel_genlist_policy_set(Evas_Object *obj, Elm_Scroller_Policy policy_h,
                                         Elm_Scroller_Policy policy_v)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return;
   elm_scroller_policy_set(wx->glist, policy_h, policy_v);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_at_xy_item_get(Evas_Object *obj, int x, int y, int *posret)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist) return NULL;
   return elm_genlist_at_xy_item_get(wx->glist, x, y, posret);
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_next_get(Elm_Object_Item *it)
{
   if (!it) return NULL;
   return elm_genlist_item_next_get(it);
}

void
evisum_ui_widget_exel_genlist_item_selected_set(Elm_Object_Item *it, Eina_Bool selected)
{
   if (!it) return;
   elm_genlist_item_selected_set(it, selected);
}

void
evisum_ui_widget_exel_genlist_item_update(Elm_Object_Item *it)
{
   if (!it) return;
   elm_genlist_item_update(it);
}

static void
_evisum_ui_widget_exel_genlist_item_contents_release(Exel_Data *wx, Elm_Object_Item *it,
                                                     Eina_Bool delete_unhandled)
{
   Eina_List *contents = NULL;
   Evas_Object *obj;

   if (!it) return;
   elm_genlist_item_all_contents_unset(it, &contents);

   EINA_LIST_FREE (contents, obj)
     {
        if (_evisum_ui_widget_exel_item_cache_item_release(wx, obj)) continue;
        if (delete_unhandled) evas_object_del(obj);
     }
}

static void
_evisum_ui_widget_exel_genlist_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Exel_Data *wx = data;
   _evisum_ui_widget_exel_genlist_item_contents_release(wx, event_info, wx->unrealized_delete_unhandled);
}

static void
_evisum_ui_widget_exel_genlist_unrealized_release_enable(Exel_Data *wx, Eina_Bool delete_unhandled)
{
   if (!wx || !wx->glist) return;

   if (!wx->unrealized_release_enabled)
     {
        evas_object_smart_callback_add(wx->glist, "unrealized", _evisum_ui_widget_exel_genlist_unrealized_cb, wx);
        wx->unrealized_release_enabled = EINA_TRUE;
     }
   wx->unrealized_delete_unhandled = delete_unhandled;
}

Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_append(Evas_Object *obj, Elm_Genlist_Item_Class *itc, const void *data,
                                          Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Evas_Smart_Cb func,
                                          const void *func_data)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !wx->glist || !itc) return NULL;
   return elm_genlist_item_append(wx->glist, itc, data, parent, type, func, (void *) func_data);
}

void *
evisum_ui_widget_exel_object_item_data_get(Elm_Object_Item *it)
{
   if (!it) return NULL;
   return elm_object_item_data_get(it);
}

void
evisum_ui_widget_exel_object_item_data_set(Elm_Object_Item *it, void *data)
{
   if (!it) return;
   elm_object_item_data_set(it, data);
}

void
evisum_ui_widget_exel_deferred_call_schedule(Evas_Object *obj, double delay_seconds, void (*cb)(void *data),
                                             void *data)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx || !cb) return;

   if (wx->deferred_timer) ecore_timer_del(wx->deferred_timer);

   wx->deferred_cb = cb;
   wx->deferred_data = data;
   wx->deferred_timer = ecore_timer_add(delay_seconds, _evisum_ui_widget_exel_deferred_timer_cb, wx);
}

void
evisum_ui_widget_exel_deferred_call_cancel(Evas_Object *obj)
{
   Exel_Data *wx = _exel_data_get(obj);
   if (!wx) return;
   if (wx->deferred_timer) ecore_timer_del(wx->deferred_timer);
   wx->deferred_timer = NULL;
   wx->deferred_cb = NULL;
   wx->deferred_data = NULL;
}
