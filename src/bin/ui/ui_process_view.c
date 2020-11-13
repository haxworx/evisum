#include "ui_process_view.h"
#include "../system/process.h"
#include "util.c"

typedef struct
{
   Evas_Object     *win;
   Evas_Object     *content;

   Evas_Object     *btn_main;
   Evas_Object     *btn_tree;
   Evas_Object     *btn_info;
   Evas_Object     *btn_thread;

   Evas_Object     *main_view;
   Evas_Object     *tree_view;
   Evas_Object     *info_view;
   Evas_Object     *thread_view;

   Evas_Object     *entry_info;

   Evas_Object     *genlist_threads;
   Evas_Object     *genlist_tree;
   Evisum_Ui_Cache *cache;

   Evas_Object     *entry_pid_cmd;
   Evas_Object     *entry_pid_cmd_args;
   Evas_Object     *entry_pid_user;
   Evas_Object     *entry_pid_pid;
   Evas_Object     *entry_pid_ppid;
   Evas_Object     *entry_pid_uid;
   Evas_Object     *entry_pid_cpu;
   Evas_Object     *entry_pid_threads;
   Evas_Object     *entry_pid_virt;
   Evas_Object     *entry_pid_rss;
   Evas_Object     *entry_pid_shared;
   Evas_Object     *entry_pid_size;
   Evas_Object     *entry_pid_started;
   Evas_Object     *entry_pid_nice;
   Evas_Object     *entry_pid_pri;
   Evas_Object     *entry_pid_state;
   Evas_Object     *entry_pid_cpu_usage;
   Evas_Object     *btn_start;
   Evas_Object     *btn_stop;
   Evas_Object     *btn_kill;

   Evas_Object     *btn_thread_id;
   Evas_Object     *btn_thread_name;
   Evas_Object     *btn_thread_state;
   Evas_Object     *btn_thread_cpu_id;
   Evas_Object     *btn_thread_cpu_usage;

   Eina_Hash       *hash_cpu_times;

   int              poll_delay;
   char            *selected_cmd;
   int              selected_pid;
   int64_t          pid_cpu_time;
   Eina_Bool        info_init;
   Eina_Bool        threads_ready;
   Eina_Bool        sort_reverse;

   int              (*sort_cb)(const void *p1, const void *p2);

   Ecore_Timer     *timer_pid;

} Ui_Data;

typedef struct
{
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
   Ui_Data *pd;
   Evas_Object *o;
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
   Ui_Data *pd;
   Thread_Info *th;
   Evas_Object *l, *r;
   Evas_Coord w;

   th = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!th) return NULL;
   pd = evas_object_data_get(obj, "ui");
   if (!pd) return NULL;
   if (!pd->threads_ready) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(pd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(pd->btn_thread_id, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "tid");
   elm_object_text_set(l, eina_slstr_printf("%d", th->tid));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(pd->btn_thread_name, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "name");
   elm_object_text_set(l, eina_slstr_printf("%s", th->name));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(pd->btn_thread_state, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "state");
   elm_object_text_set(l, eina_slstr_printf("%s", th->state));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(pd->btn_thread_cpu_id, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "cpu_id");
   elm_object_text_set(l, eina_slstr_printf("%d", th->cpu_id));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(pd->btn_thread_cpu_usage, NULL, NULL, &w, NULL);
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
_thread_info_set(Ui_Data *pd, Proc_Info *proc)
{
   Proc_Info *p;
   Thread_Info *t;
   Elm_Object_Item *it;
   Eina_List *l, *threads = NULL;

   if (!pd->hash_cpu_times)
     pd->hash_cpu_times = eina_hash_string_superfast_new(_hash_free_cb);

   _genlist_ensure_n_items(pd->genlist_threads, eina_list_count(proc->threads));

   EINA_LIST_FOREACH(proc->threads, l, p)
     {
        long *cpu_time, *cpu_time_prev;
        double cpu_usage = 0.0;
        const char *key = eina_slstr_printf("%s:%d", p->thread_name, p->tid);

        if ((cpu_time_prev = eina_hash_find(pd->hash_cpu_times, key)) == NULL)
          {
             cpu_time = malloc(sizeof(long));
             *cpu_time = p->cpu_time;
             eina_hash_add(pd->hash_cpu_times, key, cpu_time);
          }
        else
          {
             cpu_usage = (double) (p->cpu_time - *cpu_time_prev)
                       / pd->poll_delay;
             *cpu_time_prev = p->cpu_time;
          }

        t = _thread_info_new(p, cpu_usage);
        if (t)
          threads = eina_list_append(threads, t);
     }

   if (pd->sort_cb)
     threads = eina_list_sort(threads, eina_list_count(threads), pd->sort_cb);
   if (pd->sort_reverse)
     threads = eina_list_reverse(threads);

   it = elm_genlist_first_item_get(pd->genlist_threads);

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

static void
_item_tree_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ui_Data *pd;
   Elm_Object_Item *it;
   Proc_Info *proc;

   pd = data;
   it = event_info;

   elm_genlist_item_selected_set(it, EINA_FALSE);

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   ui_process_win_add(pd->win, proc->pid, proc->command, 3);
}

static char *
_tree_text_get(void *data, Evas_Object *obj, const char *part)

{
   Proc_Info *child = data;
   char buf[256];

   snprintf(buf, sizeof(buf), "%s (%d) ", child->command, child->pid);

   return strdup(buf);
}

static Evas_Object *
_tree_icon_get(void *data, Evas_Object *obj, const char *part)
{
   Proc_Info *proc;
   Evas_Object *ic = elm_icon_add(obj);
   proc = data;

   if (!strcmp(part, "elm.swallow.icon"))
     {
        elm_icon_standard_set(ic, evisum_icon_path_get(evisum_icon_cache_find(proc->command)));
     }

   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);

   return ic;
}

static void
_tree_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;

   eina_list_free(proc->children);
   proc_info_free(proc);
}

static int
_sort_by_age(const void *p1, const void *p2)
{
   const Proc_Info *c1 = p1, *c2 = p2;

   return c1->start - c2->start;
}

static void
_tree_populate(Evas_Object *genlist_tree, Elm_Object_Item *parent, Eina_List *children)
{
   Elm_Genlist_Item_Class *itc;
   Eina_List *l;
   Elm_Object_Item *it;
   Proc_Info *child;

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.content_get = _tree_icon_get;
   itc->func.text_get = _tree_text_get;
   itc->func.filter_get = NULL;
   itc->func.del = _tree_del;

   EINA_LIST_FOREACH(children, l, child)
     {
        it = elm_genlist_item_append(genlist_tree, itc, child, parent,
                      child->children ? ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE, NULL, NULL);
        elm_genlist_item_update(it);
        if (child->children)
          {
             child->children = eina_list_sort(child->children, eina_list_count(child->children), _sort_by_age);
             _tree_populate(genlist_tree, it, child->children);
          }
     }

   elm_genlist_item_class_free(itc);
}

static Eina_Bool
_tree_view_update(void *data)
{
   Eina_List *children, *l;
   Proc_Info *child;
   Ui_Data *pd = data;

   elm_genlist_clear(pd->genlist_tree);

   if (pd->selected_pid == 0) return EINA_FALSE;

   children = proc_info_pid_children_get(pd->selected_pid);
   EINA_LIST_FOREACH(children, l, child)
     {
        if (child->pid == pd->selected_pid)
          {
             child->children = eina_list_sort(child->children, eina_list_count(child->children), _sort_by_age);
             _tree_populate(pd->genlist_tree, NULL, child->children);
             break;
          }
     }

   elm_genlist_realized_items_update(pd->genlist_tree);

   child = eina_list_nth(children, 0);
   if (child)
     proc_info_free(child);

   return EINA_TRUE;
}

static Eina_Bool
_proc_info_update(void *data)
{
   Ui_Data *pd;
   struct passwd *pwd_entry;
   Proc_Info *proc;
   double cpu_usage = 0.0;

   pd = data;

   if (!pd->timer_pid)
     pd->timer_pid = ecore_timer_add(pd->poll_delay, _proc_info_update, pd);

   proc = proc_info_by_pid(pd->selected_pid);
   if (!proc)
     {
         if (pd->timer_pid)
           ecore_timer_del(pd->timer_pid);
         pd->timer_pid = NULL;

        _win_title_set(pd->win, _("%s (%d) - Not running"), pd->selected_cmd,
                       pd->selected_pid);

        elm_object_disabled_set(pd->btn_start, EINA_TRUE);
        elm_object_disabled_set(pd->btn_stop, EINA_TRUE);
        elm_object_disabled_set(pd->btn_kill, EINA_TRUE);
        return ECORE_CALLBACK_CANCEL;
     }

   if (!strcmp(proc->state, "stop"))
     {
        elm_object_disabled_set(pd->btn_stop, EINA_TRUE);
        elm_object_disabled_set(pd->btn_start, EINA_FALSE);
     }
   else
     {
        elm_object_disabled_set(pd->btn_stop, EINA_FALSE);
        elm_object_disabled_set(pd->btn_start, EINA_TRUE);
     }

   elm_object_text_set(pd->entry_pid_cmd, proc->command);
   pwd_entry = getpwuid(proc->uid);
   if (pwd_entry)
     elm_object_text_set(pd->entry_pid_user, pwd_entry->pw_name);

   if (proc->arguments)
     elm_object_text_set(pd->entry_pid_cmd_args, proc->arguments);
   else
     elm_object_text_set(pd->entry_pid_cmd_args, "");

   elm_object_text_set(pd->entry_pid_pid, eina_slstr_printf("%d", proc->pid));
   elm_object_text_set(pd->entry_pid_uid, eina_slstr_printf("%d", proc->uid));
   elm_object_text_set(pd->entry_pid_cpu,
                   eina_slstr_printf("%d", proc->cpu_id));
   elm_object_text_set(pd->entry_pid_ppid, eina_slstr_printf("%d", proc->ppid));
   elm_object_text_set(pd->entry_pid_threads,
                   eina_slstr_printf("%d", proc->numthreads));
   elm_object_text_set(pd->entry_pid_virt, evisum_size_format(proc->mem_virt));
   elm_object_text_set(pd->entry_pid_rss, evisum_size_format(proc->mem_rss));
#if !defined(__linux__)
   elm_object_text_set(pd->entry_pid_shared, "N/A");
#else
   elm_object_text_set(pd->entry_pid_shared,
                   evisum_size_format(proc->mem_shared));
#endif
   elm_object_text_set(pd->entry_pid_size, evisum_size_format(proc->mem_size));

   char *t = _time_string(proc->start);
   if (t)
     {
        elm_object_text_set(pd->entry_pid_started, t);
        free(t);
     }
   elm_object_text_set(pd->entry_pid_nice, eina_slstr_printf("%d", proc->nice));
   elm_object_text_set(pd->entry_pid_pri,
                   eina_slstr_printf("%d", proc->priority));
   elm_object_text_set(pd->entry_pid_state, proc->state);

   if (pd->pid_cpu_time && proc->cpu_time >= pd->pid_cpu_time)
     cpu_usage = (double)(proc->cpu_time - pd->pid_cpu_time) / pd->poll_delay;

   elm_object_text_set(pd->entry_pid_cpu_usage,
                   eina_slstr_printf("%.1f%%", cpu_usage));

   pd->pid_cpu_time = proc->cpu_time;

   _thread_info_set(pd, proc);

   proc_info_free(proc);

   return ECORE_CALLBACK_RENEW;
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->selected_pid == -1)
     return;

   kill(pd->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->selected_pid == -1)
     return;

   kill(pd->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

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
_process_tab_add(Evas_Object *parent, Ui_Data *pd)
{
   Evas_Object *frame, *hbox, *table;
   Evas_Object *label, *entry, *button, *border;
   int i = 0;
   int r, g, b, a;

   frame = elm_frame_add(parent);
   //elm_object_text_set(frame, _("General"));
   elm_object_style_set(frame, "pad_small");
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_show(frame);

   if (evisum_ui_effects_enabled_get())
     {
        evas_object_color_get(frame, &r, &g, &b, &a);
        evas_object_color_set(frame, r * 0.75, g * 0.75, b * 0.75, a * 0.75);
     }

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);
   elm_object_content_set(frame, table);

   label = _label_add(parent, _("Command:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_cmd = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Command line:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_cmd_args = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("PID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_pid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Username:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_user = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("UID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_uid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("PPID:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_ppid = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

#if defined(__MacOS__)
   label = _label_add(parent, _("WQ #:"));
#else
   label = _label_add(parent, _("CPU #:"));
#endif
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_cpu = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Threads:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_threads = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Memory :"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_size = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Shared memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_shared = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent,  _(" Resident memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_rss = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Virtual memory:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_virt = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _(" Start time:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_started = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Nice:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_nice = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("Priority:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_pri = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("State:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_state = entry = _entry_add(parent);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = _label_add(parent, _("CPU %:"));
   elm_table_pack(table, label, 0, i, 1, 1);
   pd->entry_pid_cpu_usage = entry = _entry_add(parent);
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

   button = evisum_ui_tab_add(parent, &pd->btn_stop, _("Stop"),
                   _btn_stop_clicked_cb, pd);
   pd->btn_stop = button;
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = evisum_ui_tab_add(parent, &pd->btn_start, _("Start"),
                   _btn_start_clicked_cb, pd);
   pd->btn_start = button;
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _btn_start_clicked_cb, pd);

   border = elm_frame_add(parent);
   evas_object_size_hint_align_set(border, FILL, FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = evisum_ui_tab_add(parent, &pd->btn_kill, _("Kill"),
                   _btn_kill_clicked_cb, pd);
   pd->btn_kill = button;
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
   evas_object_color_set(icon, 255, 255, 255, 255);

   evas_object_show(icon);
}

static void
_btn_name_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->sort_cb == _sort_by_name)
     pd->sort_reverse = !pd->sort_reverse;

   _btn_icon_state_set(obj, pd->sort_reverse);
   pd->sort_cb = _sort_by_name;
}

static void
_btn_thread_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->sort_cb == _sort_by_tid)
     pd->sort_reverse = !pd->sort_reverse;

   _btn_icon_state_set(obj, pd->sort_reverse);
   pd->sort_cb = _sort_by_tid;
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->sort_cb == _sort_by_state)
     pd->sort_reverse = !pd->sort_reverse;

   _btn_icon_state_set(obj, pd->sort_reverse);
   pd->sort_cb = _sort_by_state;
}

static void
_btn_cpu_id_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->sort_cb == _sort_by_cpu_id)
     pd->sort_reverse = !pd->sort_reverse;

   pd->sort_cb = _sort_by_cpu_id;
   _btn_icon_state_set(obj, pd->sort_reverse);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                          void *event_info EINA_UNUSED)
{
   Ui_Data *pd = data;

   if (pd->sort_cb == _sort_by_cpu_usage)
     pd->sort_reverse = !pd->sort_reverse;

   pd->sort_cb = _sort_by_cpu_usage;
   _btn_icon_state_set(obj, pd->sort_reverse);
}

static Evas_Object *
_threads_tab_add(Evas_Object *parent, Ui_Data *pd)
{
   Evas_Object *frame, *box, *hbox, *btn, *genlist;
   int r, g, b, a;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");

   if (evisum_ui_effects_enabled_get())
     {
        evas_object_color_get(frame, &r, &g, &b, &a);
        evas_object_color_set(frame, r * 0.75, g * 0.75, b * 0.75, a * 0.75);
     }

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

   pd->btn_thread_id = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("ID"));
   _btn_icon_state_set(btn, pd->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_thread_clicked_cb, pd);
   elm_box_pack_end(hbox, btn);

   pd->btn_thread_name = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Name"));
   _btn_icon_state_set(btn, pd->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_name_clicked_cb, pd);
   elm_box_pack_end(hbox, btn);

   pd->btn_thread_state = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("State"));
   _btn_icon_state_set(btn, pd->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_state_clicked_cb, pd);
   elm_box_pack_end(hbox, btn);

   pd->btn_thread_cpu_id = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("CPU ID"));
   _btn_icon_state_set(btn, pd->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_cpu_id_clicked_cb, pd);
   elm_box_pack_end(hbox, btn);

   pd->btn_thread_cpu_usage = btn = elm_button_add(hbox);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("CPU %"));
   _btn_icon_state_set(btn, pd->sort_reverse);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked",
                   _btn_cpu_usage_clicked_cb, pd);
   elm_box_pack_end(hbox, btn);

   pd->genlist_threads = genlist = elm_genlist_add(parent);
   evas_object_data_set(genlist, "ui", pd);
   elm_object_focus_allow_set(genlist, EINA_FALSE);
   elm_genlist_homogeneous_set(genlist, EINA_TRUE);
   elm_genlist_select_mode_set(genlist, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   evas_object_show(genlist);

   evas_object_smart_callback_add(pd->genlist_threads, "unrealized",
                   _item_unrealized_cb, pd);

   elm_box_pack_end(box, hbox);
   elm_box_pack_end(box, genlist);

   return frame;
}

static Evas_Object *
_tree_tab_add(Evas_Object *parent, Ui_Data *pd)
{
   Evas_Object *frame, *box, *genlist;
   int r, g, b, a;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");

   if (evisum_ui_effects_enabled_get())
     {
        evas_object_color_get(frame, &r, &g, &b, &a);
        evas_object_color_set(frame, r * 0.75, g * 0.75, b * 0.75, a * 0.75);
     }

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_object_content_set(frame, box);

   pd->genlist_tree = genlist = elm_genlist_add(parent);
   evas_object_data_set(genlist, "ui", pd);
   elm_object_focus_allow_set(genlist, EINA_FALSE);
   elm_genlist_homogeneous_set(genlist, EINA_TRUE);
   elm_genlist_select_mode_set(genlist, ELM_OBJECT_SELECT_MODE_DEFAULT);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   evas_object_show(genlist);
   evas_object_smart_callback_add(genlist, "selected",
                   _item_tree_clicked_cb, pd);

   elm_box_pack_end(box, genlist);

   return frame;
}

static Evas_Object *
_info_tab_add(Evas_Object *parent, Ui_Data *pd)
{
   Evas_Object *frame, *box, *entry;
   int r, g, b, a;

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");

   if (evisum_ui_effects_enabled_get())
     {
        evas_object_color_get(frame, &r, &g, &b, &a);
        evas_object_color_set(frame, r * 0.75, g * 0.75, b * 0.75, a * 0.75);
     }

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_object_content_set(frame, box);

   pd->entry_info = entry = elm_entry_add(box);
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
_hide_all(Ui_Data *pd, Evas_Object *btn)
{
   elm_object_disabled_set(pd->btn_main, EINA_FALSE);
   elm_object_disabled_set(pd->btn_info, EINA_FALSE);
   elm_object_disabled_set(pd->btn_thread, EINA_FALSE);
   elm_object_disabled_set(pd->btn_tree, EINA_FALSE);
   elm_object_disabled_set(btn, EINA_TRUE);
   evas_object_hide(pd->main_view);
   evas_object_hide(pd->tree_view);
   evas_object_hide(pd->info_view);
   evas_object_hide(pd->thread_view);
}

static void
_btn_process_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Ui_Data *pd;

   pd = data;

   _hide_all(pd, obj);
   evas_object_show(pd->main_view);
}

static void
_btn_tree_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd;

   pd = data;

   _tree_view_update(pd);
   _hide_all(pd, obj);
   evas_object_show(pd->tree_view);
}

static void
_btn_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Ui_Data *pd;

   pd = data;
   pd->threads_ready = EINA_TRUE;

   _hide_all(pd, obj);
   evas_object_show(pd->thread_view);
}

static void
_btn_info_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Ui_Data *pd;
   Eina_List *lines = NULL;

   pd = data;

   _hide_all(pd, obj);
   evas_object_show(pd->info_view);

   if (pd->info_init) return;

   if (pd->selected_cmd && pd->selected_cmd[0] && !strchr(pd->selected_cmd, ' '))
     lines =_exe_response(eina_slstr_printf("man %s | col -b", pd->selected_cmd));

   if (!lines)
     {
        // LAZY!!!
        if (!strcmp(pd->selected_cmd, "evisum"))
          elm_object_text_set(pd->entry_info, _evisum_docs());
        else
          {
             elm_object_text_set(pd->entry_info,
                                 eina_slstr_printf(_("No documentation found for %s."),
                                 pd->selected_cmd));
          }
     }
   else
     {
        char *line;
        int n = 1;
        Eina_Strbuf *buf = eina_strbuf_new();

        eina_strbuf_append(buf, "<code>");

        n = 1;
        EINA_LIST_FREE(lines, line)
          {
             if (n++ > 1)
               eina_strbuf_append_printf(buf, "%s<br>", line);
             free(line);
          }
        eina_strbuf_append(buf, "</code>");
        elm_object_text_set(pd->entry_info, eina_strbuf_string_get(buf));
        eina_strbuf_free(buf);
     }

   pd->info_init = EINA_TRUE;
}

static Evas_Object *
_tabs_add(Evas_Object *parent, Ui_Data *pd)
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

   btn = evisum_ui_tab_add(parent, &pd->btn_main, _("Process"),
                   _btn_process_clicked_cb, pd);
   elm_object_disabled_set(pd->btn_main, EINA_TRUE);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->btn_tree, _("Children"),
                   _btn_tree_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->btn_thread, _("Threads"),
                   _btn_threads_clicked_cb, pd);
   elm_object_content_set(pad, btn);
   elm_box_pack_end(hbox, pad);

   pad = elm_frame_add(parent);
   elm_object_style_set(pad, "pad_small");
   evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   evas_object_show(pad);

   btn = evisum_ui_tab_add(parent, &pd->btn_info, _("Manual"),
                   _btn_info_clicked_cb, pd);
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
   Ui_Data *pd;

   pd  = data;
   win = obj;

   if (pd->hash_cpu_times)
     eina_hash_free(pd->hash_cpu_times);
   if (pd->timer_pid)
     ecore_timer_del(pd->timer_pid);
   if (pd->selected_cmd)
     free(pd->selected_cmd);
   if (pd->cache)
     evisum_ui_item_cache_free(pd->cache);

   evas_object_del(win);

   free(pd);
}
static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd = data;

   elm_genlist_realized_items_update(pd->genlist_threads);
}

void
ui_process_win_add(Evas_Object *parent_win, int pid, const char *cmd, int poll_delay)
{
   Evas_Object *win, *ic, *box, *tabs;
   Evas_Coord x, y, w, h;

   Ui_Data *pd = calloc(1, sizeof(Ui_Data));
   pd->selected_pid = pid;
   pd->selected_cmd = strdup(cmd);
   pd->poll_delay = poll_delay;
   pd->cache = NULL;
   pd->sort_reverse = EINA_TRUE;
   pd->sort_cb = _sort_by_cpu_usage;

   pd->win = win = elm_win_util_standard_add("evisum", "evisum");
   _win_title_set(win, "%s (%d)", cmd, pid);
   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, "evisum");
   elm_win_icon_object_set(win, ic);
   tabs = _tabs_add(win, pd);

   evisum_ui_background_random_add(win, evisum_ui_effects_enabled_get());

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);
   elm_box_pack_end(box, tabs);

   pd->content = elm_table_add(box);
   evas_object_size_hint_weight_set(pd->content, 0.5, EXPAND);
   evas_object_size_hint_align_set(pd->content, FILL, FILL);
   evas_object_show(pd->content);

   pd->main_view = _process_tab_add(win, pd);
   pd->tree_view = _tree_tab_add(win, pd);
   pd->thread_view = _threads_tab_add(win, pd);
   pd->info_view = _info_tab_add(win, pd);

   elm_table_pack(pd->content, pd->info_view, 0, 0, 1, 1);
   elm_table_pack(pd->content, pd->tree_view, 0, 0, 1, 1);
   elm_table_pack(pd->content, pd->main_view, 0, 0, 1, 1);
   elm_table_pack(pd->content, pd->thread_view, 0, 0, 1, 1);

   elm_box_pack_end(box, pd->content);
   elm_object_content_set(win, box);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                   _win_resize_cb, pd);

   evas_object_resize(win, 480 * elm_config_scale_get(), -1);
   if (parent_win)
     evas_object_geometry_get(parent_win, &x, &y, &w, &h);
   if (parent_win && x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 10);
   else
     elm_win_center(win, EINA_TRUE, EINA_TRUE);

   evas_object_show(win);

   pd->cache = evisum_ui_item_cache_new(pd->genlist_threads, _item_create, 10);

   _proc_info_update(pd);
}

