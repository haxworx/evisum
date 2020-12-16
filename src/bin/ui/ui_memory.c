#include "ui_memory.h"
#include "system/filesystems.h"

#include <Elementary.h>

typedef struct  {
   Evas_Object  *win;
   Evas_Object  *bg;

   Evas_Object  *used;
   Evas_Object  *cached;
   Evas_Object  *buffered;
   Evas_Object  *shared;
   Evas_Object  *swap;
   Evas_Object  *video[MEM_VIDEO_CARD_MAX];

   double        scale;

   Ui           *ui;
} Ui_Data;

#define MAX_HIST  2048
#define UPOLLTIME 100000

#define GR_USED   0
#define GR_CACHED 1
#define GR_BUFFER 2
#define GR_SHARED 3

#define COLOR_USED   229, 64, 89, 255
#define COLOR_CACHED 253, 179, 106, 255
#define COLOR_BUFFER 142, 31, 81, 255
#define COLOR_SHARED 89, 229, 64, 255
#define COLOR_NONE   0, 0, 0, 0

static Eina_Lock _lock;

typedef struct
{
   Eina_List *blocks;
   int        pos;
   double     history[MAX_HIST];
   int        r, g, b, a;
} Graph;

static Graph graphs[4];

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, 0, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   elm_object_text_set(lb, eina_slstr_printf("%s",text));
   evas_object_show(lb);

   return lb;
}

static Evas_Object *
_progress_add(Evas_Object *parent)
{
   Evas_Object *pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   evas_object_show(pb);

   return pb;
}

static Evas_Object *
vg_add(Evas_Object *w)
{
   Evas_Object *o;
   Evas_Vg_Container *con;
   Evas_Vg_Shape *sh;

   o = evas_object_vg_add(evas_object_evas_get(w));
   con = evas_vg_container_add(o);
   sh = evas_vg_shape_add(con);
   evas_object_vg_root_node_set(o, con);
   evas_object_show(o);
   evas_object_data_set(o, "con", con);
   evas_object_data_set(o, "shape", sh);
   return o;
}

static void
vg_fill(Evas_Object *o, Ui_Data *pd, int w, int h, double *pt, int r, int g, int b, int a)
{
   Evas_Vg_Shape *sh;
   int i;
   double v;

   evas_object_resize(o, w, h);
   sh = evas_object_data_get(o, "shape");
   evas_vg_shape_reset(sh);
   if (pd->scale > 1.0)
     evas_vg_shape_stroke_width_set(sh, 2);
   else
     evas_vg_shape_stroke_width_set(sh, 1);

   evas_vg_shape_stroke_color_set(sh, r, g, b, a);
   evas_vg_shape_append_move_to(sh, 0, 1 + h - ((h / 100) * pt[0]));
   for (i = 0; i < w; i++)
     {
        v = (h / 100) * pt[i];
        if      (v < -h) v = -h;
        else if (v >  h) v = h;
        evas_vg_shape_append_line_to(sh, i, h - v);
     }
   evas_vg_node_origin_set(evas_object_data_get(0, "shape"), 0, 0);
}

static void
position_shrink_list(Eina_List *list)
{
   Evas_Object *o;
   Eina_List *l, *ll;
   int i = 0;

   EINA_LIST_FOREACH_SAFE(list, l, ll, o)
     {
        if (i++ > 0)
          {
             evas_object_del(o);
             list = eina_list_remove_list(list, l);
          }
     }
}

static void
position_gr_list(Eina_List *list, int w, int h, int offset)
{
   Evas_Object *o;
   Eina_List *l, *ll;
   int x = w - offset;

   EINA_LIST_FOREACH_SAFE(list, l, ll, o)
     {
        if (x + w < 0)
          {
             evas_object_del(o);
             list = eina_list_remove_list(list, l);
          }
        else
          {
             evas_object_move(o, x, 0);
             evas_object_resize(o, w, h);
          }
        x -= (w - 1);
     }
}

static void
_mem_usage_main_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   static meminfo_t memory;

   while (!ecore_thread_check(thread))
     {
        memset(&memory, 0, sizeof(memory));
        system_memory_usage_get(&memory);
        if (file_system_in_use("ZFS"))
          memory.used += memory.zfs_arc_used;

        ecore_thread_feedback(thread, &memory);
        usleep(UPOLLTIME);
     }
}

static void
_update_widgets(Ui_Data *pd, meminfo_t *memory)
{
   Evas_Object *pb;
   double ratio, value;

   ratio = memory->total / 100.0;

   pb = pd->used;
   value = memory->used / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->used),
                   evisum_size_format(memory->total)));

   pb = pd->cached;
   value = memory->cached / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->cached),
                   evisum_size_format(memory->total)));

   pb = pd->buffered;
   value = memory->buffered / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->buffered),
                   evisum_size_format(memory->total)));
   pb = pd->shared;
   value = memory->shared / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->shared),
                   evisum_size_format(memory->total)));
   pb = pd->swap;
   if (memory->swap_total)
     {
        elm_object_disabled_set(pb, EINA_FALSE);
        ratio = memory->swap_total / 100.0;
        value = memory->swap_used / ratio;
     }
   else
     {
        elm_object_disabled_set(pb, EINA_TRUE);
        value = 0.0;
     }

   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->swap_used),
                   evisum_size_format(memory->swap_total)));

   for (int i = 0; i < memory->video_count; i++)
     {
        pb = pd->video[i];
        if (!pb) break;
        if (memory->video[i].total) elm_object_disabled_set(pb, EINA_FALSE);
        else elm_object_disabled_set(pb, EINA_TRUE);
        ratio = memory->video[i].total / 100.0;
        value = memory->video[i].used / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        evisum_size_format(memory->video[i].used),
                        evisum_size_format(memory->video[i].total)));
     }
}

static void
_update_graph(Graph *graph, double perc, Ui_Data *pd, Evas_Coord w, Evas_Coord h)
{
   int i, r, g, b;
   Evas_Object *o;

   r = graph->r; g = graph->g; b = graph->b;

   if (graph->pos == 0)
     {
        for (i = 0; i < MAX_HIST; i++) graph->history[i] = perc;
        o = vg_add(pd->bg);
        graph->blocks = eina_list_prepend(graph->blocks, o);
     }
   o = graph->blocks->data;
   graph->history[graph->pos] = perc;
   vg_fill(o, pd, w, h, graph->history, r, g, b, 255);
   position_gr_list(graph->blocks, w, h, graph->pos);

   graph->pos++;
   if (graph->pos >= w) graph->pos = 0;
}

static void
_mem_usage_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Evas_Coord w, h;
   Ui_Data *pd;
   meminfo_t *memory;
   double ratio;

   pd = data;
   memory = msgdata;

   if (ecore_thread_check(thread)) return;

   ratio = memory->total / 100.0;
   if (ratio == 0.0) return;

   _update_widgets(pd, memory);

   evas_object_geometry_get(pd->bg, NULL, NULL, &w, &h);

   eina_lock_take(&_lock);

   _update_graph(&graphs[GR_USED], memory->used / ratio, pd, w, h);
   _update_graph(&graphs[GR_CACHED], memory->cached / ratio, pd, w, h);
   _update_graph(&graphs[GR_BUFFER], memory->buffered / ratio, pd, w, h);
   _update_graph(&graphs[GR_SHARED], memory->shared / ratio, pd, w, h);

   eina_lock_release(&_lock);
}

static void
_graph_init(Graph *graph, int r, int g, int b, int a)
{
   memset(graph, 0, sizeof(Graph));
   graph->r = r; graph->g = g; graph->b = b, graph->a = a;
}

static Evas_Object *
_graph_guide(Evas_Object *parent, int r, int g, int b, int a)
{
   Evas_Object *btn, *rec;;

   btn = elm_button_add(parent);
   evas_object_show(btn);

   rec = evas_object_rectangle_add(parent);
   evas_object_color_set(rec, r, g, b, a);
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_show(rec);
   elm_object_part_content_set(btn, "elm.swallow.content", rec);

   return btn;
}

static Eina_Bool
_elm_config_changed_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ui_Data *pd = data;

   pd->scale = elm_config_scale_get();

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   ecore_thread_cancel(ui->mem.thread);
   ecore_thread_wait(ui->mem.thread, 0.5);

   evas_object_del(obj);
   ui->mem.win = NULL;
   free(pd);
   eina_lock_free(&_lock);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   eina_lock_take(&_lock);

   position_shrink_list((&graphs[GR_USED])->blocks);
   position_shrink_list((&graphs[GR_CACHED])->blocks);
   position_shrink_list((&graphs[GR_BUFFER])->blocks);
   position_shrink_list((&graphs[GR_SHARED])->blocks);

   eina_lock_release(&_lock);

   evisum_ui_config_save(ui);
}

void
ui_win_memory_add(Ui *ui)
{
   Evas_Object *win, *lb, *bx, *tbl, *rec, *pb;
   Evas_Object *fr;
   int i;
   Evas_Coord x = 0, y = 0;
   meminfo_t memory;

   if (ui->mem.win)
     {
        elm_win_raise(ui->mem.win);
        return;
     }

   Ui_Data *pd = calloc(1, sizeof(Ui_Data));
   if (!pd) return;
   pd->ui = ui;

   eina_lock_new(&_lock);

   memset(&memory, 0, sizeof(memory));
   system_memory_usage_get(&memory);

   ui->mem.win = win = elm_win_util_standard_add("evisum", _("Memory Usage"));
   pd->win = win;
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, (evisum_ui_effects_enabled_get() ||
                                   evisum_ui_backgrounds_enabled_get()));

   _graph_init(&graphs[GR_USED],   COLOR_USED);
   _graph_init(&graphs[GR_CACHED], COLOR_CACHED);
   _graph_init(&graphs[GR_BUFFER], COLOR_BUFFER);
   _graph_init(&graphs[GR_SHARED], COLOR_SHARED);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EXPAND, 0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(win, bx);

   pd->bg = rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_color_set(rec, 32, 32, 32, 255);
   evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(200));
   evas_object_size_hint_max_set(rec, MAX_HIST, ELM_SCALE_SIZE(480));
   evas_object_show(rec);

   elm_box_pack_end(bx, rec);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_medium");
   evas_object_show(fr);

   tbl = elm_table_add(win);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   elm_table_padding_set(tbl, 8, 2);
   evas_object_show(tbl);

   elm_object_content_set(fr, tbl);
   elm_box_pack_end(bx, fr);

   lb = _label_mem(tbl, _("Used"));
   pd->used = pb = _progress_add(tbl);
   rec = _graph_guide(tbl, COLOR_USED);
   elm_table_pack(tbl, rec, 0, 1, 1, 1);
   elm_table_pack(tbl, lb, 1, 1, 1, 1);
   elm_table_pack(tbl, pb, 2, 1, 1, 1);

   lb = _label_mem(tbl, _("Cached"));
   pd->cached = pb = _progress_add(tbl);
   rec = _graph_guide(tbl, COLOR_CACHED);
   elm_table_pack(tbl, rec, 0, 2, 1, 1);
   elm_table_pack(tbl, lb, 1, 2, 1, 1);
   elm_table_pack(tbl, pb, 2, 2, 1, 1);

   lb = _label_mem(tbl, _("Buffered"));
   pd->buffered = pb = _progress_add(tbl);
   rec = _graph_guide(tbl, COLOR_BUFFER);
   elm_table_pack(tbl, rec, 0, 3, 1, 1);
   elm_table_pack(tbl, lb, 1, 3, 1, 1);
   elm_table_pack(tbl, pb, 2, 3, 1, 1);

   lb = _label_mem(tbl, _("Shared"));
   pd->shared = pb = _progress_add(tbl);
   rec = _graph_guide(tbl, COLOR_SHARED);
   elm_table_pack(tbl, rec, 0, 4, 1, 1);
   elm_table_pack(tbl, lb, 1, 4, 1, 1);
   elm_table_pack(tbl, pb, 2, 4, 1, 1);

   lb = _label_mem(tbl, _("Swapped"));
   pd->swap = pb = _progress_add(tbl);
   elm_table_pack(tbl, lb, 1, 5, 1, 1);
   elm_table_pack(tbl, pb, 2, 5, 1, 1);

   for (i = 0; i < memory.video_count; i++)
     {
        lb = _label_mem(tbl, _("Video"));
        pd->video[i] = pb = _progress_add(tbl);
        elm_table_pack(tbl, lb, 1, 6 + i, 1, 1);
        elm_table_pack(tbl, pb, 2, 6 + i, 1, 1);
     }

   if (ui->mem.width > 0 && ui->mem.height > 0)
     evas_object_resize(win, ui->mem.width, ui->mem.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH , UI_CHILD_WIN_HEIGHT);

   if (ui->win) evas_object_geometry_get(ui->win, &x, &y, NULL, NULL);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 20);
   else
     elm_win_center(win, 1, 1);

   _elm_config_changed_cb(pd, 0, NULL);

   ecore_event_handler_add(ELM_EVENT_CONFIG_ALL_CHANGED,
                           _elm_config_changed_cb, pd);

   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_show(win);

   ui->mem.thread = ecore_thread_feedback_run(_mem_usage_main_cb,
                                              _mem_usage_feedback_cb,
                                              NULL,
                                              NULL,
                                              pd, EINA_TRUE);
}
