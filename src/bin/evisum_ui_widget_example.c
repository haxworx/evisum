#include <Elementary.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ui/evisum_ui_widget_exel.h"

typedef enum { EX_FIELD_CMD = 0, EX_FIELD_MEM_SIZE, EX_FIELD_CPU_USAGE, EX_FIELD_MAX } Example_Field;

typedef struct {
    char *cmd;
    unsigned long long mem_size_kib;
    double cpu_usage;
} Example_Row;

typedef struct {
    Evas_Object *win;
    Evas_Object *header_tb;
    Evisum_Ui_Widget_Exel *wx;
    Elm_Genlist_Item_Class itc;
    Ecore_Timer *timer;
    unsigned int fields_mask;
    int field_widths[EX_FIELD_MAX];
} Example_App;

static Example_Row *
_row_new(const char *cmd, unsigned long long mem_kib, double cpu_usage) {
    Example_Row *row = calloc(1, sizeof(Example_Row));
    if (!row) return NULL;
    row->cmd = strdup(cmd);
    row->mem_size_kib = mem_kib;
    row->cpu_usage = cpu_usage;
    return row;
}

static void
_row_free(Example_Row *row) {
    if (!row) return;
    free(row->cmd);
    free(row);
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED) {
    _row_free(data);
}

static Evas_Object *
_item_factory(Evas_Object *parent) {
    Evisum_Ui_Widget_Exel_Item_Cell_Def defs[] = {
        { .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
         .key = "cmd",
         .align_x = 0.0,
         .weight_x = EVAS_HINT_EXPAND,
         .boxed = 1,
         .spacer = 6,
         .icon_size = 0 },
        { .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
         .key = "mem_size",
         .align_x = 1.0,
         .weight_x = EVAS_HINT_EXPAND,
         .boxed = 1,
         .spacer = 6,
         .icon_size = 0 },
        { .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
         .key = "cpu_usage",
         .unit_format = "%1.1f %%",
         .align_x = EVAS_HINT_FILL,
         .weight_x = EVAS_HINT_EXPAND,
         .boxed = 0,
         .spacer = 0,
         .icon_size = 0 },
    };

    return evisum_ui_widget_exel_item_row_add(parent, defs, 3);
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source) {
    Example_Row *row = data;
    Example_App *app;
    Evas_Object *item;
    char buf[64];

    if (strcmp(source, "elm.swallow.content")) return NULL;
    if (!row) return NULL;

    app = evas_object_data_get(obj, "app");
    if (!app) return NULL;

    item = evisum_ui_widget_exel_item_cache_object_get(app->wx);
    if (!item) return NULL;

    evisum_ui_widget_exel_item_field_text_set(app->wx, item, EX_FIELD_CMD, "cmd", row->cmd);

    snprintf(buf, sizeof(buf), "%llu MiB", row->mem_size_kib / 1024ULL);
    evisum_ui_widget_exel_item_field_text_set(app->wx, item, EX_FIELD_MEM_SIZE, "mem_size", buf);

    snprintf(buf, sizeof(buf), "%1.1f %%", row->cpu_usage);
    evisum_ui_widget_exel_item_field_progress_set(app->wx, item, EX_FIELD_CPU_USAGE, "cpu_usage",
                                                  row->cpu_usage / 100.0, buf);

    return item;
}

static void
_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Example_App *app = data;
    Eina_List *contents = NULL;
    Evas_Object *content;

    evisum_ui_widget_exel_genlist_item_all_contents_unset(event_info, &contents);

    EINA_LIST_FREE(contents, content) {
        if (!evisum_ui_widget_exel_item_cache_item_release(app->wx, content)) evas_object_del(content);
    }
}

static unsigned int
_reference_mask_get_cb(void *data) {
    Example_App *app = data;
    return app->fields_mask;
}

static void
_fields_changed_cb(void *data EINA_UNUSED, Eina_Bool changed EINA_UNUSED) {}

static void
_fields_applied_cb(void *data, Eina_Bool changed) {
    Example_App *app = data;
    if (!changed) return;
    evisum_ui_widget_exel_item_cache_reset(app->wx, NULL, NULL);
}

static void
_resize_live_cb(void *data) {
    Example_App *app = data;
    evisum_ui_widget_exel_genlist_realized_items_update(app->wx);
}

static void
_resize_done_cb(void *data EINA_UNUSED) {}

static void
_header_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info) {
    Example_App *app = data;
    Evas_Event_Mouse_Up *ev = event_info;
    evisum_ui_widget_exel_field_resize_mouse_up(app->wx, ev);
    evisum_ui_widget_exel_fields_menu_show(app->wx, obj, ev);
}

static void
_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Example_App *app = data;
    Evas_Event_Mouse_Move *ev = event_info;
    evisum_ui_widget_exel_field_resize_mouse_move(app->wx, ev);
}

static Eina_Bool
_update_timer_cb(void *data) {
    Example_App *app = data;
    Elm_Object_Item *it = evisum_ui_widget_exel_genlist_first_item_get(app->wx);

    while (it) {
        Example_Row *row = evisum_ui_widget_exel_object_item_data_get(it);
        if (row) {
            row->cpu_usage += (rand() % 11) - 5;
            if (row->cpu_usage < 0.0) row->cpu_usage = 0.0;
            if (row->cpu_usage > 100.0) row->cpu_usage = 100.0;
            row->mem_size_kib += (rand() % 1024);
        }
        evisum_ui_widget_exel_genlist_item_update(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }

    return ECORE_CALLBACK_RENEW;
}

static void
_fill_demo_rows(Example_App *app) {
    Example_Row *rows[3];
    Elm_Object_Item *it;

    evisum_ui_widget_exel_genlist_items_ensure(app->wx, 3, &app->itc);
    it = evisum_ui_widget_exel_genlist_first_item_get(app->wx);

    rows[0] = _row_new("evisum", 512000, 24.3);
    rows[1] = _row_new("clang", 342000, 8.1);
    rows[2] = _row_new("python", 768000, 57.8);

    for (int i = 0; i < 3 && it; i++) {
        Example_Row *old = evisum_ui_widget_exel_object_item_data_get(it);
        if (old) _row_free(old);
        evisum_ui_widget_exel_object_item_data_set(it, rows[i]);
        evisum_ui_widget_exel_genlist_item_update(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Example_App *app = data;

    if (app->timer) ecore_timer_del(app->timer);

    evisum_ui_widget_exel_genlist_clear(app->wx);
    evisum_ui_widget_exel_free(app->wx);
    free(app);
    elm_exit();
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED) {
    Example_App *app = calloc(1, sizeof(Example_App));
    Evas_Object *root;
    Evas_Object *glist;
    Evas_Object *btn_cmd;
    Evas_Object *btn_mem;
    Evas_Object *btn_cpu;
    Evisum_Ui_Widget_Exel_Params wp;
    Evisum_Ui_Widget_Exel_Genlist_Params gp;

    if (!app) return EXIT_FAILURE;

    srand((unsigned int) time(NULL));

    app->fields_mask = (1u << EX_FIELD_CMD) | (1u << EX_FIELD_MEM_SIZE) | (1u << EX_FIELD_CPU_USAGE);
    app->field_widths[EX_FIELD_CMD] = 160;
    app->field_widths[EX_FIELD_MEM_SIZE] = 120;
    app->field_widths[EX_FIELD_CPU_USAGE] = 150;

    app->win = elm_win_util_standard_add("evisum_test", "Widget Example");
    elm_win_autodel_set(app->win, EINA_TRUE);
    evas_object_event_callback_add(app->win, EVAS_CALLBACK_DEL, _win_del_cb, app);
    evas_object_event_callback_add(app->win, EVAS_CALLBACK_MOUSE_MOVE, _win_mouse_move_cb, app);

    root = elm_box_add(app->win);
    evas_object_size_hint_weight_set(root, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(root, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(root);
    elm_win_resize_object_add(app->win, root);

    app->header_tb = elm_table_add(root);
    evas_object_size_hint_weight_set(app->header_tb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(app->header_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_show(app->header_tb);
    elm_box_pack_end(root, app->header_tb);

    btn_cmd = evisum_ui_widget_exel_sort_header_button_add(app->header_tb, "command", EINA_FALSE, 1.0, EVAS_HINT_FILL,
                                                           NULL, app);
    btn_mem = evisum_ui_widget_exel_sort_header_button_add(app->header_tb, "mem size", EINA_FALSE, 1.0, EVAS_HINT_FILL,
                                                           NULL, app);
    btn_cpu = evisum_ui_widget_exel_sort_header_button_add(app->header_tb, "cpu %", EINA_FALSE, 1.0, EVAS_HINT_FILL,
                                                           NULL, app);

    wp.win = app->win;
    wp.parent = app->win;
    wp.field_first = EX_FIELD_CMD;
    wp.field_max = EX_FIELD_MAX;
    wp.resize_hit_width = 8;
    wp.fields_mask = &app->fields_mask;
    wp.field_widths = app->field_widths;
    wp.reference_mask_get_cb = _reference_mask_get_cb;
    wp.fields_changed_cb = _fields_changed_cb;
    wp.fields_applied_cb = _fields_applied_cb;
    wp.resize_live_cb = _resize_live_cb;
    wp.resize_done_cb = _resize_done_cb;
    wp.data = app;

    app->wx = evisum_ui_widget_exel_add(&wp);
    if (!app->wx) return EXIT_FAILURE;

    evisum_ui_widget_exel_field_register(app->wx, EX_FIELD_CMD, btn_cmd, "Command", 160, 90, EINA_TRUE);
    evisum_ui_widget_exel_field_register(app->wx, EX_FIELD_MEM_SIZE, btn_mem, "Memory Size", 120, 80, EINA_FALSE);
    evisum_ui_widget_exel_field_register(app->wx, EX_FIELD_CPU_USAGE, btn_cpu, "CPU Usage", 150, 90, EINA_FALSE);

    evisum_ui_widget_exel_field_resize_attach(app->wx, btn_cmd, EX_FIELD_CMD);
    evisum_ui_widget_exel_field_resize_attach(app->wx, btn_mem, EX_FIELD_MEM_SIZE);
    evisum_ui_widget_exel_field_resize_attach(app->wx, btn_cpu, EX_FIELD_CPU_USAGE);
    evas_object_event_callback_add(btn_cmd, EVAS_CALLBACK_MOUSE_UP, _header_mouse_up_cb, app);
    evas_object_event_callback_add(btn_mem, EVAS_CALLBACK_MOUSE_UP, _header_mouse_up_cb, app);
    evas_object_event_callback_add(btn_cpu, EVAS_CALLBACK_MOUSE_UP, _header_mouse_up_cb, app);

    evisum_ui_widget_exel_field_width_apply(app->wx, EX_FIELD_CMD);
    evisum_ui_widget_exel_field_width_apply(app->wx, EX_FIELD_MEM_SIZE);
    evisum_ui_widget_exel_field_width_apply(app->wx, EX_FIELD_CPU_USAGE);
    evisum_ui_widget_exel_field_proportions_apply(app->wx);
    evisum_ui_widget_exel_fields_pack_visible(app->wx, app->header_tb, 0);

    gp.homogeneous = EINA_TRUE;
    gp.focus_allow = EINA_TRUE;
    gp.multi_select = EINA_FALSE;
    gp.bounce_h = EINA_FALSE;
    gp.bounce_v = EINA_FALSE;
    gp.select_mode = ELM_OBJECT_SELECT_MODE_NONE;
    gp.policy_h = ELM_SCROLLER_POLICY_OFF;
    gp.policy_v = ELM_SCROLLER_POLICY_AUTO;
    gp.weight_x = EVAS_HINT_EXPAND;
    gp.weight_y = EVAS_HINT_EXPAND;
    gp.align_x = EVAS_HINT_FILL;
    gp.align_y = EVAS_HINT_FILL;
    gp.data_key = "app";
    gp.data = app;

    glist = evisum_ui_widget_exel_genlist_add(app->wx, root, &gp);
    elm_box_pack_end(root, glist);

    app->itc.item_style = "full";
    app->itc.func.text_get = NULL;
    app->itc.func.content_get = _content_get;
    app->itc.func.filter_get = NULL;
    app->itc.func.del = _item_del;

    evisum_ui_widget_exel_genlist_smart_callback_add(app->wx, "unrealized", _unrealized_cb, app);
    evisum_ui_widget_exel_item_cache_attach(app->wx, _item_factory, 16);

    _fill_demo_rows(app);
    app->timer = ecore_timer_add(1.0, _update_timer_cb, app);

    evas_object_resize(app->win, 680, 360);
    evas_object_show(app->win);
    elm_run();

    return EXIT_SUCCESS;
}
ELM_MAIN()
