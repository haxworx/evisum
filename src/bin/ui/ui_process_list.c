#include "config.h"
#include "evisum_config.h"

#include "ui.h"
#include "ui/ui_process_list.h"
#include "ui/ui_process_view.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

extern Evisum_Config *_evisum_config;
extern int EVISUM_EVENT_CONFIG_CHANGED;

Eina_Lock _lock;

typedef struct
{
   Ecore_Thread    *thread;
   Evisum_Ui_Cache *cache;
   Eina_List       *cpu_times;
   Eina_List       *cpu_list;

   Ui              *ui;

   pid_t            selected_pid;
   char            *search_text;

   Evas_Object     *win;
   Evas_Object     *menu;

   Ecore_Timer     *timer_entry;
   Evas_Object     *entry_pop;
   Evas_Object     *entry;

   Evas_Object     *scroller;
   Evas_Object     *genlist;

   Evas_Object     *btn_menu;

   Evas_Object     *btn_pid;
   Evas_Object     *btn_uid;
   Evas_Object     *btn_cmd;
   Evas_Object     *btn_size;
   Evas_Object     *btn_rss;
   Evas_Object     *btn_state;
   Evas_Object     *btn_cpu_usage;

} Ui_Data;

#define PAD_W 2

static Ui_Data *_private_data = NULL;

// See pb_format_cb.
static double _cpu_usage = 0.0;

static int
_sort_by_pid(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

static int
_sort_by_uid(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->uid - inf2->uid;
}

static int
_sort_by_nice(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->nice - inf2->nice;
}

static int
_sort_by_pri(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->priority - inf2->priority;
}

static int
_sort_by_cpu(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

static int
_sort_by_threads(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numthreads - inf2->numthreads;
}

static int
_sort_by_size(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_size;
   size2 = inf2->mem_size;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

static int
_sort_by_rss(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_rss;
   size2 = inf2->mem_rss;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   double one, two;

   inf1 = p1; inf2 = p2;

   one = inf1->cpu_usage;
   two = inf2->cpu_usage;

   if (one > two)
     return 1;
   else if (one < two)
     return -1;
   else return 0;
}

static int
_sort_by_cmd(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcasecmp(inf1->command, inf2->command);
}

static int
_sort_by_state(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}

static Eina_List *
_list_sort(Ui *ui, Eina_List *list)
{
   switch (ui->settings.sort_type)
     {
      case SORT_BY_NONE:
      case SORT_BY_PID:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_pid);
        break;

      case SORT_BY_UID:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_uid);
        break;

      case SORT_BY_NICE:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_nice);
        break;

      case SORT_BY_PRI:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_pri);
        break;

      case SORT_BY_CPU:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_cpu);
        break;

      case SORT_BY_THREADS:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_threads);
        break;

      case SORT_BY_SIZE:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_size);
        break;

      case SORT_BY_RSS:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_rss);
        break;

      case SORT_BY_CMD:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_cmd);
        break;

      case SORT_BY_STATE:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_state);
        break;

      case SORT_BY_CPU_USAGE:
        list = eina_list_sort(list, eina_list_count(list), _sort_by_cpu_usage);
        break;
     }

   if (ui->settings.sort_reverse)
     list = eina_list_reverse(list);

   return list;
}

typedef struct {
   pid_t pid;
   int64_t cpu_time_prev;
} pid_cpu_time_t;

static void
_proc_pid_cpu_times_free(Ui_Data *pd)
{
   pid_cpu_time_t *tmp;

   EINA_LIST_FREE(pd->cpu_times, tmp)
     {
        free(tmp);
     }
}

static void
_proc_pid_cpu_times_reset(Ui_Data *pd)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;

   EINA_LIST_FOREACH(pd->cpu_times, l, tmp)
     tmp->cpu_time_prev = 0;
}

static void
_proc_pid_cpu_time_save(Ui_Data *pd, Proc_Info *proc)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;

   EINA_LIST_FOREACH(pd->cpu_times, l, tmp)
     {
        if (tmp->pid == proc->pid)
          {
             tmp->cpu_time_prev = proc->cpu_time;
             return;
          }
     }

   tmp = calloc(1, sizeof(pid_cpu_time_t));
   if (tmp)
     {
        tmp->pid = proc->pid;
        tmp->cpu_time_prev = proc->cpu_time;
        pd->cpu_times = eina_list_append(pd->cpu_times, tmp);
     }
}

static void
_proc_pid_cpu_usage_get(Ui_Data *pd, Proc_Info *proc)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;

   EINA_LIST_FOREACH(pd->cpu_times, l, tmp)
     {
        if (tmp->pid == proc->pid)
          {
             if (tmp->cpu_time_prev && proc->cpu_time > tmp->cpu_time_prev)
               {
                  proc->cpu_usage =
                     (double) (proc->cpu_time - tmp->cpu_time_prev) /
                     pd->ui->settings.poll_delay;
               }
             _proc_pid_cpu_time_save(pd, proc);
             return;
          }
     }

   _proc_pid_cpu_time_save(pd, proc);
}

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Evas_Object *o;
   Ui_Data *pd;
   Eina_List *contents = NULL;

   pd = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        evisum_ui_item_cache_item_release(pd->cache, o);
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;
   proc_info_free(proc);
}

static Evas_Object *
_item_column_add(Evas_Object *tbl, const char *text, int col)
{
   Evas_Object *rec, *lbl;

   lbl = elm_label_add(tbl);
   evas_object_data_set(tbl, text, lbl);
   evas_object_size_hint_align_set(lbl, FILL, FILL);
   evas_object_size_hint_weight_set(lbl, EXPAND, EXPAND);
   evas_object_show(lbl);

   rec = evas_object_rectangle_add(tbl);
   evas_object_data_set(lbl, "rec", rec);

   elm_table_pack(tbl, rec, col, 0, 1, 1);
   elm_table_pack(tbl, lbl, col, 0, 1, 1);

   return lbl;
}

static char *
_pb_format_cb(double val)
{
   static char buf[32];

   snprintf(buf, sizeof(buf), "%1.1f %%", _cpu_usage);

   return strdup(buf);
}

static void
_pb_format_free_cb(char *str)
{
   free(str);
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *obj, *tbl, *lbl, *ic, *rec;
   Evas_Object *hbx, *pb;
   int i = 0;

   obj = parent;

   tbl = elm_table_add(obj);
   evas_object_size_hint_align_set(tbl, FILL, 0);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);

   hbx = elm_box_add(tbl);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, 0.0, FILL);
   evas_object_size_hint_weight_set(hbx, EXPAND, EXPAND);
   evas_object_show(hbx);

   rec = evas_object_rectangle_add(tbl);
   evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(24));
   elm_box_pack_end(hbx, rec);

   ic = elm_icon_add(tbl);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   evas_object_size_hint_align_set(ic, FILL, FILL);
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   evas_object_size_hint_max_set(ic, ELM_SCALE_SIZE(20), ELM_SCALE_SIZE(20));
   evas_object_show(ic);
   evas_object_data_set(tbl, "icon", ic);
   elm_box_pack_end(hbx, ic);
   elm_table_pack(tbl, hbx, i, 0, 1, 1);

   rec = evas_object_rectangle_add(tbl);
   evas_object_size_hint_min_set(rec, 6, 1);
   elm_box_pack_end(hbx, rec);

   rec = evas_object_rectangle_add(tbl);
   evas_object_data_set(ic, "rec", rec);
   elm_table_pack(tbl, rec, i++, 0, 1, 1);

   lbl = elm_label_add(tbl);
   evas_object_size_hint_weight_set(lbl, 0, EXPAND);
   evas_object_data_set(tbl, "proc_cmd", lbl);
   evas_object_data_set(lbl, "hbx", hbx);
   evas_object_show(lbl);
   elm_box_pack_end(hbx, lbl);

   rec = evas_object_rectangle_add(tbl);
   evas_object_size_hint_min_set(rec, 4, 1);
   elm_box_pack_end(hbx, rec);

   lbl =_item_column_add(tbl, "proc_uid", i++);
   evas_object_size_hint_align_set(lbl, 0.0, FILL);
   lbl = _item_column_add(tbl, "proc_pid", i++);
   evas_object_size_hint_align_set(lbl, 0.0, FILL);
   lbl = _item_column_add(tbl, "proc_size", i++);
   evas_object_size_hint_align_set(lbl, 0.0, FILL);
   lbl = _item_column_add(tbl, "proc_rss", i++);
   evas_object_size_hint_align_set(lbl, 0.0, FILL);
   lbl = _item_column_add(tbl, "proc_state", i++);
   evas_object_size_hint_align_set(lbl, 0.5, FILL);

   hbx = elm_box_add(tbl);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_size_hint_weight_set(hbx, EXPAND, EXPAND);
   evas_object_show(hbx);
   elm_table_pack(tbl, hbx, i++, 0, 1, 1);

   pb = elm_progressbar_add(hbx);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   elm_progressbar_unit_format_function_set(pb, _pb_format_cb, _pb_format_free_cb);
   elm_box_pack_end(hbx, pb);
   evas_object_data_set(tbl, "proc_cpu_usage", pb);

   return tbl;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Proc_Info *proc;
   struct passwd *pwd_entry;
   Evas_Object *l, *r, *o, *hbx, *pb;
   Evas_Coord w, ow;
   Ui_Data *pd = _private_data;

   proc = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!proc) return NULL;
   if (!pd->ui->state.ready) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(pd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(pd->btn_pid, NULL, NULL, &w, NULL);
   w += PAD_W;
   l = evas_object_data_get(it->obj, "proc_pid");
   elm_object_text_set(l, eina_slstr_printf("%d", proc->pid));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_pid, w, 1);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(pd->btn_uid, NULL, NULL, &w, NULL);
   w += PAD_W;
   l = evas_object_data_get(it->obj, "proc_uid");
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(l, eina_slstr_printf("%s", pwd_entry->pw_name));
   else
     elm_object_text_set(l, eina_slstr_printf("%d", proc->uid));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_uid, w, 1);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(pd->btn_size, NULL, NULL, &w, NULL);
   w += PAD_W;
   l = evas_object_data_get(it->obj, "proc_size");
   elm_object_text_set(l, eina_slstr_printf("%s", evisum_size_format(proc->mem_size)));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_size, w, 1);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(pd->btn_rss, NULL, NULL, &w, NULL);
   w += PAD_W;
   l = evas_object_data_get(it->obj, "proc_rss");
   elm_object_text_set(l, eina_slstr_printf("%s", evisum_size_format(proc->mem_rss)));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_rss, w, 1);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(pd->btn_menu, NULL, NULL, &ow, NULL);
   evas_object_geometry_get(pd->btn_cmd, NULL, NULL, &w, NULL);
   w += ow + PAD_W;
   l = evas_object_data_get(it->obj, "proc_cmd");
   elm_object_text_set(l, eina_slstr_printf("%s", proc->command));
   hbx = evas_object_data_get(l, "hbx");
   evas_object_geometry_get(hbx, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_cmd, w, 1);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   o = evas_object_data_get(it->obj, "icon");
   elm_icon_standard_set(o, evisum_icon_path_get(evisum_icon_cache_find(proc)));
   r = evas_object_data_get(o, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(o);

   evas_object_geometry_get(pd->btn_state, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_state");
   elm_object_text_set(l, eina_slstr_printf("%s", proc->state));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   r = evas_object_data_get(l, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   pb = evas_object_data_get(it->obj, "proc_cpu_usage");
   _cpu_usage = proc->cpu_usage;
   elm_progressbar_value_set(pb, proc->cpu_usage / 100.0);
   evas_object_show(pb);

   evas_object_show(it->obj);
   // Let the genlist resize but align the text.
   elm_table_align_set(it->obj, 0, 0.5);

   return it->obj;
}

static void
_genlist_ensure_n_items(Evas_Object *genlist, unsigned int items)
{
   Elm_Object_Item *it;
   Elm_Genlist_Item_Class *itc;
   unsigned int i, existing = elm_genlist_items_count(genlist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
           {
              it = elm_genlist_last_item_get(genlist);
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
        elm_genlist_item_append(genlist, itc, NULL, NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_item_class_free(itc);
}

static Eina_Bool
_show_items(void *data)
{
   Ui_Data *pd = data;

   elm_genlist_realized_items_update(pd->genlist);
   evas_object_show(pd->genlist);

   return EINA_FALSE;
}

static Eina_Bool
_bring_in(void *data)
{
   Ui_Data *pd;
   int h_page, v_page;

   pd = data;
   elm_scroller_gravity_set(pd->scroller, 0.0, 0.0);
   elm_scroller_last_page_get(pd->scroller, &h_page, &v_page);
   elm_scroller_page_bring_in(pd->scroller, h_page, v_page);

   ecore_timer_add(0.5, _show_items, pd);

   return EINA_FALSE;
}

static void
_process_list_cancel_cb(void *data, Ecore_Thread *thread)
{
   Ui_Data *pd = data;

   _proc_pid_cpu_times_free(pd);
}

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED,
                          void *msg EINA_UNUSED)
{
   Ui *ui;
   Ui_Data *pd;
   Eina_List *list, *l, *l_next;
   Proc_Info *proc;
   Elm_Object_Item *it;
   int len = 0;
   list = NULL;

   pd = data;
   ui = pd->ui;

   if (!eina_lock_take_try(&_lock))
     return;

   if (ui->settings.show_desktop)
     {
        int pid = 0;
        list = proc_info_all_get();
        EINA_LIST_FREE(list, proc)
          {
             if (!strcmp(proc->command, "enlightenment"))
               pid = proc->pid;
             proc_info_free(proc);
          }
        if (pid != -1)
          list = proc_info_pid_children_get(pid);
     }

   if (!list)
     list = proc_info_all_get();

   if (pd->search_text && pd->search_text[0])
     len = strlen(pd->search_text);

   if (ui->settings.show_user)
     {
        static uid_t uid = 0;

        if (!uid) uid = getuid();

        EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
          {
             if (proc->uid != uid)
               {
                  proc_info_free(proc);
                  list = eina_list_remove_list(list, l);
               }
          }
     }

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
        if (((len && (strncasecmp(proc->command, pd->search_text, len))) ||
            (!ui->settings.show_self && (proc->pid == ui->program_pid))))
         {
            proc_info_free(proc);
            list = eina_list_remove_list(list, l);
         }
        else
         {
            _proc_pid_cpu_usage_get(pd, proc);
         }
     }

   _genlist_ensure_n_items(pd->genlist, eina_list_count(list));

   it = elm_genlist_first_item_get(pd->genlist);

   list = _list_sort(ui, list);

   EINA_LIST_FREE(list, proc)
     {
        if (!it)
          proc_info_free(proc);
        else
          {
             Proc_Info *prev = elm_object_item_data_get(it);
             if (prev)
               proc_info_free(prev);

             elm_object_item_data_set(it, proc);

             it = elm_genlist_item_next_get(it);
          }
     }

   elm_genlist_realized_items_update(pd->genlist);

   eina_lock_release(&_lock);
}

static void
_process_list_update(Ui_Data *pd)
{
   _process_list_feedback_cb(pd, NULL, NULL);
}

static void
_process_list(void *data, Ecore_Thread *thread)
{
   Ui_Data *pd;
   Ui *ui;
   int delay = 1;

   pd = data;
   ui = pd->ui;

   while (1)
     {
        if (ecore_thread_check(thread)) return;
        ecore_thread_feedback(thread, ui);
        for (int i = 0; i < delay * 4; i++)
          {
             if (ecore_thread_check(thread)) return;

             if (ui->state.skip_wait)
               {
                  ui->state.skip_wait = EINA_FALSE;
                  break;
               }
             usleep(250000);
          }
        ui->state.ready = EINA_TRUE;
        delay = ui->settings.poll_delay;
     }
}

static void
_btn_icon_state_update(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *icon = elm_icon_add(button);

   if (reverse)
     elm_icon_standard_set(icon, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, evisum_icon_path_get("go-up"));

   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
}

static void
_btn_icon_state_init(Evas_Object *button, Eina_Bool reverse,
                     Eina_Bool selected EINA_UNUSED)
{
   Evas_Object *icon = elm_icon_add(button);

   if (reverse)
     elm_icon_standard_set(icon, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, evisum_icon_path_get("go-up"));

   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
}

static void
_btn_clicked_state_save(Ui_Data *pd, Evas_Object *btn)
{
   Ui *ui = pd->ui;

   _btn_icon_state_update(btn, ui->settings.sort_reverse);

   evisum_ui_config_save(ui);

   _process_list_update(pd);

   elm_scroller_page_bring_in(pd->scroller, 0, 0);
}

static void
_btn_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_PID)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_PID;

   _btn_clicked_state_save(pd, pd->btn_pid);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_UID)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_UID;

   _btn_clicked_state_save(pd, pd->btn_uid);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_CPU_USAGE)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_CPU_USAGE;

   _btn_clicked_state_save(pd, pd->btn_cpu_usage);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_SIZE)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_SIZE;

   _btn_clicked_state_save(pd, pd->btn_size);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_RSS)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_RSS;

   _btn_clicked_state_save(pd, pd->btn_rss);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_CMD)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_CMD;

   _btn_clicked_state_save(pd, pd->btn_cmd);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (ui->settings.sort_type == SORT_BY_STATE)
     ui->settings.sort_reverse = !ui->settings.sort_reverse;
   ui->settings.sort_type = SORT_BY_STATE;

   _btn_clicked_state_save(pd, pd->btn_state);
}

static void
_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                        void *ev EINA_UNUSED)
{
   Ui_Data *pd = data;

   evas_object_del(obj);

   pd->menu = NULL;
}

static void
_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   kill(pd->selected_pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   kill(pd->selected_pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   kill(pd->selected_pid, SIGKILL);
}

static void
_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;
   elm_menu_close(pd->menu);
   pd->menu = NULL;
}

static void
_item_menu_debug_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Proc_Info *proc;
   const char *terminal = "xterm";
   Ui_Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   proc = proc_info_by_pid(pd->selected_pid);
   if (!proc) return;

   if (ecore_file_app_installed("terminology"))
     terminal = "terminology";

   ecore_exe_run(eina_slstr_printf("%s -e gdb attach %d", terminal, proc->pid),
                 NULL);

   proc_info_free(proc);
}

static void
_item_menu_actions_add(Evas_Object *menu, Elm_Object_Item *menu_it, Ui_Data *pd)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("bug"),
                     _("Debug"), _item_menu_debug_cb, pd);
}

static void
_process_win_add(Evas_Object *parent, int pid, int delay)
{
   Proc_Info *proc;

   proc = proc_info_by_pid(pid);
   if (!proc) return;

   ui_process_win_add(parent, proc->pid, proc->command, delay);

   proc_info_free(proc);
}

static void
_item_menu_properties_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Ui *ui;
   Ui_Data *pd = data;

   ui = pd->ui;

   _item_menu_cancel_cb(pd, NULL, NULL);

   _process_win_add(ui->win, pd->selected_pid, ui->settings.poll_delay);
}

static Evas_Object *
_item_menu_create(Ui_Data *pd, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;
   Ui *ui = pd->ui;

   if (!proc) return NULL;

   pd->selected_pid = proc->pid;

   pd->menu = menu = elm_menu_add(ui->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed",
                                  _item_menu_dismissed_cb, pd);

   stopped = !(!strcmp(proc->state, "stop"));

   menu_it = elm_menu_item_add(menu, NULL,
                               evisum_icon_path_get(evisum_icon_cache_find(proc)),
                               proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                                _("Actions"), NULL, NULL);
   _item_menu_actions_add(menu, menu_it2, pd);
   elm_menu_item_separator_add(menu, menu_it);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("start"),
                                _("Start"), _item_menu_start_cb, pd);

   elm_object_item_disabled_set(menu_it2, stopped);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("stop"),
                                _("Stop"), _item_menu_stop_cb, pd);

   elm_object_item_disabled_set(menu_it2, !stopped);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("kill"), "Kill",
                     _item_menu_kill_cb, pd);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("info"),
                                _("Properties"), _item_menu_properties_cb, pd);

   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("cancel"),
                     _("Cancel"), _item_menu_cancel_cb, pd);

   return menu;
}

static void
_item_pid_secondary_clicked_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                               Evas_Object *obj, void *event_info)
{
   Evas_Object *menu;
   Evas_Event_Mouse_Up *ev;
   Ui_Data *pd;
   Elm_Object_Item *it;
   Proc_Info *proc;

   ev = event_info;
   if (ev->button != 3) return;

   it = elm_genlist_at_xy_item_get(obj, ev->output.x, ev->output.y, NULL);
   proc = elm_object_item_data_get(it);
   if (!proc) return;

   pd = data;

   menu = _item_menu_create(pd, proc);
   if (!menu) return;

   elm_menu_move(menu, ev->canvas.x, ev->canvas.y);
   evas_object_show(menu);
}

static void
_item_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ui *ui;
   Elm_Object_Item *it;
   Proc_Info *proc;
   Ui_Data *pd = data;

   ui = pd->ui;
   it = event_info;

   elm_genlist_item_selected_set(it, EINA_FALSE);
   if (pd->menu) return;

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   pd->selected_pid = proc->pid;
   ui_process_win_add(ui->win, proc->pid, proc->command,
                      ui->settings.poll_delay);
}

static void
_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *ev EINA_UNUSED)
{
   Ui *ui = data;

   elm_ctxpopup_dismiss(ui->main_menu);
   evas_object_del(ui->main_menu);

   ui->main_menu = NULL;
}

static Evas_Object *
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb,
            void *data)
{
   Evas_Object *btn, *ic;

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), 1);
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);

   elm_object_tooltip_text_set(btn, text);
   evas_object_smart_callback_add(btn, "clicked", cb, data);

   return btn;
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (!ui->main_menu)
     evisum_ui_main_menu_create(ui, obj);
   else
     _main_menu_dismissed_cb(ui, NULL, NULL);
}

static void
_evisum_process_filter(Ui_Data *pd, const char *text)
{
   if (pd->search_text)
     free(pd->search_text);

   pd->search_text = strdup(text);
}

static Eina_Bool
_search_empty(void *data)
{
   Ui_Data *pd = data;

   if (pd->search_text && !pd->search_text[0])
     {
        evas_object_lower(pd->entry_pop);
        return EINA_TRUE;
     }

   return EINA_TRUE;
}

static void
_evisum_search_keypress_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                           void *event_info)
{
   Ui *ui;
   const char *markup;
   Evas_Coord w, h;
   char *text;
   Evas_Object *entry;
   Evas_Event_Key_Down *event;
   Ui_Data *pd;

   pd = data;
   ui = pd->ui;
   event = event_info;
   entry = obj;

   if (!event) return;

   ui->state.skip_wait = EINA_TRUE;

   evas_object_geometry_get(pd->win, NULL, NULL, &w, &h);
   evas_object_move(pd->entry_pop, w / 2, h / 2);
   evas_object_raise(pd->entry_pop);

   if (!pd->timer_entry)
     pd->timer_entry = ecore_timer_add(1.0, _search_empty, pd);

   markup = elm_object_part_text_get(entry, NULL);
   text = elm_entry_markup_to_utf8(markup);
   if (text)
     {
       _evisum_process_filter(pd, text);
       free(text);
     }
}

static void
_evisum_search_add(Ui_Data *pd)
{
   Evas_Object *tbl, *rec, *entry;

   pd->entry = entry = elm_entry_add(pd->win);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_FALSE);
   elm_entry_editable_set(entry, EINA_TRUE);
   evas_object_event_callback_add(pd->entry, EVAS_CALLBACK_KEY_DOWN,
                                  _evisum_search_keypress_cb, pd);
   evas_object_show(entry);

   pd->entry_pop = tbl = elm_table_add(pd->win);
   evas_object_lower(tbl);
   evas_object_show(tbl);

   rec = evas_object_rectangle_add(evas_object_evas_get(pd->win));
   evas_object_color_set(rec, 0, 0, 0, 128);
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(192), ELM_SCALE_SIZE(128));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(192), ELM_SCALE_SIZE(128));
   evas_object_show(rec);
   elm_table_pack(tbl, rec, 0, 0, 1, 1);
   elm_table_pack(tbl, entry, 0, 0, 1, 1);
}

static Evas_Object *
_ui_content_system_add(Ui_Data *pd, Evas_Object *parent)
{
   Evas_Object *bx, *fr, *tbl, *btn, *plist;
   Ui *ui = pd->ui;
   int i = 0;

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, 0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   tbl = elm_table_add(parent);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_show(tbl);
   elm_table_padding_set(tbl, PAD_W, 0);

   pd->btn_menu = btn = _btn_create(tbl, "menu", NULL, _btn_menu_clicked_cb, ui);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);

   pd->btn_cmd = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_CMD ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_CMD);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Command"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_cmd_clicked_cb, pd);

   pd->btn_uid = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_UID ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_UID);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("User"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_uid_clicked_cb, pd);

   pd->btn_pid = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_PID ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_PID);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("PID"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_pid_clicked_cb, pd);

   pd->btn_size = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_SIZE ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_SIZE);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Size"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_size_clicked_cb, pd);

   pd->btn_rss = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_RSS ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_RSS);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Res"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_rss_clicked_cb, pd);

   pd->btn_state = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_STATE ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_STATE);
   evas_object_size_hint_weight_set(btn, 0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("State"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_state_clicked_cb, pd);

   pd->btn_cpu_usage = btn = elm_button_add(parent);
   _btn_icon_state_init(btn,
            (ui->settings.sort_type == SORT_BY_CPU_USAGE ?
            ui->settings.sort_reverse : EINA_FALSE),
            ui->settings.sort_type == SORT_BY_CPU_USAGE);
   evas_object_size_hint_weight_set(btn, EXPAND, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("CPU %"));
   evas_object_show(btn);
   elm_table_pack(tbl, btn, i++, 1, 1, 1);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_cpu_usage_clicked_cb, pd);

   pd->scroller = pd->genlist = plist = elm_genlist_add(parent);
   elm_scroller_gravity_set(pd->scroller, 0.0, 1.0);
   elm_object_focus_allow_set(plist, EINA_FALSE);
   elm_scroller_movement_block_set(pd->scroller,
                                  ELM_SCROLLER_MOVEMENT_BLOCK_HORIZONTAL);
   elm_scroller_policy_set(pd->scroller, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_AUTO);
   elm_genlist_multi_select_set(plist, EINA_FALSE);
   evas_object_size_hint_weight_set(plist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(plist, FILL, FILL);
   elm_table_pack(tbl, plist, 0, 2, i, 1);


   _evisum_search_add(pd);

   evas_object_smart_callback_add(pd->genlist, "selected",
                   _item_pid_clicked_cb, pd);
   evas_object_event_callback_add(pd->genlist, EVAS_CALLBACK_MOUSE_UP,
                   _item_pid_secondary_clicked_cb, pd);
   evas_object_smart_callback_add(pd->genlist, "unrealized",
                   _item_unrealized_cb, pd);

   elm_box_pack_end(bx, tbl);

   fr = elm_frame_add(parent);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_small");
   evas_object_show(fr);
   elm_object_content_set(fr, bx);

   ecore_timer_add(2.0, _bring_in, pd);

   return fr;
}

static void
_evisum_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Ui *ui;
   Ui_Data *pd = data;
   Eina_Bool control;

   ev = event_info;
   ui = pd->ui;

   if (!ev || !ev->keyname)
     return;

   ui->state.skip_wait = EINA_TRUE;

   if (!strcmp(ev->keyname, "Escape"))
     {
        ecore_main_loop_quit();
        return;
     }

   control = evas_key_modifier_is_set(ev->modifiers, "Control");
   if (!control)
     {
        elm_object_focus_set(pd->entry, EINA_TRUE);
        return;
     }

   if (ev->keyname[0] == 'e' || ev->keyname[0] == 'E')
     ui->settings.show_self = !ui->settings.show_self;

   if (ev->keyname[0] == 'k' || ev->keyname[0] == 'K')
     ui->settings.show_kthreads = !ui->settings.show_kthreads;

   evisum_ui_config_save(ui);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   if (eina_lock_take_try(&_lock))
     {
        elm_genlist_realized_items_update(pd->genlist);
        eina_lock_release(&_lock);
     }

   evas_object_lower(pd->entry_pop);
   if (ui->main_menu)
     _main_menu_dismissed_cb(ui, NULL, NULL);

   evisum_ui_config_save(ui);
}

static Eina_Bool
_elm_config_changed_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ui_Data *pd = data;

   elm_genlist_clear(pd->genlist);
   _process_list_update(pd);

   return EINA_TRUE;
}

static Eina_Bool
_evisum_config_changed_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ui_Data *pd = data;

   _proc_pid_cpu_times_reset(pd);

   return EINA_TRUE;
}

static void
_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui;
   Ui_Data *pd = data;

   ui = pd->ui;

   evas_object_del(obj);

   if (pd->timer_entry)
     ecore_timer_del(pd->timer_entry);

   if (pd->thread)
     ecore_thread_cancel(pd->thread);

   if (pd->thread)
     ecore_thread_wait(pd->thread, 0.2);

   pd->thread = NULL;
   ui->win = NULL;

   if (ui->processes.animator)
     ecore_animator_del(ui->processes.animator);

   if (pd->cache)
     evisum_ui_item_cache_free(pd->cache);

   if (pd->search_text) free(pd->search_text);

   _proc_pid_cpu_times_free(pd);

   eina_lock_free(&_lock);

   free(pd);
}

void
ui_process_list_win_add(Ui *ui)
{
   Evas_Object *win, *icon;
   Evas_Object *o;

   if (ui->win)
     {
        elm_win_raise(ui->win);
        return;
     }

   eina_lock_new(&_lock);

   Ui_Data *pd = _private_data = calloc(1, sizeof(Ui_Data));
   if (!pd) return;
   pd->selected_pid = -1;
   pd->ui = ui;

   ui->win = pd->win = win = elm_win_util_standard_add("evisum", "evisum");
   elm_win_autodel_set(win, EINA_TRUE);
   elm_win_title_set(win, _("EFL System Monitor"));
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "evisum");
   elm_win_icon_object_set(win, icon);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);

   if (_evisum_config->width > 1 && _evisum_config->height > 1)
     evas_object_resize(win, _evisum_config->width, _evisum_config->height);
   else
     evas_object_resize(win, EVISUM_WIN_WIDTH * elm_config_scale_get(),
                        EVISUM_WIN_HEIGHT * elm_config_scale_get());
   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_show(win);

   if (evisum_ui_effects_enabled_get() || evisum_ui_backgrounds_enabled_get())
     evisum_ui_background_add(ui->win, EINA_TRUE);

   o = _ui_content_system_add(pd, win);
   elm_object_content_set(win, o);
   pd->cache = evisum_ui_item_cache_new(pd->genlist, _item_create, 50);

   if (evisum_ui_effects_enabled_get())
     evisum_ui_animate(ui);

   pd->thread =
      ecore_thread_feedback_run(_process_list,
                                _process_list_feedback_cb,
                                _process_list_cancel_cb, NULL, pd, EINA_FALSE);
   _process_list_update(pd);

   evas_object_event_callback_add(ui->win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, pd);
   evas_object_event_callback_add(o, EVAS_CALLBACK_KEY_DOWN,
                                  _evisum_key_down_cb, pd);

   ecore_event_handler_add(ELM_EVENT_CONFIG_ALL_CHANGED,
                           _elm_config_changed_cb, pd);
   ecore_event_handler_add(EVISUM_EVENT_CONFIG_CHANGED,
                           _evisum_config_changed_cb, pd);
}

