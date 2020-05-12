#include "ui_process.h"
#include "../system/process.h"

static void
_list_item_del_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   pid_t *pid = data;

   free(pid);
}

static int
_sort_by_pid(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

static void
_process_panel_pids_update(Ui *ui)
{
   Proc_Info *proc;
   Elm_Widget_Item *item;
   Eina_List *list;
   pid_t *pid;

   if (!ui->panel_visible)
     return;

   elm_list_clear(ui->list_pid);

   list = proc_info_all_get();
   list = eina_list_sort(list, eina_list_count(list), _sort_by_pid);

   EINA_LIST_FREE(list, proc)
     {
        pid = malloc(sizeof(pid_t));
        *pid = proc->pid;

        item = elm_list_item_append(ui->list_pid, eina_slstr_printf("%d", proc->pid), NULL, NULL, NULL, pid);
        elm_object_item_del_cb_set(item, _list_item_del_cb);
        proc_info_free(proc);
     }

   elm_list_go(ui->list_pid);

   if (list)
     eina_list_free(list);
}

Eina_Bool
ui_process_panel_update(void *data)
{
   Ui *ui;
   const Eina_List *l, *list;
   Elm_Widget_Item *it;
   struct passwd *pwd_entry;
   Proc_Info *proc;
   double cpu_usage = 0.0;

   ui = data;

   proc = proc_info_by_pid(ui->selected_pid);
   if (!proc)
     {
        _process_panel_pids_update(ui);
        return ECORE_CALLBACK_CANCEL;
     }

   list = elm_list_items_get(ui->list_pid);
   EINA_LIST_FOREACH(list, l, it)
     {
        pid_t *pid = elm_object_item_data_get(it);
        if (pid && *pid == ui->selected_pid)
          {
             elm_list_item_selected_set(it, EINA_TRUE);
             elm_list_item_bring_in(it);
             break;
          }
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
_process_panel_list_selected_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Elm_Object_Item *it;
   Ui *ui;
   const char *text;

   ui = data;

   it = elm_list_selected_item_get(obj);
   text = elm_object_item_text_get(it);

   if (ui->timer_pid)
     {
        ecore_timer_del(ui->timer_pid);
        ui->timer_pid = NULL;
     }

   ui->selected_pid = atoi(text);
   ui->pid_cpu_time = 0;

   ui_process_panel_update(ui);

   ui->timer_pid = ecore_timer_add(ui->poll_delay, ui_process_panel_update, ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_panel_scrolled_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->panel_visible = !ui->panel_visible;
}

static void
_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGCONT);
}

static void
_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGSTOP);
}

static void
_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->selected_pid == -1)
     return;

   kill(ui->selected_pid, SIGKILL);
}

void
ui_process_panel_add(Ui *ui)
{
   Evas_Object *parent, *panel, *hbox, *frame, *scroller, *table;
   Evas_Object *label, *list, *entry, *button, *border;
   int i = 0;

   parent = ui->content;

   ui->panel = panel = elm_panel_add(parent);
   evas_object_size_hint_weight_set(panel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_panel_orient_set(panel, ELM_PANEL_ORIENT_BOTTOM);
   elm_panel_toggle(panel);
   elm_object_content_set(ui->win, panel);
   evas_object_hide(panel);
   evas_object_smart_callback_add(ui->panel, "scroll", _panel_scrolled_cb, ui);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_object_content_set(panel, hbox);
   evas_object_show(hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, 0.2, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "PID");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->list_pid = list = elm_list_add(frame);
   evas_object_size_hint_weight_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_gravity_set(list, 0.5, 0.0);
   evas_object_show(list);
   evas_object_smart_callback_add(ui->list_pid, "selected", _process_panel_list_selected_cb, ui);
   elm_object_content_set(frame, list);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, _("Process Statistics"));
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   table = elm_table_add(frame);
   evas_object_size_hint_weight_set(table, 0.5, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, table);
   elm_object_content_set(frame, scroller);
   elm_box_pack_end(hbox, frame);

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
   elm_table_pack(table, hbox, 1, i, 1, 1);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
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
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, _("Start"));
   elm_object_content_set(border, button);
   evas_object_show(button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _btn_start_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, _("Kill"));
   elm_box_pack_end(hbox, border);
   evas_object_show(button);
   elm_object_content_set(border, button);
   evas_object_smart_callback_add(button, "clicked", _btn_kill_clicked_cb, ui);
}

