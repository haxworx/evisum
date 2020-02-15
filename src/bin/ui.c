#include "config.h"
#include "system.h"
#include "process.h"
#include "disks.h"
#include "ui.h"
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

#if defined(__APPLE__) && defined(__MACH__)
# define __MacOS__
#endif

static Eina_Lock _lock;

void
ui_shutdown(Ui *ui)
{
   evas_object_del(ui->win);

   ui->shutting_down = EINA_TRUE;

   if (ui->thread_system)
     {
        ecore_thread_cancel(ui->thread_system);
     }

   if (ui->thread_process)
     {
        ecore_thread_cancel(ui->thread_process);
     }

   if (ui->thread_system)
     {
        ecore_thread_wait(ui->thread_system, 1.0);
     }

   if (ui->thread_process)
     {
        ecore_thread_wait(ui->thread_process, 1.0);
     }

   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        free(ui->text_fields[i]);
     }

   eina_lock_free(&_lock);

   ecore_main_loop_quit();
}

static void
_system_stats(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (EINA_TRUE)
     {
        results_t *results = malloc(sizeof(results_t));
        system_stats_all_get(results);
        ecore_thread_feedback(thread, results);

        // Let's wait 3/4 of a second before updating.
        for (int i = 0; i < 3; i++)
          {
             if (ecore_thread_check(thread))
               return;

             if (ui->skip_wait)
               {
                  ui->skip_wait = EINA_FALSE;
                  break;
               }

             usleep(250000);
          }
     }
}

static unsigned long
_mem_adjust(Data_Unit unit, unsigned long value)
{
   if (unit == DATA_UNIT_MB)
     {
        value >>= 10;
     }
   else if (unit == DATA_UNIT_GB)
     {
        value >>= 20;
     }

   return value;
}

static char *
_network_transfer_format(double rate)
{
   const char *unit = "B/s";

   if (rate > 1048576)
     {
        rate /= 1048576;
        unit = "MB/s";
     }
   else if (rate > 1024 && rate < 1048576)
     {
        rate /= 1024;
        unit = "KB/s";
     }

   return strdup(eina_slstr_printf("%.2f %s", rate, unit));
}

char *
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
_icon_path_get(const char *name)
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

static void
_tab_misc_update(Ui *ui, results_t *results)
{
   Evas_Object *box, *frame, *progress;
   char *tmp;

   if (!ui->misc_visible)
     return;

   elm_box_clear(ui->misc_activity);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   for (int i = 0; i < results->power.battery_count; i++)
     {
        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
        evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
 
        Eina_Strbuf *buf = eina_strbuf_new();
        eina_strbuf_append_printf(buf, "Battery %s ", results->power.battery_names[i]);
        if (results->power.have_ac && i == 0)
          {
             eina_strbuf_append(buf, "(plugged in)");
          }

        elm_object_text_set(frame, eina_strbuf_string_get(buf));

        eina_strbuf_free(buf);
        evas_object_show(frame);

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.0f%%");
        elm_progressbar_value_set(progress, (double)results->power.batteries[i]->percent / 100);
        evas_object_show(progress);
        elm_object_content_set(frame, progress);
        elm_box_pack_end(box, frame);

        free(results->power.battery_names[i]);
        free(results->power.batteries[i]);
     }

   if (results->power.batteries)
     free(results->power.batteries);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Network Incoming");
   evas_object_show(frame);

   progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);

   tmp = _network_transfer_format(results->incoming);
   elm_progressbar_unit_format_set(progress, tmp);
   free(tmp);

   if (results->incoming == 0)
     elm_progressbar_value_set(progress, 0.0);
   else
     elm_progressbar_value_set(progress, 1.0);

   evas_object_show(progress);

   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Network Outgoing");
   evas_object_show(frame);

   progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   tmp = _network_transfer_format(results->outgoing);
   elm_progressbar_unit_format_set(progress, tmp);
   free(tmp);

   if (results->outgoing == 0)
     elm_progressbar_value_set(progress, 0.0);
   else
     elm_progressbar_value_set(progress, 1.0);

   evas_object_show(progress);

   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   elm_box_pack_end(ui->misc_activity, box);
}

static unsigned long
_disk_adjust(Data_Unit unit, unsigned long value)
{
   if (unit == DATA_UNIT_KB)
     {
        value >>= 10;
     }
   else if (unit == DATA_UNIT_MB)
     {
        value >>= 20;
     }
   else if (unit == DATA_UNIT_GB)
     {
        value >>= 30;
     }

   return value;
}

static void
_ui_disk_add(Ui *ui, const char *path, const char *mount, unsigned long total, unsigned long used)
{
   Evas_Object *frame, *progress;
   double ratio, value;

   frame = elm_frame_add(ui->disk_activity);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   elm_object_text_set(frame, eina_slstr_printf("%s on %s", path, mount));
   evas_object_show(frame);

   progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);

   ratio = total / 100.0;
   value = used / ratio;

   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu%c of %lu%c (%1.0f &#37;)",
                                   _disk_adjust(ui->data_unit, used), ui->data_unit,
                                   _disk_adjust(ui->data_unit, total), ui->data_unit, value));

   elm_object_content_set(frame, progress);

   if (used == 0 && total == 0)
     elm_progressbar_value_set(progress, 1.0);
   else
     elm_progressbar_value_set(progress, value / 100.0);

   evas_object_show(progress);

   elm_box_pack_end(ui->disk_activity, frame);
}

static void
_tab_disk_update(Ui *ui)
{
   Eina_List *disks;
   char *path;
   unsigned long total, used;

   if (!ui->disk_visible)
     return;

   elm_box_clear(ui->disk_activity);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        char *mount = disk_mount_point_get(path);
        if (mount)
          {
             if (disk_usage_get(mount, &total, &used))
               {
                  _ui_disk_add(ui, path, mount, total, used);
               }
             free(mount);
          }

        free(path);
     }
   if (disks)
     free(disks);
}

static void
_tab_memory_update(Ui *ui, results_t *results)
{
   Evas_Object *progress;
   double ratio, value;

   if (!ui->mem_visible)
     return;

   progress = ui->progress_mem_used;
   ratio = results->memory.total / 100.0;
   value = results->memory.used / ratio;
   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));
   elm_progressbar_value_set(progress, value / 100);

   progress = ui->progress_mem_cached;
   ratio = results->memory.total / 100.0;
   value = results->memory.cached / ratio;
   elm_progressbar_unit_format_set(progress, eina_slstr_printf(
                                   "%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.cached), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));
   elm_progressbar_value_set(progress, value / 100);

   progress = ui->progress_mem_buffered;
   ratio = results->memory.total / 100.0;
   value = results->memory.buffered / ratio;
   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.buffered), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));
   elm_progressbar_value_set(progress, value / 100);

   progress = ui->progress_mem_shared;
   ratio = results->memory.total / 100.0;
   value = results->memory.shared / ratio;
   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.shared), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));
   ratio = results->memory.total / 100.0;
   value = results->memory.shared / ratio;
   elm_progressbar_value_set(progress, value / 100);

   progress = ui->progress_mem_swap;
   ratio = results->memory.swap_total / 100.0;
   value = results->memory.swap_used / ratio;
   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.swap_used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.swap_total), ui->data_unit, value));
   elm_progressbar_value_set(progress, value / 100);
}

static void
_tab_cpu_update(Ui *ui, results_t *results)
{
   Evas_Object *box, *frame, *progress;
   int i;

   if (!ui->cpu_visible)
     return;

   elm_box_clear(ui->cpu_activity);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   for (i = 0; i < results->cpu_count; i++)
     {
        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
        evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

        if (i == 0 && results->temperature != INVALID_TEMP)
          elm_object_text_set(frame, eina_slstr_printf("CPU %d (%d °C)", i, results->temperature));
        else
          elm_object_text_set(frame, eina_slstr_printf("CPU %d", i));

        evas_object_show(frame);

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.2f%%");

        elm_progressbar_value_set(progress, results->cores[i]->percent / 100);
        evas_object_show(progress);
        elm_object_content_set(frame, progress);
        elm_box_pack_end(box, frame);
     }

   elm_box_pack_end(ui->cpu_activity, box);
}

static void
_system_stats_feedback_cb(void *data, Ecore_Thread *thread, void *msg)
{
   Ui *ui;
   Evas_Object *progress;
   results_t *results;
   double ratio, value, cpu_usage = 0.0;

   ui = data;
   results = msg;

   if (ecore_thread_check(thread))
     goto out;

   _tab_cpu_update(ui, results);
   _tab_memory_update(ui, results);
   _tab_disk_update(ui);
   _tab_misc_update(ui, results);

   for (int i = 0; i < results->cpu_count; i++)
     {
        cpu_usage += results->cores[i]->percent;

        free(results->cores[i]);
     }

   cpu_usage = cpu_usage / system_cpu_online_count_get();

   elm_progressbar_value_set(ui->progress_cpu, (double)cpu_usage / 100);

   progress = ui->progress_mem;
   elm_progressbar_unit_format_set(progress,
                                   eina_slstr_printf(
                                   "%lu %c / %lu %c",
                                   _mem_adjust(ui->data_unit, results->memory.used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit));

   ratio = results->memory.total / 100.0;
   value = results->memory.used / ratio;

   elm_progressbar_value_set(progress, value / 100);

out:
   free(results->cores);
   free(results);
}

static int
_sort_by_pid(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

static int
_sort_by_uid(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->uid - inf2->uid;
}

static int
_sort_by_nice(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->nice - inf2->nice;
}

static int
_sort_by_pri(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->priority - inf2->priority;
}

static int
_sort_by_cpu(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

static int
_sort_by_threads(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numthreads - inf2->numthreads;
}

static int
_sort_by_size(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_size;
   size2 = inf2->mem_size;

   if (size1 < size2)
     return -1;
   if (size2 > size1)
     return 1;

   return 0;
}

static int
_sort_by_rss(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_rss;
   size2 = inf2->mem_rss;

   if (size1 < size2)
     return -1;
   if (size2 > size1)
     return 1;

   return 0;
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;
   double one, two;

   inf1 = p1; inf2 = p2;

   one = inf1->cpu_usage;
   two = inf2->cpu_usage;

   if (one < two)
     return -1;
   if (two > one)
     return 1;

   return 0;
}

static int
_sort_by_cmd(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcasecmp(inf1->command, inf2->command);
}

static int
_sort_by_state(const void *p1, const void *p2)
{
   const Proc_Stats *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}

static void
_text_fields_init(Ui *ui)
{
   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        ui->text_fields[i] = malloc(TEXT_FIELD_MAX * sizeof(char));
        ui->text_fields[i][0] = '\0';
     }
}

static void
_entry_cmd_size_set(Ui *ui)
{
   static Evas_Coord minw = 0, minh = 0;
   Evas_Coord w, h, oh;

   evas_object_geometry_get(ui->btn_cmd, NULL, NULL, NULL, &h);
   evas_object_geometry_get(ui->entry_cmd, NULL, NULL, &w, &oh);

   if ((!minw && !minh) || (w > minw))
     {
        minw = w; minh = h;
        evas_object_size_hint_min_set(ui->entry_cmd, minw, oh);
        evas_object_size_hint_min_set(ui->btn_cmd, minw, minh);
        evas_object_size_hint_min_set(ui->btn_expand, minw, minh);
     }
}
static void
_text_fields_append(Ui *ui, Proc_Stats *proc)
{
   int64_t mem_size, mem_rss;

   if (ui->program_pid == proc->pid)
     return;

   if (ui->search_text && ui->search_text[0])
     {
        if (strncasecmp(proc->command, ui->search_text, strlen(ui->search_text)))
          return;
     }

   mem_size = proc->mem_size;
   mem_rss = proc->mem_rss;

   if (ui->data_unit == DATA_UNIT_KB)
     {
        mem_size >>= 10;
        mem_rss >>= 10;
     }
   else if (ui->data_unit == DATA_UNIT_MB)
     {
        mem_size >>= 20;
        mem_rss >>= 20;
     }
   else
     {
        mem_size >>= 30;
        mem_rss >>= 30;
     }

   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_PID], eina_slstr_printf("<link>%d</link> <br>", proc->pid), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_UID], eina_slstr_printf("%d <br>", proc->uid), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_SIZE], eina_slstr_printf("%lld %c<br>", mem_size, ui->data_unit), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_RSS], eina_slstr_printf("%lld %c<br>", mem_rss, ui->data_unit), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_COMMAND], eina_slstr_printf("%s<br>", proc->command), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_STATE], eina_slstr_printf("%s <br>", proc->state), TEXT_FIELD_MAX);
   eina_strlcat(ui->text_fields[PROCESS_INFO_FIELD_CPU_USAGE], eina_slstr_printf("%.1f%% <br>", proc->cpu_usage), TEXT_FIELD_MAX);
}

static void
_text_fields_show(Ui *ui)
{
   elm_object_text_set(ui->entry_pid, ui->text_fields[PROCESS_INFO_FIELD_PID]);
   elm_object_text_set(ui->entry_uid, ui->text_fields[PROCESS_INFO_FIELD_UID]);
   elm_object_text_set(ui->entry_size, ui->text_fields[PROCESS_INFO_FIELD_SIZE]);
   elm_object_text_set(ui->entry_rss, ui->text_fields[PROCESS_INFO_FIELD_RSS]);
   elm_object_text_set(ui->entry_cmd, ui->text_fields[PROCESS_INFO_FIELD_COMMAND]);
   elm_object_text_set(ui->entry_state, ui->text_fields[PROCESS_INFO_FIELD_STATE]);
   elm_object_text_set(ui->entry_cpu_usage, ui->text_fields[PROCESS_INFO_FIELD_CPU_USAGE]);
}

static void
_text_fields_clear(Ui *ui)
{
   for (int i = 0; i < PROCESS_INFO_FIELDS; i++)
     {
        ui->text_fields[i][0] = '\0';
     }
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

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED)
{
   Ui *ui;
   Eina_List *list, *l;
   Proc_Stats *proc;

   eina_lock_take(&_lock);

   ui = data;

   list = proc_info_all_get();

   EINA_LIST_FOREACH(list, l, proc)
     {
        int64_t time_prev = ui->cpu_times[proc->pid];
        proc->cpu_usage = 0;
        if (!ui->first_run && proc->cpu_time > time_prev)
          {
             proc->cpu_usage = (double)(proc->cpu_time - time_prev) / ui->poll_delay;
          }
     }

   list = _list_sort(ui, list);

   EINA_LIST_FREE(list, proc)
     {
        _text_fields_append(ui, proc);
        ui->first_run = EINA_FALSE;
        ui->cpu_times[proc->pid] = proc->cpu_time;

        free(proc);
     }

   if (list)
     eina_list_free(list);

   _entry_cmd_size_set(ui);

   _text_fields_show(ui);
   _text_fields_clear(ui);

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
   int i;

   ui = data;

   while (EINA_TRUE)
     {
        ecore_thread_feedback(thread, ui);
        for (i = 0; i < ui->poll_delay * 4; i++)
          {
             if (ecore_thread_check(thread))
               return;

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
_btn_icon_state_set(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *icon = elm_icon_add(button);
   if (reverse)
     elm_icon_standard_set(icon, _icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, _icon_path_get("go-up"));

   elm_object_part_content_set(button, "icon", icon);
   evas_object_show(icon);
}

static void
_btn_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_PID)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_pid, ui->sort_reverse);

   ui->sort_type = SORT_BY_PID;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_uid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_UID)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_uid, ui->sort_reverse);

   ui->sort_type = SORT_BY_UID;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_cpu_usage_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CPU_USAGE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cpu_usage, ui->sort_reverse);

   ui->sort_type = SORT_BY_CPU_USAGE;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_size_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_SIZE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_size, ui->sort_reverse);

   ui->sort_type = SORT_BY_SIZE;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_rss_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_RSS)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_rss, ui->sort_reverse);

   ui->sort_type = SORT_BY_RSS;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_cmd_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_CMD)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_cmd, ui->sort_reverse);

   ui->sort_type = SORT_BY_CMD;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_state_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sort_type == SORT_BY_STATE)
     ui->sort_reverse = !ui->sort_reverse;

   _btn_icon_state_set(ui->btn_state, ui->sort_reverse);

   ui->sort_type = SORT_BY_STATE;

   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_quit_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_shutdown(ui);
}

static void
_list_item_del_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   pid_t *pid = data;

   free(pid);
}

static void
_process_panel_pids_update(Ui *ui)
{
   Proc_Stats *proc;
   Elm_Widget_Item *item;
   Eina_List *list;
   pid_t *pid;

   if (!ui->panel_visible)
     return;

   list = proc_info_all_get();
   list = eina_list_sort(list, eina_list_count(list), _sort_by_pid);

   elm_list_clear(ui->list_pid);

   EINA_LIST_FREE(list, proc)
     {
        pid = malloc(sizeof(pid_t));
        *pid = proc->pid;

        item = elm_list_item_append(ui->list_pid, eina_slstr_printf("%d", proc->pid), NULL, NULL, NULL, pid);
        elm_object_item_del_cb_set(item, _list_item_del_cb);

        free(proc);
     }

   elm_list_go(ui->list_pid);

   if (list)
     eina_list_free(list);
}

static Eina_Bool
_process_panel_update(void *data)
{
   Ui *ui;
   const Eina_List *l, *list;
   Elm_Widget_Item *it;
   struct passwd *pwd_entry;
   Proc_Stats *proc;
   double cpu_usage = 0.0;

   ui = data;

   if (ui->shutting_down)
     return ECORE_CALLBACK_CANCEL;

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

   elm_object_text_set(ui->entry_pid_pid, eina_slstr_printf("%d", proc->pid));
   elm_object_text_set(ui->entry_pid_uid, eina_slstr_printf("%d", proc->uid));
   elm_object_text_set(ui->entry_pid_cpu, eina_slstr_printf("%d", proc->cpu_id));
   elm_object_text_set(ui->entry_pid_threads, eina_slstr_printf("%d", proc->numthreads));
   elm_object_text_set(ui->entry_pid_size, eina_slstr_printf("%lld bytes", proc->mem_size));
   elm_object_text_set(ui->entry_pid_rss, eina_slstr_printf("%lld bytes", proc->mem_rss));
   elm_object_text_set(ui->entry_pid_nice, eina_slstr_printf("%d", proc->nice));
   elm_object_text_set(ui->entry_pid_pri, eina_slstr_printf("%d", proc->priority));
   elm_object_text_set(ui->entry_pid_state, proc->state);

   if (ui->pid_cpu_time && proc->cpu_time >= ui->pid_cpu_time)
     {
        cpu_usage = (double)(proc->cpu_time - ui->pid_cpu_time) / ui->poll_delay;
     }

   elm_object_text_set(ui->entry_pid_cpu_usage, eina_slstr_printf("%.1f%%", cpu_usage));

   ui->pid_cpu_time = proc->cpu_time;

   free(proc);

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

   _process_panel_update(ui);

   ui->timer_pid = ecore_timer_add(ui->poll_delay, _process_panel_update, ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_panel_scrolled_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->panel_visible = !ui->panel_visible;
}

static void
_panel_toggled_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evas_object_show(ui->panel);
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

static void
_entry_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;
   Evas_Object *textblock;
   Evas_Textblock_Cursor *pos;
   const char *text;
   char *pid_text, *start, *end;

   ui = data;

   textblock = elm_entry_textblock_get(obj);
   if (!textblock)
     return;

   pos = evas_object_textblock_cursor_get(textblock);
   if (!pos)
     return;

   text = evas_textblock_cursor_paragraph_text_get(pos);
   if (!text || !text[0])
     return;

   pid_text = strdup(text);

   start = strchr(pid_text, '>') + 1;
   if (start)
     {
        end = strchr(start, '<');
        if (end)
          *end = '\0';
     }
   else
     {
        free(pid_text);
        return;
     }

   ui->selected_pid = atol(start);

   free(pid_text);

   _process_panel_update(ui);

   if (ui->timer_pid)
     {
        ecore_timer_del(ui->timer_pid);
        ui->timer_pid = NULL;
     }

   ui->timer_pid = ecore_timer_add(ui->poll_delay, _process_panel_update, ui);

   elm_panel_toggle(ui->panel);
   ui->panel_visible = EINA_TRUE;
}

static void
_ui_tab_system_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *table;
   Evas_Object *progress, *button, *entry;
   Evas_Object *scroller;

   parent = ui->content;

   ui->system_activity = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);
   elm_table_pack(ui->content, ui->system_activity, 0, 1, 1, 1);

   hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, 0);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System CPU");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->progress_cpu = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   elm_progressbar_unit_format_set(progress, "%1.2f%%");
   elm_object_content_set(frame, progress);
   evas_object_show(progress);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System Memory");
   elm_box_pack_end(hbox, frame);
   evas_object_show(frame);

   ui->progress_mem = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   elm_object_content_set(frame, progress);
   evas_object_show(progress);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, 0);
   evas_object_show(table);
   elm_box_pack_end(box, table);

   ui->btn_pid = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "PID");
   evas_object_show(button);
   elm_table_pack(table, button, 0, 0, 1, 1);

   ui->btn_uid = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "UID");
   evas_object_show(button);
   elm_table_pack(table, button, 1, 0, 1, 1);

   ui->btn_size = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Size");
   evas_object_show(button);
   elm_table_pack(table, button, 2, 0, 1, 1);

   ui->btn_rss = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Res");
   evas_object_show(button);
   elm_table_pack(table, button, 3, 0, 1, 1);

   ui->btn_cmd = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Command");
   evas_object_show(button);
   elm_table_pack(table, button, 4, 0, 1, 1);

   ui->btn_state = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "State");
   evas_object_show(button);
   elm_table_pack(table, button, 5, 0, 1, 1);

   ui->btn_cpu_usage = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "CPU %");
   evas_object_show(button);
   elm_table_pack(table, button, 6, 0, 1, 1);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_EXPAND);
   evas_object_show(table);

   ui->scroller = scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   elm_scroller_wheel_disabled_set(scroller, EINA_FALSE);
   evas_object_show(scroller);
   elm_object_content_set(scroller, table);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_small");
   elm_box_pack_end(box, frame);
   evas_object_show(frame);
   elm_object_content_set(frame, scroller);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "PID");
   elm_table_pack(table, button, 0, 0, 1, 1);

   ui->entry_pid = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=center'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_scrollable_set(entry, EINA_FALSE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_table_pack(table, entry, 0, 0, 1, 1);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "UID");
   elm_table_pack(table, button, 1, 0, 1, 1);

   ui->entry_uid = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=center'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 1, 0, 1, 1);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Size");
   elm_table_pack(table, button, 2, 0, 1, 1);

   ui->entry_size = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=right'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 2, 0, 1, 1);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Res");
   elm_table_pack(table, button, 3, 0, 1, 1);

   ui->entry_rss = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=right'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_table_pack(table, entry, 3, 0, 1, 1);

   ui->btn_expand = button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Command");
   elm_table_pack(table, button, 4, 0, 1, 1);

   ui->entry_cmd = entry = elm_entry_add(parent);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_entry_scrollable_set(entry, EINA_FALSE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(entry);
   elm_table_pack(table, entry, 4, 0, 1, 1);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "State");
   elm_table_pack(table, button, 5, 0, 1, 1);

   ui->entry_state = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=center'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   elm_entry_line_wrap_set(entry, 1);
   evas_object_show(entry);
   elm_table_pack(table, entry, 5, 0, 1, 1);

   button = elm_button_add(parent);
   _btn_icon_state_set(button, EINA_FALSE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "CPU %");
   elm_table_pack(table, button, 6, 0, 1, 1);

   ui->entry_cpu_usage = entry = elm_entry_add(parent);
   elm_entry_text_style_user_push(entry, "DEFAULT='align=center'");
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 0);
   elm_entry_editable_set(entry, 0);
   elm_entry_line_wrap_set(entry, 1);
   evas_object_show(entry);
   elm_table_pack(table, entry, 6, 0, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_box_pack_end(box, hbox);

   evas_object_smart_callback_add(ui->btn_pid, "clicked", _btn_pid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_uid, "clicked", _btn_uid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_size, "clicked", _btn_size_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_rss, "clicked", _btn_rss_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cmd, "clicked", _btn_cmd_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_state, "clicked", _btn_state_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cpu_usage, "clicked", _btn_cpu_usage_clicked_cb, ui);
   evas_object_smart_callback_add(ui->entry_pid, "clicked", _entry_pid_clicked_cb, ui);
}

static void
_ui_process_panel_add(Ui *ui)
{
   Evas_Object *parent, *panel, *hbox, *frame, *scroller, *table;
   Evas_Object *label, *list, *entry, *button, *border;

   parent = ui->content;

   ui->panel = panel = elm_panel_add(parent);
   evas_object_size_hint_weight_set(panel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_panel_orient_set(panel, ELM_PANEL_ORIENT_BOTTOM);
   elm_panel_toggle(panel);
   elm_object_content_set(ui->win, panel);
   evas_object_hide(panel);
   evas_object_smart_callback_add(ui->panel, "scroll", _panel_scrolled_cb, ui);
   evas_object_smart_callback_add(ui->panel, "toggled", _panel_toggled_cb, ui);

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
   elm_object_content_set(frame, list);
   evas_object_smart_callback_add(ui->list_pid, "selected", _process_panel_list_selected_cb, ui);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Process Statistics");
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
   elm_object_text_set(label, "Name:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 0, 1, 1);

   ui->entry_pid_cmd = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 0, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "PID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 1, 1, 1);

   ui->entry_pid_pid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 1, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Username:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 2, 1, 1);

   ui->entry_pid_user = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 2, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "UID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 3, 1, 1);

   ui->entry_pid_uid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 3, 1, 1);

   label = elm_label_add(parent);
#if defined(__MacOS__)
   elm_object_text_set(label, "WQ #:");
#else
   elm_object_text_set(label, "CPU #:");
#endif
   evas_object_show(label);
   elm_table_pack(table, label, 0, 4, 1, 1);

   ui->entry_pid_cpu = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 4, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Threads:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 5, 1, 1);

   ui->entry_pid_threads = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 5, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Total memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 6, 1, 1);

   ui->entry_pid_size = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 6, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, " Reserved memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 7, 1, 1);

   ui->entry_pid_rss = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 7, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Nice:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 8, 1, 1);

   ui->entry_pid_nice = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 8, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Priority:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 9, 1, 1);

   ui->entry_pid_pri = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 9, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "State:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 10, 1, 1);

   ui->entry_pid_state = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 10, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "CPU %:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 11, 1, 1);

   ui->entry_pid_cpu_usage = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 0);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, 1);
   elm_table_pack(table, entry, 1, 11, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);
   elm_table_pack(table, hbox, 1, 12, 1, 1);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Stop");
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _btn_stop_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, 0.5);
   elm_object_text_set(button, "Start");
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
   elm_object_text_set(button, "Kill");
   elm_box_pack_end(hbox, border);
   evas_object_show(button);
   elm_object_content_set(border, button);
   evas_object_smart_callback_add(button, "clicked", _btn_kill_clicked_cb, ui);
}

static void
_ui_tab_disk_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->disk_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->disk_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->disk_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Storage Overview");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);
}

static void
_ui_tab_misc_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->misc_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->misc_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->misc_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Misc");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);
}

static void
_ui_tab_cpu_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->cpu_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->cpu_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->cpu_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Processor Overview");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);
}

static void
_ui_tab_memory_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *progress, *scroller;

   parent = ui->content;

   ui->mem_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->mem_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->mem_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Memory Overview");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_ON);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);
   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Memory Used");
   evas_object_show(frame);
   ui->progress_mem_used = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Memory Cached");
   evas_object_show(frame);
   ui->progress_mem_cached = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Memory Buffered");
   evas_object_show(frame);
   ui->progress_mem_buffered = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Memory Shared");
   evas_object_show(frame);
   ui->progress_mem_shared = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(frame, "Memory Swapped");
   evas_object_show(frame);
   ui->progress_mem_swap = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);
   elm_box_pack_end(box, frame);

   elm_box_pack_end(ui->mem_activity, box);
}

static void
_tab_state_changed(Ui *ui, Evas_Object *btn_active)
{
   elm_object_disabled_set(ui->btn_general, EINA_FALSE);
   elm_object_disabled_set(ui->btn_cpu, EINA_FALSE);
   elm_object_disabled_set(ui->btn_mem, EINA_FALSE);
   elm_object_disabled_set(ui->btn_storage, EINA_FALSE);
   elm_object_disabled_set(ui->btn_misc, EINA_FALSE);

   elm_object_disabled_set(btn_active, EINA_TRUE);
}

static void
_tab_memory_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui->mem_visible = EINA_TRUE;
   ui->misc_visible = EINA_FALSE;
   ui->disk_visible = EINA_FALSE;
   ui->cpu_visible = EINA_FALSE;

   _tab_state_changed(ui, obj);

   evas_object_show(ui->mem_view);
   evas_object_hide(ui->entry_search);
   evas_object_hide(ui->system_activity);
   evas_object_hide(ui->panel);
   evas_object_hide(ui->disk_view);
   evas_object_hide(ui->misc_view);
   evas_object_hide(ui->cpu_view);
}

static void
_tab_system_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui->misc_visible = EINA_FALSE;
   ui->disk_visible = EINA_FALSE;
   ui->cpu_visible = EINA_FALSE;
   ui->mem_visible = EINA_FALSE;

   _tab_state_changed(ui, obj);

   evas_object_show(ui->system_activity);
   evas_object_show(ui->entry_search);
   evas_object_hide(ui->panel);
   evas_object_hide(ui->disk_view);
   evas_object_hide(ui->misc_view);
   evas_object_hide(ui->cpu_view);
   evas_object_hide(ui->mem_view);
}

static void
_tab_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui->misc_visible = EINA_FALSE;
   ui->disk_visible = EINA_TRUE;
   ui->cpu_visible = EINA_FALSE;
   ui->mem_visible = EINA_FALSE;

   _tab_state_changed(ui, obj);

   evas_object_show(ui->disk_view);
   evas_object_hide(ui->entry_search);
   evas_object_hide(ui->system_activity);
   evas_object_hide(ui->panel);
   evas_object_hide(ui->misc_view);
   evas_object_hide(ui->cpu_view);
   evas_object_hide(ui->mem_view);
}

static void
_tab_misc_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui->misc_visible = EINA_TRUE;
   ui->disk_visible = EINA_FALSE;
   ui->cpu_visible = EINA_FALSE;
   ui->mem_visible = EINA_FALSE;

   _tab_state_changed(ui, obj);

   evas_object_show(ui->misc_view);
   evas_object_hide(ui->entry_search);
   evas_object_hide(ui->system_activity);
   evas_object_hide(ui->panel);
   evas_object_hide(ui->disk_view);
   evas_object_hide(ui->cpu_view);
   evas_object_hide(ui->mem_view);
}

static void
_tab_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui->mem_visible = EINA_FALSE;
   ui->misc_visible = EINA_FALSE;
   ui->disk_visible = EINA_FALSE;
   ui->cpu_visible = EINA_TRUE;

   _tab_state_changed(ui, obj);

   evas_object_show(ui->cpu_view);
   evas_object_hide(ui->entry_search);
   evas_object_hide(ui->misc_view);
   evas_object_hide(ui->system_activity);
   evas_object_hide(ui->panel);
   evas_object_hide(ui->disk_view);
   evas_object_hide(ui->mem_view);
}

static void
_evisum_process_filter(Ui *ui, const char *text)
{
   if (ui->search_text)
     free(ui->search_text);

   ui->search_text = strdup(text);
}

static void
_evisum_search_keypress_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
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
   Evas_Object *table, *box, *entry, *hbox, *pad, *frame, *button;
   Evas_Object *border;

   ui->content = table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(parent, table);
   evas_object_show(table);

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "Options");
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_general = button = elm_button_add(hbox);
   elm_object_disabled_set(ui->btn_general, EINA_TRUE);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, TAB_BTN_SIZE * elm_config_scale_get(), 0);
   elm_object_text_set(button, "General");
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_system_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_cpu = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, TAB_BTN_SIZE * elm_config_scale_get(), 0);
   elm_object_text_set(button, "CPU");
   elm_object_content_set(border, button);
   evas_object_show(button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_cpu_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_mem = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, TAB_BTN_SIZE * elm_config_scale_get(), 0);
   elm_object_text_set(button, "Memory");
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_memory_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_storage = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, TAB_BTN_SIZE * elm_config_scale_get(), 0);
   elm_object_text_set(button, "Storage");
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_disk_activity_clicked_cb, ui);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->btn_misc = button = elm_button_add(hbox);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, TAB_BTN_SIZE * elm_config_scale_get(), 0);
   elm_object_text_set(button, "Misc");
   evas_object_show(button);
   elm_object_content_set(border, button);
   elm_box_pack_end(hbox, border);
   evas_object_smart_callback_add(button, "clicked", _tab_misc_clicked_cb, ui);

   elm_object_content_set(frame, hbox);
   elm_table_pack(ui->content, frame, 0, 0, 1, 1);

   hbox = elm_box_add(parent);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(box, EINA_TRUE);
   evas_object_show(box);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   pad = elm_box_add(parent);
   evas_object_size_hint_weight_set(pad, 0.01, 0);
   evas_object_size_hint_align_set(pad, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(pad, EINA_TRUE);
   evas_object_show(pad);
   elm_box_pack_end(box, pad);

   ui->entry_search = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_box_pack_end(box, entry);
   evas_object_event_callback_add(entry, EVAS_CALLBACK_KEY_DOWN, _evisum_search_keypress_cb, ui);
   evas_object_show(entry);

   pad = elm_box_add(parent);
   evas_object_size_hint_weight_set(pad, 0.01, 0);
   evas_object_size_hint_align_set(pad, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_horizontal_set(pad, EINA_TRUE);
   evas_object_show(pad);
   elm_box_pack_end(box, pad);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, 0.1, 0);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Close");
   elm_box_pack_end(box, button);

   elm_object_content_set(border, box);
   elm_box_pack_end(hbox, border);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", _btn_quit_clicked_cb, ui);

   elm_table_pack(ui->content, hbox, 0, 2, 1, 1);

   return table;
}

static void
_evisum_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Ui *ui;

   ev = event_info;
   ui = data;

   if (!ev || !ev->keyname)
     return;

   ui->skip_wait = EINA_TRUE;

   if ((ev->keyname[0] == 'K' || ev->keyname[0] == 'k') && !ev->keyname[1])
     {
        ui->data_unit = DATA_UNIT_KB;
     }
   else if ((ev->keyname[0] == 'M' || ev->keyname[0] == 'm') && !ev->keyname[1])
     {
        ui->data_unit = DATA_UNIT_MB;
     }
   else if ((ev->keyname[0] == 'G' || ev->keyname[0] == 'g') && !ev->keyname[1])
     {
        ui->data_unit = DATA_UNIT_GB;
     }
   else if (!strcmp(ev->keyname, "Escape"))
     {
        ui_shutdown(ui);
     }
}

Ui *
ui_add(Evas_Object *parent)
{
   eina_lock_new(&_lock);

   Ui *ui = calloc(1, sizeof(Ui));
   if (!ui) return NULL;

   ui->win = parent;
   ui->first_run = EINA_TRUE;
   ui->poll_delay = 3;
   ui->sort_reverse = EINA_FALSE;
   ui->sort_type = SORT_BY_PID;
   ui->selected_pid = -1;
   ui->program_pid = getpid();
   ui->panel_visible = EINA_TRUE;
   ui->disk_visible = ui->cpu_visible = ui->mem_visible = ui->misc_visible = EINA_TRUE;
   ui->data_unit = DATA_UNIT_MB;

   _text_fields_init(ui);

   memset(ui->cpu_times, 0, PID_MAX * sizeof(int64_t));

   /* Create the tabs, content area and the rest */
   _ui_tabs_add(parent, ui);
   _ui_tab_system_add(ui);
   _ui_process_panel_add(ui);
   _ui_tab_cpu_add(ui);
   _ui_tab_memory_add(ui);
   _ui_tab_disk_add(ui);
   _ui_tab_misc_add(ui);

   /* Start polling the data */
   _process_list_update(ui);
   _process_panel_update(ui);

   ui->thread_system  = ecore_thread_feedback_run(_system_stats, _system_stats_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);
   ui->thread_process = ecore_thread_feedback_run(_process_list, _process_list_feedback_cb, _thread_end_cb, _thread_error_cb, ui, EINA_FALSE);

   evas_object_event_callback_add(ui->content, EVAS_CALLBACK_KEY_DOWN, _evisum_key_down_cb, ui);

   return ui;
}

