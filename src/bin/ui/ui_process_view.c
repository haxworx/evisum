#include "ui_process_view.h"
#include "../system/process.h"
#include "util.c"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef struct
{
   Evas_Object     *win;

   Evas_Object     *tab_general;
   Evas_Object     *tab_children;
   Evas_Object     *tab_thread;
   Evas_Object     *tab_manual;

   Evas_Object     *general_view;
   Evas_Object     *children_view;
   Evas_Object     *thread_view;
   Evas_Object     *manual_view;

   Evas_Object     *current_view;

   int              poll_delay;
   int64_t          start;
   char            *selected_cmd;
   int              selected_pid;
   uint32_t         poll_count;
   int64_t          pid_cpu_time;

   Ecore_Thread    *thread;

   struct
   {
      Evas_Object   *entry_cmd;
      Evas_Object   *entry_cmd_args;
      Evas_Object   *entry_user;
      Evas_Object   *entry_pid;
      Evas_Object   *entry_ppid;
      Evas_Object   *entry_uid;
      Evas_Object   *entry_cpu;
      Evas_Object   *entry_threads;
      Evas_Object   *entry_files;
      Evas_Object   *entry_virt;
      Evas_Object   *entry_rss;
      Evas_Object   *entry_shared;
      Evas_Object   *entry_size;
      Evas_Object   *entry_started;
      Evas_Object   *entry_run_time;
      Evas_Object   *entry_nice;
      Evas_Object   *entry_pri;
      Evas_Object   *entry_state;
      Evas_Object   *entry_cpu_usage;
      Evas_Object   *btn_start;
      Evas_Object   *btn_stop;
      Evas_Object   *btn_kill;
   } general;

   struct
   {
      Evas_Object  *glist;
   } children;

   struct
   {
      struct
      {
         int           cpu_count;
         unsigned int  cpu_colormap[256];
         unsigned int  cores[256];
         Evas_Object   *obj;
         Evas_Object   *lb;
      } graph;

      Eina_Hash       *hash_cpu_times;
      Evisum_Ui_Cache *cache;
      Evas_Object     *glist;
      Evas_Object     *btn_id;
      Evas_Object     *btn_name;
      Evas_Object     *btn_state;
      Evas_Object     *btn_cpu_id;
      Evas_Object     *btn_cpu_usage;
      int             (*sort_cb)(const void *p1, const void *p2);

      Eina_Bool       sort_reverse;
   } threads;

   struct
   {
      Evas_Object  *entry;
      Eina_Bool     init;
   } manual;

} Data;

typedef struct _Color_Point {
   unsigned int val;
   unsigned int color;
} Color_Point;

#define COLOR_CPU_NUM 5
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 },
   {  25, 0xff2030a0 },
   {  50, 0xffa040a0 },
   {  75, 0xffff9040 },
   { 100, 0xffffffff },
   { 256, 0xffffffff }
};

#define AVAL(x) (((x) >> 24) & 0xff)
#define RVAL(x) (((x) >> 16) & 0xff)
#define GVAL(x) (((x) >>  8) & 0xff)
#define BVAL(x) (((x)      ) & 0xff)
#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

#define BAR_HEIGHT 2

static void
_color_init(const Color_Point *col_in, unsigned int n, unsigned int *col)
{
   unsigned int pos, interp, val, dist, d;
   unsigned int a, r, g, b;
   unsigned int a1, r1, g1, b1, v1;
   unsigned int a2, r2, g2, b2, v2;

   // walk colormap_in until colormap table is full
   for (pos = 0, val = 0; pos < n; pos++)
     {
        // get first color and value position
        v1 = col_in[pos].val;
        a1 = AVAL(col_in[pos].color);
        r1 = RVAL(col_in[pos].color);
        g1 = GVAL(col_in[pos].color);
        b1 = BVAL(col_in[pos].color);
        // get second color and valuje position
        v2 = col_in[pos + 1].val;
        a2 = AVAL(col_in[pos + 1].color);
        r2 = RVAL(col_in[pos + 1].color);
        g2 = GVAL(col_in[pos + 1].color);
        b2 = BVAL(col_in[pos + 1].color);
        // get distance between values (how many entires to fill)
        dist = v2 - v1;
        // walk over the span of colors from point a to point b
        for (interp = v1; interp < v2; interp++)
          {
             // distance from starting point
             d = interp - v1;
             // calculate linear interpolation between start and given d
             a = ((d * a2) + ((dist - d) * a1)) / dist;
             r = ((d * r2) + ((dist - d) * r1)) / dist;
             g = ((d * g2) + ((dist - d) * g1)) / dist;
             b = ((d * b2) + ((dist - d) * b1)) / dist;
             // write out resulting color value
             col[val] = ARGB(a, r, g, b);
             val++;
          }
     }
}

typedef struct
{
   long cpu_time;
   long cpu_time_prev;
} Thread_Cpu_Info;

typedef struct
{
   int     tid;
   char   *name;
   char   *state;
   int     cpu_id;
   long    cpu_time;
   double  cpu_usage;
} Thread_Info;

static Thread_Info *
_thread_info_new(Data *pd, Proc_Info *th)
{
   Thread_Info *t;
   Thread_Cpu_Info *inf;
   const char *key;
   double cpu_usage = 0.0;

   t = calloc(1, sizeof(Thread_Info));
   if (!t) return NULL;

   key = eina_slstr_printf("%s:%d", th->thread_name, th->tid);

   inf = eina_hash_find(pd->threads.hash_cpu_times, key);
   if (inf)
     {
        if (inf->cpu_time_prev)
          cpu_usage = (inf->cpu_time - inf->cpu_time_prev);
        inf->cpu_time_prev = th->cpu_time;
     }

   t->tid = th->tid;
   t->name = strdup(th->thread_name);
   t->cpu_time = th->cpu_time;
   t->state = strdup(th->state);
   t->cpu_id = th->cpu_id;
   t->cpu_usage = cpu_usage;

   return t;
}

Eina_List *
_exe_response(const char *command)
{
   FILE *p;
   Eina_List *lines;
   char buf[1024];

   p = popen(command, "r");
   if (!p)
     return NULL;

   lines = NULL;

   while ((fgets(buf, sizeof(buf), p)) != NULL)
     {
        lines = eina_list_append(lines, _man2entry(buf));
     }

   pclose(p);

   return lines;
}

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Data *pd;
   Evas_Object *o;
   Eina_List *contents = NULL;

   pd = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        evisum_ui_item_cache_item_release(pd->threads.cache, o);
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Thread_Info *t = data;

   if (t->name)
     free(t->name);
   if (t->state)
     free(t->state);
   free(t);
}

static Evas_Object *
_item_column_add(Evas_Object *tb, const char *text, int col)
{
   Evas_Object *rec, *lb;

   lb = elm_label_add(tb);
   evas_object_size_hint_weight_set(lb, 0, EXPAND);
   evas_object_size_hint_align_set(lb, 0.0, FILL);
   evas_object_data_set(tb, text, lb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_data_set(lb, "rec", rec);

   elm_table_pack(tb, lb, col, 0, 1, 1);
   elm_table_pack(tb, rec, col, 0, 1, 1);
   evas_object_show(lb);

   return lb;
}

static void
_hash_free_cb(void *data)
{
   Thread_Cpu_Info *inf = data;
   if (inf)
     free(inf);
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *tb, *lb, *pb;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   lb = _item_column_add(tb, "tid", 0);
   evas_object_size_hint_align_set(lb, 0.5, FILL);
   _item_column_add(tb, "name", 1);
   lb = _item_column_add(tb, "state", 2);
   evas_object_size_hint_align_set(lb, 0.0, FILL);
   lb = _item_column_add(tb, "cpu_id", 3);
   evas_object_size_hint_align_set(lb, 0.5, FILL);

   pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_unit_format_set(pb, "%1.0f %%");
   evas_object_data_set(tb, "cpu_usage", pb);
   elm_table_pack(tb, pb, 4, 0, 1, 1);

   return tb;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Data *pd;
   Thread_Info *th;
   Evas_Object *lb, *rec, *pb;
   Evas_Coord w, ow;

   th = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!th) return NULL;
   pd = evas_object_data_get(obj, "ui");
   if (!pd) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(pd->threads.cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(pd->threads.btn_id, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "tid");
   elm_object_text_set(lb, eina_slstr_printf("%d", th->tid));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->threads.btn_id, w, 1);
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);

   evas_object_geometry_get(pd->threads.btn_name, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "name");
   elm_object_text_set(lb, eina_slstr_printf("%s", th->name));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->threads.btn_name, w, 1);
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);

   evas_object_geometry_get(pd->threads.btn_state, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "state");
   elm_object_text_set(lb, eina_slstr_printf("%s", th->state));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->threads.btn_state, w, 1);
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);

   evas_object_geometry_get(pd->threads.btn_cpu_id, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "cpu_id");
   elm_object_text_set(lb, eina_slstr_printf("%d", th->cpu_id));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->threads.btn_cpu_id, w, 1);
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);

   pb = evas_object_data_get(it->obj, "cpu_usage");
   elm_progressbar_value_set(pb, th->cpu_usage > 0 ? th->cpu_usage / 100.0 : 0);
   evas_object_show(pb);

   return it->obj;
}

static void
_glist_ensure_n_items(Evas_Object *glist, unsigned int items)
{
   Elm_Object_Item *it;
   Elm_Genlist_Item_Class *itc;
   unsigned int i, existing = elm_genlist_items_count(glist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
           {
              it = elm_genlist_last_item_get(glist);
              if (it)
                elm_object_item_del(it);
           }
      }

   if (items == existing) return;

   itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _content_get;
   itc->func.filter_get = NULL;
   itc->func.del = _item_del;

   for (i = existing; i < items; i++)
     {
        elm_genlist_item_append(glist, itc, NULL, NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_item_class_free(itc);
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Thread_Info *inf1, *inf2;
   double one, two;

   inf1 = p1; inf2 = p2;
   one = inf1->cpu_usage; two = inf2->cpu_usage;

   if (one > two)
     return 1;
   else if (one < two)
     return -1;
   else return 0;
}

static int
_sort_by_cpu_id(const void *p1, const void *p2)
{
   const Thread_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

static int
_sort_by_state(const void *p1, const void *p2)
{
   const Thread_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}

static int
_sort_by_name(const void *p1, const void *p2)
{
   const Thread_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->name, inf2->name);
}

static int
_sort_by_tid(const void *p1, const void *p2)
{
   const Thread_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->tid - inf2->tid;
}

static void
_thread_view_update(Data *pd, Proc_Info *proc)
{
   Proc_Info *p;
   Thread_Info *t;
   Elm_Object_Item *it;
   Eina_List *l, *threads = NULL;

   _glist_ensure_n_items(pd->threads.glist, eina_list_count(proc->threads));

   EINA_LIST_FOREACH(proc->threads, l, p)
     {
        t = _thread_info_new(pd, p);
        if (t)
          threads = eina_list_append(threads, t);
     }

   if (pd->threads.sort_cb)
     threads = eina_list_sort(threads, eina_list_count(threads), pd->threads.sort_cb);
   if (pd->threads.sort_reverse)
     threads = eina_list_reverse(threads);

   it = elm_genlist_first_item_get(pd->threads.glist);

   EINA_LIST_FREE(threads, t)
     {
        if (!it)
          _item_del(t, NULL);
        else
          {
             Thread_Info *prev = elm_object_item_data_get(it);
             if (prev)
              _item_del(prev, NULL);
             elm_object_item_data_set(it, t);
             elm_genlist_item_update(it);
             it = elm_genlist_item_next_get(it);
          }
     }
}

static void
_threads_cpu_usage(Data *pd, Proc_Info *proc)
{
   Eina_List *l;
   Proc_Info *p;

   if (!pd->threads.hash_cpu_times)
     pd->threads.hash_cpu_times = eina_hash_string_superfast_new(_hash_free_cb);

   EINA_LIST_FOREACH(proc->threads, l, p)
     {
        Thread_Cpu_Info *inf;
        double cpu_usage = 0.0;
        const char *key = eina_slstr_printf("%s:%d", p->thread_name, p->tid);

        if ((inf = eina_hash_find(pd->threads.hash_cpu_times, key)) == NULL)
          {
             inf = calloc(1, sizeof(Thread_Cpu_Info));
             inf->cpu_time = p->cpu_time;
             eina_hash_add(pd->threads.hash_cpu_times, key, inf);
          }
        else
          {
             cpu_usage = (double) (p->cpu_time - inf->cpu_time) * 10;
             inf->cpu_time = p->cpu_time;
          }
        pd->threads.graph.cores[p->cpu_id] += cpu_usage;
     }
}

static void
_item_children_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info)
{
   Elm_Object_Item *it;
   Proc_Info *proc;

   it = event_info;

   elm_genlist_item_selected_set(it, 0);

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   ui_process_view_win_add(proc->pid, PROC_VIEW_DEFAULT);
}

static char *
_children_text_get(void *data, Evas_Object *obj, const char *part)

{
   Proc_Info *child = data;
   char buf[256];

   snprintf(buf, sizeof(buf), "%s (%d) ", child->command, child->pid);

   return strdup(buf);
}

static Evas_Object *
_children_icon_get(void *data, Evas_Object *obj, const char *part)
{
   Proc_Info *proc;
   Evas_Object *ic = elm_icon_add(obj);
   proc = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        elm_icon_standard_set(ic,
                              evisum_icon_path_get(
                              evisum_icon_cache_find(proc)));
     }

   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);

   return ic;
}

static void
_children_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;

   eina_list_free(proc->children);
   proc_info_free(proc);
}

static void
_children_populate(Evas_Object *glist, Elm_Object_Item *parent,
                   Eina_List *children)
{
   Elm_Genlist_Item_Class *itc;
   Eina_List *l;
   Elm_Object_Item *it;
   Proc_Info *child;

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.content_get = _children_icon_get;
   itc->func.text_get = _children_text_get;
   itc->func.filter_get = NULL;
   itc->func.del = _children_del;

   EINA_LIST_FOREACH(children, l, child)
     {
        it = elm_genlist_item_append(glist, itc, child, parent,
                                     (child->children ?
                                     ELM_GENLIST_ITEM_TREE :
                                     ELM_GENLIST_ITEM_NONE), NULL, NULL);
        elm_genlist_item_update(it);
        if (child->children)
          {
             child->children = eina_list_sort(child->children,
                                              eina_list_count(child->children),
                                              proc_sort_by_age);
             _children_populate(glist, it, child->children);
          }
     }
   elm_genlist_item_class_free(itc);
}

static Eina_Bool
_children_view_update(void *data)
{
   Eina_List *children, *l;
   Proc_Info *child;
   Data *pd = data;

   elm_genlist_clear(pd->children.glist);

   if (pd->selected_pid == 0) return 0;

   children = proc_info_pid_children_get(pd->selected_pid);
   EINA_LIST_FOREACH(children, l, child)
     {
        if (child->pid == pd->selected_pid)
          {
             child->children = eina_list_sort(child->children,
                                              eina_list_count(child->children),
                                              proc_sort_by_age);
             _children_populate(pd->children.glist, NULL, child->children);
             break;
          }
     }
   child = eina_list_nth(children, 0);
   if (child)
     proc_info_free(child);

   return 1;
}

static void
_proc_info_main(void *data, Ecore_Thread *thread)
{
   Data *pd = data;

   while (!ecore_thread_check(thread))
     {
        Proc_Info *proc = proc_info_by_pid(pd->selected_pid);
        ecore_thread_feedback(thread, proc);

        if (ecore_thread_check(thread))
          return;

        usleep(100000);
     }
}

static void
_graph_summary_update(Data *pd, Proc_Info *proc)
{
   elm_object_text_set(pd->threads.graph.lb, eina_slstr_printf(
                       _("<b>"
                       "CPU: %.0f%%<br>"
                       "Size: %s<br>"
                       "Reserved: %s<br>"
                       "Virtual: %s"
                       "</>"),
                       proc->cpu_usage,
                       evisum_size_format(proc->mem_size),
                       evisum_size_format(proc->mem_rss),
                       evisum_size_format(proc->mem_virt)));
}

static void
_graph_update(Data *pd, Proc_Info *proc)
{
   Evas_Object *obj = pd->threads.graph.obj;
   unsigned int *pixels, *pix;
   Evas_Coord x, y, w, h;
   int iw, stride;
   Eina_Bool clear = 0;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_image_size_get(obj, &iw, NULL);

   if (iw != w)
     {
        evas_object_image_size_set(obj, w, pd->threads.graph.cpu_count);
        clear = 1;
     }

   pixels = evas_object_image_data_get(obj, 1);
   if (!pixels) return;

   stride = evas_object_image_stride_get(obj);

   for (y = 0; y < pd->threads.graph.cpu_count; y++)
     {
        if (clear)
          {
             pix = &(pixels[y * (stride / 4)]);
             for (x = 0; x < (w - 1); x++)
               pix[x] = pd->threads.graph.cpu_colormap[0];
          }
        else
          {
             pix = &(pixels[y * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
          }
        unsigned int c1;
        c1 = pd->threads.graph.cpu_colormap[pd->threads.graph.cores[y] & 0xff];
        pix = &(pixels[y * (stride / 4)]);
        pix[x] = c1;
     }

   evas_object_image_data_set(obj, pixels);
   evas_object_image_data_update_add(obj, 0, 0, w, pd->threads.graph.cpu_count);
   memset(pd->threads.graph.cores, 0, 255 * sizeof(unsigned int));
}

static Evas_Object *
_graph(Evas_Object *parent, Data *pd)
{
   Evas_Object *tb, *obj, *tb2, *lb, *scr, *fr, *rec;

   pd->threads.graph.cpu_count = system_cpu_count_get();

   tb = elm_table_add(parent);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_show(tb);

   scr = elm_scroller_add(parent);
   evas_object_size_hint_align_set(scr, FILL, FILL);
   evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
   evas_object_show(scr);

   pd->threads.graph.obj = obj = evas_object_image_add(evas_object_evas_get(parent));
   evas_object_size_hint_align_set(obj, FILL, FILL);
   evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
   evas_object_image_smooth_scale_set(obj, 0);
   evas_object_image_filled_set(obj, 1);
   evas_object_image_alpha_set(obj, 0);
   evas_object_show(obj);

   evas_object_size_hint_min_set(obj, 100,
                                 (BAR_HEIGHT * pd->threads.graph.cpu_count)
                                  * elm_config_scale_get());
   elm_object_content_set(scr, obj);

   _color_init(cpu_colormap_in, COLOR_CPU_NUM, pd->threads.graph.cpu_colormap);

   // Overlay
   fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_small");
   evas_object_size_hint_align_set(fr, 0.01, 0.03);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_show(fr);

   tb2 = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb2, EXPAND, 0);
   evas_object_size_hint_align_set(tb2, 0.0, 0.0);
   evas_object_show(tb2);

   rec = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_color_set(rec, 0, 0, 0, 64);
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(128), ELM_SCALE_SIZE(92));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(128), ELM_SCALE_SIZE(92));
   evas_object_show(rec);

   pd->threads.graph.lb = lb = elm_entry_add(parent);
   elm_entry_single_line_set(lb, 1);
   elm_entry_select_allow_set(lb, 1);
   elm_entry_editable_set(lb, 0);
   elm_object_focus_allow_set(lb, 0);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(lb, 0.5, 0.5);
   evas_object_show(lb);

   elm_table_pack(tb2, rec, 0, 0, 1, 1);
   elm_table_pack(tb2, lb, 0, 0, 1, 1);
   elm_object_content_set(fr, tb2);

   elm_table_pack(tb, scr, 0, 0, 1, 1);
   elm_table_pack(tb, fr, 0, 0, 1, 1);

   return tb;
}

static char *
_time_string(int64_t epoch)
{
   struct tm *info;
   char buf[256];
   time_t rawtime = (time_t) epoch;

   info = localtime(&rawtime);
   strftime(buf, sizeof(buf), "%F %T", info);

   return strdup(buf);
}

static char *
_run_time_string(int64_t secs)
{
   char buf[256];
   int rem;

   if (secs < 86400)
     snprintf(buf, sizeof(buf), "%02"PRIi64":%02"PRIi64, secs / 60, secs % 60);
   else
     {
        rem = secs % 3600;
        snprintf(buf, sizeof(buf), "%02"PRIi64":%02d:%02d", secs / 3600, rem / 60, rem % 60);
     }
   return strdup(buf);
}

static void
_manual_init_cb(void *data, Ecore_Thread *thread)
{
   Eina_List *lines = NULL;
   Eina_Strbuf *sbuf = NULL;
   char buf[4096];
   char *line;
   int n = 1;
   Data *pd = data;

   setenv("MANWIDTH", "80", 1);
   ecore_thread_feedback(thread, strdup("<code>"));

   if (!strchr(pd->selected_cmd, ' '))
     {
        snprintf(buf, sizeof(buf), "man %s | col -bx", pd->selected_cmd);
        lines = _exe_response(buf);
     }
   if (!lines)
     {
        snprintf(buf, sizeof(buf), _("No documentation found for %s."),
                 pd->selected_cmd);
        ecore_thread_feedback(thread, strdup(buf));
     }
   else sbuf = eina_strbuf_new();
   EINA_LIST_FREE(lines, line)
     {
        if (n++ > 1)
          {
             eina_strbuf_append_printf(sbuf, "%s<br>", line);
             if (eina_strbuf_length_get(sbuf) >= 4096)
               ecore_thread_feedback(thread, eina_strbuf_string_steal(sbuf));
          }
       free(line);
     }
   if (sbuf)
     {
        if (eina_strbuf_length_get(sbuf))
          ecore_thread_feedback(thread, eina_strbuf_string_steal(sbuf));
        eina_strbuf_free(sbuf);
     }
   ecore_thread_feedback(thread, strdup("</code>"));
   unsetenv("MANWIDTH");
   pd->manual.init = 1;
}

static void
_manual_init_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata)
{
   Evas_Object *ent;
   char *s;
   Data *pd = data;

   ent = pd->manual.entry;
   s = msgdata;
   elm_entry_entry_append(ent, s);

   free(s);
}

static void
_manual_init(Data *pd)
{
   if (pd->manual.init) return;

   if ((!pd->selected_cmd) || (!pd->selected_cmd[0]))
     return;

   ecore_thread_feedback_run(_manual_init_cb,
                             _manual_init_feedback_cb,
                             NULL, NULL, pd, 1);
}

static void
_general_view_update(Data *pd, Proc_Info *proc)
{
   struct passwd *pwd_entry;
   char *s;

   if (!strcmp(proc->state, "stop"))
     {
        elm_object_disabled_set(pd->general.btn_stop, 1);
        elm_object_disabled_set(pd->general.btn_start, 0);
     }
   else
     {
        elm_object_disabled_set(pd->general.btn_stop, 0);
        elm_object_disabled_set(pd->general.btn_start, 1);
     }

   elm_object_text_set(pd->general.entry_cmd,
                       eina_slstr_printf("<subtitle>%s</subtitle>",
                                         proc->command));
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(pd->general.entry_user, pwd_entry->pw_name);

   if (proc->arguments)
     elm_object_text_set(pd->general.entry_cmd_args, proc->arguments);
   else
     elm_object_text_set(pd->general.entry_cmd_args, "");

   elm_object_text_set(pd->general.entry_pid,
                       eina_slstr_printf("%d", proc->pid));
   elm_object_text_set(pd->general.entry_uid,
                       eina_slstr_printf("%d", proc->uid));
   elm_object_text_set(pd->general.entry_cpu,
                       eina_slstr_printf("%d", proc->cpu_id));
   elm_object_text_set(pd->general.entry_ppid,
                       eina_slstr_printf("%d", proc->ppid));
   elm_object_text_set(pd->general.entry_threads,
                       eina_slstr_printf("%d", proc->numthreads));
   elm_object_text_set(pd->general.entry_files,
                       eina_slstr_printf("%d", proc->numfiles));
   elm_object_text_set(pd->general.entry_virt,
                       evisum_size_format(proc->mem_virt));
   elm_object_text_set(pd->general.entry_rss,
                       evisum_size_format(proc->mem_rss));
   elm_object_text_set(pd->general.entry_shared,
                       evisum_size_format(proc->mem_shared));
   elm_object_text_set(pd->general.entry_size,
                       evisum_size_format(proc->mem_size));
   s = _run_time_string(proc->run_time);
   if (s)
     {
        elm_object_text_set(pd->general.entry_run_time, s);
        free(s);
     }
   s = _time_string(proc->start);
   if (s)
     {
        elm_object_text_set(pd->general.entry_started, s);
        free(s);
     }
   elm_object_text_set(pd->general.entry_nice,
                       eina_slstr_printf("%d", proc->nice));
   elm_object_text_set(pd->general.entry_pri,
                       eina_slstr_printf("%d", proc->priority));
   elm_object_text_set(pd->general.entry_state,
                       proc->state);
   elm_object_text_set(pd->general.entry_cpu_usage,
                       eina_slstr_printf("%.0f%%", proc->cpu_usage));
}

static void
_proc_gone(Data *pd)
{
    const char *fmt = _("%s (%d) - Not running");

    elm_win_title_set(pd->win,
                      eina_slstr_printf(fmt,
                                        pd->selected_cmd,
                                        pd->selected_pid));

   elm_object_disabled_set(pd->general.btn_start, 1);
   elm_object_disabled_set(pd->general.btn_stop, 1);
   elm_object_disabled_set(pd->general.btn_kill, 1);

   if (!ecore_thread_check(pd->thread))
     ecore_thread_cancel(pd->thread);
   pd->thread = NULL;
}

static void
_proc_info_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Data *pd;
   Proc_Info *proc;
   double cpu_usage = 0.0;

   pd = data;
   proc = msg;

   if (!proc || (pd->start && (proc->start != pd->start)))
     {
        if (proc) proc_info_free(proc);
        _proc_gone(pd);
        return;
     }

   if (ecore_thread_check(thread))
     {
        proc_info_free(proc);
        return;
     }

   _threads_cpu_usage(pd, proc);

   if ((pd->poll_count != 0) && (pd->poll_count % 10))
     {
        _graph_update(pd, proc);
        proc_info_free(proc);
        pd->poll_count++;
        return;
     }

   if ((pd->pid_cpu_time) && (proc->cpu_time >= pd->pid_cpu_time))
     cpu_usage = (double)(proc->cpu_time - pd->pid_cpu_time) / pd->poll_delay;

   proc->cpu_usage = cpu_usage;

   _graph_update(pd, proc);
   _graph_summary_update(pd, proc);
   _thread_view_update(pd, proc);
   _general_view_update(pd, proc);

   pd->poll_count++;
   pd->pid_cpu_time = proc->cpu_time;

   proc_info_free(proc);
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->selected_pid == -1)
     return;

   kill(pd->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->selected_pid == -1)
     return;

   kill(pd->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->selected_pid == -1)
     return;

   kill(pd->selected_pid, SIGKILL);
}

static Evas_Object *
_entry_add(Evas_Object *parent)
{
   Evas_Object *entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_show(entry);

   return entry;
}

static Evas_Object *
_lb_add(Evas_Object *parent, const char *text)
{
   Evas_Object *lb = elm_label_add(parent);
   elm_object_text_set(lb, text);
   evas_object_show(lb);

   return lb;
}

static Evas_Object *
_general_tab_add(Evas_Object *parent, Data *pd)
{
   Evas_Object *fr, *hbx, *tb;
   Evas_Object *lb, *entry, *btn, *pad, *ic;
   Evas_Object *rec;
   Proc_Info *proc;
   int i = 0;

   fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_medium");
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);
   elm_object_focus_allow_set(tb, 1);
   elm_object_content_set(fr, tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(64), ELM_SCALE_SIZE(64));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(64), ELM_SCALE_SIZE(64));
   evas_object_size_hint_align_set(rec, FILL, 1.0);
   elm_table_pack(tb, rec, 0, i, 1, 1);

   proc = proc_info_by_pid(pd->selected_pid);
   if (proc)
     {
        ic = elm_icon_add(parent);
        evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
        evas_object_size_hint_align_set(ic, FILL, FILL);
        elm_icon_standard_set(ic,
                              evisum_icon_path_get(
                              evisum_icon_cache_find(proc)));
        evas_object_show(ic);
        proc_info_free(proc);
        elm_table_pack(tb, ic, 0, i, 1, 1);
     }

   lb = _lb_add(parent, _("Command:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_cmd = entry = elm_label_add(parent);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, 0.0, 0.5);
   evas_object_show(entry);
   evas_object_hide(lb);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Command line:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_cmd_args = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("PID:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_pid = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Username:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_user = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("UID:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_uid = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("PPID:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_ppid = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

#if defined(__MacOS__)
   lb = _lb_add(parent, _("WQ #:"));
#else
   lb = _lb_add(parent, _("CPU #:"));
#endif
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_cpu = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Threads:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_threads = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Open Files:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_files = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _(" Memory :"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_size = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _(" Shared memory:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_shared = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent,  _(" Resident memory:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_rss = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _(" Virtual memory:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_virt = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _(" Start time:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_started = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _(" Run time:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_run_time = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Nice:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_nice = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("Priority:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_pri = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("State:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_state = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   lb = _lb_add(parent, _("CPU %:"));
   elm_table_pack(tb, lb, 0, i, 1, 1);
   pd->general.entry_cpu_usage = entry = _entry_add(parent);
   elm_table_pack(tb, entry, 1, i++, 1, 1);

   hbx = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_align_set(hbx, 1.0, FILL);
   elm_box_horizontal_set(hbx, 1);
   elm_box_homogeneous_set(hbx, 1);
   evas_object_show(hbx);
   elm_table_pack(tb, hbx, 1, i, 1, 1);

   pad = elm_frame_add(parent);
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   elm_object_style_set(pad, "pad_small");
   evas_object_show(pad);
   elm_box_pack_end(hbx, pad);
   pad = elm_frame_add(parent);
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   elm_object_style_set(pad, "pad_small");
   evas_object_show(pad);
   elm_box_pack_end(hbx, pad);

   btn = evisum_ui_button_add(parent, NULL, _("stop"), "stop",
                              _btn_stop_clicked_cb, pd);
   evas_object_show(btn);
   pd->general.btn_stop = btn;
   elm_box_pack_end(hbx, btn);

   btn = evisum_ui_button_add(parent, NULL, _("start"), "start",
                              _btn_start_clicked_cb, pd);
   evas_object_show(btn);
   pd->general.btn_start = btn;
   elm_box_pack_end(hbx, btn);

   btn = evisum_ui_button_add(parent, NULL, _("kill"), "kill",
                              _btn_kill_clicked_cb, pd);
   evas_object_show(btn);
   pd->general.btn_kill = btn;
   elm_box_pack_end(hbx, btn);

   return fr;
}

static void
_btn_icon_state_set(Evas_Object *btn, Eina_Bool reverse)
{
   Evas_Object *ic = elm_icon_add(btn);
   if (reverse)
     elm_icon_standard_set(ic, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(ic, evisum_icon_path_get("go-up"));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);
}

static void
_threads_list_reorder(Data *pd)
{
   pd->poll_count = 0;
   elm_scroller_page_bring_in(pd->threads.glist, 0, 0);
}

static void
_btn_name_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->threads.sort_cb == _sort_by_name)
     pd->threads.sort_reverse = !pd->threads.sort_reverse;
   _btn_icon_state_set(obj, pd->threads.sort_reverse);
   pd->threads.sort_cb = _sort_by_name;
   _threads_list_reorder(pd);
}

static void
_btn_thread_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->threads.sort_cb == _sort_by_tid)
     pd->threads.sort_reverse = !pd->threads.sort_reverse;
   _btn_icon_state_set(obj, pd->threads.sort_reverse);
   pd->threads.sort_cb = _sort_by_tid;
   _threads_list_reorder(pd);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->threads.sort_cb == _sort_by_state)
     pd->threads.sort_reverse = !pd->threads.sort_reverse;
   _btn_icon_state_set(obj, pd->threads.sort_reverse);
   pd->threads.sort_cb = _sort_by_state;
   _threads_list_reorder(pd);
}

static void
_btn_cpu_id_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->threads.sort_cb == _sort_by_cpu_id)
     pd->threads.sort_reverse = !pd->threads.sort_reverse;
   pd->threads.sort_cb = _sort_by_cpu_id;
   _btn_icon_state_set(obj, pd->threads.sort_reverse);
   _threads_list_reorder(pd);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Data *pd = data;

   if (pd->threads.sort_cb == _sort_by_cpu_usage)
     pd->threads.sort_reverse = !pd->threads.sort_reverse;

   pd->threads.sort_cb = _sort_by_cpu_usage;
   _btn_icon_state_set(obj, pd->threads.sort_reverse);
   _threads_list_reorder(pd);
}

static Evas_Object *
_threads_tab_add(Evas_Object *parent, Data *pd)
{
   Evas_Object *fr, *bx, *bx2, *tb, *rec, *btn, *glist;
   Evas_Object *graph;
   int i = 0;

   fr = elm_frame_add(parent);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_box_homogeneous_set(bx, 1);
   elm_object_content_set(fr, bx);

   graph = _graph(parent, pd);
   elm_box_pack_end(bx, graph);

   bx2 = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx2, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx2, FILL, FILL);
   evas_object_show(bx2);

   tb = elm_table_add(bx2);
   evas_object_size_hint_weight_set(tb, EXPAND, 0);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   elm_box_pack_end(bx2, tb);
   evas_object_show(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
   evas_object_size_hint_max_set(rec, -1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
   elm_table_pack(tb, rec, i++, 0, 1, 1);

   pd->threads.btn_id = btn = elm_button_add(tb);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("id"));
   _btn_icon_state_set(btn, pd->threads.sort_reverse);
   evas_object_smart_callback_add(btn, "clicked", _btn_thread_clicked_cb, pd);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   pd->threads.btn_name = btn = elm_button_add(tb);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("name"));
   _btn_icon_state_set(btn, pd->threads.sort_reverse);
   evas_object_smart_callback_add(btn, "clicked", _btn_name_clicked_cb, pd);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   pd->threads.btn_state = btn = elm_button_add(tb);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("state"));
   _btn_icon_state_set(btn, pd->threads.sort_reverse);
   evas_object_smart_callback_add(btn, "clicked", _btn_state_clicked_cb, pd);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   pd->threads.btn_cpu_id = btn = elm_button_add(tb);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("cpu id"));
   _btn_icon_state_set(btn, pd->threads.sort_reverse);
   evas_object_smart_callback_add(btn, "clicked", _btn_cpu_id_clicked_cb, pd);
   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   pd->threads.btn_cpu_usage = btn = elm_button_add(tb);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("cpu %"));
   _btn_icon_state_set(btn, pd->threads.sort_reverse);
   evas_object_smart_callback_add(btn, "clicked", _btn_cpu_usage_clicked_cb, pd);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   pd->threads.glist = glist = elm_genlist_add(parent);
   evas_object_data_set(glist, "ui", pd);
   elm_object_focus_allow_set(glist, 0);
   elm_genlist_homogeneous_set(glist, 1);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_NONE);
   elm_scroller_policy_set(glist, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_AUTO);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);

   evas_object_smart_callback_add(pd->threads.glist, "unrealized",
                                  _item_unrealized_cb, pd);
   elm_box_pack_end(bx2, glist);
   evas_object_show(glist);
   elm_box_pack_end(bx, bx2);

   return fr;
}

static Evas_Object *
_children_tab_add(Evas_Object *parent, Data *pd)
{
   Evas_Object *fr, *bx, *glist;

   fr = elm_frame_add(parent);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(fr, bx);

   pd->children.glist = glist = elm_genlist_add(parent);
   evas_object_data_set(glist, "ui", pd);
   elm_object_focus_allow_set(glist, 1);
   elm_genlist_homogeneous_set(glist, 1);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_DEFAULT);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   evas_object_show(glist);
   evas_object_smart_callback_add(glist, "selected",
                                  _item_children_clicked_cb, pd);
   elm_box_pack_end(bx, glist);

   return fr;
}

static Evas_Object *
_manual_tab_add(Evas_Object *parent, Data *pd)
{
   Evas_Object *fr, *bx, *entry;
   Evas_Object *tb;
   int sz;

   fr = elm_frame_add(parent);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(fr, bx);

   pd->manual.entry = entry = elm_entry_add(bx);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, 0);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_entry_editable_set(entry, 0);
   elm_entry_scrollable_set(entry, 1);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(entry);
   elm_box_pack_end(bx, entry);

   tb = elm_entry_textblock_get(entry);
   sz = evisum_ui_textblock_font_size_get(tb);
   evisum_ui_textblock_font_size_set(tb, sz - ELM_SCALE_SIZE(1));

   return fr;
}

static void
_tab_change(Data *pd, Evas_Object *view, Evas_Object *obj)
{
   Elm_Transit *trans;

   elm_object_disabled_set(pd->tab_general, 0);
   elm_object_disabled_set(pd->tab_children, 0);
   elm_object_disabled_set(pd->tab_thread, 0);
   elm_object_disabled_set(pd->tab_manual, 0);
   evas_object_hide(pd->general_view);
   evas_object_hide(pd->children_view);
   evas_object_hide(pd->manual_view);
   evas_object_hide(pd->thread_view);

   trans = elm_transit_add();
   elm_transit_object_add(trans, pd->current_view);
   elm_transit_object_add(trans, view);
   elm_transit_duration_set(trans, 0.15);
   elm_transit_effect_blend_add(trans);

   pd->current_view = view;
   evas_object_show(view);
   elm_transit_go(trans);

   elm_object_disabled_set(obj, 1);
}

static void
_tab_general_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _tab_change(pd, pd->general_view, obj);
   elm_object_focus_set(pd->tab_children, 1);
}

static void
_tab_children_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _children_view_update(pd);
   _tab_change(pd, pd->children_view, obj);
   elm_object_focus_set(pd->tab_thread, 1);
}

static void
_tab_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _tab_change(pd, pd->thread_view, obj);
   elm_object_focus_set(pd->tab_manual, 1);
}

static void
_tab_manual_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _tab_change(pd, pd->manual_view, obj);
   elm_object_focus_set(pd->tab_general, 1);
   _manual_init(pd);
}

static Evas_Object *
_tabs_add(Evas_Object *parent, Data *pd)
{
   Evas_Object *hbx, *pad, *btn;

   hbx = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_align_set(hbx, FILL, 0.5);
   elm_box_horizontal_set(hbx, 1);
   evas_object_show(hbx);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_medium");
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);
   elm_box_pack_end(hbx, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->tab_general, _("Process"),
                           _tab_general_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbx, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->tab_children, _("Children"),
                           _tab_children_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbx, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->tab_thread, _("Threads"),
                           _tab_threads_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbx, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->tab_manual, _("Manual"),
                           _tab_manual_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbx, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_medium");
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);
   elm_box_pack_end(hbx, pad);

   return hbx;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Evas_Object *win;
   Data *pd;

   pd  = data;
   win = obj;

   if (pd->thread)
     {
        ecore_thread_cancel(pd->thread);
        ecore_thread_wait(pd->thread, 0.5);
     }

   if (pd->threads.hash_cpu_times)
     eina_hash_free(pd->threads.hash_cpu_times);
   if (pd->selected_cmd)
     free(pd->selected_cmd);
   if (pd->threads.cache)
     evisum_ui_item_cache_free(pd->threads.cache);

   evas_object_del(win);

   free(pd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Data *pd = data;

   elm_genlist_realized_items_update(pd->threads.glist);
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Data *pd;

   pd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(pd->win);
}

static void
_activate(Data *pd, Evisum_Proc_Action action)
{
   switch (action)
     {
       case PROC_VIEW_DEFAULT:
         pd->current_view = pd->general_view;
         _tab_general_clicked_cb(pd, pd->tab_general, NULL);
         break;
       case PROC_VIEW_CHILDREN:
         pd->current_view = pd->children_view;
         _tab_children_clicked_cb(pd, pd->tab_children, NULL);
         break;
       case PROC_VIEW_THREADS:
         pd->current_view = pd->thread_view;
         _tab_threads_clicked_cb(pd, pd->tab_thread, NULL);
         break;
       case PROC_VIEW_MANUAL:
         pd->current_view = pd->manual_view;
         _tab_manual_clicked_cb(pd, pd->tab_manual, NULL);
         break;
     }
   evas_object_show(pd->current_view);
}

void
ui_process_view_win_add(int pid, Evisum_Proc_Action action)
{
   Evas_Object *win, *ic, *bx, *tabs, *tb;
   Proc_Info *proc;

   Data *pd = calloc(1, sizeof(Data));
   pd->selected_pid = pid;
   pd->poll_delay = 1;
   pd->threads.cache = NULL;
   pd->threads.sort_reverse = 1;
   pd->threads.sort_cb = _sort_by_cpu_usage;

   proc = proc_info_by_pid(pid);
   if (!proc)
     pd->selected_cmd = strdup(_("Unknown"));
   else
     {
        pd->selected_cmd = strdup(proc->command);
        pd->start = proc->start;
        proc_info_free(proc);
     }

   pd->win = win = elm_win_util_standard_add("evisum", "evisum");
   elm_win_autodel_set(win, 1);
   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, "evisum");
   elm_win_icon_object_set(win, ic);

   elm_win_title_set(pd->win, eina_slstr_printf("%s (%i)",
                     pd->selected_cmd, pd->selected_pid));

   tabs = _tabs_add(win, pd);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EXPAND, 0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_box_pack_end(bx, tabs);

   tb = elm_table_add(bx);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, 0.0);
   evas_object_show(tb);

   pd->general_view = _general_tab_add(tabs, pd);
   pd->children_view = _children_tab_add(tabs, pd);
   pd->thread_view = _threads_tab_add(tabs, pd);
   pd->manual_view = _manual_tab_add(tabs, pd);

   elm_table_pack(tb, pd->general_view, 0, 0, 1, 1);
   elm_table_pack(tb, pd->children_view, 0, 0, 1, 1);
   elm_table_pack(tb, pd->thread_view, 0, 0, 1, 1);
   elm_table_pack(tb, pd->manual_view, 0, 0, 1, 1);

   elm_box_pack_end(bx, tb);
   elm_object_content_set(win, bx);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);
   evas_object_event_callback_add(bx, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, pd);

   evas_object_resize(win, -1, -1);
   elm_win_center(win, 1, 1);
   evas_object_show(win);

   _activate(pd, action);

   pd->threads.cache = evisum_ui_item_cache_new(pd->threads.glist,
                                                _item_create, 10);

   pd->thread = ecore_thread_feedback_run(_proc_info_main,
                                          _proc_info_feedback_cb,
                                          NULL,
                                          NULL,
                                          pd, 0);
}

