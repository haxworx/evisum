#ifndef __EVISUM_UI_GRAPH_H__
#define __EVISUM_UI_GRAPH_H__

#include "evisum_ui.h"

typedef struct _Evisum_Ui_Graph_Series
{
   const double *history;
   int           history_count;
   uint8_t       color_r;
   uint8_t       color_g;
   uint8_t       color_b;
} Evisum_Ui_Graph_Series;

typedef struct _Evisum_Ui_Graph_Layer
{
   double offset;
   double alpha;
} Evisum_Ui_Graph_Layer;

void
evisum_ui_graph_draw(Evas_Object                       *graph_bg,
                     Evas_Object                       *graph_img,
                     int                                sample_count,
                     double                             y_max,
                     const Evisum_Ui_Graph_Series      *series,
                     int                                series_count,
                     const Evisum_Ui_Graph_Layer       *layers,
                     int                                layer_count);

void
evisum_ui_graph_bg_set(Evas_Object *graph_bg);

#endif
