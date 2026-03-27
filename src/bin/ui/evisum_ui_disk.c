#include "evisum_ui_disk.h"
#include "evisum_ui_graph.h"
#include "../system/filesystems.h"
#include "config.h"
#include "evisum_ui_colors.h"

typedef struct {
    Evas_Object *win;
    Evas_Object *main_menu;
    Elm_Layout *btn_menu;
    Evas_Object *graph_bg;
    Evas_Object *graph_img;
    Evas_Object *legend_tb;
    Ecore_Thread *thread;
    Eina_Bool skip_wait;
    Eina_List *history;
    Eina_Bool btn_visible;

    Evisum_Ui *ui;
} Evisum_Ui_Disk_View;

#define DISK_GRAPH_SAMPLES       120
#define DISK_GRID_X_STEP_SAMPLES 5
#define DISK_GRID_Y_STEP_PERCENT 10

typedef struct {
    char *key;
    char *name;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    double history[DISK_GRAPH_SAMPLES];
    int history_count;
    Eina_Bool seen;
    Eina_Bool enabled;
    Evisum_Ui_Disk_View *view;
    Evas_Object *legend_row;
    Evas_Object *legend_btn;
    Evas_Object *legend_swatch;
    Evas_Object *legend_label;
    Evas_Object *legend_pb;
} Disk_History;

static void _evisum_ui_disk_graph_redraw(Evisum_Ui_Disk_View *view);
static const Evisum_Ui_Graph_Layer _disk_layers[] = {
    { -0.6, 0.24 },
    { 0.6,  0.24 },
    { 0.0,  0.92 },
};

static Eina_Bool
_evisum_ui_disk_graph_objects_valid(Evisum_Ui_Disk_View *view) {
    return view && view->graph_bg && view->graph_img && evas_object_evas_get(view->graph_bg)
           && evas_object_evas_get(view->graph_img);
}

static void
_evisum_ui_disk_legend_toggle_state_apply(Disk_History *entry) {
    if (!entry) return;

    if (!entry->enabled && entry->legend_swatch) evas_object_hide(entry->legend_swatch);
    else if (entry->enabled && entry->legend_swatch) evas_object_show(entry->legend_swatch);

    if (entry->legend_pb && evas_object_evas_get(entry->legend_pb))
        elm_object_disabled_set(entry->legend_pb, !entry->enabled);
    else entry->legend_pb = NULL;
}

static void
_evisum_ui_disk_legend_toggle_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Disk_History *entry = data;

    if (!entry || !entry->view || !entry->legend_btn || !entry->legend_row) return;
    if (!_evisum_ui_disk_graph_objects_valid(entry->view)) return;
    entry->enabled = !entry->enabled;
    _evisum_ui_disk_legend_toggle_state_apply(entry);
    _evisum_ui_disk_graph_redraw(entry->view);
}

static void
_evisum_ui_disk_history_legend_repack(Evisum_Ui_Disk_View *view) {
    Eina_List *l;
    Disk_History *entry;
    int row = 0;

    if (!view->legend_tb) return;

    elm_table_clear(view->legend_tb, 0);

    EINA_LIST_FOREACH(view->history, l, entry) {
        if (!entry->legend_row || !entry->legend_pb) continue;
        elm_table_pack(view->legend_tb, entry->legend_row, 0, row, 1, 1);
        elm_table_pack(view->legend_tb, entry->legend_pb, 1, row, 1, 1);
        evas_object_show(entry->legend_row);
        evas_object_show(entry->legend_pb);
        row++;
    }
}

static void
_evisum_ui_disk_history_add_sample(Disk_History *entry, double value) {
    if (entry->history_count < DISK_GRAPH_SAMPLES) entry->history[entry->history_count++] = value;
    else {
        memmove(&entry->history[0], &entry->history[1], sizeof(double) * (DISK_GRAPH_SAMPLES - 1));
        entry->history[DISK_GRAPH_SAMPLES - 1] = value;
    }
}

static void
_evisum_ui_disk_history_legend_add(Evisum_Ui_Disk_View *view, Disk_History *entry) {
    Evas_Object *left, *swatch, *lb, *pb, *btn;
    Evas *evas;

    if (!view->legend_tb || entry->legend_row) return;

    left = elm_box_add(view->legend_tb);
    elm_box_horizontal_set(left, EINA_TRUE);
    elm_box_padding_set(left, 4 * elm_config_scale_get(), 0);
    evas_object_size_hint_weight_set(left, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(left, EVAS_HINT_FILL, 0.5);
    evas_object_show(left);

    evas = evas_object_evas_get(left);
    btn = elm_button_add(left);
    evas_object_size_hint_min_set(btn, 16 * elm_config_scale_get(), 16 * elm_config_scale_get());
    evas_object_size_hint_max_set(btn, 16 * elm_config_scale_get(), 16 * elm_config_scale_get());
    evas_object_size_hint_align_set(btn, 0.0, 0.5);
    evas_object_show(btn);
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_disk_legend_toggle_cb, entry);
    elm_box_pack_end(left, btn);

    swatch = evas_object_rectangle_add(evas);
    evas_object_color_set(swatch, entry->color_r, entry->color_g, entry->color_b, 255);
    evas_object_size_hint_min_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_max_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_align_set(swatch, 0.0, 0.5);
    elm_object_content_set(btn, swatch);
    evas_object_show(swatch);

    lb = elm_label_add(left);
    evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    elm_object_text_set(lb, entry->name ? entry->name : entry->key);
    elm_box_pack_end(left, lb);
    evas_object_show(lb);

    pb = elm_progressbar_add(view->legend_tb);
    elm_object_text_set(pb, NULL);
    elm_progressbar_span_size_set(pb, ELM_SCALE_SIZE(220));
    elm_progressbar_unit_format_set(pb, "0 B / 0 B");
    evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
    evas_object_show(pb);

    entry->legend_row = left;
    entry->legend_btn = btn;
    entry->legend_swatch = swatch;
    entry->legend_label = lb;
    entry->legend_pb = pb;
    _evisum_ui_disk_legend_toggle_state_apply(entry);
    _evisum_ui_disk_history_legend_repack(view);
}

static void
_evisum_ui_disk_history_legend_del(Disk_History *entry) {
    if (entry->legend_pb) evas_object_del(entry->legend_pb);
    if (entry->legend_row) evas_object_del(entry->legend_row);
    entry->view = NULL;
    entry->legend_row = NULL;
    entry->legend_btn = NULL;
    entry->legend_swatch = NULL;
    entry->legend_label = NULL;
    entry->legend_pb = NULL;
}

static void
_evisum_ui_disk_history_legend_update(Disk_History *entry, double used_percent, int64_t used, int64_t total) {
    if (!entry->legend_pb || !entry->legend_label) return;

    elm_object_text_set(entry->legend_label, entry->name ? entry->name : entry->key);
    elm_progressbar_value_set(entry->legend_pb, used_percent / 100.0);
    elm_progressbar_unit_format_set(
            entry->legend_pb, eina_slstr_printf("%s / %s", evisum_size_format(used, 0), evisum_size_format(total, 0)));
}

static Disk_History *
_evisum_ui_disk_history_find_or_create(Evisum_Ui_Disk_View *view, const File_System *fs) {
    Eina_List *l;
    Disk_History *entry;
    char keybuf[4096];

    snprintf(keybuf, sizeof(keybuf), "%s|%s", fs->path, fs->mount);
    EINA_LIST_FOREACH(view->history, l, entry) {
        if (!strcmp(entry->key, keybuf)) return entry;
    }

    entry = calloc(1, sizeof(Disk_History));
    if (!entry) return NULL;
    entry->key = strdup(keybuf);
    if (!entry->key) {
        free(entry);
        return NULL;
    }

    entry->name = strdup(fs->mount);
    if (!entry->name) entry->name = strdup(fs->path);
    entry->enabled = EINA_TRUE;
    entry->view = view;

    evisum_graph_color_get(entry->key, &entry->color_r, &entry->color_g, &entry->color_b);

    view->history = eina_list_append(view->history, entry);
    return entry;
}

static void
_evisum_ui_disk_history_compact(Evisum_Ui_Disk_View *view) {
    Eina_List *l, *l2;
    Disk_History *entry;

    EINA_LIST_FOREACH_SAFE(view->history, l, l2, entry) {
        if (entry->seen) continue;
        view->history = eina_list_remove_list(view->history, l);
        _evisum_ui_disk_history_legend_del(entry);
        free(entry->name);
        free(entry->key);
        free(entry);
    }

    _evisum_ui_disk_history_legend_repack(view);
}

static void
_evisum_ui_disk_graph_redraw(Evisum_Ui_Disk_View *view) {
    Eina_List *l;
    Disk_History *entry;
    int total, nseries;
    Evisum_Ui_Graph_Series *series;

    if (!_evisum_ui_disk_graph_objects_valid(view)) return;

    total = eina_list_count(view->history);
    nseries = 0;
    series = calloc(total, sizeof(Evisum_Ui_Graph_Series));
    if ((total > 0) && (!series)) return;

    EINA_LIST_FOREACH(view->history, l, entry) {
        if (!entry->enabled || (entry->history_count < 2)) continue;
        series[nseries].history = entry->history;
        series[nseries].history_count = entry->history_count;
        series[nseries].color_r = entry->color_r;
        series[nseries].color_g = entry->color_g;
        series[nseries].color_b = entry->color_b;
        nseries++;
    }

    evisum_ui_graph_draw(view->graph_bg, view->graph_img, DISK_GRAPH_SAMPLES, DISK_GRID_X_STEP_SAMPLES,
                         DISK_GRID_Y_STEP_PERCENT, 100.0, series, nseries, _disk_layers,
                         EINA_C_ARRAY_LENGTH(_disk_layers));
    free(series);
}

static void
_evisum_ui_disk_graph_bg_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                   void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view = data;

    _evisum_ui_disk_graph_redraw(view);
}

static void
_evisum_ui_disk_disks_poll(void *data, Ecore_Thread *thread) {
    Evisum_Ui_Disk_View *view = data;

    while (!ecore_thread_check(thread)) {
        for (int i = 0; i < 8; i++) {
            if (view->skip_wait) {
                view->skip_wait = 0;
                break;
            }
            if (ecore_thread_check(thread)) return;
            usleep(125000);
        }
        ecore_thread_feedback(thread, file_system_info_all_get());
    }
}

static void
_evisum_ui_disk_disks_poll_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata) {
    File_System *fs;
    Evisum_Ui_Disk_View *view;
    Eina_List *l;
    Eina_List *mounted;
    Disk_History *entry;

    view = data;
    mounted = msgdata;

    EINA_LIST_FOREACH(view->history, l, entry)
    entry->seen = EINA_FALSE;

    EINA_LIST_FOREACH(mounted, l, fs) {
        double used_percent = 0.0;

        entry = _evisum_ui_disk_history_find_or_create(view, fs);
        if (!entry) continue;

        if (fs->usage.total) used_percent = ((double) fs->usage.used / (double) fs->usage.total) * 100.0;
        if (used_percent < 0.0) used_percent = 0.0;
        if (used_percent > 100.0) used_percent = 100.0;

        _evisum_ui_disk_history_add_sample(entry, used_percent);
        _evisum_ui_disk_history_legend_add(view, entry);
        _evisum_ui_disk_history_legend_update(entry, used_percent, fs->usage.used, fs->usage.total);
        entry->seen = EINA_TRUE;
    }
    _evisum_ui_disk_history_compact(view);

    EINA_LIST_FREE(mounted, fs) { file_system_info_free(fs); }
    _evisum_ui_disk_graph_redraw(view);
}

static void
_evisum_ui_disk_win_key_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Disk_View *view = data;
    Evas_Event_Key_Down *ev;

    ev = event_info;

    if (!ev || !ev->keyname) return;

    if (!strcmp(ev->keyname, "Escape") && view && view->win) evas_object_del(view->win);
}

static void
_evisum_ui_disk_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view;
    Evisum_Ui *ui;

    view = data;
    ui = view->ui;

    evas_object_geometry_get(obj, &ui->disk.x, &ui->disk.y, NULL, NULL);
}

static void
_evisum_ui_disk_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                           void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view;
    Evisum_Ui *ui;
    Disk_History *entry;

    view = data;
    ui = view->ui;

    evisum_ui_config_save(ui);
    ecore_thread_cancel(view->thread);
    ecore_thread_wait(view->thread, 0.5);
    if (view->main_menu) evas_object_del(view->main_menu);

    EINA_LIST_FREE(view->history, entry) {
        _evisum_ui_disk_history_legend_del(entry);
        free(entry->name);
        free(entry->key);
        free(entry);
    }

    free(view);

    ui->disk.win = NULL;
}

static void
_evisum_ui_disk_win_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view = data;
    Evisum_Ui *ui = view->ui;

    _evisum_ui_disk_graph_redraw(view);
    evas_object_geometry_get(obj, NULL, NULL, &ui->disk.width, &ui->disk.height);
}

static void
_evisum_ui_disk_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Disk_View *view = data;
    Evas_Event_Mouse_Move *ev = event_info;
    Evas_Coord gx, gy, gw, gh;
    Eina_Bool on_hotzone = EINA_FALSE;

    if (!view->graph_bg || !view->btn_menu || !ev) return;

    evas_object_geometry_get(view->graph_bg, &gx, &gy, &gw, &gh);
    if ((ev->cur.canvas.x >= (gx + gw - 128)) && (ev->cur.canvas.x <= (gx + gw)) && (ev->cur.canvas.y >= gy)
        && (ev->cur.canvas.y <= (gy + 128)))
        on_hotzone = EINA_TRUE;

    if (on_hotzone) {
        if (!view->btn_visible) {
            elm_object_signal_emit(view->btn_menu, "menu,show", "evisum/menu");
            view->btn_visible = 1;
        }
    } else if ((view->btn_visible) && (!view->main_menu)) {
        elm_object_signal_emit(view->btn_menu, "menu,hide", "evisum/menu");
        view->btn_visible = 0;
    }
}

static void
_evisum_ui_disk_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view = data;

    view->main_menu = NULL;
}

static void
_evisum_ui_disk_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Disk_View *view = data;

    if (!view->main_menu) {
        view->main_menu = evisum_ui_main_menu_create(view->ui, view->win, obj);
        evas_object_smart_callback_add(view->main_menu, "dismissed", _evisum_ui_disk_main_menu_dismissed_cb, view);
    } else {
        evas_object_del(view->main_menu);
        view->main_menu = NULL;
    }
}

void
evisum_ui_disk_win_add(Evisum_Ui *ui) {
    Evas_Object *win, *tb, *graph_tb, *legend_fr;
    Evas_Object *btn, *ic;
    Elm_Layout *lay;
    Evas *evas;

    if (ui->disk.win) {
        elm_win_raise(ui->disk.win);
        return;
    }

    ui->disk.win = win = elm_win_util_standard_add("evisum", _("Storage"));
    elm_win_autodel_set(win, 1);
    evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
    evas_object_size_hint_align_set(win, FILL, FILL);
    evas = evas_object_evas_get(win);

    Evisum_Ui_Disk_View *view = calloc(1, sizeof(Evisum_Ui_Disk_View));
    view->win = win;
    view->ui = ui;
    view->skip_wait = 1;

    tb = elm_table_add(win);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_show(tb);

    graph_tb = elm_table_add(win);
    evas_object_size_hint_weight_set(graph_tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(graph_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_min_set(graph_tb, 420 * elm_config_scale_get(), 260 * elm_config_scale_get());
    elm_table_pack(tb, graph_tb, 0, 0, 1, 1);
    evas_object_show(graph_tb);

    view->graph_bg = evas_object_rectangle_add(evas);
    evisum_ui_graph_bg_set(view->graph_bg);
    evas_object_size_hint_weight_set(view->graph_bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(view->graph_bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(graph_tb, view->graph_bg, 0, 0, 1, 1);
    evas_object_show(view->graph_bg);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_RESIZE, _evisum_ui_disk_graph_bg_resize_cb, view);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_MOVE, _evisum_ui_disk_graph_bg_resize_cb, view);

    view->graph_img = evas_object_image_filled_add(evas);
    evas_object_image_alpha_set(view->graph_img, EINA_FALSE);
    evas_object_size_hint_weight_set(view->graph_img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(view->graph_img, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(graph_tb, view->graph_img, 0, 0, 1, 1);
    evas_object_show(view->graph_img);
    evas_object_stack_above(view->graph_img, view->graph_bg);

    btn = elm_button_add(win);
    ic = elm_icon_add(btn);
    elm_icon_standard_set(ic, "menu");
    elm_object_part_content_set(btn, "icon", ic);
    evas_object_show(ic);
    elm_object_focus_allow_set(btn, 0);
    evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_disk_btn_menu_clicked_cb, view);

    view->btn_menu = lay = elm_layout_add(win);
    evas_object_size_hint_weight_set(lay, 1.0, 1.0);
    evas_object_size_hint_align_set(lay, 0.99, 0.01);
    elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
    elm_layout_content_set(lay, "evisum/menu", btn);
    elm_table_pack(graph_tb, lay, 0, 0, 1, 1);
    evas_object_show(lay);

    legend_fr = elm_frame_add(win);
    elm_object_text_set(legend_fr, _("Mounts"));
    evas_object_size_hint_weight_set(legend_fr, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(legend_fr, EVAS_HINT_FILL, 0.0);
    elm_table_pack(tb, legend_fr, 0, 1, 1, 1);
    evas_object_show(legend_fr);

    view->legend_tb = elm_table_add(legend_fr);
    elm_table_padding_set(view->legend_tb, 8 * elm_config_scale_get(), 2 * elm_config_scale_get());
    evas_object_size_hint_weight_set(view->legend_tb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(view->legend_tb, EVAS_HINT_FILL, 0.0);
    elm_object_content_set(legend_fr, view->legend_tb);
    evas_object_show(view->legend_tb);

    elm_object_content_set(win, tb);

    if ((ui->disk.width > 0) && (ui->disk.height > 0)) evas_object_resize(win, ui->disk.width, ui->disk.height);
    else evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.5), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT * 0.5));

    if ((ui->disk.x > 0) && (ui->disk.y > 0)) evas_object_move(win, ui->disk.x, ui->disk.y);
    else elm_win_center(win, 1, 1);

    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_disk_win_del_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _evisum_ui_disk_win_move_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_disk_win_resize_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_disk_win_mouse_move_cb, view);
    elm_object_focus_allow_set(tb, EINA_TRUE);
    elm_object_focus_set(tb, EINA_TRUE);
    evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_disk_win_key_down_cb, view);
    evas_object_show(win);

    view->thread = ecore_thread_feedback_run(_evisum_ui_disk_disks_poll, _evisum_ui_disk_disks_poll_feedback_cb, NULL,
                                             NULL, view, 1);
}
