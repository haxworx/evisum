#include "cpu_default.h"
#include "ui/evisum_ui_graph.h"
#include "../../background/evisum_background.h"

#define CPU_GRAPH_SAMPLES       120
#define CPU_GRID_X_STEP_SAMPLES 5
#define CPU_GRID_Y_STEP_PERCENT 10

typedef struct _Ext Ext;

typedef struct
{
   char name[64];
   uint8_t color_r, color_g, color_b;
   double history[CPU_GRAPH_SAMPLES];
   int history_count;
   int id;
   int used;
   int total;
   Eina_Bool enabled;
   Eina_Bool visible;
   Ext *view;
   Evas_Object *legend_row;
   Evas_Object *legend_btn;
   Evas_Object *legend_swatch;
   Evas_Object *legend_label;
   Evas_Object *legend_pb;
} Cpu_Series;

struct _Ext
{
   Evas_Object *graph_bg;
   Evas_Object *graph_img;
   Evas_Object *legend_tb;
   Cpu_Series *series;
   int series_count;
};

static const Evisum_Ui_Graph_Layer _cpu_layers[] = {
   { -0.6, 0.24 },
   { 0.6, 0.24 },
   { 0.0, 0.92 },
};

static void
_cpu_graph_redraw(Ext *view);

static Eina_Bool
_cpu_graph_objects_valid(Ext *view)
{
   return view && view->graph_bg && view->graph_img && evas_object_evas_get(view->graph_bg)
          && evas_object_evas_get(view->graph_img);
}

static void
_cpu_legend_toggle_state_apply(Cpu_Series *entry)
{
   if (!entry) return;

   if (!entry->visible && entry->legend_swatch) evas_object_hide(entry->legend_swatch);
   else if (entry->visible && entry->legend_swatch) evas_object_show(entry->legend_swatch);

   if (entry->legend_pb && evas_object_evas_get(entry->legend_pb))
      elm_object_disabled_set(entry->legend_pb, !entry->visible || !entry->total);
   else entry->legend_pb = NULL;
}

static void
_cpu_legend_toggle_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Cpu_Series *entry = data;

   if (!entry || !entry->view || !entry->legend_btn || !entry->legend_row) return;
   if (!_cpu_graph_objects_valid(entry->view)) return;
   entry->visible = !entry->visible;
   _cpu_legend_toggle_state_apply(entry);
   _cpu_graph_redraw(entry->view);
}

static void
_cpu_legend_repack(Ext *view)
{
   int row = 0;

   if (!view->legend_tb) return;

   elm_table_clear(view->legend_tb, 0);

   for (int i = 0; i < view->series_count; i++)
     {
        Cpu_Series *entry = &view->series[i];

        if (!entry->enabled || !entry->legend_row || !entry->legend_pb)
          {
             if (entry->legend_row) evas_object_hide(entry->legend_row);
             if (entry->legend_pb) evas_object_hide(entry->legend_pb);
             continue;
          }

        elm_table_pack(view->legend_tb, entry->legend_row, 0, row, 1, 1);
        elm_table_pack(view->legend_tb, entry->legend_pb, 1, row, 1, 1);
        evas_object_show(entry->legend_row);
        evas_object_show(entry->legend_pb);
        row++;
     }
}

static void
_cpu_series_history_add(Cpu_Series *entry, double value)
{
   if (entry->history_count < CPU_GRAPH_SAMPLES) entry->history[entry->history_count++] = value;
   else
     {
        memmove(&entry->history[0], &entry->history[1], sizeof(double) * (CPU_GRAPH_SAMPLES - 1));
        entry->history[CPU_GRAPH_SAMPLES - 1] = value;
     }
}

static void
_cpu_series_legend_add(Ext *view, Cpu_Series *entry)
{
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
   evas_object_smart_callback_add(btn, "clicked", _cpu_legend_toggle_cb, entry);
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
   elm_object_text_set(lb, entry->name);
   elm_box_pack_end(left, lb);
   evas_object_show(lb);

   pb = elm_progressbar_add(view->legend_tb);
   elm_object_text_set(pb, NULL);
   elm_progressbar_span_size_set(pb, ELM_SCALE_SIZE(220));
   elm_progressbar_unit_format_set(pb, "%.0f%%");
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
   evas_object_show(pb);

   entry->legend_row = left;
   entry->legend_btn = btn;
   entry->legend_swatch = swatch;
   entry->legend_label = lb;
   entry->legend_pb = pb;
   _cpu_legend_toggle_state_apply(entry);
}

static void
_cpu_series_legend_update(Cpu_Series *entry)
{
   double value = 0.0;

   if (!entry->legend_pb || !entry->legend_label) return;

   elm_object_text_set(entry->legend_label, entry->name);

   if (entry->total)
     {
        value = (double) entry->used / (double) entry->total;
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
     }

   elm_progressbar_value_set(entry->legend_pb, value);
   _cpu_legend_toggle_state_apply(entry);
}

static void
_cpu_graph_redraw(Ext *view)
{
   Evisum_Ui_Graph_Series *series;
   int nseries = 0;

   if (!_cpu_graph_objects_valid(view)) return;

   series = calloc(view->series_count, sizeof(*series));
   if (!series) return;

   for (int i = 0; i < view->series_count; i++)
     {
        Cpu_Series *entry = &view->series[i];

        if (!entry->enabled || !entry->visible || (entry->history_count < 2)) continue;

        series[nseries].history = entry->history;
        series[nseries].history_count = entry->history_count;
        series[nseries].color_r = entry->color_r;
        series[nseries].color_g = entry->color_g;
        series[nseries].color_b = entry->color_b;
        nseries++;
     }

   evisum_ui_graph_draw(view->graph_bg, view->graph_img, CPU_GRAPH_SAMPLES, CPU_GRID_X_STEP_SAMPLES,
                        CPU_GRID_Y_STEP_PERCENT, 100.0, series, nseries, _cpu_layers, EINA_C_ARRAY_LENGTH(_cpu_layers));
   free(series);
}

static void
_cpu_graph_bg_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Ext *view = data;

   _cpu_graph_redraw(view);
}

static void
_core_times_main_cb(void *data, Ecore_Thread *thread)
{
   Evisum_Ui_Cpu_Visual_Data *pd = data;
   Ext *view = pd->ext;
   uint64_t seq = 0;
   uint32_t last_snapshot_time = 0;

   ecore_thread_name_set(thread, "cpu");
   while (!ecore_thread_check(thread))
     {
        Cpu_Core **cores;
        Core *sample;
        uint32_t snapshot_time;
        int ncpu;

        if (!evisum_background_update_wait(&seq)) continue;
        snapshot_time = evisum_engine_live_time_get();
        if (!snapshot_time || (snapshot_time == last_snapshot_time)) continue;

        cores = system_cpu_state_get(&ncpu);
        if (!cores || ncpu <= 0)
          {
             free(cores);
             continue;
          }
        sample = calloc(view->series_count, sizeof(*sample));
        if (sample)
          {
             for (int i = 0; i < view->series_count; i++)
               {
                  int id = view->series[i].id;
                  if (id >= 0 && id < ncpu) sample[i].percent = cores[id]->percent;
               }
             last_snapshot_time = snapshot_time;
             ecore_thread_feedback(thread, sample);
          }
        free(cores);
     }
}

static void
_core_times_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Evisum_Ui_Cpu_Visual_Data *pd = data;
   Ext *view = pd->ext;
   Core *cores = msgdata;

   for (int i = 0; i < view->series_count; i++)
     {
        Cpu_Series *entry = &view->series[i];
        int percent = cores[i].percent;

        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        entry->used = percent;
        _cpu_series_history_add(entry, percent);
        _cpu_series_legend_update(entry);
     }
   _cpu_graph_redraw(view);
   free(cores);
}

static void
_cb_free(void *data)
{
   Ext *view = data;

   free(view->series);
   free(view);
}

Evisum_Ui_Cpu_Visual_Data *
evisum_ui_cpu_visual_default(Evas_Object *parent_box)
{
   Evisum_Ui_Cpu_Visual_Data *pd;
   Ext *view;
   Evas_Object *tb, *graph_tb, *legend_fr, *legend_sc;
   Evas *evas = evas_object_evas_get(parent_box);
   int *cpu_order;

   pd = calloc(1, sizeof(*pd));
   if (!pd) return NULL;
   pd->ext = view = calloc(1, sizeof(*view));
   if (!view)
     {
        free(pd);
        return NULL;
     }
   pd->ext_free_cb = _cb_free;
   view->series_count = system_cpu_count_get();
   if (view->series_count < 1) view->series_count = 1;
   view->series = calloc(view->series_count, sizeof(*view->series));
   cpu_order = malloc(view->series_count * sizeof(*cpu_order));
   if (!view->series || !cpu_order)
     {
        free(cpu_order);
        _cb_free(view);
        free(pd);
        return NULL;
     }
   for (int i = 0; i < view->series_count; i++) cpu_order[i] = i;
   system_cpu_topology_get(cpu_order, view->series_count);

   tb = elm_table_add(parent_box);
   evas_object_size_hint_weight_set(tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(parent_box, tb);
   evas_object_show(tb);

   graph_tb = elm_table_add(tb);
   evas_object_size_hint_weight_set(graph_tb, EVAS_HINT_EXPAND, 1.0);
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
   evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_RESIZE, _cpu_graph_bg_resize_cb, view);
   evas_object_event_callback_add(view->graph_bg, EVAS_CALLBACK_MOVE, _cpu_graph_bg_resize_cb, view);

   view->graph_img = evas_object_vg_add(evas);
   evas_object_size_hint_weight_set(view->graph_img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(view->graph_img, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(graph_tb, view->graph_img, 0, 0, 1, 1);
   evas_object_show(view->graph_img);
   evas_object_stack_above(view->graph_img, view->graph_bg);

   legend_fr = elm_frame_add(tb);
   elm_object_text_set(legend_fr, _("CPU Cores"));
   evas_object_size_hint_weight_set(legend_fr, EVAS_HINT_EXPAND, 1.6);
   evas_object_size_hint_align_set(legend_fr, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(legend_fr, 1, ELM_SCALE_SIZE(200));
   elm_table_pack(tb, legend_fr, 0, 1, 1, 1);
   evas_object_show(legend_fr);

   legend_sc = elm_scroller_add(legend_fr);
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

   for (int i = 0; i < view->series_count; i++)
     {
        Cpu_Series *entry = &view->series[i];

        entry->id = cpu_order[i];
        entry->view = view;
        entry->enabled = entry->visible = EINA_TRUE;
        entry->total = 100;
        snprintf(entry->name, sizeof(entry->name), "%s %d", _("CPU"), entry->id);
        evisum_graph_color_get(eina_slstr_printf("cpu_%d", entry->id), &entry->color_r, &entry->color_g,
                               &entry->color_b);
        _cpu_series_legend_add(view, entry);
        _cpu_series_legend_update(entry);
     }
   free(cpu_order);
   _cpu_legend_repack(view);
   pd->thread = ecore_thread_feedback_run(_core_times_main_cb, _core_times_feedback_cb, NULL, NULL, pd, 1);
   return pd;
}
