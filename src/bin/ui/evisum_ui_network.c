#include "evisum_ui_network.h"
#include "evisum_ui_graph.h"
#include "system/machine.h"
#include "evisum_ui_colors.h"
#include "config.h"

/* It seems appropriate to note AI has modified this file.
   Authored by Alastair Poole and Codex AI Agent.
*/

typedef struct {
    Ecore_Thread *thread;
    Evas_Object *win;
    Evas_Object *menu;
    Evas_Object *graph_bg;
    Evas_Object *graph_img;
    Evas_Object *legend_bx;
    Elm_Layout *btn_menu;
    Eina_Bool btn_visible;

    Eina_List *interfaces;
    double graph_peak;

    Evisum_Ui *ui;
} Evisum_Ui_Network_View;

#define NETWORK_GRAPH_SAMPLES       120
#define NETWORK_GRID_X_STEP_SAMPLES 5
#define NETWORK_GRID_Y_STEP_PERCENT 10
#define NETWORK_POLL_USEC           1000000
#define WIN_WIDTH                   320
#define WIN_HEIGHT                  480


typedef struct {
    char name[255];

    uint64_t total_in;
    uint64_t total_out;

    uint64_t peak_in;
    uint64_t peak_out;

    uint64_t in;
    uint64_t out;

    double history[NETWORK_GRAPH_SAMPLES];
    int history_count;

    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;

    Evas_Object *legend_row;
    Evas_Object *legend_btn;
    Evas_Object *legend_swatch;
    Evas_Object *legend_label;
    Eina_Bool enabled;
    Evisum_Ui_Network_View *view;

    Eina_Bool is_new;
    Eina_Bool delete_me;
} Network_Interface;

static void _evisum_ui_network_graph_redraw(Evisum_Ui_Network_View *view, Eina_List *interfaces);
static const Evisum_Ui_Graph_Layer _network_layers[] = {
    { -0.6, 0.28 },
    { 0.6,  0.28 },
    { -1.2, 0.14 },
    { 1.2,  0.14 },
    { 0.0,  0.95 },
};

static Eina_Bool
_evisum_ui_network_graph_objects_valid(Evisum_Ui_Network_View *view) {
    return view && view->graph_bg && view->graph_img && evas_object_evas_get(view->graph_bg)
           && evas_object_evas_get(view->graph_img);
}

static void
_evisum_ui_network_legend_toggle_state_apply(Network_Interface *iface) {
    if (!iface) return;

    if (!iface->enabled && iface->legend_swatch) evas_object_hide(iface->legend_swatch);
    else if (iface->enabled && iface->legend_swatch) evas_object_show(iface->legend_swatch);
}

static void
_evisum_ui_network_legend_toggle_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Network_Interface *iface = data;

    if (!iface || !iface->view || !iface->legend_btn || !iface->legend_row) return;
    if (!_evisum_ui_network_graph_objects_valid(iface->view)) return;
    iface->enabled = !iface->enabled;
    _evisum_ui_network_legend_toggle_state_apply(iface);
    _evisum_ui_network_graph_redraw(iface->view, iface->view->interfaces);
}

static void
_evisum_ui_network_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Network_View *view = data;
    Evas_Coord w, h;
    Evas_Event_Mouse_Move *ev;

    ev = event_info;
    evas_object_geometry_get(obj, NULL, NULL, &w, &h);

    if ((ev->cur.canvas.x >= (w - 128)) && (ev->cur.canvas.y <= 128)) {
        if (!view->btn_visible) {
            elm_object_signal_emit(view->btn_menu, "menu,show", "evisum/menu");
            view->btn_visible = 1;
        }
    } else if ((view->btn_visible) && (!view->menu)) {
        elm_object_signal_emit(view->btn_menu, "menu,hide", "evisum/menu");
        view->btn_visible = 0;
    }
}

static void
_evisum_ui_network_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Network_View *view = data;
    Evisum_Ui *ui = view->ui;

    if (!view->menu) view->menu = evisum_ui_main_menu_create(ui, ui->network.win, obj);
    else {
        evas_object_del(view->menu);
        view->menu = NULL;
    }
}

static void
_evisum_ui_network_iface_color_apply(Network_Interface *iface) {
    evisum_graph_color_get(iface->name, &iface->color_r, &iface->color_g, &iface->color_b);
}

static char *
_evisum_ui_network_transfer_format(double rate) {
    const char *unit = "B/s";
    char buf[256];

    if (rate > 1048576) {
        rate /= 1048576;
        unit = "MB/s";
    } else if ((rate > 1024) && (rate < 1048576)) {
        rate /= 1024;
        unit = "KB/s";
    }

    snprintf(buf, sizeof(buf), "%.2f %s", rate, unit);
    return strdup(buf);
}

static void
_evisum_ui_network_iface_legend_add(Evisum_Ui_Network_View *view, Network_Interface *iface) {
    Evas_Object *row, *swatch, *lb, *btn;
    Evas *evas;

    if (!view->legend_bx || iface->legend_row) return;

    row = elm_box_add(view->legend_bx);
    elm_box_horizontal_set(row, EINA_TRUE);
    elm_box_padding_set(row, 4 * elm_config_scale_get(), 0);
    evas_object_size_hint_weight_set(row, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(row, EVAS_HINT_FILL, 0.5);
    elm_box_pack_end(view->legend_bx, row);
    evas_object_show(row);

    evas = evas_object_evas_get(row);
    btn = elm_button_add(row);
    evas_object_size_hint_min_set(btn, 16 * elm_config_scale_get(), 16 * elm_config_scale_get());
    evas_object_size_hint_max_set(btn, 16 * elm_config_scale_get(), 16 * elm_config_scale_get());
    evas_object_size_hint_align_set(btn, 0.0, 0.5);
    evas_object_show(btn);
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_network_legend_toggle_cb, iface);
    elm_box_pack_end(row, btn);

    swatch = evas_object_rectangle_add(evas);
    evas_object_color_set(swatch, iface->color_r, iface->color_g, iface->color_b, 255);
    evas_object_size_hint_min_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_max_set(swatch, 12 * elm_config_scale_get(), 12 * elm_config_scale_get());
    evas_object_size_hint_align_set(swatch, 0.0, 0.5);
    elm_object_content_set(btn, swatch);
    evas_object_show(swatch);

    lb = elm_label_add(row);
    evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    elm_object_text_set(lb, iface->name);
    elm_box_pack_end(row, lb);
    evas_object_show(lb);

    iface->legend_row = row;
    iface->legend_btn = btn;
    iface->legend_swatch = swatch;
    iface->legend_label = lb;
    iface->view = view;
    _evisum_ui_network_legend_toggle_state_apply(iface);
}

static void
_evisum_ui_network_iface_legend_del(Network_Interface *iface) {
    if (iface->legend_row) evas_object_del(iface->legend_row);
    if (iface->legend_label) evas_object_del(iface->legend_label);
    iface->legend_row = NULL;
    iface->legend_btn = NULL;
    iface->legend_swatch = NULL;
    iface->legend_label = NULL;
    iface->view = NULL;
}

static void
_evisum_ui_network_iface_legend_update(Network_Interface *iface) {
    char *s1, *s2;

    if (!iface->legend_label) return;

    s1 = _evisum_ui_network_transfer_format(iface->in);
    s2 = _evisum_ui_network_transfer_format(iface->out);
    elm_object_text_set(iface->legend_label, eina_slstr_printf("%s (%s / %s)", iface->name, s1, s2));
    free(s1);
    free(s2);
}

static void
_evisum_ui_network_iface_history_add(Network_Interface *iface, double value) {
    if (iface->history_count < NETWORK_GRAPH_SAMPLES) iface->history[iface->history_count++] = value;
    else {
        memmove(&iface->history[0], &iface->history[1], sizeof(double) * (NETWORK_GRAPH_SAMPLES - 1));
        iface->history[NETWORK_GRAPH_SAMPLES - 1] = value;
    }
}

static void
_evisum_ui_network_interface_gone(net_iface_t **ifaces, int n, Eina_List *list) {
    Eina_List *l;
    Network_Interface *iface;

    EINA_LIST_FOREACH(list, l, iface) {
        Eina_Bool found = EINA_FALSE;
        for (int i = 0; i < n; i++) {
            net_iface_t *nwif = ifaces[i];
            if (!strcmp(nwif->name, iface->name)) {
                found = EINA_TRUE;
                break;
            }
        }
        if (!found) iface->delete_me = EINA_TRUE;
    }
}

static void
_evisum_ui_network_graph_redraw(Evisum_Ui_Network_View *view, Eina_List *interfaces) {
    Eina_List *l;
    Network_Interface *iface;
    double peak;
    int total, nseries;
    Evisum_Ui_Graph_Series *series;

    if (!_evisum_ui_network_graph_objects_valid(view)) return;

    peak = view->graph_peak;
    if (peak < 1.0) peak = 1.0;

    total = eina_list_count(interfaces);
    nseries = 0;
    series = calloc(total, sizeof(Evisum_Ui_Graph_Series));
    if ((total > 0) && (!series)) return;

    EINA_LIST_FOREACH(interfaces, l, iface) {
        if (iface->delete_me || !iface->enabled || (iface->history_count < 2)) continue;
        series[nseries].history = iface->history;
        series[nseries].history_count = iface->history_count;
        series[nseries].color_r = iface->color_r;
        series[nseries].color_g = iface->color_g;
        series[nseries].color_b = iface->color_b;
        nseries++;
    }

    evisum_ui_graph_draw(view->graph_bg, view->graph_img, NETWORK_GRAPH_SAMPLES, NETWORK_GRID_X_STEP_SAMPLES,
                         NETWORK_GRID_Y_STEP_PERCENT, peak, series, nseries, _network_layers,
                         EINA_C_ARRAY_LENGTH(_network_layers));
    free(series);
}

static void
_evisum_ui_network_graph_bg_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                      void *event_info EINA_UNUSED) {
    Evisum_Ui_Network_View *view = data;

    _evisum_ui_network_graph_redraw(view, view->interfaces);
}

static void
_evisum_ui_network_update(void *data EINA_UNUSED, Ecore_Thread *thread) {
    Eina_List *interfaces = NULL;
    Network_Interface *iface;

    ecore_thread_name_set(thread, "network");

    while (!ecore_thread_check(thread)) {
        int n;
        net_iface_t *nwif, **ifaces = system_network_ifaces_get(&n);

        _evisum_ui_network_interface_gone(ifaces, n, interfaces);

        for (int i = 0; i < n; i++) {
            Network_Interface *iface2;
            Eina_List *l;

            nwif = ifaces[i];
            iface = NULL;
            EINA_LIST_FOREACH(interfaces, l, iface2) {
                if (!strcmp(nwif->name, iface2->name)) {
                    iface = iface2;
                    break;
                }
            }

            if (!iface) {
                iface = calloc(1, sizeof(Network_Interface));
                iface->is_new = EINA_TRUE;
                iface->enabled = EINA_TRUE;
                snprintf(iface->name, sizeof(iface->name), "%s", nwif->name);
                iface->total_in = nwif->xfer.in;
                iface->total_out = nwif->xfer.out;
                interfaces = eina_list_append(interfaces, iface);
            } else {
                iface->is_new = EINA_FALSE;
                iface->in = (iface->total_in == 0) ? 0 : (nwif->xfer.in - iface->total_in);
                iface->out = (iface->total_out == 0) ? 0 : (nwif->xfer.out - iface->total_out);

                if (iface->in > iface->peak_in) iface->peak_in = iface->in;
                if (iface->out > iface->peak_out) iface->peak_out = iface->out;

                iface->total_in = nwif->xfer.in;
                iface->total_out = nwif->xfer.out;
            }

            free(nwif);
        }

        free(ifaces);
        ecore_thread_feedback(thread, interfaces);

        if (!ecore_thread_check(thread)) usleep(NETWORK_POLL_USEC);
    }

    EINA_LIST_FREE(interfaces, iface)
    free(iface);
}

static void
_evisum_ui_network_update_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata) {
    Evisum_Ui_Network_View *view = data;
    Eina_List *interfaces = msgdata;
    Eina_List *l, *l2;
    Network_Interface *iface;

    view->interfaces = interfaces;

    EINA_LIST_FOREACH_SAFE(interfaces, l, l2, iface) {
        double rate = (double) iface->in + (double) iface->out;

        _evisum_ui_network_iface_history_add(iface, rate);
        if (rate > view->graph_peak) view->graph_peak = rate;

        if (iface->delete_me) {
            _evisum_ui_network_iface_legend_del(iface);
            free(iface);
            interfaces = eina_list_remove_list(interfaces, l);
            view->interfaces = interfaces;
            continue;
        }

        if (iface->is_new) {
            _evisum_ui_network_iface_color_apply(iface);
            _evisum_ui_network_iface_legend_add(view, iface);
        }

        _evisum_ui_network_iface_legend_update(iface);
    }

    _evisum_ui_network_graph_redraw(view, interfaces);
}

static void
_evisum_ui_network_win_key_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Network_View *view = data;
    Evas_Event_Key_Down *ev = event_info;

    if (!ev || !ev->keyname) return;

    if (!strcmp(ev->keyname, "Escape")) evas_object_del(view->ui->network.win);
}

static void
_evisum_ui_network_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Network_View *view = data;
    Evisum_Ui *ui = view->ui;

    evas_object_geometry_get(obj, &ui->network.x, &ui->network.y, NULL, NULL);
}

static void
_evisum_ui_network_win_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Network_View *view = data;
    Evisum_Ui *ui = view->ui;

    evas_object_geometry_get(obj, NULL, NULL, &ui->network.width, &ui->network.height);
    _evisum_ui_network_graph_redraw(view, view->interfaces);
}

static void
_evisum_ui_network_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED) {
    Evisum_Ui_Network_View *view = data;
    Evisum_Ui *ui = view->ui;

    if (view->menu) evas_object_del(view->menu);

    if (view->interfaces) {
        Eina_List *l;
        Network_Interface *iface;

        EINA_LIST_FOREACH(view->interfaces, l, iface)
        _evisum_ui_network_iface_legend_del(iface);
    }

    evisum_ui_config_save(ui);
    ecore_thread_cancel(view->thread);
    ecore_thread_wait(view->thread, 0.5);
    ui->network.win = NULL;
    free(view);
}

void
evisum_ui_network_win_add(Evisum_Ui *ui) {
    Evas_Object *win, *root_bx, *graph_bx, *graph_tb, *legend_fr;
    Evas_Object *tb, *btn, *ic;
    Elm_Layout *lay;
    Evas *evas;
    int scale;

    if (ui->network.win) {
        elm_win_raise(ui->network.win);
        return;
    }

    Evisum_Ui_Network_View *view = calloc(1, sizeof(Evisum_Ui_Network_View));
    if (!view) return;
    view->ui = ui;

    ui->network.win = view->win = win = elm_win_util_standard_add("evisum", _("Network"));
    elm_win_autodel_set(win, 1);
    evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
    evas_object_size_hint_align_set(win, FILL, FILL);
    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_network_win_del_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _evisum_ui_network_win_move_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_network_win_resize_cb, view);

    scale = elm_config_scale_get();
    evas = evas_object_evas_get(win);

    tb = elm_table_add(win);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_show(tb);
    elm_win_resize_object_add(win, tb);

    root_bx = elm_box_add(win);
    elm_box_padding_set(root_bx, 0, 6 * scale);
    evas_object_size_hint_weight_set(root_bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(root_bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(tb, root_bx, 0, 0, 1, 1);
    evas_object_show(root_bx);

    graph_bx = elm_box_add(win);
    elm_box_padding_set(graph_bx, 0, 6 * scale);
    evas_object_size_hint_weight_set(graph_bx, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(graph_bx, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(root_bx, graph_bx);
    evas_object_show(graph_bx);

    graph_tb = elm_table_add(win);
    evas_object_size_hint_weight_set(graph_tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(graph_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_min_set(graph_tb, 420 * scale, 260 * scale);
    elm_box_pack_end(graph_bx, graph_tb);
    evas_object_show(graph_tb);

    view->graph_bg = evas_object_rectangle_add(evas);
    evisum_ui_graph_bg_set(view->graph_bg);
    evas_object_size_hint_weight_set(view->graph_bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(view->graph_bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(graph_tb, view->graph_bg, 0, 0, 1, 1);
    evas_object_show(view->graph_bg);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_RESIZE, _evisum_ui_network_graph_bg_resize_cb, view);
    evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_MOVE, _evisum_ui_network_graph_bg_resize_cb, view);

    view->graph_img = evas_object_image_filled_add(evas);
    evas_object_image_alpha_set(view->graph_img, EINA_FALSE);
    evas_object_size_hint_weight_set(view->graph_img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_size_hint_align_set(view->graph_img, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_table_pack(graph_tb, view->graph_img, 0, 0, 1, 1);
    evas_object_show(view->graph_img);
    evas_object_stack_above(view->graph_img, view->graph_bg);

    legend_fr = elm_frame_add(win);
    elm_object_text_set(legend_fr, _("Interfaces"));
    evas_object_size_hint_weight_set(legend_fr, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(legend_fr, EVAS_HINT_FILL, 0.0);
    elm_box_pack_end(root_bx, legend_fr);
    evas_object_show(legend_fr);

    view->legend_bx = elm_box_add(legend_fr);
    elm_box_padding_set(view->legend_bx, 0, 2 * scale);
    evas_object_size_hint_weight_set(view->legend_bx, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(view->legend_bx, EVAS_HINT_FILL, 0.0);
    evas_object_size_hint_min_set(view->legend_bx, 420 * scale, 0);
    elm_object_content_set(legend_fr, view->legend_bx);
    evas_object_show(view->legend_bx);

    btn = elm_button_add(win);
    ic = elm_icon_add(btn);
    elm_icon_standard_set(ic, "menu");
    elm_object_part_content_set(btn, "icon", ic);
    evas_object_show(ic);
    elm_object_focus_allow_set(btn, 0);
    evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_network_btn_menu_clicked_cb, view);

    view->btn_menu = lay = elm_layout_add(win);
    evas_object_size_hint_weight_set(lay, 1.0, 1.0);
    evas_object_size_hint_align_set(lay, 0.99, 0.01);
    elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
    elm_layout_content_set(lay, "evisum/menu", btn);
    elm_table_pack(tb, lay, 0, 0, 1, 1);
    evas_object_show(lay);

    if ((ui->network.width > 0) && (ui->network.height > 0))
        evas_object_resize(win, ui->network.width, ui->network.height);
    else evas_object_resize(win, WIN_WIDTH, WIN_HEIGHT);

    if ((ui->network.x > 0) && (ui->network.y > 0)) evas_object_move(win, ui->network.x, ui->network.y);
    else elm_win_center(win, 1, 1);

    elm_object_focus_allow_set(tb, EINA_TRUE);
    elm_object_focus_set(tb, EINA_TRUE);
    evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_network_win_key_down_cb, view);
    evas_object_show(win);
    evas_object_event_callback_add(root_bx, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_network_win_mouse_move_cb, view);

    view->thread = ecore_thread_feedback_run(_evisum_ui_network_update, _evisum_ui_network_update_feedback_cb, NULL,
                                             NULL, view, 1);
}
