#include "ui_process_view.h"
#include "../system/process.h"
#include "util.c"

typedef struct {
   int     tid;
   char   *name;
   char   *state;
   int     cpu_id;
   double  cpu_usage;
} Thread_Info;

static Thread_Info *
_thread_info_new(Proc_Info *thr, double cpu_usage)
{
   Thread_Info *t = calloc(1, sizeof(Thread_Info));
   if (!t) return NULL;

   t->tid = thr->tid;
   t->name = strdup(thr->thread_name);
   t->state = strdup(thr->state);
   t->cpu_id = thr->cpu_id;
   t->cpu_usage = cpu_usage;

   return t;
}

Eina_List *
_exe_response(const char *command)
{
   FILE *p;
   Eina_List *lines;
   char buf[8192];

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
   Ui_Process *ui;
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
   Thread_Info *t = data;

   if (t->name)
     free(t->name);
   if (t->state)
     free(t->state);
   free(t);
}

static Evas_Object *
_item_column_add(Evas_Object *table, const char *text, int col)
{
   Evas_Object *rect, *label;

   label = elm_label_add(table);
   evas_object_size_hint_align_set(label, 0.0, EXPAND);
   evas_object_size_hint_weight_set(label, FILL, FILL);
   evas_object_data_set(table, text, label);
   evas_object_show(label);

   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);

   elm_table_pack(table, label, col, 0, 1, 1);
   elm_table_pack(table, rect, col, 0, 1, 1);

   return label;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *table, *label;

   table = elm_table_add(parent);
   evas_object_size_hint_align_set(table, EXPAND, EXPAND);
   evas_object_size_hint_weight_set(table, FILL, FILL);
   evas_object_show(table);

   label = _item_column_add(table, "tid", 0);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);
   _item_column_add(table, "name", 1);
   label = _item_column_add(table, "state", 2);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);
   label = _item_column_add(table, "cpu_id", 3);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);
   label = _item_column_add(table, "cpu_usage", 4);
   evas_object_size_hint_align_set(label, 0.5, EXPAND);

   return table;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Ui_Process *ui;
   Thread_Info *th;
   Evas_Object *l, *r;
   Evas_Coord w;

   th = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!th) return NULL;
   ui = evas_object_data_get(obj, "ui");
   if (!ui) return NULL;
   if (!ui->threads_ready) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(ui->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(ui->btn_thread_id, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "tid");
   elm_object_text_set(l, eina_slstr_printf("%d", th->tid));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_thread_name, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "name");
   elm_object_text_set(l, eina_slstr_printf("%s", th->name));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_thread_state, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "state");
   elm_object_text_set(l, eina_slstr_printf("%s", th->state));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_thread_cpu_id, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "cpu_id");
   elm_object_text_set(l, eina_slstr_printf("%d", th->cpu_id));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_thread_cpu_usage, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "cpu_usage");
   elm_object_text_set(l, eina_slstr_printf("%1.1f%%", th->cpu_usage));
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
_hash_free_cb(void *data)
{
   long *cpu_time = data;
   if (cpu_time)
     free(cpu_time);
}

static void
_thread_info_set(Ui_Process *ui, Proc_Info *proc)
{
   Proc_Info *p;
   Thread_Info *t;
   Elm_Object_Item *it;
   Eina_List *l, *threads = NULL;

   if (!ui->hash_cpu_times)
     ui->hash_cpu_times = eina_hash_string_superfast_new(_hash_free_cb);

   _genlist_ensure_n_items(ui->genlist_threads, eina_list_count(proc->threads));

   EINA_LIST_FOREACH(proc->threads, l, p)
     {
        long *cpu_time, *cpu_time_prev;
        double cpu_usage = 0.0;
        const char *key = eina_slstr_printf("%s:%d", p->thread_name, p->tid);

        if ((cpu_time_prev = eina_hash_find(ui->hash_cpu_times, key)) == NULL)
          {
             cpu_time = malloc(sizeof(long));
             *cpu_time = p->cpu_time;
             eina_hash_add(ui->hash_cpu_times, key, cpu_time);
          }
        else
          {
             cpu_usage = (double) (p->cpu_time - *cpu_time_prev)
                       / ui->poll_delay;
             *cpu_time_prev = p->cpu_time;
          }

        t = _thread_info_new(p, cpu_usage);
        if (t)
          threads = eina_list_append(threads, t);
     }

   if (ui->sort_cb)
     threads = eina_list_sort(threads, eina_list_count(threads), ui->sort_cb);
   if (ui->sort_reverse)
     threads = eina_list_reverse(threads);

   it = elm_genlist_first_item_get(ui->genlist_threads);

   EINA_LIST_FREE(threads, t)
     {
        Thread_Info *prev = elm_object_item_data_get(it);
        if (prev)
          _item_del(prev, NULL);
        elm_object_item_data_set(it, t);
        elm_genlist_item_update(it);
        it = elm_genlist_item_next_get(it);
     }
}

static void
_win_title_set(Evas_Object *win, const char *fmt, const char *cmd, int pid)
{
    elm_win_title_set(win, eina_slstr_printf(fmt, cmd, pid));
}

static char *
_time_string(int64_t epoch)
{
   struct tm *info;
   time_t rawtime;
   char buf[256];

   rawtime = (time_t) epoch;
   info = localtime(&rawtime);
   strftime(buf, sizeof(buf), "%F %T", info);

   return strdup(buf);
}

static Eina_Bool
_proc_info_update(void *data)
{
   Ui_Process *ui;
   struct passwd *pwd_entry;
   Proc_Info *proc;
   double cpu_usage = 0.0;

   ui = data;

   if (!ui->timer_pid)
     ui->timer_pid = ecore_timer_add(ui->poll_delay, _proc_info_update, ui);

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc)
     {
         if (ui->timer_pid)
           ecore_timer_del(ui->timer_pid);
         ui->timer_pid = NULL;

        _win_title_set(ui->win, _("%s (%d) - Not running"), ui->selected_cmd,
                       ui->selected_pid);

        elm_object_disabled_set(ui->btn_start, EINA_TRUE);
        elm_object_disabled_set(ui->btn_stop, EINA_TRUE);
        elm_object_disabled_set(ui->btn_kill, EINA_TRUE);
        return ECORE_CALLBACK_CANCEL;
     }

   if (!strcmp(proc->state, "stop"))
     {
        elm_object_disabled_set(ui->btn_stop, EINA_TRUE);
        elm_object_disabled_set(ui->btn_start, EINA_FALSE);
     }
   else
     {
        elm_object_disabled_set(ui->btn_stop, EINA_FALSE);
        elm_object_disabled_set(ui->btn_start, EINA_TRUE);
     }

   elm_object_text_set(ui->entry_pid_cmd, proc->command);
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(ui->entry_pid_user, pwd_entry->pw_name);

   if (proc->arguments)
     elm_object_text_set(ui->entry_pid_cmd_args, proc->arguments);
   else
     elm_object_text_set(ui->entry_pid_cmd_args, "");

   elm_object_text_set(ui->entry_pid_pid, eina_slstr_printf("%d", proc->pid));
   elm_object_text_set(ui->entry_pid_uid, eina_slstr_printf("%d", proc->uid));
   elm_object_text_set(ui->entry_pid_cpu,
                   eina_slstr_printf("%d", proc->cpu_id));
   elm_object_text_set(ui->entry_pid_ppid, eina_slstr_printf("%d", proc->ppid));
   elm_object_text_set(ui->entry_pid_threads,
                   eina_slstr_printf("%d", proc->numthreads));
   elm_object_text_set(ui->entry_pid_virt, evisum_size_format(proc->mem_virt));
   elm_object_text_set(ui->entry_pid_rss, evisum_size_format(proc->mem_rss));
#if !defined(__linux__)
   elm_object_text_set(ui->entry_pid_shared, "N/A");
#else
   elm_object_text_set(ui->entry_pid_shared,
                   evisum_size_format(proc->mem_shared));
#endif
   elm_object_text_set(ui->entry_pid_size, evisum_size_format(proc->mem_size));

   char *t = _time_string(proc->start);
   if (t)
     {
        elm_object_text_set(ui->entry_pid_started, t);
        free(t);
     }
   elm_object_text_set(ui->entry_pid_nice, eina_slstr_printf("%d", proc->nice));
   elm_object_text_set(ui->entry_pid_pri,
                   eina_slstr_printf("%d", proc->priority));
   elm_object_text_set(ui->entry_pid_state, proc->state);

   if (ui->pid_cpu_time && proc->cpu_time >= ui->pid_cpu_time)
     cpu_usage = (double)(proc->cpu_time - ui->pid_cpu_time) / ui->poll_delay;

   elm_object_text_set(ui->entry_pid_cpu_usage,
                   eina_slstr_printf("%.1f%%", cpu_usage));

   ui->pid_cpu_time = proc->cpu_time;

   _thread_info_set(ui, proc);

   proc_info_free(proc);

   return ECORE_CALLBACK_RENEW;
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGKILL);
}

static Evas_Object *
_entry_add(Evas_Object *parent)
{
   Evas_Object *entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_show(entry);

   return entry;
}

static Evas_Object *
_label_add(Evas_Object *parent, const char *text)
{
   Evas_Object *label = elm_label_add(parent);
   elm_object_text_set(label, text);
   evas_object_show(label);

   return label;
}

static Evas_Object *
_process_tab_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *frame, *hbox, *table;
   Evas_Object *label, *entry, *button, *border;
   int i = 0;

   frame = elm_frame_add(parent);
   elm_object_text_set(frame, _("General"));
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_show(frame);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);
   elm_object_content_set(frame, table);

   label = _label_add(parent, _("Command:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_cmd = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Command line:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_cmd_args = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("PID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_pid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Username:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_user = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("UID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_uid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("PPID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_ppid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

#if defined(__MacOS__)
   label = _label_add(parent, _("WQ #:"));
#else
   label = _label_add(parent, _("CPU #:"));
#endif
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_cpu = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Threads:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_threads = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Memory :"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_size = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Shared memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_shared = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent,  _(" Resident memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_rss = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Virtual memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_virt = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Start time:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_started = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Nice:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_nice = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Priority:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_pri = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("State:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_state = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("CPU %:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_cpu_usage = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, 0.5, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, 0);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_table_pack(table, hbox, 0, i, 2, 1);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);
   elm_box_pack_end(hbox, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = evisum_ui_button_add(parent, &ui->btn_stop, _("Stop"),
                   _btn_stop_clicked_cb, ui);
   ui->btn_stop = button;
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = evisum_ui_button_add(parent, &ui->btn_start, _("Start"),
                   _btn_start_clicked_cb, ui);
   ui->btn_start = button;
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _btn_start_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = evisum_ui_button_add(parent, &ui->btn_kill, _("Kill"),
                   _btn_kill_clicked_cb, ui);
   ui->btn_kill = button;
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);

   return frame;
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
   evas_object_color_set(icon, 47, 153, 255, 255);

   evas_object_show(icon);
}

static void
_btn_name_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->sort_cb == _sort_by_name)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(obj, ui->sort_reverse);
   ui->sort_cb = _sort_by_name;
}

static void
_btn_thread_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->sort_cb == _sort_by_tid)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(obj, ui->sort_reverse);
   ui->sort_cb = _sort_by_tid;
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->sort_cb == _sort_by_state)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(obj, ui->sort_reverse);
   ui->sort_cb = _sort_by_state;
}

static void
_btn_cpu_id_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->sort_cb == _sort_by_cpu_id)
     ui->sort_reverse = !ui->sort_reverse;

   ui->sort_cb = _sort_by_cpu_id;
   _btn_icon_state_set(obj, ui->sort_reverse);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->sort_cb == _sort_by_cpu_usage)
     ui->sort_reverse = !ui->sort_reverse;

   ui->sort_cb = _sort_by_cpu_usage;
   _btn_icon_state_set(obj, ui->sort_reverse);
}

static Evas_Object *
_threads_tab_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *frame, *box, *hbox, *btn, *genlist;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("Threads"));

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_object_content_set(frame, box);

   hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_homogeneous_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ui->btn_thread_id = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("ID"));
   _btn_icon_state_set(btn, ui->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_thread_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->btn_thread_name = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Name"));
   _btn_icon_state_set(btn, ui->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_name_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->btn_thread_state = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("State"));
   _btn_icon_state_set(btn, ui->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_state_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->btn_thread_cpu_id = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("CPU ID"));
   _btn_icon_state_set(btn, ui->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_cpu_id_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->btn_thread_cpu_usage = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("CPU %"));
   _btn_icon_state_set(btn, ui->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked",
                   _btn_cpu_usage_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   ui->genlist_threads = genlist = elm_genlist_add(parent);
   evas_object_data_set(genlist, "ui", ui);
   elm_object_focus_allow_set(genlist, EINA_FALSE);
   elm_genlist_homogeneous_set(genlist, EINA_TRUE);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   evas_object_show(genlist);

   evas_object_smart_callback_add(ui->genlist_threads, "unrealized",
                   _item_unrealized_cb, ui);

   elm_box_pack_end(box, hbox);
   elm_box_pack_end(box, genlist);

   return frame;
}

static Evas_Object *
_info_tab_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *frame, *box, *entry;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_text_set(frame, _("Documentation"));

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_object_content_set(frame, box);

   ui->entry_info = entry = elm_entry_add(box);
   evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
   evas_object_size_hint_align_set(entry, FILL, FILL);
   elm_entry_single_line_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   evas_object_show(entry);
   elm_box_pack_end(box, entry);

   return frame;
}

static void
_hide_all(Ui_Process *ui, Evas_Object *btn)
{
   elm_object_disabled_set(ui->btn_main, EINA_FALSE);
   elm_object_disabled_set(ui->btn_info, EINA_FALSE);
   elm_object_disabled_set(ui->btn_thread, EINA_FALSE);
   elm_object_disabled_set(btn, EINA_TRUE);
   evas_object_hide(ui->main_view);
   evas_object_hide(ui->info_view);
   evas_object_hide(ui->thread_view);
}

static void
_btn_process_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Ui_Process *ui;

   ui = data;

   _hide_all(ui, obj);
   evas_object_show(ui->main_view);
}

static void
_btn_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Ui_Process *ui;

   ui = data;
   ui->threads_ready = EINA_TRUE;

   _hide_all(ui, obj);
   evas_object_show(ui->thread_view);
}

static void
_btn_info_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Process *ui;
   char *line;
   Eina_Strbuf *buf;
   int n;

   ui = data;

   _hide_all(ui, obj);
   evas_object_show(ui->info_view);

   if (ui->info_init) return;

   Eina_List *lines =
          _exe_response(eina_slstr_printf("man %s | col -b", ui->selected_cmd));
   if (lines)
     {
        buf = eina_strbuf_new();
        eina_strbuf_append(buf, "<code>");

        n = 1;
        EINA_LIST_FREE(lines, line)
          {
             if (n++ > 1)
               eina_strbuf_append_printf(buf, "%s<br>", line);
             free(line);
          }
        eina_strbuf_append(buf, "</code>");
        elm_object_text_set(ui->entry_info, eina_strbuf_string_get(buf));
        eina_strbuf_free(buf);
     }
   else
     {
        elm_object_text_set(ui->entry_info,
                        eina_slstr_printf(_("No documentation found for %s."),
                        ui->selected_cmd));
     }

   ui->info_init = EINA_TRUE;
}

static Evas_Object *
_tabs_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *hbox, *pad, *btn;

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, 0.5);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_medium");
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &ui->btn_main, _("Process"),
                   _btn_process_clicked_cb, ui);
   elm_object_disabled_set(ui->btn_main, EINA_TRUE);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &ui->btn_thread, _("Threads"),
                   _btn_threads_clicked_cb, ui);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &ui->btn_info, _("Manual"),
                   _btn_info_clicked_cb, ui);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_medium");
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);
   elm_box_pack_end(hbox, pad);

   return hbox;
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Evas_Object *win;
   Ui_Process *ui;

   ui  = data;
   win = obj;

   if (ui->hash_cpu_times)
     eina_hash_free(ui->hash_cpu_times);
   if (ui->timer_pid)
     ecore_timer_del(ui->timer_pid);
   if (ui->selected_cmd)
     free(ui->selected_cmd);
   if (ui->cache)
     evisum_ui_item_cache_free(ui->cache);

   evas_object_del(win);

   free(ui);
}
static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Process *ui = data;

   elm_genlist_realized_items_update(ui->genlist_threads);
}

void
ui_process_win_add(Evas_Object *parent_win, int pid, const char *cmd)
{
   Evas_Object *win, *ic, *box, *tabs;
   Evas_Coord x, y, w, h;

   Ui_Process *ui = calloc(1, sizeof(Ui_Process));
   ui->selected_pid = pid;
   ui->selected_cmd = strdup(cmd);
   ui->poll_delay = 3;
   ui->cache = NULL;
   ui->sort_reverse = EINA_TRUE;
   ui->sort_cb = _sort_by_cpu_usage;

   ui->win = win = elm_win_util_standard_add("evisum", "evisum");
   _win_title_set(win, "%s (%d)", cmd, pid);
   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, "evisum");
   elm_win_icon_object_set(win, ic);
   tabs = _tabs_add(win, ui);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_box_pack_end(box, tabs);

   ui->content = elm_table_add(box);
   evas_object_size_hint_weight_set(ui->content, 0.5, EXPAND);
   evas_object_size_hint_align_set(ui->content, FILL, FILL);
   evas_object_show(ui->content);

   ui->main_view = _process_tab_add(win, ui);
   ui->thread_view = _threads_tab_add(win, ui);
   ui->info_view = _info_tab_add(win, ui);

   elm_table_pack(ui->content, ui->info_view, 0, 0, 1, 1);
   elm_table_pack(ui->content, ui->main_view, 0, 0, 1, 1);
   elm_table_pack(ui->content, ui->thread_view, 0, 0, 1, 1);

   elm_box_pack_end(box, ui->content);
   elm_object_content_set(win, box);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                   _win_resize_cb, ui);

   evas_object_resize(win, 480 * elm_config_scale_get(), -1);
   evas_object_geometry_get(parent_win, &x, &y, &w, &h);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 10);
   else
     elm_win_center(win, EINA_TRUE, EINA_TRUE);

   evas_object_show(win);

   ui->cache = evisum_ui_item_cache_new(ui->genlist_threads, _item_create, 10);

   _proc_info_update(ui);
}

