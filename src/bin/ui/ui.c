#include "config.h"
#include "ui.h"
#include "ui/ui_disk.h"
#include "ui/ui_misc.h"
#include "ui/ui_memory.h"
#include "ui/ui_cpu.h"
#include "ui/ui_process_view.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

Ui *_ui;
Evisum_Config *_evisum_config;
static Eina_Lock _lock;

static void
_config_save(Ui *ui)
{
   Evas_Coord w, h;

   if (!_evisum_config) return;

   evas_object_geometry_get(ui->win, NULL, NULL, &w, &h);

   _evisum_config->sort_type    = ui->sort_type;
   _evisum_config->sort_reverse = ui->sort_reverse;
   _evisum_config->width = w;
   _evisum_config->height = h;

   config_save(_evisum_config);
}

static void
_config_load(Ui *ui)
{
   _evisum_config   = config_load();

   ui->sort_type    = _evisum_config->sort_type;
   ui->sort_reverse = _evisum_config->sort_reverse;

   if ((_evisum_config->width > 0) && (_evisum_config->height > 0))
     evas_object_resize(ui->win, _evisum_config->width, _evisum_config->height);
}

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
   switch (ui->sort_type)
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

   if (ui->sort_reverse)
     list = eina_list_reverse(list);

   return list;
}

typedef struct {
   pid_t pid;
   int64_t cpu_time_prev;
} pid_cpu_time_t;

static void
_proc_pid_cpu_times_free(Ui *ui)
{
   pid_cpu_time_t *tmp;

   EINA_LIST_FREE(ui->cpu_times, tmp)
     {
        free(tmp);
     }

   if (ui->cpu_times)
     eina_list_free(ui->cpu_times);
}

static void
_proc_pid_cpu_time_save(Ui *ui, Proc_Info *proc)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;

   EINA_LIST_FOREACH(ui->cpu_times, l, tmp)
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
        ui->cpu_times = eina_list_append(ui->cpu_times, tmp);
     }
}

static void
_proc_pid_cpu_usage_get(Ui *ui, Proc_Info *proc)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;

   EINA_LIST_FOREACH(ui->cpu_times, l, tmp)
     {
        if (tmp->pid == proc->pid)
          {
             if (tmp->cpu_time_prev && proc->cpu_time > tmp->cpu_time_prev)
               {
                  proc->cpu_usage =
                     (double) (proc->cpu_time - tmp->cpu_time_prev) /
                     ui->poll_delay;
               }
             _proc_pid_cpu_time_save(ui, proc);
             return;
          }
     }

   _proc_pid_cpu_time_save(ui, proc);
}

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui;
   Evas_Object *o;
   Eina_List *contents = NULL;

   ui = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        evisum_ui_item_cache_item_release(ui->cache, o);
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;
   proc_info_free(proc);
   proc = NULL;
}

static Evas_Object *
_item_column_add(Evas_Object *table, const char *text, int col)
{
   Evas_Object *rect, *label;

   label = elm_label_add(table);
   evas_object_size_hint_align_set(label, EXPAND, EXPAND);
   evas_object_size_hint_weight_set(label, FILL, FILL);
   evas_object_data_set(table, text, label);
   evas_object_show(label);

   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);

   elm_table_pack(table, rect, col, 0, 1, 1);
   elm_table_pack(table, label, col, 0, 1, 1);

   return label;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *obj, *table, *label;

   obj = parent;

   table = elm_table_add(obj);
   evas_object_size_hint_align_set(table, EXPAND, EXPAND);
   evas_object_size_hint_weight_set(table, FILL, FILL);
   evas_object_show(table);

   _item_column_add(table, "proc_pid", 0);
   _item_column_add(table, "proc_uid", 1);
   _item_column_add(table, "proc_size", 2);
   _item_column_add(table, "proc_rss", 3);
   _item_column_add(table, "proc_cmd", 4);

   label = _item_column_add(table, "proc_state", 5);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);

   label = _item_column_add(table, "proc_cpu_usage", 6);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);

   return table;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Ui *ui;
   Proc_Info *proc;
   struct passwd *pwd_entry;
   Evas_Object *l, *r;
   Evas_Coord w, ow;

   proc = (void *) data;
   ui = _ui;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!proc) return NULL;
   if (!ui->ready) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(ui->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(ui->btn_pid, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_pid");
   elm_object_text_set(l, eina_slstr_printf("%d", proc->pid));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_pid, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_uid, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_uid");
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(l, pwd_entry->pw_name);
   else
     elm_object_text_set(l, eina_slstr_printf("%d", proc->uid));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_uid, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_size, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_size");
   elm_object_text_set(l, evisum_size_format(proc->mem_size));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_size, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_rss, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_rss");
   elm_object_text_set(l, evisum_size_format(proc->mem_rss));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_rss, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_cmd, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_cmd");
   elm_object_text_set(l, eina_slstr_printf("  %s", proc->command));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_cmd, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_state, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_state");
   elm_object_text_set(l, eina_slstr_printf("%s", proc->state));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_state, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_cpu_usage, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_cpu_usage");
   elm_object_text_set(l, eina_slstr_printf("%.1f%%", proc->cpu_usage));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_cpu_usage, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

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

static void
_bring_in(Ui *ui)
{
   int h_page, v_page;
   static Eina_Bool init_done = EINA_FALSE;

   if (init_done || !ui->ready) return;

   elm_scroller_gravity_set(ui->scroller, 0.0, 0.0);
   elm_scroller_last_page_get(ui->scroller, &h_page, &v_page);
   elm_scroller_page_bring_in(ui->scroller, h_page, v_page);
   init_done = EINA_TRUE;
}

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED,
                          void *msg EINA_UNUSED)
{
   Ui *ui;
   Eina_List *list, *l, *l_next;
   Proc_Info *proc;
   Elm_Object_Item *it;
   int len = 0;

   ui = data;

   eina_lock_take(&_lock);

   list = proc_info_all_get();

   if (ui->search_text && ui->search_text[0])
     {
        len = strlen(ui->search_text);
     }

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
        if (((len && (strncasecmp(proc->command, ui->search_text, len))) ||
            (!ui->show_self && (proc->pid == ui->program_pid))))
         {
            proc_info_free(proc);
            list = eina_list_remove_list(list, l);
         }
        else
         {
            _proc_pid_cpu_usage_get(ui, proc);
         }
     }

   _genlist_ensure_n_items(ui->genlist_procs, eina_list_count(list));
   it = elm_genlist_first_item_get(ui->genlist_procs);

   list = _list_sort(ui, list);
   EINA_LIST_FREE(list, proc)
     {
        Proc_Info *prev = elm_object_item_data_get(it);
        if (prev)
          proc_info_free(prev);

        elm_object_item_data_set(it, proc);
        elm_genlist_item_update(it);
        it = elm_genlist_item_next_get(it);
     }

   if (list)
     eina_list_free(list);

   eina_lock_release(&_lock);

   _bring_in(ui);
}

static void
_process_list_update(Ui *ui)
{
   _process_list_feedback_cb(ui, NULL, NULL);
}

static void
_process_list(void *data, Ecore_Thread *thread)
{
   Ui *ui;
   int delay = 1;

   ui = data;

   while (1)
     {
        ecore_thread_feedback(thread, ui);
        for (int i = 0; i < delay * 4; i++)
          {
             if (ecore_thread_check(thread)) return;

             if (ui->skip_wait)
               {
                  ui->skip_wait = EINA_FALSE;
                  break;
               }
             usleep(250000);
          }
        ui->ready = EINA_TRUE;
        delay = ui->poll_delay;
     }
}

static void
_btn_icon_state_set(Evas_Object *button, Eina_Bool reverse)
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
_btn_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PID)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_pid, ui->sort_reverse);

   ui->sort_type = SORT_BY_PID;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_UID)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_uid, ui->sort_reverse);

   ui->sort_type = SORT_BY_UID;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cpu_usage, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU_USAGE;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_SIZE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_size, ui->sort_reverse);

   ui->sort_type = SORT_BY_SIZE;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_RSS)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_rss, ui->sort_reverse);

   ui->sort_type = SORT_BY_RSS;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CMD)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cmd, ui->sort_reverse);

   ui->sort_type = SORT_BY_CMD;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_STATE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_state, ui->sort_reverse);

   ui->sort_type = SORT_BY_STATE;

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_quit_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   exit(0);
}

static void
_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                        void *ev EINA_UNUSED)
{
   Ui *ui = data;

   evas_object_del(obj);

   ui->menu = NULL;
}

static void
_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGKILL);
}

static void
_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   elm_menu_close(ui->menu);
   ui->menu = NULL;
}

static void
_item_menu_priority_increase_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info EINA_UNUSED)
{
   Proc_Info *proc = data;
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice - 1);
}

static void
_item_menu_priority_decrease_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info EINA_UNUSED)
{
   Proc_Info *proc = data;
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice + 1);
}

static void
_item_menu_priority_add(Evas_Object *menu, Elm_Object_Item *menu_it,
                        Proc_Info *proc)
{
   Elm_Object_Item *it;

   it = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   eina_slstr_printf("%d", proc->nice), NULL, NULL);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("increase"),
                   _("Increase"), _item_menu_priority_increase_cb, proc);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("decrease"),
                   _("Decrease"), _item_menu_priority_decrease_cb, proc);
   elm_object_item_disabled_set(it, EINA_TRUE);
}

static Evas_Object *
_item_menu_create(Ui *ui, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;
   if (!proc) return NULL;

   ui->menu = menu = elm_menu_add(ui->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed",
                   _item_menu_dismissed_cb, ui);

   stopped = !!strcmp(proc->state, "stop");

   menu_it = elm_menu_item_add(menu, NULL, evisum_icon_path_get("window"),
                   proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   _("Priority"), NULL, NULL);
   _item_menu_priority_add(menu, menu_it2, proc);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("start"),
                   _("Start"), _item_menu_start_cb, proc);

   if (stopped) elm_object_item_disabled_set(menu_it2, EINA_TRUE);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("stop"),
                   _("Stop"), _item_menu_stop_cb, proc);

   if (!stopped) elm_object_item_disabled_set(menu_it2, EINA_TRUE);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("kill"), "Kill",
                   _item_menu_kill_cb, proc);

   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("cancel"),
                   _("Cancel"), _item_menu_cancel_cb, ui);

   return menu;
}

static void
_item_pid_secondary_clicked_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                               Evas_Object *obj, void *event_info)
{
   Evas_Object *menu;
   Evas_Event_Mouse_Up *ev;
   Ui *ui;
   Elm_Object_Item *it;
   Proc_Info *proc;

   ev = event_info;
   if (ev->button != 3) return;

   it = elm_genlist_at_xy_item_get(obj, ev->output.x, ev->output.y, NULL);
   proc = elm_object_item_data_get(it);
   if (!proc) return;

   ui = data;

   menu = _item_menu_create(ui, proc);
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

   ui = data;
   it = event_info;

   elm_genlist_item_selected_set(it, EINA_FALSE);
   if (ui->menu) return;

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   ui->selected_pid = proc->pid;
   ui_process_win_add(proc->pid, proc->command);
}

static void
_ui_tab_system_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *table;
   Evas_Object *progress, *button, *plist;
   int i = 0;

   parent = ui->content;

   ui->system_activity = ui->current_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_table_pack(ui->content, ui->system_activity, 0, 1, 1, 1);

   hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, 0);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_box_pack_end(box, hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("System CPU"));
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

   ui->progress_cpu = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, FILL, FILL);
   evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   elm_progressbar_unit_format_set(progress, "%1.2f%%");
   elm_object_content_set(frame, progress);
   evas_object_show(progress);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("System Memory"));
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

   ui->progress_mem = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, FILL, FILL);
   evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, 0);
   evas_object_size_hint_align_set(table, FILL, 0);
   evas_object_show(table);

   ui->btn_pid = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_PID ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("PID"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_uid = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_UID ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("UID"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_size = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_SIZE ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Size"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_rss = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_RSS ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Res"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_cmd = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_CMD ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Command"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_state = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_STATE ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("State"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_cpu_usage = button = elm_button_add(parent);
   _btn_icon_state_set(button,
            ui->sort_type == SORT_BY_CPU_USAGE ? ui->sort_reverse : EINA_FALSE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("CPU %"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->scroller = ui->genlist_procs = plist = elm_genlist_add(parent);
   elm_scroller_gravity_set(ui->scroller, 0.0, 1.0);
   elm_object_focus_allow_set(plist, EINA_FALSE);
   elm_genlist_homogeneous_set(plist, EINA_TRUE);
   evas_object_size_hint_weight_set(plist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(plist, FILL, FILL);
   evas_object_show(plist);

   elm_box_pack_end(box, table);
   elm_box_pack_end(box, plist);

   evas_object_smart_callback_add(ui->btn_pid, "clicked",
                   _btn_pid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_uid, "clicked",
                   _btn_uid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_size, "clicked",
                   _btn_size_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_rss, "clicked",
                   _btn_rss_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cmd, "clicked",
                   _btn_cmd_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_state, "clicked",
                   _btn_state_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cpu_usage, "clicked",
                   _btn_cpu_usage_clicked_cb, ui);
   evas_object_smart_callback_add(ui->genlist_procs, "selected",
                   _item_pid_clicked_cb, ui);
   evas_object_event_callback_add(ui->genlist_procs, EVAS_CALLBACK_MOUSE_UP,
                   _item_pid_secondary_clicked_cb, ui);
   evas_object_smart_callback_add(ui->genlist_procs, "unrealized",
                   _item_unrealized_cb, ui);
}

static void
_tabs_hide(Ui *ui)
{
   ui->mem_visible = EINA_FALSE;
   ui->misc_visible = EINA_FALSE;
   ui->disk_visible = EINA_FALSE;
   ui->cpu_visible = EINA_FALSE;

   evas_object_hide(ui->entry_search);
   evas_object_hide(ui->system_activity);
   evas_object_hide(ui->cpu_view);
   evas_object_hide(ui->mem_view);
   evas_object_hide(ui->disk_view);
   evas_object_hide(ui->misc_view);
}

static void
_transit_del_cb(void *data, Elm_Transit *transit)
{
   Ui *ui = data;

   ui->transit = transit = NULL;
}

static void
_tab_state_changed(Ui *ui, Evas_Object *btn_active, Evas_Object *view)
{
   Elm_Transit *transit;

   if (ui->transit) return;

   elm_object_disabled_set(ui->btn_general, EINA_FALSE);
   elm_object_disabled_set(ui->btn_cpu, EINA_FALSE);
   elm_object_disabled_set(ui->btn_mem, EINA_FALSE);
   elm_object_disabled_set(ui->btn_storage, EINA_FALSE);
   elm_object_disabled_set(ui->btn_misc, EINA_FALSE);
   elm_object_disabled_set(btn_active, EINA_TRUE);

   _tabs_hide(ui);

   evas_object_show(view);

   ui->transit = transit = elm_transit_add();
   elm_transit_object_add(transit, ui->current_view);
   elm_transit_object_add(transit, view);
   elm_transit_duration_set(transit, 0.5);
   elm_transit_effect_blend_add(transit);
   elm_transit_del_cb_set(transit, _transit_del_cb, ui);
   elm_transit_go(transit);

   ui->current_view = view;
}

static void
_tab_memory_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   _tab_state_changed(ui, obj, ui->mem_view);

   ui->mem_visible = EINA_TRUE;
}

static void
_tab_system_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   _tab_state_changed(ui, obj, ui->system_activity);

   evas_object_show(ui->entry_search);
}

static void
_tab_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   _tab_state_changed(ui, obj, ui->disk_view);

   ui->disk_visible = EINA_TRUE;
}

static void
_tab_misc_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   _tab_state_changed(ui, obj, ui->misc_view);

   ui->misc_visible = EINA_TRUE;
}

static void
_tab_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                             void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   _tab_state_changed(ui, obj, ui->cpu_view);

   ui->cpu_visible = EINA_TRUE;
}

static void
_evisum_process_filter(Ui *ui, const char *text)
{
   if (ui->search_text)
     free(ui->search_text);

   ui->search_text = strdup(text);
}

static void
_evisum_search_keypress_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                           void *event_info)
{
   Ui * ui;
   const char *markup;
   char *text;
   Evas_Object *entry;
   Evas_Event_Key_Down *event;

   event = event_info;
   entry = obj;
   ui = data;

   if (!event) return;

   ui->skip_wait = EINA_TRUE;

   markup = elm_object_part_text_get(entry, NULL);
   text = elm_entry_markup_to_utf8(markup);
   if (text)
     {
       _evisum_process_filter(ui, text);
       free(text);
     }
}

static Evas_Object *
_ui_tabs_add(Evas_Object *parent, Ui *ui)
{
   Evas_Object *table, *box, *entry, *hbox, *frame, *button;
   Evas_Object *border;

   ui->content = table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   elm_object_content_set(parent, table);
   evas_object_show(table);

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, 0);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("Options"));
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   elm_box_pack_end(hbox, border);
   evas_object_show(border);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_general = button = elm_button_add(hbox);
   elm_object_disabled_set(ui->btn_general, EINA_TRUE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_size_hint_min_set(button,
                   elm_config_scale_get() * TAB_BTN_SIZE, 0);
   elm_object_text_set(button, _("General"));
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked",
                   _tab_system_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_cpu = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_size_hint_min_set(button,
                   elm_config_scale_get() * TAB_BTN_SIZE, 0);
   elm_object_text_set(button, _("CPU"));
   elm_object_content_set(border, button);
   evas_object_show(button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked",
                   _tab_cpu_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_mem = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_size_hint_min_set(button,
                   elm_config_scale_get() * TAB_BTN_SIZE, 0);
   elm_object_text_set(button, _("Memory"));
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked",
                   _tab_memory_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_storage = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_size_hint_min_set(button,
                   elm_config_scale_get() * TAB_BTN_SIZE, 0);
   elm_object_text_set(button, _("Storage"));
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked",
                   _tab_disk_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_misc = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, FILL, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_size_hint_min_set(button,
                   elm_config_scale_get() * TAB_BTN_SIZE, 0);
   elm_object_text_set(button, _("Misc"));
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_misc_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   elm_box_pack_end(hbox, border);
   evas_object_show(border);

   elm_object_content_set(frame, hbox);
   elm_table_pack(ui->content, frame, 0, 0, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, 0);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_box_horizontal_set(box, EINA_TRUE);
   evas_object_show(box);

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, 0);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->entry_search = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   evas_object_show(entry);
   elm_object_content_set(border, entry);
   elm_box_pack_end(box, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0.1, 0);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Close"));
   elm_object_content_set(border, button);
   elm_box_pack_end(box, border);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_quit_clicked_cb, ui);

   elm_object_content_set(frame, box);
   elm_box_pack_end(hbox, frame);
   elm_table_pack(ui->content, hbox, 0, 2, 1, 1);

   return table;
}

static void
_evisum_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Ui *ui;
   Eina_Bool control;

   ev = event_info;
   ui = data;

   if (!ev || !ev->keyname)
     return;

   ui->skip_wait = EINA_TRUE;

   if (!strcmp(ev->keyname, "Escape"))
     {
        ecore_main_loop_quit();
        return;
     }

   control = evas_key_modifier_is_set(ev->modifiers, "Control");
   if (!control) return;

   if (ev->keyname[0] == 'e' || ev->keyname[0] == 'E')
     ui->show_self = !ui->show_self;

   _config_save(ui);
}

static void
_evisum_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   elm_genlist_realized_items_update(ui->genlist_procs);

   _config_save(ui);
}

const char *
evisum_size_format(unsigned long bytes)
{
   const char *s, *unit = "BKMGTPEZY";
   unsigned long int value, powi = 1;
   unsigned int precision = 2, powj = 1;

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       if (unit[1] == '\0') break;
       powi *= 1024;
       ++unit;
     }

   if (*unit == 'B') precision = 0;

   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }

   s = eina_slstr_printf("%1.*f %c", precision, (double) value / powi, *unit);

   return s;
}

static char *
_path_append(const char *path, const char *file)
{
   char *concat;
   int len;
   char separator = '/';

   len = strlen(path) + strlen(file) + 2;
   concat = malloc(len * sizeof(char));
   snprintf(concat, len, "%s%c%s", path, separator, file);

   return concat;
}

const char *
evisum_icon_path_get(const char *name)
{
   char *path;
   const char *icon_path, *directory = PACKAGE_DATA_DIR "/images";
   icon_path = name;

   path = _path_append(directory, eina_slstr_printf("%s.png", name));
   if (path)
     {
        if (ecore_file_exists(path))
          icon_path = eina_slstr_printf("%s", path);

        free(path);
     }

   return icon_path;
}

void
evisum_ui_shutdown(Ui *ui)
{
   evas_object_del(ui->win);

   if (ui->timer_pid)
     ecore_timer_del(ui->timer_pid);

   if (ui->thread_system)
     ecore_thread_cancel(ui->thread_system);

   if (ui->thread_process)
     ecore_thread_cancel(ui->thread_process);

   if (ui->thread_system)
     ecore_thread_wait(ui->thread_system, 1.0);

   if (ui->thread_process)
     ecore_thread_wait(ui->thread_process, 1.0);

   _proc_pid_cpu_times_free(ui);

   if (ui->cpu_list)
     eina_list_free(ui->cpu_list);

   if (ui->cache)
     evisum_ui_item_cache_free(ui->cache);

   eina_lock_free(&_lock);
}

static void
_thread_end_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   thread = NULL;
}

static void
_thread_error_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   thread = NULL;
}

static void
_sys_info_all_poll(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (1)
     {
        Sys_Info *sysinfo = sys_info_all_get();
        if (!sysinfo)
          {
             ecore_main_loop_quit();
             return;
          }

        ecore_thread_feedback(thread, sysinfo);

        for (int i = 0; i < 4; i++)
          {
             if (ecore_thread_check(thread)) return;

             if (ui->skip_wait)
               {
                  ui->skip_wait = EINA_FALSE;
                  break;
               }
             usleep(250000);
          }
     }
}

static void
_sys_info_all_poll_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui;
   Evas_Object *progress;
   Sys_Info *sysinfo;
   double ratio, value, cpu_usage = 0.0;

   ui = data;
   sysinfo = msg;

   if (ecore_thread_check(thread))
     goto out;

   ui_tab_cpu_update(ui, sysinfo);
   ui_tab_memory_update(ui, sysinfo);
   ui_tab_disk_update(ui);
   ui_tab_misc_update(ui, sysinfo);

   for (int i = 0; i < sysinfo->cpu_count; i++)
     {
        cpu_usage += sysinfo->cores[i]->percent;
        free(sysinfo->cores[i]);
     }

   cpu_usage = cpu_usage / system_cpu_online_count_get();

   elm_progressbar_value_set(ui->progress_cpu, cpu_usage / 100);

   if (ui->zfs_mounted)
     sysinfo->memory.used += sysinfo->memory.zfs_arc_used;

   progress = ui->progress_mem;
   ratio = sysinfo->memory.total / 100.0;
   value = sysinfo->memory.used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s",
                   evisum_size_format(sysinfo->memory.used),
                   evisum_size_format(sysinfo->memory.total)));
out:
   free(sysinfo->cores);
   free(sysinfo);
}

static void
_ui_launch(Ui *ui)
{
   _process_list_update(ui);

   ui->thread_system =
      ecore_thread_feedback_run(_sys_info_all_poll,
                   _sys_info_all_poll_feedback_cb, _thread_end_cb,
                   _thread_error_cb, ui, EINA_FALSE);

   ui->thread_process =
      ecore_thread_feedback_run(_process_list, _process_list_feedback_cb,
                   _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);

   evas_object_event_callback_add(ui->win, EVAS_CALLBACK_RESIZE,
                    _evisum_resize_cb, ui);
   evas_object_event_callback_add(ui->content, EVAS_CALLBACK_KEY_DOWN,
                   _evisum_key_down_cb, ui);
   evas_object_event_callback_add(ui->entry_search, EVAS_CALLBACK_KEY_DOWN,
                   _evisum_search_keypress_cb, ui);
}

static Ui *
_ui_init(Evas_Object *parent)
{
   Ui *ui = calloc(1, sizeof(Ui));
   if (!ui) return NULL;

   ui->win = parent;
   ui->poll_delay = 3;
   ui->sort_reverse = EINA_FALSE;
   ui->sort_type = SORT_BY_PID;
   ui->selected_pid = -1;
   ui->program_pid = getpid();
   ui->disk_visible = ui->cpu_visible = EINA_TRUE;
   ui->mem_visible = ui->misc_visible = EINA_TRUE;
   ui->cpu_times = NULL;
   ui->cpu_list = NULL;

   ui->zfs_mounted = disk_zfs_mounted_get();

   _ui = NULL;
   _evisum_config = NULL;

   _config_load(ui);

   _ui_tabs_add(parent, ui);
   _ui_tab_system_add(ui);

   ui_tab_cpu_add(ui);
   ui_tab_memory_add(ui);
   ui_tab_disk_add(ui);
   ui_tab_misc_add(ui);

   ui->cache = evisum_ui_item_cache_new(ui->genlist_procs, _item_create, 50);

   return ui;
}

Ui *
evisum_ui_add(Evas_Object *parent)
{
   eina_lock_new(&_lock);

   Ui *ui = _ui = _ui_init(parent);
   if (!ui) return NULL;

   _ui_launch(ui);

   return ui;
}

