#include <Elementary.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ui/evisum_ui_widget_exel.h"

typedef enum {
    EX2_FIELD_AINT = 0,
    EX2_FIELD_NOBODY,
    EX2_FIELD_GOT,
    EX2_FIELD_TIME,
    EX2_FIELD_FOR,
    EX2_FIELD_THAT,
    EX2_FIELD_MAX
} Example2_Field;

typedef struct {
    int index;
    pid_t base_pid;
    unsigned long long base_mem_kib;
    pid_t pid;
    unsigned long long mem_kib;
    double cpu_usage;
    char name[32];
    char tag[32];
} Example2_Row;

typedef struct {
    Evisum_Ui_Widget_Exel *wx;
    Ecore_Timer *timer;
    unsigned int tick;
    int sort_field;
    Eina_Bool sort_reverse;
} Example2_App;

static int
_row_compare(const Example2_Row *a, const Example2_Row *b, int field_id) {
    switch (field_id) {
        case EX2_FIELD_AINT:
            if (a->pid < b->pid) return -1;
            if (a->pid > b->pid) return 1;
            return 0;
        case EX2_FIELD_NOBODY:
            return strcmp(a->name, b->name);
        case EX2_FIELD_GOT:
            if (a->mem_kib < b->mem_kib) return -1;
            if (a->mem_kib > b->mem_kib) return 1;
            return 0;
        case EX2_FIELD_TIME:
            if (a->cpu_usage < b->cpu_usage) return -1;
            if (a->cpu_usage > b->cpu_usage) return 1;
            return 0;
        case EX2_FIELD_FOR:
            return strcmp(a->tag, b->tag);
        case EX2_FIELD_THAT:
        default:
            if (a->index < b->index) return -1;
            if (a->index > b->index) return 1;
            return 0;
    }
}

static void
_rows_sort_apply(Example2_App *app) {
    Example2_Row *rows[40];
    Elm_Object_Item *it;
    int count = 0;

    if (!app || !app->wx) return;

    it = evisum_ui_widget_exel_genlist_first_item_get(app->wx);
    while (it && count < 40) {
        rows[count++] = evisum_ui_widget_exel_object_item_data_get(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }

    for (int i = 1; i < count; i++) {
        Example2_Row *key = rows[i];
        int j = i - 1;
        while (j >= 0) {
            int cmp = _row_compare(rows[j], key, app->sort_field);
            if (app->sort_reverse) cmp = -cmp;
            if (cmp <= 0) break;
            rows[j + 1] = rows[j];
            j--;
        }
        rows[j + 1] = key;
    }

    it = evisum_ui_widget_exel_genlist_first_item_get(app->wx);
    for (int i = 0; i < count && it; i++) {
        evisum_ui_widget_exel_object_item_data_set(it, rows[i]);
        evisum_ui_widget_exel_genlist_item_update(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }
}

static void
_field_sort_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Example2_App *app = data;
    int field_id;

    if (!app || !obj) return;

    field_id = (int) (intptr_t) evas_object_data_get(obj, "field_id");
    if (field_id < EX2_FIELD_AINT || field_id >= EX2_FIELD_MAX) return;

    if (app->sort_field == field_id) app->sort_reverse = !app->sort_reverse;
    else {
        app->sort_field = field_id;
        app->sort_reverse = EINA_FALSE;
    }

    evisum_ui_widget_exel_sort_header_button_state_set(obj, app->sort_reverse);
    _rows_sort_apply(app);
}

static Example2_Row *
_row_new(int index) {
    Example2_Row *row;

    row = calloc(1, sizeof(Example2_Row));
    if (!row) return NULL;

    row->index = index;
    row->base_pid = 1200 + (index * 3);
    row->base_mem_kib = 256000ULL + ((unsigned long long) index * 8192ULL);
    row->pid = row->base_pid;
    row->mem_kib = row->base_mem_kib;
    row->cpu_usage = (double) ((index * 7) % 100);
    snprintf(row->name, sizeof(row->name), "task-%02d", index + 1);
    snprintf(row->tag, sizeof(row->tag), "slot-%02d", index + 1);

    return row;
}

static void
_row_free(Example2_Row *row) {
    if (!row) return;
    free(row);
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED) {
    _row_free(data);
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source) {
    Example2_Row *row = data;
    Evisum_Ui_Widget_Exel *wx;
    Evas_Object *item;
    char buf[64];

    if (strcmp(source, "elm.swallow.content")) return NULL;
    if (!row) return NULL;

    wx = evas_object_data_get(obj, "widget_exel");
    if (!wx) return NULL;

    item = evisum_ui_widget_exel_item_cache_object_get(wx);
    if (!item) return NULL;

    snprintf(buf, sizeof(buf), "%d", (int) row->pid);
    evisum_ui_widget_exel_item_field_text_set(wx, item, EX2_FIELD_AINT, "aint", buf);

    evisum_ui_widget_exel_item_field_text_set(wx, item, EX2_FIELD_NOBODY, "nobody", row->name);

    snprintf(buf, sizeof(buf), "%llu MiB", row->mem_kib / 1024ULL);
    evisum_ui_widget_exel_item_field_text_set(wx, item, EX2_FIELD_GOT, "got", buf);

    snprintf(buf, sizeof(buf), "%1.1f %%", row->cpu_usage);
    evisum_ui_widget_exel_item_field_progress_set(wx, item, EX2_FIELD_TIME, "time", row->cpu_usage / 100.0, buf);

    evisum_ui_widget_exel_item_field_text_set(wx, item, EX2_FIELD_FOR, "for", row->tag);

    snprintf(buf, sizeof(buf), "row-%02d", row->index + 1);
    evisum_ui_widget_exel_item_field_text_set(wx, item, EX2_FIELD_THAT, "that", buf);

    return item;
}

static Eina_Bool
_update_timer_cb(void *data) {
    Example2_App *app = data;
    Elm_Object_Item *it;
    unsigned int tick;

    if (!app || !app->wx) return ECORE_CALLBACK_CANCEL;

    app->tick++;
    tick = app->tick;
    it = evisum_ui_widget_exel_genlist_first_item_get(app->wx);

    while (it) {
        Example2_Row *row = evisum_ui_widget_exel_object_item_data_get(it);

        if (row) {
            row->pid = (pid_t) (row->base_pid + (pid_t) ((tick + (unsigned int) row->index) % 20));
            row->mem_kib = row->base_mem_kib + (((tick * 37U) + ((unsigned int) row->index * 59U)) % 32768U);
            row->cpu_usage = (double) (((tick * 9U) + ((unsigned int) row->index * 13U)) % 100U);
            snprintf(row->tag, sizeof(row->tag), "slot-%02u", ((tick + (unsigned int) row->index) % 40U) + 1U);
        }

        evisum_ui_widget_exel_genlist_item_update(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }

    _rows_sort_apply(app);

    return ECORE_CALLBACK_RENEW;
}

static void
_fill_rows(Example2_App *app, Elm_Genlist_Item_Class *itc) {
    Elm_Object_Item *it;
    Evisum_Ui_Widget_Exel *wx;

    if (!app || !app->wx) return;
    wx = app->wx;

    evisum_ui_widget_exel_genlist_items_ensure(wx, 40, itc);
    it = evisum_ui_widget_exel_genlist_first_item_get(wx);

    for (int i = 0; i < 40 && it; i++) {
        Example2_Row *old;
        Example2_Row *row;

        old = evisum_ui_widget_exel_object_item_data_get(it);
        if (old) _row_free(old);

        row = _row_new(i);
        evisum_ui_widget_exel_object_item_data_set(it, row);
        evisum_ui_widget_exel_genlist_item_update(it);
        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }

    _rows_sort_apply(app);
}

static void
_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Example2_App *app;

    app = evas_object_data_get(obj, "app");
    if (!app) {
        elm_exit();
        return;
    }

    if (app->timer) ecore_timer_del(app->timer);

    if (app->wx) {
        evisum_ui_widget_exel_genlist_clear(app->wx);
        evisum_ui_widget_exel_free(app->wx);
    }

    free(app);
    elm_exit();
}

EAPI_MAIN int
elm_main(int argc EINA_UNUSED, char **argv EINA_UNUSED) {
    Evas_Object *win, *root;
    Example2_App *app;
    Evisum_Ui_Widget_Exel_Field_Params fp;
    Elm_Genlist_Item_Class itc;

    memset(&itc, 0, sizeof(itc));

    win = elm_win_util_standard_add("widget_test2", "Widget Example 2");
    elm_win_autodel_set(win, EINA_TRUE);
    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, NULL);

    app = calloc(1, sizeof(Example2_App));
    if (!app) return EXIT_FAILURE;

    app->wx = evisum_ui_widget_exel_create(win);
    if (!app->wx) {
        free(app);
        return EXIT_FAILURE;
    }

    evas_object_data_set(win, "app", app);
    app->sort_field = EX2_FIELD_AINT;
    app->sort_reverse = EINA_FALSE;

    root = evisum_ui_widget_exel_object_get(app->wx);
    elm_win_resize_object_add(win, root);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_AINT,
        .title = "aint",
        .desc = "Aint",
        .key = "aint",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 1.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 90,
        .min_width = 60,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_NOBODY,
        .title = "nobody",
        .desc = "Nobody",
        .key = "nobody",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 0.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 140,
        .min_width = 90,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_GOT,
        .title = "got",
        .desc = "Got",
        .key = "got",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 1.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 110,
        .min_width = 80,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_TIME,
        .title = "time",
        .desc = "Time",
        .key = "time",
        .unit_format = "%1.1f %%",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
        .align_x = EVAS_HINT_FILL,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_FALSE,
        .default_width = 130,
        .min_width = 90,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_FOR,
        .title = "for",
        .desc = "For",
        .key = "for",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 0.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 100,
        .min_width = 80,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    fp = (Evisum_Ui_Widget_Exel_Field_Params) {
        .id = EX2_FIELD_THAT,
        .title = "that",
        .desc = "That",
        .key = "that",
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
        .align_x = 0.0,
        .weight_x = EVAS_HINT_EXPAND,
        .boxed = EINA_TRUE,
        .spacer = 6,
        .default_width = 90,
        .min_width = 70,
        .always_visible = EINA_TRUE,
        .reverse = EINA_FALSE,
        .clicked_cb = _field_sort_clicked_cb,
        .clicked_data = app,
    };
    evisum_ui_widget_exel_field_add(app->wx, &fp);

    evisum_ui_widget_exel_fields_apply(app->wx);

    elm_box_pack_end(root, evisum_ui_widget_exel_genlist_add(app->wx, root));

    itc.item_style = "full";
    itc.func.text_get = NULL;
    itc.func.content_get = _content_get;
    itc.func.filter_get = NULL;
    itc.func.del = _item_del;

    _fill_rows(app, &itc);
    app->timer = ecore_timer_add(1.0, _update_timer_cb, app);

    evas_object_resize(win, 860, 420);
    evas_object_show(win);
    elm_run();

    return EXIT_SUCCESS;
}
ELM_MAIN()
