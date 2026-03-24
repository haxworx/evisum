#include "evisum_ui_graph.h"
#include <math.h>

static uint32_t
_argb(unsigned int r, unsigned int g, unsigned int b)
{
   return 0xff000000 | (r << 16) | (g << 8) | b;
}

static void
_pixel_set(uint32_t *px, int w, int h, int x, int y, uint32_t color)
{
   if ((x < 0) || (y < 0) || (x >= w) || (y >= h))
     return;
   px[y * w + x] = color;
}

static void
_pixel_blend(uint32_t *px, int w, int h, int x, int y,
             uint8_t sr, uint8_t sg, uint8_t sb, double alpha)
{
   uint32_t p;
   uint8_t dr, dg, db;
   uint8_t r, g, b;

   if ((x < 0) || (y < 0) || (x >= w) || (y >= h))
     return;
   if (alpha <= 0.0)
     return;
   if (alpha > 1.0)
     alpha = 1.0;

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
_fpart(double x)
{
   return x - floor(x);
}

static double
_rfpart(double x)
{
   return 1.0 - _fpart(x);
}

static void
_pixel_line_aa(uint32_t *px, int w, int h,
               double x0, double y0, double x1, double y1,
               uint8_t r, uint8_t g, uint8_t b, double alpha)
{
   Eina_Bool steep;
   double dx, dy, grad, xend, yend, xgap, intery;
   int xpxl1, ypxl1, xpxl2, ypxl2;

   steep = fabs(y1 - y0) > fabs(x1 - x0);
   if (steep)
     {
        double t;
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
     }
   if (x0 > x1)
     {
        double t;
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
     }

   dx = x1 - x0;
   dy = y1 - y0;
   grad = (dx == 0.0) ? 1.0 : (dy / dx);

   xend = round(x0);
   yend = y0 + grad * (xend - x0);
   xgap = _rfpart(x0 + 0.5);
   xpxl1 = (int) xend;
   ypxl1 = (int) floor(yend);
   if (steep)
     {
        _pixel_blend(px, w, h, ypxl1,     xpxl1, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, ypxl1 + 1, xpxl1, r, g, b, _fpart(yend) * xgap * alpha);
     }
   else
     {
        _pixel_blend(px, w, h, xpxl1, ypxl1,     r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, xpxl1, ypxl1 + 1, r, g, b, _fpart(yend) * xgap * alpha);
     }
   intery = yend + grad;

   xend = round(x1);
   yend = y1 + grad * (xend - x1);
   xgap = _fpart(x1 + 0.5);
   xpxl2 = (int) xend;
   ypxl2 = (int) floor(yend);
   if (steep)
     {
        _pixel_blend(px, w, h, ypxl2,     xpxl2, r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, ypxl2 + 1, xpxl2, r, g, b, _fpart(yend) * xgap * alpha);
     }
   else
     {
        _pixel_blend(px, w, h, xpxl2, ypxl2,     r, g, b, _rfpart(yend) * xgap * alpha);
        _pixel_blend(px, w, h, xpxl2, ypxl2 + 1, r, g, b, _fpart(yend) * xgap * alpha);
     }

   if (steep)
     {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++)
          {
             int iy = (int) floor(intery);
             _pixel_blend(px, w, h, iy,     x, r, g, b, _rfpart(intery) * alpha);
             _pixel_blend(px, w, h, iy + 1, x, r, g, b, _fpart(intery) * alpha);
             intery += grad;
          }
     }
   else
     {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++)
          {
             int iy = (int) floor(intery);
             _pixel_blend(px, w, h, x, iy,     r, g, b, _rfpart(intery) * alpha);
             _pixel_blend(px, w, h, x, iy + 1, r, g, b, _fpart(intery) * alpha);
             intery += grad;
          }
     }
}

void
evisum_ui_graph_draw(Evas_Object                 *graph_bg,
                     Evas_Object                 *graph_img,
                     int                          sample_count,
                     double                       y_max,
                     const Evisum_Ui_Graph_Series *series,
                     int                          series_count,
                     const Evisum_Ui_Graph_Layer *layers,
                     int                          layer_count)
{
   uint32_t *px;
   int gx, gy, gw, gh;
   int x, y, cell;

   if (!graph_bg || !graph_img || (sample_count < 2))
     return;

   if (y_max <= 0.0)
     y_max = 1.0;

   evas_object_geometry_get(graph_bg, &gx, &gy, &gw, &gh);
   if ((gw < 10) || (gh < 10))
     return;

   evas_object_move(graph_img, gx, gy);
   evas_object_resize(graph_img, gw, gh);
   evas_object_image_size_set(graph_img, gw, gh);
   evas_object_image_fill_set(graph_img, 0, 0, gw, gh);

   px = evas_object_image_data_get(graph_img, EINA_TRUE);
   if (!px)
     return;

   for (y = 0; y < gh; y++)
     for (x = 0; x < gw; x++)
       px[y * gw + x] = _argb(32, 32, 32);

   cell = (gw < gh) ? (gw / 10) : (gh / 10);
   if (cell < 12) cell = 12;

   for (x = 0; x < gw; x += cell)
     for (y = 0; y < gh; y++)
       _pixel_set(px, gw, gh, x, y, _argb(56, 56, 56));

   for (y = 0; y < gh; y += cell)
     for (x = 0; x < gw; x++)
       _pixel_set(px, gw, gh, x, y, _argb(48, 48, 48));

   for (int s = 0; s < series_count; s++)
     {
        int idx, start, count;
        int x0, y0, x1, y1;
        const Evisum_Ui_Graph_Series *line = &series[s];

        if (!line->history || (line->history_count < 2))
          continue;

        count = line->history_count;
        start = 0;
        if (count > sample_count)
          {
             start = count - sample_count;
             count = sample_count;
          }

        idx = sample_count - count;
        x0 = (int) (((double) idx / (double) (sample_count - 1)) * (double) (gw - 1));
        y0 = (int) ((1.0 - (line->history[start] / y_max)) * (double) (gh - 1));
        if (y0 < 0) y0 = 0;
        if (y0 >= gh) y0 = gh - 1;

        for (int i = 1; i < count; i++)
          {
             idx = sample_count - count + i;
             x1 = (int) (((double) idx / (double) (sample_count - 1)) * (double) (gw - 1));
             y1 = (int) ((1.0 - (line->history[start + i] / y_max)) * (double) (gh - 1));
             if (y1 < 0) y1 = 0;
             if (y1 >= gh) y1 = gh - 1;

             for (int j = 0; j < layer_count; j++)
               {
                  _pixel_line_aa(px, gw, gh,
                                 (double) x0, (double) y0 + layers[j].offset,
                                 (double) x1, (double) y1 + layers[j].offset,
                                 line->color_r, line->color_g, line->color_b, layers[j].alpha);
               }

             x0 = x1;
             y0 = y1;
          }
     }

   evas_object_image_data_set(graph_img, px);
   evas_object_image_data_update_add(graph_img, 0, 0, gw, gh);
}
