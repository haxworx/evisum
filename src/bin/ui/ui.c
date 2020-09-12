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
   _evisum_config->effects = evisum_ui_effects_enabled_get();
   _evisum_config->poll_delay = ui->poll_delay;

   config_save(_evisum_config);
}

static void
_config_load(Ui *ui)
{
   _evisum_config   = config_load();

   ui->sort_type    = _evisum_config->sort_type;
   ui->sort_reverse = _evisum_config->sort_reverse;
   ui->poll_delay   = _evisum_config->poll_delay;

   if ((_evisum_config->width > 0) && (_evisum_config->height > 0))
     evas_object_resize(ui->win, _evisum_config->width, _evisum_config->height);

   evisum_ui_effects_enabled_set(_evisum_config->effects);
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
}

static void
_proc_pid_cpu_times_reset(Ui *ui)
{
   Eina_List *l;
   pid_cpu_time_t *tmp;
   EINA_LIST_FOREACH(ui->cpu_times, l, tmp)
     tmp->cpu_time_prev = 0;
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

   label = _item_column_add(table, "proc_pid", 0);
   evas_object_size_hint_align_set(label, 1.0, EXPAND);
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
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_uid, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_uid");
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(l, eina_slstr_printf("%s%s", TEXT_PAD, pwd_entry->pw_name));
   else
     elm_object_text_set(l, eina_slstr_printf("%d", proc->uid));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_uid, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_size, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_size");
   elm_object_text_set(l, evisum_size_format(proc->mem_size));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_size, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_rss, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_rss");
   elm_object_text_set(l, evisum_size_format(proc->mem_rss));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_rss, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_cmd, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_cmd");
   elm_object_text_set(l, eina_slstr_printf("%s%s", TEXT_PAD, proc->command));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_cmd, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_state, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_state");
   elm_object_text_set(l, eina_slstr_printf("%s", proc->state));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_state, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(ui->btn_cpu_usage, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_cpu_usage");
   elm_object_text_set(l, eina_slstr_printf("%.1f %%", proc->cpu_usage));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_cpu_usage, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

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
   Ui *ui = data;
   elm_genlist_realized_items_update(ui->genlist_procs);

   return EINA_FALSE;
}

static Eina_Bool
_bring_in(void *data)
{
   Ui *ui;
   int h_page, v_page;

   ui = data;
   elm_scroller_gravity_set(ui->scroller, 0.0, 0.0);
   elm_scroller_last_page_get(ui->scroller, &h_page, &v_page);
   elm_scroller_page_bring_in(ui->scroller, h_page, v_page);

   ecore_timer_add(0.5, _show_items, ui);

   return EINA_FALSE;
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

   if (!eina_lock_take_try(&_lock))
     return;

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

   elm_genlist_realized_items_update(ui->genlist_procs);

   eina_lock_release(&_lock);
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

Evas_Object *_selected;

static void
_btn_icon_state_update(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *icon = elm_icon_add(button);

   if (_selected)
     evas_object_color_set(_selected, 255, 255, 255, 255);

   if (reverse)
     elm_icon_standard_set(icon, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, evisum_icon_path_get("go-up"));

   _selected = icon;
   evas_object_color_set(_selected, 255, 255, 255, 255);

   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
}

static void
_btn_icon_state_init(Evas_Object *button, Eina_Bool reverse, Eina_Bool selected)
{
   Evas_Object *icon = elm_icon_add(button);

   if (reverse)
     elm_icon_standard_set(icon, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, evisum_icon_path_get("go-up"));

   if (!selected)
     evas_object_color_set(icon, 255, 255, 255, 255);
   else
     {
        _selected = icon;
        evas_object_color_set(icon, 255, 255, 255, 255);
     }

   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
}

static void
_btn_clicked_state_save(Ui *ui, Evas_Object *btn)
{
   _btn_icon_state_update(btn, ui->sort_reverse);

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PID)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_PID;

   _btn_clicked_state_save(ui, ui->btn_pid);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_UID)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_UID;

   _btn_clicked_state_save(ui, ui->btn_uid);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_CPU_USAGE;

   _btn_clicked_state_save(ui, ui->btn_cpu_usage);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_SIZE)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_SIZE;

   _btn_clicked_state_save(ui, ui->btn_size);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_RSS)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_RSS;

   _btn_clicked_state_save(ui, ui->btn_rss);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CMD)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_CMD;

   _btn_clicked_state_save(ui, ui->btn_cmd);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_STATE)
     ui->sort_reverse = !ui->sort_reverse;
   ui->sort_type = SORT_BY_STATE;

   _btn_clicked_state_save(ui, ui->btn_state);
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
   Ui *ui = data;

   kill(ui->selected_pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   kill(ui->selected_pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   kill(ui->selected_pid, SIGKILL);
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
   Ui *ui;
   Proc_Info *proc;

   ui = data;

   _item_menu_cancel_cb(ui, NULL, NULL);

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice - 1);

   proc_info_free(proc);
}

static void
_item_menu_priority_decrease_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                void *event_info EINA_UNUSED)
{
   Ui *ui;
   Proc_Info *proc;

   ui = data;

   _item_menu_cancel_cb(ui, NULL, NULL);

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice + 1);

   proc_info_free(proc);
}

static void
_item_menu_debug_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Ui *ui;
   Proc_Info *proc;
   const char *terminal = "xterm";

   ui = data;

   _item_menu_cancel_cb(ui, NULL, NULL);

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc) return;

   if (ecore_file_app_installed("terminology"))
     terminal = "terminology";

   ecore_exe_run(eina_slstr_printf("%s -e gdb attach %d", terminal, proc->pid), NULL);

   proc_info_free(proc);
}

static void
_item_menu_priority_add(Evas_Object *menu, Elm_Object_Item *menu_it,
                        Ui *ui)
{
   Elm_Object_Item *it;
   Proc_Info *proc;

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc) return;

   it = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   eina_slstr_printf("%d", proc->nice), NULL, NULL);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("increase"),
                   _("Increase"), _item_menu_priority_increase_cb, ui);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("decrease"),
                   _("Decrease"), _item_menu_priority_decrease_cb, ui);
   elm_object_item_disabled_set(it, EINA_TRUE);
   proc_info_free(proc);
}

static void
_item_menu_actions_add(Evas_Object *menu, Elm_Object_Item *menu_it,
                       Ui *ui)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("bug"),
                   _("Debug"), _item_menu_debug_cb, ui);
}

static void
_item_menu_properties_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Ui *ui;
   Proc_Info *proc;

   ui = data;

   _item_menu_cancel_cb(ui, NULL, NULL);

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc) return;

   ui_process_win_add(ui->win, proc->pid, proc->command, ui->poll_delay);

   proc_info_free(proc);
}

static Evas_Object *
_item_menu_create(Ui *ui, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;

   if (!proc) return NULL;

   ui->selected_pid = proc->pid;

   ui->menu = menu = elm_menu_add(ui->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed",
                   _item_menu_dismissed_cb, ui);

   stopped = !(!strcmp(proc->state, "stop"));

   menu_it = elm_menu_item_add(menu, NULL, evisum_icon_path_get("window"),
                   proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   _("Actions"), NULL, NULL);
   _item_menu_actions_add(menu, menu_it2, ui);
   elm_menu_item_separator_add(menu, menu_it);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   _("Priority"), NULL, NULL);
   _item_menu_priority_add(menu, menu_it2, ui);
   elm_menu_item_separator_add(menu, menu_it);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("start"),
                   _("Start"), _item_menu_start_cb, ui);

   elm_object_item_disabled_set(menu_it2, stopped);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("stop"),
                   _("Stop"), _item_menu_stop_cb, ui);

   elm_object_item_disabled_set(menu_it2, !stopped);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("kill"), "Kill",
                   _item_menu_kill_cb, ui);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("window"),
                   _("Properties"), _item_menu_properties_cb, ui);

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
   ui_process_win_add(ui->win, proc->pid, proc->command, ui->poll_delay);
}

static void
_about_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evisum_about_window_show(ui);
}

static void
_menu_memory_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                 void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_memory_add(ui);
}

static void
_menu_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_disk_add(ui);
}

static void
_menu_misc_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_misc_add(ui);
}

static void
_menu_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_cpu_add(ui);
}

static void
_menu_effects_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evisum_ui_effects_enabled_set(!evisum_ui_effects_enabled_get());

   _config_save(ui);
   ecore_app_restart();
   ecore_main_loop_quit();
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
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb, void *data)
{
   Evas_Object *btn, *ic;

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, 24 * elm_config_scale_get(), 24 * elm_config_scale_get());
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);

   elm_object_tooltip_text_set(btn, text);
   evas_object_smart_callback_add(btn, "clicked", cb, data);

   return btn;
}

static void
_main_menu_slider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->poll_delay = elm_slider_value_get(obj) + 0.5;

   if (ui->poll_delay > 1)
     elm_slider_unit_format_set(obj, _("%1.0f secs"));
   else
     elm_slider_unit_format_set(obj, _("%1.0f sec"));

   _config_save(ui);

   _proc_pid_cpu_times_reset(ui);
}

static void
_main_menu_create(Ui *ui, Evas_Object *btn)
{
   Evas_Object *o, *bx, *hbox, *sep, *fr, *sli;
   Evas_Coord ox, oy, ow, oh;

   evas_object_geometry_get(btn, &ox, &oy, &ow, &oh);
   o = elm_ctxpopup_add(ui->win);
   evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
   evas_object_size_hint_align_set(o, FILL, FILL);
   elm_object_style_set(o, "noblock");

   bx = elm_box_add(o);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   fr = elm_frame_add(o);
   elm_object_text_set(fr, _("Options"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_show(fr);

   hbox = elm_box_add(o);
   elm_box_horizontal_set(hbox, 1);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_show(hbox);

   btn = _btn_create(hbox, "cpu", _("CPU"), _menu_cpu_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "memory", _("Memory"), _menu_memory_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "storage", _("Storage"), _menu_disk_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "misc", _("Misc"), _menu_misc_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_box_pack_end(hbox, sep);

   btn = _btn_create(hbox, "effects", _("Effects"), _menu_effects_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_box_pack_end(hbox, sep);

   btn = _btn_create(hbox, "evisum", _("About"), _about_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   sli = elm_slider_add(o);
   evas_object_size_hint_weight_set(sli, EXPAND, EXPAND);
   elm_slider_min_max_set(sli, 1.0, 10.0);
   elm_slider_span_size_set(sli, 10.0);
   elm_slider_step_set(sli, 1 / 10.0);
   elm_slider_indicator_format_set(sli, "%1.0f");
   elm_slider_unit_format_set(sli, _("%1.0f secs"));
   elm_slider_value_set(sli, ui->poll_delay);
   evas_object_size_hint_align_set(sli, FILL, FILL);
   elm_object_tooltip_text_set(sli, _("Poll delay (seconds)"));
   evas_object_smart_callback_add(sli, "slider,drag,stop", _main_menu_slider_changed_cb, ui);
   evas_object_smart_callback_add(sli, "changed", _main_menu_slider_changed_cb, ui);
   evas_object_show(sli);
   _main_menu_slider_changed_cb(ui, sli, NULL);

   elm_box_pack_end(bx, hbox);
   elm_box_pack_end(bx, sli);

   evas_object_size_hint_min_set(fr, 100, 100);
   elm_object_content_set(fr, bx);
   elm_object_content_set(o, fr);

   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_UP, ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_LEFT, ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy);
   evas_object_show(o);
   ui->main_menu = o;
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (!ui->main_menu)
     _main_menu_create(ui, obj);
   else
     _main_menu_dismissed_cb(ui, NULL, NULL);
}

static void
_ui_content_system_add(Ui *ui)
{
   Evas_Object *parent, *box, *box2, *hbox, *frame, *table;
   Evas_Object *entry, *pb, *button, *plist, *btn;
   int i = 0;

   parent = ui->content;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("System Overview"));
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   ui->system_activity = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_object_content_set(frame, box);
   elm_table_pack(ui->content, frame, 0, 1, 1, 1);

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
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

   ui->progress_cpu = pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   elm_progressbar_unit_format_set(pb, "%1.2f %%");
   elm_object_content_set(frame, pb);
   evas_object_show(pb);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("System Memory"));
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

   ui->progress_mem = pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   evas_object_show(pb);
   elm_object_content_set(frame, pb);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, 0);
   evas_object_size_hint_align_set(table, FILL, 0);
   evas_object_show(table);

   ui->btn_pid = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_PID ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_PID);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("PID"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_uid = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_UID ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_UID);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("User"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_size = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_SIZE ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_SIZE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Size"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_rss = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_RSS ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_RSS);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Res"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_cmd = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_CMD ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_CMD);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("Command"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_state = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_STATE ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_STATE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("State"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->btn_cpu_usage = button = elm_button_add(parent);
   _btn_icon_state_init(button,
            ui->sort_type == SORT_BY_CPU_USAGE ? ui->sort_reverse : EINA_FALSE,
            ui->sort_type == SORT_BY_CPU_USAGE);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   elm_object_text_set(button, _("CPU %"));
   evas_object_show(button);
   elm_table_pack(table, button, i++, 0, 1, 1);

   ui->scroller = ui->genlist_procs = plist = elm_genlist_add(parent);
   elm_scroller_gravity_set(ui->scroller, 0.0, 1.0);
   elm_object_focus_allow_set(plist, EINA_FALSE);
   elm_scroller_movement_block_set(ui->scroller, ELM_SCROLLER_MOVEMENT_BLOCK_HORIZONTAL);
   elm_scroller_policy_set(ui->scroller, ELM_SCROLLER_POLICY_OFF,
                   ELM_SCROLLER_POLICY_AUTO);
   elm_genlist_homogeneous_set(plist, EINA_TRUE);
   elm_genlist_multi_select_set(plist, EINA_FALSE);
   evas_object_size_hint_weight_set(plist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(plist, FILL, FILL);
   evas_object_show(plist);
   elm_win_resize_object_add(ui->win, plist);

   box2 = elm_box_add(parent);
   evas_object_size_hint_weight_set(box2, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box2, FILL, FILL);
   evas_object_show(box2);

   elm_box_pack_end(box2, table);
   elm_box_pack_end(box2, plist);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_box_pack_end(box2, hbox);

   btn = _btn_create(hbox, "menu", NULL, _btn_menu_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->entry_search = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   evas_object_show(entry);
   elm_box_pack_end(hbox, entry);

   elm_box_pack_end(ui->system_activity, box2);

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
_ui_content_add(Evas_Object *parent, Ui *ui)
{
   Evas_Object *table;

   ui->content = table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   elm_object_content_set(parent, table);
   evas_object_show(table);

   _ui_content_system_add(ui);

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
   if (!control)
     {
        elm_object_focus_set(ui->entry_search, EINA_TRUE);
        return;
     }

   if (ev->keyname[0] == 'e' || ev->keyname[0] == 'E')
     ui->show_self = !ui->show_self;

   if (ev->keyname[0] == 'k' || ev->keyname[0] == 'K')
     proc_info_kthreads_show_set(!proc_info_kthreads_show_get());

   _config_save(ui);
}

static void
_evisum_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   if (eina_lock_take_try(&_lock))
     {
        elm_genlist_realized_items_update(ui->genlist_procs);
        eina_lock_release(&_lock);
     }

   if (ui->main_menu)
     _main_menu_dismissed_cb(ui, NULL, NULL);

   _config_save(ui);
}

void
evisum_ui_shutdown(Ui *ui)
{
   if (ui->shutdown_now)
     exit(0);

   if (ui->win_cpu)
     evas_object_smart_callback_call(ui->win_cpu, "delete,request", NULL);
   if (ui->win_mem)
     evas_object_smart_callback_call(ui->win_mem, "delete,request", NULL);
   if (ui->win_disk)
     evas_object_smart_callback_call(ui->win_disk, "delete,request", NULL);
   if (ui->win_misc)
     evas_object_smart_callback_call(ui->win_misc, "delete,request", NULL);
   if (ui->win_about)
     evas_object_smart_callback_call(ui->win_about, "delete,request", NULL);

   ecore_main_loop_quit();
}

void
evisum_ui_del(Ui *ui)
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

   if (ui->cache)
     evisum_ui_item_cache_free(ui->cache);

   eina_lock_free(&_lock);

   free(ui);
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
_system_info_all_poll(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (1)
     {
        Sys_Info *info = system_info_all_get();
        if (!info)
          {
             ecore_main_loop_quit();
             return;
          }
        ecore_thread_feedback(thread, info);
        for (int i = 0; i < 4 * ui->poll_delay; i++)
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
_system_info_all_poll_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui;
   Evas_Object *pb;
   Sys_Info *info;
   double ratio, value, cpu_usage = 0.0;
   int Hz;

   ui = data;
   info = msg;

   if (ecore_thread_check(thread))
     goto out;

   for (int i = 0; i < info->cpu_count; i++)
     cpu_usage += info->cores[i]->percent;

   cpu_usage = cpu_usage / system_cpu_online_count_get();

   elm_progressbar_value_set(ui->progress_cpu, cpu_usage / 100);
   Hz = system_cpu_frequency_get();
   if (Hz != -1)
     {
        if (Hz > 1000000)
          elm_object_tooltip_text_set(ui->progress_cpu, eina_slstr_printf("%1.1f GHz", (double) Hz / 1000000.0));
        else
          elm_object_tooltip_text_set(ui->progress_cpu, eina_slstr_printf("%d MHz",  Hz / 1000));
     }

   ui->cpu_usage = cpu_usage;

   if (ui->zfs_mounted)
     info->memory.used += info->memory.zfs_arc_used;

   pb = ui->progress_mem;
   ratio = info->memory.total / 100.0;
   value = info->memory.used / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb, eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.used),
                   evisum_size_format(info->memory.total)));

   ui->network_usage = info->network_usage;
out:
   system_info_all_free(info);
}

static Eina_Bool
_elm_config_change_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ui *ui = data;

   elm_genlist_clear(ui->genlist_procs);
   _process_list_update(ui);

   return EINA_TRUE;
}

static void
_ui_launch(Ui *ui)
{
   _process_list_update(ui);

   ecore_timer_add(2.0, _bring_in, ui);
   elm_object_focus_set(ui->entry_search, EINA_TRUE);

   ui->thread_system =
      ecore_thread_feedback_run(_system_info_all_poll, _system_info_all_poll_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ui->thread_process =
      ecore_thread_feedback_run(_process_list, _process_list_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);

   evas_object_event_callback_add(ui->win, EVAS_CALLBACK_RESIZE,  _evisum_resize_cb, ui);
   evas_object_event_callback_add(ui->content, EVAS_CALLBACK_KEY_DOWN, _evisum_key_down_cb, ui);
   evas_object_event_callback_add(ui->entry_search, EVAS_CALLBACK_KEY_DOWN, _evisum_search_keypress_cb, ui);
   ecore_event_handler_add(ELM_EVENT_CONFIG_ALL_CHANGED, _elm_config_change_cb, ui);
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
   ui->cpu_times = NULL;
   ui->cpu_list = NULL;

   ui->zfs_mounted = file_system_in_use("ZFS");

   _ui = NULL;
   _evisum_config = NULL;
   _selected = NULL;

   _config_load(ui);

   if (evisum_ui_effects_enabled_get())
     evisum_ui_background_random_add(ui->win, 1);

   _ui_content_add(parent, ui);

   if (evisum_ui_effects_enabled_get())
     evisum_ui_animate(ui);

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

