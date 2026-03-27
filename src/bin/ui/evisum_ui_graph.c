#include "evisum_ui_graph.h"
#include "evisum_ui_colors.h"
#include <math.h>

static Eina_Bool
_graph_target_valid(Evas_Object *graph_bg, Evas_Object *graph_img) {
    return graph_bg && graph_img && evas_object_evas_get(graph_bg) && evas_object_evas_get(graph_img);
}

static uint32_t
_argb(unsigned int r, unsigned int g, unsigned int b) {
    return 0xff000000 | (r << 16) | (g << 8) | b;
}

static void
_pixel_set(uint32_t *px, int w, int h, int x, int y, uint32_t color) {
    if ((x < 0) || (y < 0) || (x >= w) || (y >= h)) return;
    px[y * w + x] = color;
}

static void
_pixel_blend(uint32_t *px, int w, int h, int x, int y, uint8_t sr, uint8_t sg, uint8_t sb, double alpha) {
    uint32_t p;
    uint8_t dr, dg, db;
    uint8_t r, g, b;

    if ((x < 0) || (y < 0) || (x >= w) || (y >= h)) return;
    if (alpha <= 0.0) return;
    if (alpha > 1.0) alpha = 1.0;

    p = px[y * w + x];
    dr = (p >> 16) & 0xff;
    dg = (p >> 8) & 0xff;
    db = p & 0xff;

    r = (uint8_t) ((double) dr * (1.0 - alpha) + (double) sr * alpha);
    g = (uint8_t) ((double) dg * (1.0 - alpha) + (double) sg * alpha);
    b = (uint8_t) ((double) db * (1.0 - alpha) + (double) sb * alpha);
    px[y * w + x] = 0xff000000 | (r << 16) | (g << 8) | b;
}

static double
_fpart(double x) {
    return x - floor(x);
}

static double
_rfpart(double x) {
    return 1.0 - _fpart(x);
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

static void
_pixel_line_aa(uint32_t *px, int w, int h, double x0, double y0, double x1, double y1, uint8_t r, uint8_t g, uint8_t b,
               double alpha) {
    Eina_Bool steep;
    double dx, dy, grad, xend, yend, xgap, intery;
    int xpxl1, ypxl1, xpxl2, ypxl2;

    steep = fabs(y1 - y0) > fabs(x1 - x0);
    if (steep) {
        double t;
        t = x0;
        x0 = y0;
        y0 = t;
        t = x1;
        x1 = y1;
        y1 = t;
    }
    if (x0 > x1) {
        double t;
        t = x0;
        x0 = x1;
        x1 = t;
        t = y0;
        y0 = y1;
        y1 = t;
    }

    dx = x1 - x0;
    dy = y1 - y0;
    grad = (dx == 0.0) ? 1.0 : (dy / dx);

    xend = round(x0);
    yend = y0 + grad * (xend - x0);
    xgap = _rfpart(x0 + 0.5);
    xpxl1 = (int) xend;
    ypxl1 = (int) floor(yend);
    if (steep) {
        _pixel_blend(px, w, h, ypxl1, xpxl1, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, ypxl1 + 1, xpxl1, r, g, b, _fpart(yend) * xgap * alpha);
    } else {
        _pixel_blend(px, w, h, xpxl1, ypxl1, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, xpxl1, ypxl1 + 1, r, g, b, _fpart(yend) * xgap * alpha);
    }
    intery = yend + grad;

    xend = round(x1);
    yend = y1 + grad * (xend - x1);
    xgap = _fpart(x1 + 0.5);
    xpxl2 = (int) xend;
    ypxl2 = (int) floor(yend);
    if (steep) {
        _pixel_blend(px, w, h, ypxl2, xpxl2, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, ypxl2 + 1, xpxl2, r, g, b, _fpart(yend) * xgap * alpha);
    } else {
        _pixel_blend(px, w, h, xpxl2, ypxl2, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, xpxl2, ypxl2 + 1, r, g, b, _fpart(yend) * xgap * alpha);
    }

    if (steep) {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
            int iy = (int) floor(intery);
            _pixel_blend(px, w, h, iy, x, r, g, b, _rfpart(intery) * alpha);
            _pixel_blend(px, w, h, iy + 1, x, r, g, b, _fpart(intery) * alpha);
            intery += grad;
        }
    } else {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
            int iy = (int) floor(intery);
            _pixel_blend(px, w, h, x, iy, r, g, b, _rfpart(intery) * alpha);
            _pixel_blend(px, w, h, x, iy + 1, r, g, b, _fpart(intery) * alpha);
            intery += grad;
        }
    }
}

void
evisum_ui_graph_draw(Evas_Object *graph_bg, Evas_Object *graph_img, int sample_count, int x_grid_step_samples,
                     int y_grid_step_percent, double y_max, const Evisum_Ui_Graph_Series *series, int series_count,
                     const Evisum_Ui_Graph_Layer *layers, int layer_count) {
    uint32_t *px;
    int gx, gy, gw, gh;
    int x, y;
    int view_x, view_y, view_w, view_h;
    uint8_t bg_r, bg_g, bg_b;
    uint8_t grid_v_r, grid_v_g, grid_v_b;
    uint8_t grid_h_r, grid_h_g, grid_h_b;

    if (!_graph_target_valid(graph_bg, graph_img) || (sample_count < 2)) return;
    if (x_grid_step_samples < 1) x_grid_step_samples = 1;
    if (y_grid_step_percent < 1) y_grid_step_percent = 1;
    if (y_grid_step_percent > 100) y_grid_step_percent = 100;
    if ((series_count > 0) && !series) return;
    if ((layer_count > 0) && !layers) return;

    if (y_max <= 0.0) y_max = 1.0;

    evas_object_geometry_get(graph_bg, &gx, &gy, &gw, &gh);
    if ((gw < 10) || (gh < 10)) return;

    evas_object_move(graph_img, gx, gy);
    evas_object_resize(graph_img, gw, gh);
    evas_object_image_size_set(graph_img, gw, gh);
    evas_object_image_fill_set(graph_img, 0, 0, gw, gh);

    px = evas_object_image_data_get(graph_img, EINA_TRUE);
    if (!px) return;

    evisum_graph_widget_colors_get(&bg_r, &bg_g, &bg_b, &grid_v_r, &grid_v_g, &grid_v_b, &grid_h_r, &grid_h_g,
                                   &grid_h_b);

    for (y = 0; y < gh; y++)
        for (x = 0; x < gw; x++) px[y * gw + x] = _argb(bg_r, bg_g, bg_b);

    _square_grid_viewport_calc(gw, gh, sample_count, x_grid_step_samples, y_grid_step_percent, &view_x, &view_y,
                               &view_w, &view_h);

    for (int sample_idx = 0; sample_idx < sample_count; sample_idx += x_grid_step_samples) {
        x = view_x + _sample_x_pos(sample_idx, sample_count, view_w);
        for (y = view_y; y < (view_y + view_h); y++) _pixel_set(px, gw, gh, x, y, _argb(grid_v_r, grid_v_g, grid_v_b));
    }

    for (int percent = 0; percent <= 100; percent += y_grid_step_percent) {
        y = view_y + (int) lround((1.0 - ((double) percent / 100.0)) * (double) (view_h - 1));
        for (x = view_x; x < (view_x + view_w); x++) _pixel_set(px, gw, gh, x, y, _argb(grid_h_r, grid_h_g, grid_h_b));
    }

    for (int s = 0; s < series_count; s++) {
        int idx, start, count;
        int x0, y0, x1, y1;
        const Evisum_Ui_Graph_Series *line = &series[s];

        if (!line->history || (line->history_count < 2)) continue;

        count = line->history_count;
        start = 0;
        if (count > sample_count) {
            start = count - sample_count;
            count = sample_count;
        }

        idx = sample_count - count;
        x0 = view_x + _sample_x_pos(idx, sample_count, view_w);
        y0 = view_y + (int) ((1.0 - (line->history[start] / y_max)) * (double) (view_h - 1));
        if (y0 < view_y) y0 = view_y;
        if (y0 >= (view_y + view_h)) y0 = view_y + view_h - 1;

        for (int i = 1; i < count; i++) {
            idx = sample_count - count + i;
            x1 = view_x + _sample_x_pos(idx, sample_count, view_w);
            y1 = view_y + (int) ((1.0 - (line->history[start + i] / y_max)) * (double) (view_h - 1));
            if (y1 < view_y) y1 = view_y;
            if (y1 >= (view_y + view_h)) y1 = view_y + view_h - 1;

            for (int j = 0; j < layer_count; j++) {
                _pixel_line_aa(px, gw, gh, (double) x0, (double) y0 + layers[j].offset, (double) x1,
                               (double) y1 + layers[j].offset, line->color_r, line->color_g, line->color_b,
                               layers[j].alpha);
            }

            x0 = x1;
            y0 = y1;
        }
    }

    evas_object_image_data_set(graph_img, px);
    evas_object_image_data_update_add(graph_img, 0, 0, gw, gh);
}

void
evisum_ui_graph_bg_set(Evas_Object *graph_bg) {
    uint8_t r, g, b;

    if (!graph_bg) return;

    evisum_graph_widget_colors_get(&r, &g, &b, NULL, NULL, NULL, NULL, NULL, NULL);
    evas_object_color_set(graph_bg, r, g, b, 255);
}
