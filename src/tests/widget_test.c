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
_content_get(void *data, Evas_Object *obj, const char *source) {
    Example_Row *row = data;
    Evisum_Ui_Widget_Exel *wx;
    Evas_Object *item;
    char buf[64];

    if (strcmp(source, "elm.swallow.content")) return NULL;
    if (!row) return NULL;

    wx = evas_object_data_get(obj, "widget_exel");
    if (!wx) return NULL;

    item = evisum_ui_widget_exel_item_cache_object_get(wx);
    if (!item) return NULL;

    evisum_ui_widget_exel_item_field_text_set(wx, item, EX_FIELD_CMD, "cmd", row->cmd);

    snprintf(buf, sizeof(buf), "%llu MiB", row->mem_size_kib / 1024ULL);
    evisum_ui_widget_exel_item_field_text_set(wx, item, EX_FIELD_MEM_SIZE, "mem_size", buf);

    snprintf(buf, sizeof(buf), "%1.1f %%", row->cpu_usage);
    evisum_ui_widget_exel_item_field_progress_set(wx, item, EX_FIELD_CPU_USAGE, "cpu_usage", row->cpu_usage / 100.0,
                                                  buf);

    return item;
}

static Eina_Bool
_update_timer_cb(void *data) {
    Evisum_Ui_Widget_Exel *wx = data;
    Elm_Object_Item *it = evisum_ui_widget_exel_genlist_first_item_get(wx);

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
_fill_demo_rows(Evisum_Ui_Widget_Exel *wx, Elm_Genlist_Item_Class *itc) {
    Example_Row *rows[3];
    Elm_Object_Item *it;

    evisum_ui_widget_exel_genlist_items_ensure(wx, 3, itc);
    it = evisum_ui_widget_exel_genlist_first_item_get(wx);

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
_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Ecore_Timer *timer;
    Evisum_Ui_Widget_Exel *wx;

    timer = evas_object_data_get(obj, "timer");
    wx = evas_object_data_get(obj, "wx");
    if (timer) ecore_timer_del(timer);

    if (wx) {
        evisum_ui_widget_exel_genlist_clear(wx);
        evisum_ui_widget_exel_free(wx);
    }
    elm_exit();
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED) {
    Evas_Object *win, *root;
    Evisum_Ui_Widget_Exel *wx;
    Evisum_Ui_Widget_Exel_Field_Params fp;
    Elm_Genlist_Item_Class itc;
    Ecore_Timer *timer;

    memset(&itc, 0, sizeof(itc));

    srand((unsigned int) time(NULL));

    win = elm_win_util_standard_add("widget_test", "Widget Example");
    elm_win_autodel_set(win, EINA_TRUE);
    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, NULL);

    wx = evisum_ui_widget_exel_create(win);
    if (!wx) return EXIT_FAILURE;

    evas_object_data_set(win, "wx", wx);

    root = evisum_ui_widget_exel_object_get(wx);
    elm_win_resize_object_add(win, root);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX_FIELD_CMD,
        .title = "command",
        .desc = "Command",
        .key = "cmd",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 0.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 160,
        .min_width = 90,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
    };
    evisum_ui_widget_exel_field_add(wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX_FIELD_MEM_SIZE,
        .title = "mem size",
        .desc = "Memory Size",
        .key = "mem_size",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 1.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 120,
        .min_width = 80,
        .always_visible = EINA_FALSE,
        .reverse = EINA_FALSE,
    };
    evisum_ui_widget_exel_field_add(wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX_FIELD_CPU_USAGE,
        .title = "cpu %",
        .desc = "CPU Usage",
        .key = "cpu_usage",
        .unit_format = "%1.1f %%",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
        .align_x = EVAS_HINT_FILL,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_FALSE,
        .default_width = 150,
        .min_width = 90,
        .always_visible = EINA_FALSE,
        .reverse = EINA_FALSE,
    };
    evisum_ui_widget_exel_field_add(wx, &fp);

    evisum_ui_widget_exel_fields_apply(wx);

    elm_box_pack_end(root, evisum_ui_widget_exel_genlist_add(wx, root));

    itc.item_style = "full";
    itc.func.text_get = NULL;
    itc.func.content_get = _content_get;
    itc.func.filter_get = NULL;
    itc.func.del = _item_del;

    _fill_demo_rows(wx, &itc);
    timer = ecore_timer_add(1.0, _update_timer_cb, wx);
    evas_object_data_set(win, "timer", timer);

    evas_object_resize(win, 680, 360);
    evas_object_show(win);
    elm_run();

    return EXIT_SUCCESS;
}
ELM_MAIN()
