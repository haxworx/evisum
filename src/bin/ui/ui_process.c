#include "ui_process.h"
#include "../system/process.h"

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

   int n = 1;
   while ((fgets(buf, sizeof(buf), p)) != NULL)
     {
	if (n > 1)
          lines = eina_list_append(lines, elm_entry_markup_to_utf8(buf));
        n++;
     }

   pclose(p);

   return lines;
}

static void
_win_title_set(Evas_Object *win, const char *fmt, const char *cmd, int pid)
{
    elm_win_title_set(win, eina_slstr_printf(fmt, cmd, pid));
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

        _win_title_set(ui->win, _("%s (%d) - Not running"), ui->selected_cmd, ui->selected_pid);

        return ECORE_CALLBACK_CANCEL;
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
   elm_object_text_set(ui->entry_pid_cpu, eina_slstr_printf("%d", proc->cpu_id));
   elm_object_text_set(ui->entry_pid_threads, eina_slstr_printf("%d", proc->numthreads));
   elm_object_text_set(ui->entry_pid_virt, evisum_size_format(proc->mem_virt));
   elm_object_text_set(ui->entry_pid_rss, evisum_size_format(proc->mem_rss));
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__MacOS__) || defined(__OpenBSD__)
   elm_object_text_set(ui->entry_pid_shared, "N/A");
#else
   elm_object_text_set(ui->entry_pid_shared, evisum_size_format(proc->mem_shared));
#endif
   elm_object_text_set(ui->entry_pid_size, evisum_size_format(proc->mem_size));
   elm_object_text_set(ui->entry_pid_nice, eina_slstr_printf("%d", proc->nice));
   elm_object_text_set(ui->entry_pid_pri, eina_slstr_printf("%d", proc->priority));
   elm_object_text_set(ui->entry_pid_state, proc->state);

   if (ui->pid_cpu_time && proc->cpu_time >= ui->pid_cpu_time)
     cpu_usage = (double)(proc->cpu_time - ui->pid_cpu_time) / ui->poll_delay;

   elm_object_text_set(ui->entry_pid_cpu_usage, eina_slstr_printf("%.1f%%", cpu_usage));

   ui->pid_cpu_time = proc->cpu_time;

   proc_info_free(proc);

   return ECORE_CALLBACK_RENEW;
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGKILL);
}

static Evas_Object *
_process_tab_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *hbox, *scroller, *table;
   Evas_Object *label, *entry, *button, *border;
   int i = 0;
 
   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ui->content);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, table);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Command:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_cmd = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Command line:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_cmd_args = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("PID:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_pid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Username:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_user = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("UID:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_uid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
#if defined(__MacOS__)
   elm_object_text_set(label, _("WQ #:"));
#else
   elm_object_text_set(label, _("CPU #:"));
#endif
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_cpu = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Threads:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_threads = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _(" Memory :"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_size = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _(" Shared memory:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_shared = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _(" Resident memory:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_rss = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _(" Virtual memory:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);
   ui->entry_pid_virt = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Nice:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_nice = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("Priority:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_pri = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("State:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_state = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, _("CPU %:"));
   evas_object_show(label);
   elm_table_pack(table, label, 0, i, 1, 1);

   ui->entry_pid_cpu_usage = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, i++, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_table_pack(table, hbox, 1, i, 2, 1);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);
   elm_box_pack_end(hbox, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, _("Stop"));
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_stop_clicked_cb, ui);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, _("Start"));
   elm_object_content_set(border, button);
   evas_object_show(button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _btn_start_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0.1);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, _("Kill"));
   elm_box_pack_end(hbox, border);
   evas_object_show(button);
   elm_object_content_set(border, button);
   evas_object_smart_callback_add(button, "clicked", _btn_kill_clicked_cb, ui);

   return scroller;
}

static Evas_Object *
_threads_tab_add(Evas_Object *parent)
{
   Evas_Object *box, *entry;

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   entry = elm_entry_add(box);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_FALSE);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, "threashdjh");
   evas_object_show(entry);

   elm_box_pack_end(box, entry);

   return box;
}


static Evas_Object *
_info_tab_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *box, *entry;

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   ui->entry_info = entry = elm_entry_add(box);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   evas_object_show(entry);
   elm_box_pack_end(box, entry);

   return box;
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
_btn_process_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui;

   ui = data;

   _hide_all(ui, obj);
   evas_object_show(ui->main_view);
}

static void
_btn_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui;

   ui = data;

   _hide_all(ui, obj);
   evas_object_show(ui->thread_view);
}

static void
_btn_info_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Process *ui;

   ui = data;

   _hide_all(ui, obj);
   evas_object_show(ui->info_view);

   if (ui->info_init) return;

   Eina_List *lines = _exe_response(eina_slstr_printf("man %s | col -b", ui->selected_cmd));
   if (lines)
     {
        Eina_Strbuf *buf = eina_strbuf_new();
        eina_strbuf_append(buf, "<code>");

        char *line;
        EINA_LIST_FREE(lines, line)
          {
             eina_strbuf_append_printf(buf, "%s<br>", line);
             free(line);
          }

        eina_list_free(lines);
        eina_strbuf_append(buf, "</code>");
        elm_object_text_set(ui->entry_info, eina_strbuf_string_get(buf));
        eina_strbuf_free(buf);
     }
   ui->info_init = EINA_TRUE;
}

static Evas_Object *
_tabs_add(Evas_Object *parent, Ui_Process *ui)
{
   Evas_Object *hbox, *btn;

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ui->btn_main = btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(btn, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(btn, _("Process"));
   elm_object_disabled_set(btn, EINA_TRUE);
   evas_object_show(btn);
   elm_box_pack_end(hbox, btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_process_clicked_cb, ui);

   ui->btn_thread = btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(btn, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(btn, _("Threads"));
   elm_object_disabled_set(btn, EINA_FALSE);
   evas_object_show(btn);
   elm_box_pack_end(hbox, btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_threads_clicked_cb, ui);

   ui->btn_info = btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(btn, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(btn, _("Information"));
   elm_object_disabled_set(btn, EINA_FALSE);
   evas_object_show(btn);
   elm_box_pack_end(hbox, btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_info_clicked_cb, ui);

   return hbox;
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win;
   Ui_Process *ui;

   ui  = data;
   win = obj;

   if (ui->timer_pid)
     ecore_timer_del(ui->timer_pid);
   if (ui->selected_cmd)
     free(ui->selected_cmd);

   evas_object_del(win);
   free(ui);
}

void
ui_process_win_add(int pid, const char *cmd)
{
   Evas_Object *win, *ic, *box, *tabs;

   Ui_Process *ui = calloc(1, sizeof(Ui_Process));
   ui->selected_pid = pid;
   ui->selected_cmd = strdup(cmd);
   ui->poll_delay = 3.0;

   ui->win = win = elm_win_util_standard_add("evisum", "evisum");
   _win_title_set(win, "%s (%d)", cmd, pid);
   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, "evisum");
   elm_win_icon_object_set(win, ic);
   tabs = _tabs_add(win, ui);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);
   elm_box_pack_end(box, tabs);

   ui->content = elm_table_add(box);
   evas_object_size_hint_weight_set(ui->content, 0.5, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ui->content, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ui->content);

   ui->main_view = _process_tab_add(win, ui);
   ui->thread_view = _threads_tab_add(win);
   ui->info_view = _info_tab_add(win, ui);

   elm_table_pack(ui->content, ui->info_view, 0, 0, 1, 1);
   elm_table_pack(ui->content, ui->main_view, 0, 0, 1, 1);
   elm_table_pack(ui->content, ui->thread_view, 0, 0, 1, 1);

   elm_box_pack_end(box, ui->content);
   elm_object_content_set(win, box);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_resize(win, 640, 480);
   evas_object_show(win);

   _proc_info_update(ui);
}

