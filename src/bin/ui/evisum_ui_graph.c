#include "evisum_ui_graph.h"
#include "evisum_ui_colors.h"
#include <math.h>

#define GRAPH_LINE_STROKE_WIDTH 1.5
#define GRAPH_ROOT_DATA_KEY     "evisum_ui_graph_root"

static Eina_Bool
_graph_target_valid(Evas_Object *graph_bg, Evas_Object *graph_vg) {
    return graph_bg && graph_vg && evas_object_evas_get(graph_bg) && evas_object_evas_get(graph_vg);
}

static void
_graph_root_children_clear(Evas_Vg_Container *root) {
    Eina_Iterator *it;
    Eina_List *nodes = NULL;
    Evas_Vg_Node *node;

    if (!root) return;

    it = evas_vg_container_children_get(root);
    if (!it) return;

    EINA_ITERATOR_FOREACH(it, node)
    nodes = eina_list_append(nodes, node);

    eina_iterator_free(it);

    EINA_LIST_FREE(nodes, node)
    efl_del(node);
}

static Evas_Vg_Container *
_graph_root_reset(Evas_Object *graph_vg) {
    Evas_Vg_Container *root = evas_object_data_get(graph_vg, GRAPH_ROOT_DATA_KEY);
    Evas_Vg_Node *root_node;

    root_node = evas_object_vg_root_node_get(graph_vg);
    if (!root || (root_node != (Evas_Vg_Node *) root)) {
        root = evas_vg_container_add(graph_vg);
        if (!root) return NULL;

        evas_object_vg_root_node_set(graph_vg, (Evas_Vg_Node *) root);
        evas_object_data_set(graph_vg, GRAPH_ROOT_DATA_KEY, root);
        return root;
    }

    _graph_root_children_clear(root);
    return root;
}

static Evas_Vg_Shape *
_shape_stroke_add(Evas_Vg_Container *root, uint8_t r, uint8_t g, uint8_t b, uint8_t a, double width) {
    Evas_Vg_Shape *shape;

    shape = evas_vg_shape_add(root);
    if (!shape) return NULL;

    evas_vg_shape_stroke_width_set(shape, width);
    evas_vg_shape_stroke_color_set(shape, r, g, b, a);
    return shape;
}

static uint8_t
_alpha_to_u8(double alpha) {
    if (alpha <= 0.0) return 0;
    if (alpha >= 1.0) return 255;
    return (uint8_t) lround(alpha * 255.0);
}

static double
_clamp_double(double value, double min, double max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static double
_sample_y_pos(double value, double y_max, int view_y, int view_h) {
    double y;

    y = (double) view_y + (1.0 - (value / y_max)) * (double) (view_h - 1);
    return _clamp_double(y, (double) view_y, (double) (view_y + view_h - 1));
}

static int
_sample_x_pos(int sample_idx, int sample_count, int graph_w) {
    if (sample_count < 2) return 0;
    if (sample_idx < 0) sample_idx = 0;
    if (sample_idx >= sample_count) sample_idx = sample_count - 1;
    return (int) lround(((double) sample_idx / (double) (sample_count - 1)) * (double) (graph_w - 1));
}

static void
_square_grid_viewport_calc(int graph_w, int graph_h, int sample_count, int x_grid_step_samples, int y_grid_step_percent,
                           int *out_x, int *out_y, int *out_w, int *out_h) {
    double x_cells, y_cells, target_aspect;
    int view_w = graph_w, view_h = graph_h;
    int view_x = 0, view_y = 0;

    x_cells = (double) (sample_count - 1) / (double) x_grid_step_samples;
    y_cells = 100.0 / (double) y_grid_step_percent;
    if (x_cells <= 0.0 || y_cells <= 0.0) {
        *out_x = 0;
        *out_y = 0;
        *out_w = graph_w;
        *out_h = graph_h;
        return;
    }

    target_aspect = x_cells / y_cells;
    if (target_aspect <= 0.0) {
        *out_x = 0;
        *out_y = 0;
        *out_w = graph_w;
        *out_h = graph_h;
        return;
    }

    if (((double) graph_w / (double) graph_h) > target_aspect) {
        view_w = (int) lround((double) graph_h * target_aspect);
        if (view_w < 1) view_w = 1;
        view_x = (graph_w - view_w) / 2;
    } else {
        view_h = (int) lround((double) graph_w / target_aspect);
        if (view_h < 1) view_h = 1;
        view_y = (graph_h - view_h) / 2;
    }

    *out_x = view_x;
    *out_y = view_y;
    *out_w = view_w;
    *out_h = view_h;
}

void
evisum_ui_graph_draw(Evas_Object *graph_bg, Evas_Object *graph_vg, int sample_count, int x_grid_step_samples,
                     int y_grid_step_percent, double y_max, const Evisum_Ui_Graph_Series *series, int series_count,
                     const Evisum_Ui_Graph_Layer *layers, int layer_count) {
    Evas_Vg_Container *root;
    Evas_Vg_Shape *shape;
    int gx, gy, gw, gh;
    int view_x, view_y, view_w, view_h;
    uint8_t grid_v_r, grid_v_g, grid_v_b;
    uint8_t grid_h_r, grid_h_g, grid_h_b;

    if (!_graph_target_valid(graph_bg, graph_vg) || (sample_count < 2)) return;
    if (x_grid_step_samples < 1) x_grid_step_samples = 1;
    if (y_grid_step_percent < 1) y_grid_step_percent = 1;
    if (y_grid_step_percent > 100) y_grid_step_percent = 100;
    if ((series_count > 0) && !series) return;
    if ((layer_count > 0) && !layers) return;

    if (y_max <= 0.0) y_max = 1.0;

    evas_object_geometry_get(graph_bg, &gx, &gy, &gw, &gh);
    if ((gw < 10) || (gh < 10)) return;

    evas_object_move(graph_vg, gx, gy);
    evas_object_resize(graph_vg, gw, gh);

    evisum_graph_widget_colors_get(NULL, NULL, NULL, &grid_v_r, &grid_v_g, &grid_v_b, &grid_h_r, &grid_h_g, &grid_h_b);

    root = _graph_root_reset(graph_vg);
    if (!root) return;

    _square_grid_viewport_calc(gw, gh, sample_count, x_grid_step_samples, y_grid_step_percent, &view_x, &view_y,
                               &view_w, &view_h);
    if ((view_w < 1) || (view_h < 1)) return;

    shape = _shape_stroke_add(root, grid_v_r, grid_v_g, grid_v_b, 255, 1.0);
    if (shape) {
        for (int sample_idx = 0; sample_idx < sample_count; sample_idx += x_grid_step_samples) {
            int x;
            double vx;

            x = view_x + _sample_x_pos(sample_idx, sample_count, view_w);
            vx = (double) x + 0.5;
            evas_vg_shape_append_move_to(shape, vx, (double) view_y);
            evas_vg_shape_append_line_to(shape, vx, (double) (view_y + view_h - 1));
        }
    }

    shape = _shape_stroke_add(root, grid_h_r, grid_h_g, grid_h_b, 255, 1.0);
    if (shape) {
        for (int percent = 0; percent <= 100; percent += y_grid_step_percent) {
            double y;

            y = (double) view_y + (1.0 - ((double) percent / 100.0)) * (double) (view_h - 1) + 0.5;
            evas_vg_shape_append_move_to(shape, (double) view_x, y);
            evas_vg_shape_append_line_to(shape, (double) (view_x + view_w - 1), y);
        }
    }

    for (int s = 0; s < series_count; s++) {
        int start, count;
        const Evisum_Ui_Graph_Series *line = &series[s];

        if (!line->history || (line->history_count < 2)) continue;

        count = line->history_count;
        start = 0;
        if (count > sample_count) {
            start = count - sample_count;
            count = sample_count;
        }

        for (int j = 0; j < layer_count; j++) {
            uint8_t alpha = _alpha_to_u8(layers[j].alpha);
            uint8_t pr, pg, pb;
            double min_y = (double) view_y;
            double max_y = (double) (view_y + view_h - 1);
            int idx;
            double x0, y0;

            if (!alpha) continue;

            pr = (uint8_t) (((unsigned int) line->color_r * (unsigned int) alpha) / 255U);
            pg = (uint8_t) (((unsigned int) line->color_g * (unsigned int) alpha) / 255U);
            pb = (uint8_t) (((unsigned int) line->color_b * (unsigned int) alpha) / 255U);
            shape = _shape_stroke_add(root, pr, pg, pb, alpha, GRAPH_LINE_STROKE_WIDTH);
            if (!shape) continue;

            idx = sample_count - count;
            x0 = (double) view_x + (double) _sample_x_pos(idx, sample_count, view_w);
            y0 = _sample_y_pos(line->history[start], y_max, view_y, view_h) + layers[j].offset;
            y0 = _clamp_double(y0, min_y, max_y);
            evas_vg_shape_append_move_to(shape, x0, y0);

            for (int i = 1; i < count; i++) {
                double x1, y1;

                idx = sample_count - count + i;
                x1 = (double) view_x + (double) _sample_x_pos(idx, sample_count, view_w);
                y1 = _sample_y_pos(line->history[start + i], y_max, view_y, view_h) + layers[j].offset;
                y1 = _clamp_double(y1, min_y, max_y);
                evas_vg_shape_append_line_to(shape, x1, y1);
            }
        }
    }

    /* Keep VG output untinted; graph background color is provided by graph_bg. */
    evas_object_color_set(graph_vg, 255, 255, 255, 255);
}

void
evisum_ui_graph_bg_set(Evas_Object *graph_bg) {
    uint8_t r, g, b;

    if (!graph_bg) return;

    evisum_graph_widget_colors_get(&r, &g, &b, NULL, NULL, NULL, NULL, NULL, NULL);
    evas_object_color_set(graph_bg, r, g, b, 255);
}
