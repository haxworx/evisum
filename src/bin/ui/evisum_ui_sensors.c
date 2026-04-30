#include "evisum_ui_sensors.h"
#include "evisum_ui_graph.h"
#include "evisum_ui_colors.h"
#include "../engine/evisum_engine.h"
#include "../background/evisum_background.h"
#include "config.h"

typedef struct {
    Evas_Object *win;
    Evas_Object *main_menu;
    Elm_Layout *btn_menu;
    Evas_Object *graph_bg;
    Evas_Object *graph_img;
    Evas_Object *legend_tb;
    Ecore_Thread *thread;
    Eina_List *history;
    Eina_Bool btn_visible;
    Eina_Bool skip_wait;

    Evisum_Ui *ui;
    int n_sensors;
} Evisum_Ui_Sensors_View;

typedef struct {
    char *key;
    char *name;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
    double history[120];
    int history_count;
    double current_temp;
    Eina_Bool seen;
    Eina_Bool enabled;
    Evisum_Ui_Sensors_View *view;

    Evas_Object *legend_row;
    Evas_Object *legend_btn;
    Evas_Object *legend_swatch;
    Evas_Object *legend_label;
    Evas_Object *legend_pb;
} Sensor_History;

#define SENSOR_GRAPH_SAMPLES       120
#define SENSOR_GRID_X_STEP_SAMPLES 5
#define SENSOR_GRID_Y_STEP_PERCENT 10

static const Evisum_Ui_Graph_Layer _sensor_layers[] = {
    { -0.6, 0.24 },
    { 0.6,  0.24 },
    { 0.0,  0.92 },
};

static void _evisum_ui_sensors_graph_redraw(Evisum_Ui_Sensors_View *view);

static Eina_Bool
_evisum_ui_sensors_graph_objects_valid(Evisum_Ui_Sensors_View *view) {
    return view && view->graph_bg && view->graph_img && evas_object_evas_get(view->graph_bg)
           && evas_object_evas_get(view->graph_img);
}

static int
_evisum_ui_sensors_history_sort_cb(const void *p1, const void *p2) {
    const Sensor_History *s1 = p1;
    const Sensor_History *s2 = p2;
    const char *n1 = s1->name ? s1->name : s1->key;
    const char *n2 = s2->name ? s2->name : s2->key;

    if (!n1 && !n2) return 0;
    if (!n1) return 1;
    if (!n2) return -1;
    return strcmp(n1, n2);
}

static void
_evisum_ui_sensors_legend_toggle_state_apply(Sensor_History *entry) {
    if (!entry) return;

    if (!entry->enabled && entry->legend_swatch) evas_object_hide(entry->legend_swatch);
    else if (entry->enabled && entry->legend_swatch) evas_object_show(entry->legend_swatch);

    if (entry->legend_pb && evas_object_evas_get(entry->legend_pb))
        elm_object_disabled_set(entry->legend_pb, !entry->enabled);
    else entry->legend_pb = NULL;
}

static void
_evisum_ui_sensors_legend_toggle_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Sensor_History *entry = data;

    if (!entry || !entry->view || !entry->legend_btn || !entry->legend_row) return;
    if (!_evisum_ui_sensors_graph_objects_valid(entry->view)) return;
    entry->enabled = !entry->enabled;
    _evisum_ui_sensors_legend_toggle_state_apply(entry);
    _evisum_ui_sensors_graph_redraw(entry->view);
}

static void
_evisum_ui_sensors_sensor_name_set(char *buf, size_t len, Sensor *s) {
    char *p;
    size_t left;
    size_t copy;

    if (!buf || !len) return;

    if (!s || !s->name[0]) {
        snprintf(buf, len, "%s", _("Sensor"));
        return;
    }

    if (!s->child_name[0]) snprintf(buf, len, "%s", s->name);
    else {
        p = buf;
        left = len - 1;

        copy = strlen(s->name);
        if (copy > left) copy = left;
        memcpy(p, s->name, copy);
        p += copy;
        left -= copy;

        copy = 2;
        if (copy > left) copy = left;
        memcpy(p, " (", copy);
        p += copy;
        left -= copy;

        copy = strlen(s->child_name);
        if (copy > left) copy = left;
        memcpy(p, s->child_name, copy);
        p += copy;
        left -= copy;

        if (left) *p++ = ')';
        *p = '\0';
    }
}

static void
_evisum_ui_sensors_history_legend_repack(Evisum_Ui_Sensors_View *view) {
    Eina_List *l;
    Sensor_History *entry;
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
_evisum_ui_sensors_history_add_sample(Sensor_History *entry, double value) {
    if (entry->history_count < SENSOR_GRAPH_SAMPLES) entry->history[entry->history_count++] = value;
    else {
        memmove(&entry->history[0], &entry->history[1], sizeof(double) * (SENSOR_GRAPH_SAMPLES - 1));
        entry->history[SENSOR_GRAPH_SAMPLES - 1] = value;
    }
}

static void
_evisum_ui_sensors_history_legend_add(Evisum_Ui_Sensors_View *view, Sensor_History *entry) {
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
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_sensors_legend_toggle_cb, entry);
    elm_box_pack_end(left, btn);

    swatch = evas_object_rectangle_add(evas);
    evas_object_color_set(swatch, entry->color_r, entry->color_g, entry->color_b, 255);
    evas_object_size_hint_min_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_max_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_align_set(swatch, 0.0, 0.5);
    elm_object_content_set(btn, swatch);

    lb = elm_label_add(left);
    evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    elm_object_text_set(lb, entry->name ? entry->name : entry->key);
    elm_box_pack_end(left, lb);
    evas_object_show(lb);

    pb = elm_progressbar_add(view->legend_tb);
    elm_object_text_set(pb, NULL);
    elm_progressbar_span_size_set(pb, ELM_SCALE_SIZE(220));
    elm_progressbar_unit_format_set(pb, _("%1.1f°C"));
    evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
    evas_object_show(pb);

    entry->legend_row = left;
    entry->legend_btn = btn;
    entry->legend_swatch = swatch;
    entry->legend_label = lb;
    entry->legend_pb = pb;
    entry->enabled = EINA_FALSE;
    _evisum_ui_sensors_legend_toggle_state_apply(entry);
    _evisum_ui_sensors_history_legend_repack(view);
}

static void
_evisum_ui_sensors_history_legend_del(Sensor_History *entry) {
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
_evisum_ui_sensors_history_legend_update(Sensor_History *entry, double temp) {
    if (!entry->legend_pb || !entry->legend_label) return;

    elm_object_text_set(entry->legend_label, entry->name ? entry->name : entry->key);
    elm_progressbar_value_set(entry->legend_pb, temp / 100.0);
}

static Sensor_History *
_evisum_ui_sensors_history_find_or_create(Evisum_Ui_Sensors_View *view, Sensor *s) {
    Eina_List *l;
    Sensor_History *entry;
    char keybuf[256];
    char namebuf[256];

    if (!s || !s->name[0]) return NULL;

    _evisum_ui_sensors_sensor_name_set(namebuf, sizeof(namebuf), s);
    snprintf(keybuf, sizeof(keybuf), "%s", namebuf);

    EINA_LIST_FOREACH(view->history, l, entry) {
        if (!strcmp(entry->key, keybuf)) return entry;
    }

    entry = calloc(1, sizeof(Sensor_History));
    if (!entry) return NULL;

    entry->key = strdup(keybuf);
    entry->name = strdup(namebuf);
    if (!entry->key || !entry->name) {
        free(entry->name);
        free(entry->key);
        free(entry);
        return NULL;
    }

    evisum_graph_color_get(entry->key, &entry->color_r, &entry->color_g, &entry->color_b);
    entry->enabled = EINA_FALSE;
    entry->view = view;

    view->history = eina_list_append(view->history, entry);
    return entry;
}

static void
_evisum_ui_sensors_history_compact(Evisum_Ui_Sensors_View *view) {
    Eina_List *l, *l2;
    Sensor_History *entry;

    EINA_LIST_FOREACH_SAFE(view->history, l, l2, entry) {
        if (entry->seen) continue;
        view->history = eina_list_remove_list(view->history, l);
        _evisum_ui_sensors_history_legend_del(entry);
        free(entry->name);
        free(entry->key);
        free(entry);
    }

    _evisum_ui_sensors_history_legend_repack(view);
}

static void
_evisum_ui_sensors_graph_redraw(Evisum_Ui_Sensors_View *view) {
    Eina_List *l;
    Sensor_History *entry;
    Evisum_Ui_Graph_Series *series;
    int total, nseries;
    double y_max = 0.0;

    if (!_evisum_ui_sensors_graph_objects_valid(view)) return;

    total = eina_list_count(view->history);
    nseries = 0;
    series = calloc(total, sizeof(Evisum_Ui_Graph_Series));
    if ((total > 0) && (!series)) return;

    EINA_LIST_FOREACH(view->history, l, entry) {
        int j;

        if (entry->history_count < 2) continue;
        if (!entry->enabled) continue;

        series[nseries].history = entry->history;
        series[nseries].history_count = entry->history_count;
        series[nseries].color_r = entry->color_r;
        series[nseries].color_g = entry->color_g;
        series[nseries].color_b = entry->color_b;
        nseries++;

        for (j = 0; j < entry->history_count; j++) {
            if (entry->history[j] > y_max) y_max = entry->history[j];
        }
    }

    if (y_max < 100.0) y_max = 100.0;

    evisum_ui_graph_draw(view->graph_bg, view->graph_img, SENSOR_GRAPH_SAMPLES, SENSOR_GRID_X_STEP_SAMPLES,
                         SENSOR_GRID_Y_STEP_PERCENT, y_max, series, nseries, _sensor_layers,
                         EINA_C_ARRAY_LENGTH(_sensor_layers));
    free(series);
}

static void
_evisum_ui_sensors_graph_bg_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                      void *event_info EINA_UNUSED) {
    Evisum_Ui_Sensors_View *view = data;

    _evisum_ui_sensors_graph_redraw(view);
}

static void
_evisum_ui_sensors_poll(void *data, Ecore_Thread *thread) {
    Sensor **sensors;
    Evisum_Ui_Sensors_View *view = data;
    uint64_t seq = 0;
    int ticks = 9;

    ecore_thread_name_set(thread, "sensors");

    while (!ecore_thread_check(thread)) {
        if (view->skip_wait) {
            view->skip_wait = 0;
            ticks = 9;
        } else if (!evisum_background_update_wait(&seq)) continue;
        ticks++;
        if (ticks < 10) continue;
        ticks = 0;

        sensors = system_sensors_thermal_get(&view->n_sensors);
        if (!sensors) continue;

        ecore_thread_feedback(thread, sensors);
    }
}

static void
_evisum_ui_sensors_poll_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata) {
    Evisum_Ui_Sensors_View *view = data;
    Sensor **sensors = msgdata;
    Sensor *s;
    Sensor_History *entry;
    Eina_List *l;
    int i = 0;

    if (!view) return;

    EINA_LIST_FOREACH(view->history, l, entry)
    entry->seen = EINA_FALSE;

    if (sensors) {
        for (i = 0; i < view->n_sensors; i++) {
            Sensor_History *entry;
            double temp;

            s = sensors[i];
            if (!s) continue;

            temp = s->value;
            if (temp < 0.0) temp = 0.0;
            if (temp > 100.0) temp = 100.0;

            entry = _evisum_ui_sensors_history_find_or_create(view, s);
            if (!entry) continue;
            if (entry->seen) continue;

            entry->current_temp = temp;
            _evisum_ui_sensors_history_add_sample(entry, temp);
            _evisum_ui_sensors_history_legend_add(view, entry);
            _evisum_ui_sensors_history_legend_update(entry, temp);
            entry->seen = EINA_TRUE;
        }

        system_sensors_thermal_free(sensors, view->n_sensors);
    }

    _evisum_ui_sensors_history_compact(view);
    view->history = eina_list_sort(view->history, eina_list_count(view->history), _evisum_ui_sensors_history_sort_cb);
    _evisum_ui_sensors_history_legend_repack(view);
    _evisum_ui_sensors_graph_redraw(view);
}

static void
_evisum_ui_sensors_win_key_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evas_Event_Key_Down *ev = event_info;
    Evisum_Ui_Sensors_View *view = data;

    if (!ev || !ev->keyname) return;

    if (!strcmp(ev->keyname, "Escape") && view->win) evas_object_del(view->win);
}

static void
_evisum_ui_sensors_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Sensors_View *view = data;
    Evisum_Ui *ui = view->ui;

    evas_object_geometry_get(obj, &ui->sensors.x, &ui->sensors.y, NULL, NULL);
}

static void
_evisum_ui_sensors_win_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Sensors_View *view = data;
    Evisum_Ui *ui = view->ui;

    _evisum_ui_sensors_graph_redraw(view);
    evas_object_geometry_get(obj, NULL, NULL, &ui->sensors.width, &ui->sensors.height);
}

static void
_evisum_ui_sensors_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Sensors_View *view = data;
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
_evisum_ui_sensors_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Sensors_View *view = data;

    view->main_menu = NULL;
}

static void
_evisum_ui_sensors_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Sensors_View *view = data;

    if (!view->main_menu) {
        view->main_menu = evisum_ui_main_menu_create(view->ui, view->win, obj);
        evas_object_smart_callback_add(view->main_menu, "dismissed", _evisum_ui_sensors_main_menu_dismissed_cb, view);
    } else {
        evas_object_del(view->main_menu);
        view->main_menu = NULL;
    }
}

static void
_evisum_ui_sensors_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED) {
    Sensor_History *entry;
    Evisum_Ui_Sensors_View *view = data;
    Evisum_Ui *ui = view->ui;

    evisum_ui_config_save(ui);
    if (view->thread) {
        ecore_thread_cancel(view->thread);
        ecore_thread_wait(view->thread, 0.5);
    }

    if (view->main_menu) evas_object_del(view->main_menu);

    EINA_LIST_FREE(view->history, entry) {
        _evisum_ui_sensors_history_legend_del(entry);
        free(entry->name);
        free(entry->key);
        free(entry);
    }

    ui->sensors.win = NULL;
    free(view);
}

void
evisum_ui_sensors_win_add(Evisum_Ui *ui) {
    Evas_Object *win, *tb, *graph_tb, *legend_fr, *legend_sc;
    Evas_Object *btn, *ic;
    Elm_Layout *lay;
    Evas *evas;

    if (ui->sensors.win) {
        elm_win_raise(ui->sensors.win);
        return;
    }

    ui->sensors.win = win = elm_win_util_standard_add("evisum", _("Sensors"));
    elm_win_autodel_set(win, 1);
    evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
    evas_object_size_hint_align_set(win, FILL, FILL);
    evas = evas_object_evas_get(win);

    Evisum_Ui_Sensors_View *view = calloc(1, sizeof(Evisum_Ui_Sensors_View));
    if (!view) {
        evas_object_del(win);
        ui->sensors.win = NULL;
        return;
    }
    view->win = win;
    view->ui = ui;
    view->skip_wait = 1;

    tb = elm_table_add(win);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_show(tb);

    graph_tb = elm_table_add(win);
    evas_object_size_hint_weight_set(graph_tb, EVAS_HINT_EXPAND, 1.6);
    evas_object_size_hint_align_set(graph_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_min_set(graph_tb, 420 * elm_config_scale_get(), 200 * elm_config_scale_get());
    elm_table_pack(tb, graph_tb, 0, 0, 1, 1);
    evas_object_show(graph_tb);

    view->graph_bg = evas_object_rectangle_add(evas);
    evisum_ui_graph_bg_set(view->graph_bg);
    evas_object_size_hint_weight_set(view->graph_bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(view->graph_bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(graph_tb, view->graph_bg, 0, 0, 1, 1);
    evas_object_show(view->graph_bg);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_RESIZE, _evisum_ui_sensors_graph_bg_resize_cb, view);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_MOVE, _evisum_ui_sensors_graph_bg_resize_cb, view);

    view->graph_img = evas_object_vg_add(evas);
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
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_sensors_btn_menu_clicked_cb, view);

    view->btn_menu = lay = elm_layout_add(win);
    evas_object_size_hint_weight_set(lay, 1.0, 1.0);
    evas_object_size_hint_align_set(lay, 0.99, 0.01);
    elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
    elm_layout_content_set(lay, "evisum/menu", btn);
    elm_table_pack(graph_tb, lay, 0, 0, 1, 1);
    evas_object_show(lay);

    legend_fr = elm_frame_add(win);
    elm_object_text_set(legend_fr, _("Sensors"));
    evas_object_size_hint_weight_set(legend_fr, EVAS_HINT_EXPAND, 0.8);
    evas_object_size_hint_align_set(legend_fr, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_min_set(legend_fr, 1, ELM_SCALE_SIZE(120));
    elm_table_pack(tb, legend_fr, 0, 1, 1, 1);
    evas_object_show(legend_fr);

    legend_sc = elm_scroller_add(win);
    elm_scroller_policy_set(legend_sc, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
    elm_scroller_bounce_set(legend_sc, EINA_FALSE, EINA_TRUE);
    elm_scroller_content_min_limit(legend_sc, EINA_TRUE, EINA_FALSE);
    evas_object_size_hint_weight_set(legend_sc, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(legend_sc, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_object_content_set(legend_fr, legend_sc);
    evas_object_show(legend_sc);

    view->legend_tb = elm_table_add(legend_sc);
    elm_table_padding_set(view->legend_tb, 8 * elm_config_scale_get(), 2 * elm_config_scale_get());
    evas_object_size_hint_weight_set(view->legend_tb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(view->legend_tb, EVAS_HINT_FILL, 0.0);
    elm_object_content_set(legend_sc, view->legend_tb);
    evas_object_show(view->legend_tb);

    elm_object_content_set(win, tb);

    if ((ui->sensors.width > 0) && (ui->sensors.height > 0))
        evas_object_resize(win, ui->sensors.width, ui->sensors.height);
    else evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.5), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT * 0.5));

    if ((ui->sensors.x > 0) && (ui->sensors.y > 0)) evas_object_move(win, ui->sensors.x, ui->sensors.y);
    else elm_win_center(win, 1, 1);

    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_sensors_win_del_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _evisum_ui_sensors_win_move_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_sensors_win_resize_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_sensors_win_mouse_move_cb, view);
    elm_object_focus_allow_set(tb, EINA_TRUE);
    elm_object_focus_set(tb, EINA_TRUE);
    evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_sensors_win_key_down_cb, view);

    evas_object_show(win);

    view->thread = ecore_thread_feedback_run(_evisum_ui_sensors_poll, _evisum_ui_sensors_poll_feedback_cb, NULL, NULL,
                                             view, 1);
}
