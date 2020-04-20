#include "config.h"
#include "configuration.h"
#include "ui.h"
#include "system.h"
#include "process.h"
#include "disks.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

#if defined(__APPLE__) && defined(__MACH__)
# define __MacOS__
#endif

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
   _evisum_config->data_unit    = ui->data_unit;
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
   ui->data_unit    = _evisum_config->data_unit == 0 ? DATA_UNIT_MB : _evisum_config->data_unit;

   if (_evisum_config->width > 0 && _evisum_config->height > 0)
     {
        evas_object_resize(ui->win, _evisum_config->width, _evisum_config->height);
     }
}

static void
_system_stats(void *data, Ecore_Thread *thread)
{
   Ui *ui = data;

   while (1)
     {
        results_t *results = system_stats_get();
        if (!results)
          {
             ui_shutdown(ui);
             return;
          }

        ecore_thread_feedback(thread, results);

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

static unsigned long
_mem_adjust(Data_Unit unit, unsigned long value)
{
   if (unit == DATA_UNIT_MB)
     value >>= 10;
   else if (unit == DATA_UNIT_GB)
     value >>= 20;

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
_battery_list_add(Evas_Object *box, power_t *power)
{
   Evas_Object *frame, *vbox, *hbox, *progress, *ic, *label;
   for (int i = 0; i < power->battery_count; i++)
     {
        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_object_style_set(frame, "pad_small");
        evas_object_show(frame);

        vbox = elm_box_add(box);
        evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(vbox);

        label = elm_label_add(box);
        evas_object_size_hint_align_set(label, 1.0, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(label);
        elm_box_pack_end(vbox, label);

        Eina_Strbuf *buf = eina_strbuf_new();
        if (buf)
          {
             eina_strbuf_append_printf(buf, "<bigger>%s ", power->battery_names[i]);
             if (power->have_ac && i == 0)
               {
                    eina_strbuf_append(buf, "(plugged in)");
               }
             eina_strbuf_append(buf, "</bigger>");
             elm_object_text_set(label, eina_strbuf_string_get(buf));
             eina_strbuf_free(buf);
          }

        hbox = elm_box_add(box);
        evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_box_horizontal_set(hbox, EINA_TRUE);
        evas_object_show(hbox);

        ic = elm_image_add(box);
        elm_image_file_set(ic, _icon_path_get("battery"), NULL);
        evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(), 32 * elm_config_scale_get());
        evas_object_show(ic);
        elm_box_pack_end(hbox, ic);

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.0f%%");
        elm_progressbar_value_set(progress, (double) power->batteries[i]->percent / 100);
        evas_object_show(progress);

        elm_box_pack_end(hbox, progress);
        elm_box_pack_end(vbox, hbox);
        elm_object_content_set(frame, vbox);
        elm_box_pack_end(box, frame);

        free(power->battery_names[i]);
        free(power->batteries[i]);
     }

   if (power->batteries)
     free(power->batteries);
}

static void
_tab_misc_update(Ui *ui, results_t *results)
{
   Evas_Object *box, *hbox, *vbox, *frame, *ic, *progress;
   Evas_Object *label;

   if (!ui->misc_visible)
     return;

   elm_box_clear(ui->misc_activity);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   _battery_list_add(box, &results->power);

   vbox = elm_box_add(box);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(vbox);

   label = elm_label_add(box);
   elm_object_text_set(label, "<bigger>Network Incoming</bigger>");
   evas_object_size_hint_align_set(label, 1.0, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(label);
   elm_box_pack_end(vbox, label);

   hbox = elm_box_add(box);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ic = elm_image_add(box);
   elm_image_file_set(ic, _icon_path_get("network"), NULL);
   evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(), 32 * elm_config_scale_get());
   evas_object_show(ic);
   elm_box_pack_end(hbox, ic);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_box_pack_end(hbox, progress);
   elm_box_pack_end(vbox, hbox);

   char *tmp = _network_transfer_format(results->incoming);
   if (tmp)
     {
        elm_progressbar_unit_format_set(progress, tmp);
        free(tmp);
     }

   if (results->incoming)
     {
        if (ui->incoming_max < results->incoming)
          ui->incoming_max = results->incoming;
        elm_progressbar_value_set(progress, (double) results->incoming / ui->incoming_max);
     }

   elm_box_pack_end(box, vbox);

   vbox = elm_box_add(box);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(vbox);

   label = elm_label_add(box);
   elm_object_text_set(label, "<bigger>Network Outgoing</bigger>");
   evas_object_size_hint_align_set(label, 1.0, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(label);
   elm_box_pack_end(vbox, label);

   hbox = elm_box_add(box);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ic = elm_image_add(box);
   elm_image_file_set(ic, _icon_path_get("network"), NULL);
   evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(), 32 * elm_config_scale_get());
   evas_object_show(ic);
   elm_box_pack_end(hbox, ic);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   tmp = _network_transfer_format(results->outgoing);
   if (tmp)
     {
        elm_progressbar_unit_format_set(progress, tmp);
        free(tmp);
     }

   if (results->outgoing)
     {
        if (ui->outgoing_max < results->outgoing)
          ui->outgoing_max = results->outgoing;
        elm_progressbar_value_set(progress, (double) results->outgoing / ui->outgoing_max);
     }

   elm_box_pack_end(hbox, progress);
   elm_box_pack_end(vbox, hbox);
   elm_box_pack_end(box, vbox);

   frame = elm_frame_add(ui->misc_activity);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);
   elm_object_content_set(frame, box);

   elm_box_pack_end(ui->misc_activity, frame);
}

static unsigned long
_disk_adjust(Data_Unit unit, unsigned long value)
{
   if (unit == DATA_UNIT_KB)
     value >>= 10;
   else if (unit == DATA_UNIT_MB)
     value >>= 20;
   else if (unit == DATA_UNIT_GB)
     value >>= 30;

   return value;
}

static void
_ui_disk_add(Ui *ui, const char *path, const char *mount, unsigned long total, unsigned long used)
{
   Evas_Object *box, *progress, *label;
   double ratio, value;

   box = elm_box_add(ui->disk_activity);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   label = elm_label_add(box);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(label, eina_slstr_printf("<subtitle>%s</subtitle><br><bigger>mounted at %s</bigger><br>%lu%c of %lu%c", path, mount,
                       _disk_adjust(ui->data_unit, used), ui->data_unit, _disk_adjust(ui->data_unit, total), ui->data_unit));
   evas_object_show(label);
   elm_box_pack_end(box, label);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   ratio = total / 100.0;
   value = used / ratio;

   if (used == 0 && total == 0)
     elm_progressbar_value_set(progress, 1.0);
   else
     elm_progressbar_value_set(progress, value / 100.0);

   elm_box_pack_end(box, progress);
   elm_box_pack_end(ui->disk_activity, box);
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

   elm_object_text_set(ui->title_mem, eina_slstr_printf("<subtitle>Memory</subtitle><br>" \
                       "<bigger>Physical %lu %c</bigger><br>" \
                       "Swap %lu %c",
                       _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit,
                       _mem_adjust(ui->data_unit, results->memory.swap_total), ui->data_unit));

   progress = ui->progress_mem_used;
   ratio = results->memory.total / 100.0;
   value = results->memory.used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));

   progress = ui->progress_mem_cached;
   ratio = results->memory.total / 100.0;
   value = results->memory.cached / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.cached), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));

   progress = ui->progress_mem_buffered;
   ratio = results->memory.total / 100.0;
   value = results->memory.buffered / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.buffered), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));

   progress = ui->progress_mem_shared;
   ratio = results->memory.total / 100.0;
   value = results->memory.shared / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.shared), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit, value));

   progress = ui->progress_mem_swap;
   ratio = results->memory.swap_total / 100.0;
   value = results->memory.swap_used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c (%1.0f &#37;)",
                                   _mem_adjust(ui->data_unit, results->memory.swap_used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.swap_total), ui->data_unit, value));
}

static void
_tab_cpu_update(Ui *ui, results_t *results)
{
   Evas_Object *box, *frame, *label, *progress;

   if (!ui->cpu_visible)
     return;

   elm_box_clear(ui->cpu_activity);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   for (int i = 0; i < results->cpu_count; i++)
     {
        if (i == 0)
          {
             frame = elm_frame_add(box);
             evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
             evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_show(frame);
             elm_object_style_set(frame, "pad_large");

             label = elm_label_add(box);
             evas_object_size_hint_align_set(label, EVAS_HINT_FILL, 0);
             evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_show(label);

             if (results->temperature != INVALID_TEMP)
               elm_object_text_set(label, eina_slstr_printf("<subtitle>CPUs</subtitle><br><bigger>Total of %d CPUs</bigger><br><header>Core at (%d °C)</header>", results->cpu_count, results->temperature));
             else
               elm_object_text_set(label, eina_slstr_printf("<subtitle>CPUs</subtitle><br><bigger>Total of %d CPUs</bigger>", results->cpu_count));
             elm_box_pack_end(box, frame);
             elm_box_pack_end(box, label);
          }

        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
        evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(frame);
        elm_object_style_set(frame, "pad_large");

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.2f%%");
        evas_object_show(progress);

        elm_progressbar_value_set(progress, results->cores[i]->percent / 100);
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

   elm_progressbar_value_set(ui->progress_cpu, cpu_usage / 100);

   progress = ui->progress_mem;
   ratio = results->memory.total / 100.0;
   value = results->memory.used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%lu %c / %lu %c",
                                   _mem_adjust(ui->data_unit, results->memory.used), ui->data_unit,
                                   _mem_adjust(ui->data_unit, results->memory.total), ui->data_unit));
out:
   free(results->cores);
   free(results);
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

   if (size1 < size2)
     return -1;
   if (size2 > size1)
     return 1;

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

   if (size1 < size2)
     return -1;
   if (size2 > size1)
     return 1;

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

   if (one < two)
     return -1;
   if (two > one)
     return 1;

   return 0;
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
                  proc->cpu_usage = (double) (proc->cpu_time - tmp->cpu_time_prev) /
                     ui->poll_delay;
               }
             _proc_pid_cpu_time_save(ui, proc);
             return;
          }
     }

   _proc_pid_cpu_time_save(ui, proc);
}

#define ITEM_CACHE_INIT_SIZE 50

typedef struct _Item_Cache {
   Evas_Object *obj;
   Eina_Bool   used;
} Item_Cache;

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;
   Item_Cache *it;
   Evas_Object *o;
   Eina_List *l, *contents = NULL;

   ui = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);
   EINA_LIST_FREE(contents, o)
    {
       EINA_LIST_FOREACH(ui->item_cache, l, it)
         {
            if (it->obj == o)
              {
                 it->used = EINA_FALSE;
                 break;
              }
          }
    }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;
   free(proc);
   proc = NULL;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *obj, *label;
   Evas_Object *table, *rect;

   obj = parent;

   table = elm_table_add(obj);
   evas_object_size_hint_align_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   label = elm_label_add(obj);
   evas_object_data_set(table, "proc_pid", label);
   evas_object_size_hint_align_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, rect, 0, 0, 1, 1);
   elm_table_pack(table, label, 0, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_uid", label);
   evas_object_size_hint_align_set(label, 1.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, rect, 1, 0, 1, 1);
   elm_table_pack(table, label, 1, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_size", label);
   evas_object_size_hint_align_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, rect, 2, 0, 1, 1);
   elm_table_pack(table, label, 2, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_rss", label);
   evas_object_size_hint_align_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, rect, 3, 0, 1, 1);
   elm_table_pack(table, label, 3, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_cmd", label);
   evas_object_size_hint_align_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, rect, 4, 0, 1, 1);
   elm_table_pack(table, label, 4, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_state", label);
   evas_object_size_hint_align_set(label, 0.5, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, label, 5, 0, 1, 1);
   elm_table_pack(table, rect, 5, 0, 1, 1);

   label = elm_label_add(table);
   evas_object_data_set(table, "proc_cpu_usage", label);
   evas_object_size_hint_align_set(label, 0.5, EVAS_HINT_EXPAND);
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(label);
   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);
   elm_table_pack(table, label, 6, 0, 1, 1);
   elm_table_pack(table, rect, 6, 0, 1, 1);

   return table;
}

static void
_item_cache_init(Ui *ui)
{
   for (int i = 0; i < ITEM_CACHE_INIT_SIZE; i++)
     {
        Item_Cache *it = calloc(1, sizeof(Item_Cache));
        if (it)
          {
             it->obj = _item_create(ui->genlist_procs);
             ui->item_cache = eina_list_append(ui->item_cache, it);
          }
     }
}

static Item_Cache *
_item_cache_get(Ui *ui)
{
   Eina_List *l;
   Item_Cache *it;

   EINA_LIST_FOREACH(ui->item_cache, l, it)
     {
        if (it->used == 0)
          {
             it->used = 1;
             return it;
          }
     }

   it = calloc(1, sizeof(Item_Cache));
   if (it)
     {
        it->obj = _item_create(ui->genlist_procs);
        it->used = 1;
        ui->item_cache = eina_list_append(ui->item_cache, it);
     }
   return it;
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

   Item_Cache *it = _item_cache_get(ui);
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
   elm_object_text_set(l, eina_slstr_printf("%lu %c ", _mem_adjust(ui->data_unit, proc->mem_size >> 10), ui->data_unit));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_size, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_rss, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_rss");
   elm_object_text_set(l, eina_slstr_printf("%lu %c ", _mem_adjust(ui->data_unit, proc->mem_rss >> 10), ui->data_unit));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(ui->btn_rss, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);

   evas_object_geometry_get(ui->btn_cmd, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "proc_cmd");
   elm_object_text_set(l, eina_slstr_printf("%s", proc->command));
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
        elm_genlist_item_append(genlist, itc, NULL, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_item_class_free(itc);
}

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED)
{
   Ui *ui;
   Eina_List *list, *l, *l_next;
   Proc_Info *proc;
   Elm_Object_Item *it;

   ui = data;

   if (ui->shutting_down) return;

   eina_lock_take(&_lock);

   list = proc_info_all_get();

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
       if ((ui->search_text && ui->search_text[0] &&
           strncasecmp(proc->command, ui->search_text, strlen(ui->search_text))) ||
           (!ui->show_self && proc->pid == ui->program_pid))
         {
            free(proc);
            list = eina_list_remove_list(list, l);
         }
        else
         {
            _proc_pid_cpu_usage_get(ui, proc);
         }
     }

   if (ui->shutting_down)
     {
        eina_lock_release(&_lock);
        return;
     }

   _genlist_ensure_n_items(ui->genlist_procs, eina_list_count(list));

   it = elm_genlist_first_item_get(ui->genlist_procs);

   list = _list_sort(ui, list);
   EINA_LIST_FREE(list, proc)
     {
        elm_object_item_data_set(it, proc);
        elm_genlist_item_update(it);
        it = elm_genlist_item_next_get(it);
     }

   if (list)
     eina_list_free(list);

   eina_lock_release(&_lock);
}

static void
_process_list_update(Ui *ui)
{
   _process_list_feedback_cb(ui, NULL, NULL);
}

#define POLL_ONE_SEC 4

static void
_process_list(void *data, Ecore_Thread *thread)
{
   Ui *ui;
   int duration, delay;

   delay = 1;
   duration = POLL_ONE_SEC / 2;

   ui = data;

   while (1)
     {
        ecore_thread_feedback(thread, ui);
        for (int i = 0; i < delay * duration; i++)
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
        if (ui->ready)
          {
             delay = ui->poll_delay;
             duration = POLL_ONE_SEC;
          }
        else
          {
             delay = 1;
             duration = POLL_ONE_SEC / 2;
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

   _config_save(ui);
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

   _config_save(ui);
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

   _config_save(ui);
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

   _config_save(ui);
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

   _config_save(ui);
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

   _config_save(ui);
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

   _config_save(ui);
   _process_list_update(ui);

   elm_scroller_page_bring_in(ui->scroller, 0, 0);
}

static void
_btn_quit_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   exit(0);
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
   Proc_Info *proc;
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
     cpu_usage = (double)(proc->cpu_time - ui->pid_cpu_time) / ui->poll_delay;

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
_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj, void *ev EINA_UNUSED)
{
   Ui *ui = data;

   evas_object_del(obj);

   ui->menu = NULL;
}

static void
_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Info *proc;

   proc = data;
   if (!proc) return;

   kill(proc->pid, SIGKILL);
}

static void
_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   elm_menu_close(ui->menu);
   ui->menu = NULL;
}

static void
_item_menu_priority_increase_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Info *proc = data;
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice - 1);
}

static void
_item_menu_priority_decrease_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Proc_Info *proc = data;
   if (!proc) return;

   setpriority(PRIO_PROCESS, proc->pid, proc->nice + 1);
}

static void
_item_menu_priority_add(Evas_Object *menu, Elm_Object_Item *menu_it, Proc_Info *proc)
{
   Elm_Object_Item *it;

   it = elm_menu_item_add(menu, menu_it, _icon_path_get("window"), eina_slstr_printf("%d", proc->nice), NULL, NULL);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, _icon_path_get("increase"), "Increase", _item_menu_priority_increase_cb, proc);
   elm_menu_item_add(menu, menu_it, _icon_path_get("decrease"), "Decrease", _item_menu_priority_decrease_cb, proc);
   elm_object_item_disabled_set(it, EINA_TRUE);
}

static
Evas_Object *
_item_menu_create(Ui *ui, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;
   if (!proc) return NULL;

   ui->menu = menu = elm_menu_add(ui->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed", _item_menu_dismissed_cb, ui);

   stopped = !!strcmp(proc->state, "stop");

   menu_it = elm_menu_item_add(menu, NULL, _icon_path_get("window"), proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, _icon_path_get("window"), "Priority", NULL, NULL);
   _item_menu_priority_add(menu, menu_it2, proc);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, _icon_path_get("start"), "Start", _item_menu_start_cb, proc);
   if (stopped) elm_object_item_disabled_set(menu_it2, EINA_TRUE);
   menu_it2 = elm_menu_item_add(menu, menu_it, _icon_path_get("stop"), "Stop", _item_menu_stop_cb, proc);
   if (!stopped) elm_object_item_disabled_set(menu_it2, EINA_TRUE);
   elm_menu_item_add(menu, menu_it, _icon_path_get("kill"), "Kill", _item_menu_kill_cb, proc);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, _icon_path_get("cancel"), "Cancel", _item_menu_cancel_cb, ui);

   return menu;
}

static void
_item_pid_secondary_clicked_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info)
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
   Evas_Object *progress, *button, *plist;

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
   evas_object_show(hbox);
   elm_box_pack_end(box, hbox);

   frame = elm_frame_add(hbox);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(frame, "System CPU");
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

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
   evas_object_show(frame);
   elm_box_pack_end(hbox, frame);

   ui->progress_mem = progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_object_content_set(frame, progress);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, 0);
   evas_object_show(table);

   ui->btn_pid = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_PID)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
    _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "PID");
   evas_object_show(button);
   elm_table_pack(table, button, 0, 0, 1, 1);

   ui->btn_uid = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_UID)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
     _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "UID");
   evas_object_show(button);
   elm_table_pack(table, button, 1, 0, 1, 1);

   ui->btn_size = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_SIZE)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
    _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Size");
   evas_object_show(button);
   elm_table_pack(table, button, 2, 0, 1, 1);

   ui->btn_rss = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_RSS)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
     _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Res");
   evas_object_show(button);
   elm_table_pack(table, button, 3, 0, 1, 1);

   ui->btn_cmd = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_CMD)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
     _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Command");
   evas_object_show(button);
   elm_table_pack(table, button, 4, 0, 1, 1);

   ui->btn_state = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_STATE)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
     _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "State");
   evas_object_show(button);
   elm_table_pack(table, button, 5, 0, 1, 1);

   ui->btn_cpu_usage = button = elm_button_add(parent);
   if (ui->sort_type == SORT_BY_CPU_USAGE)
     {
        _btn_icon_state_set(button, ui->sort_reverse);
        elm_object_focus_set(button, EINA_TRUE);
     }
   else
     _btn_icon_state_set(button, EINA_FALSE);

   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "CPU %");
   evas_object_show(button);
   elm_table_pack(table, button, 6, 0, 1, 1);

   ui->scroller = ui->genlist_procs = plist = elm_genlist_add(parent);
   elm_object_focus_allow_set(plist, EINA_FALSE);
   elm_genlist_homogeneous_set(plist, EINA_TRUE);
   evas_object_size_hint_weight_set(plist, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(plist, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(plist);

   elm_box_pack_end(box, table);
   elm_box_pack_end(box, plist);

   evas_object_smart_callback_add(ui->btn_pid, "clicked", _btn_pid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_uid, "clicked", _btn_uid_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_size, "clicked", _btn_size_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_rss, "clicked", _btn_rss_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cmd, "clicked", _btn_cmd_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_state, "clicked", _btn_state_clicked_cb, ui);
   evas_object_smart_callback_add(ui->btn_cpu_usage, "clicked", _btn_cpu_usage_clicked_cb, ui);
   evas_object_smart_callback_add(ui->genlist_procs, "selected", _item_pid_clicked_cb, ui);
   evas_object_event_callback_add(ui->genlist_procs, EVAS_CALLBACK_MOUSE_UP,
                                  _item_pid_secondary_clicked_cb, ui);
   evas_object_smart_callback_add(ui->genlist_procs, "unrealized", _item_unrealized_cb, ui);
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
   evas_object_smart_callback_add(ui->list_pid, "selected", _process_panel_list_selected_cb, ui);
   elm_object_content_set(frame, list);

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
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 0, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "PID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 1, 1, 1);

   ui->entry_pid_pid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 1, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Username:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 2, 1, 1);

   ui->entry_pid_user = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 2, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "UID:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 3, 1, 1);

   ui->entry_pid_uid = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
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
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 4, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Threads:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 5, 1, 1);

   ui->entry_pid_threads = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 5, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Total memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 6, 1, 1);

   ui->entry_pid_size = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 6, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, " Reserved memory:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 7, 1, 1);

   ui->entry_pid_rss = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 7, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Nice:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 8, 1, 1);

   ui->entry_pid_nice = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 8, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "Priority:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 9, 1, 1);

   ui->entry_pid_pri = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 9, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "State:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 10, 1, 1);

   ui->entry_pid_state = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   elm_table_pack(table, entry, 1, 10, 1, 1);

   label = elm_label_add(parent);
   elm_object_text_set(label, "CPU %:");
   evas_object_show(label);
   elm_table_pack(table, label, 0, 11, 1, 1);

   ui->entry_pid_cpu_usage = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_FALSE);
   evas_object_show(entry);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
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
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, 0.5);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
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
   elm_object_style_set(frame, "pad_small");
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
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
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);
}

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, 0.1, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, eina_slstr_printf("<bigger>%s</bigger>",text));
   evas_object_show(label);

   return label;
}

static void
_ui_tab_memory_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *progress, *scroller;
   Evas_Object *label, *table;

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
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
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
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_medium");
   elm_box_pack_end(box, frame);

   ui->title_mem = label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, "<subtitle>Memory</subtitle>");
   evas_object_show(label);
   elm_box_pack_end(box, label);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_large");
   elm_box_pack_end(box, frame);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(table, 0, 20 * elm_config_scale_get());
   evas_object_show(table);

   label = _label_mem(box, "Used");

   ui->progress_mem_used = progress = elm_progressbar_add(table);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 0, 1, 1);
   elm_table_pack(table, progress, 1, 0, 1, 1);

   label = _label_mem(box, "Cached");

   ui->progress_mem_cached = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 1, 1, 1);
   elm_table_pack(table, progress, 1, 1, 1, 1);

   label = _label_mem(box, "Buffered");

   ui->progress_mem_buffered = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 2, 1, 1);
   elm_table_pack(table, progress, 1, 2, 1, 1);

   label = _label_mem(box, "Shared");

   ui->progress_mem_shared = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 3, 1, 1);
   elm_table_pack(table, progress, 1, 3, 1, 1);

   label = _label_mem(box, "Swapped");

   ui->progress_mem_swap = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 4, 1, 1);
   elm_table_pack(table, progress, 1, 4, 1, 1);

   frame = elm_frame_add(ui->mem_activity);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);

   elm_box_pack_end(box, table);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->mem_activity, frame);
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
   Evas_Object *table, *box, *entry, *hbox, *frame, *button;
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
   elm_object_text_set(button, "Processes");
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

   frame = elm_frame_add(parent);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   ui->entry_search = entry = elm_entry_add(parent);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   evas_object_show(entry);
   elm_object_content_set(border, entry);
   elm_box_pack_end(box, border);

   border = elm_frame_add(parent);
   evas_object_size_hint_weight_set(border, 0.1, 0);
   evas_object_size_hint_align_set(border, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(border, "pad_small");
   evas_object_show(border);

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(button, "Close");
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

   control = evas_key_modifier_is_set(ev->modifiers, "Control");

   ui->skip_wait = EINA_TRUE;

   if (!control) return;

   if (!strcmp(ev->keyname, "Escape"))
     {
        ui_shutdown(ui);
        return;
     }

   if ((ev->keyname[0] == 'K' || ev->keyname[0] == 'k'))
     ui->data_unit = DATA_UNIT_KB;
   else if ((ev->keyname[0] == 'M' || ev->keyname[0] == 'm'))
     ui->data_unit = DATA_UNIT_MB;
   else if ((ev->keyname[0] == 'G' || ev->keyname[0] == 'g'))
     ui->data_unit = DATA_UNIT_GB;

   if (ev->keyname[0] == 'e' || ev->keyname[0] == 'E')
     ui->show_self = !ui->show_self;

   _config_save(ui);
}

static void
_evisum_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   ui->ready = EINA_FALSE;

   elm_genlist_clear(ui->genlist_procs);
   _process_panel_update(ui);
   _config_save(ui);
}

void
ui_shutdown(Ui *ui)
{
   Item_Cache *it;

   evas_object_del(ui->win);

   ui->shutting_down = EINA_TRUE;

   if (ui->thread_system)
     ecore_thread_cancel(ui->thread_system);

   if (ui->thread_process)
     ecore_thread_cancel(ui->thread_process);

   if (ui->thread_system)
     ecore_thread_wait(ui->thread_system, 1.0);

   if (ui->thread_process)
     ecore_thread_wait(ui->thread_process, 1.0);

   _proc_pid_cpu_times_free(ui);

   EINA_LIST_FREE(ui->item_cache, it)
     {
        free(it);
     }

   if (ui->item_cache)
     eina_list_free(ui->item_cache);

   eina_lock_free(&_lock);

   ecore_main_loop_quit();
}

static void
_ui_launch(Ui *ui)
{
   _process_panel_update(ui);
   _process_list_update(ui);

   ui->thread_system  = ecore_thread_feedback_run(_system_stats, _system_stats_feedback_cb,
                                                  _thread_end_cb, _thread_error_cb, ui,
                                                  EINA_FALSE);

   ui->thread_process = ecore_thread_feedback_run(_process_list, _process_list_feedback_cb,
                                                  _thread_end_cb, _thread_error_cb, ui,
                                                  EINA_FALSE);

   evas_object_event_callback_add(ui->win, EVAS_CALLBACK_RESIZE, _evisum_resize_cb, ui);
   evas_object_event_callback_add(ui->content, EVAS_CALLBACK_KEY_DOWN, _evisum_key_down_cb, ui);
   evas_object_event_callback_add(ui->entry_search, EVAS_CALLBACK_KEY_DOWN, _evisum_search_keypress_cb, ui);
}

static Ui *
_ui_init(Evas_Object *parent)
{
   Ui *ui = calloc(1, sizeof(Ui));
   if (!ui) return NULL;

   /* Settings */
   ui->win = parent;
   ui->poll_delay = 3;
   ui->sort_reverse = EINA_FALSE;
   ui->sort_type = SORT_BY_PID;
   ui->selected_pid = -1;
   ui->program_pid = getpid();
   ui->panel_visible = ui->disk_visible = ui->cpu_visible = ui->mem_visible =ui->misc_visible = EINA_TRUE;
   ui->data_unit = DATA_UNIT_MB;
   ui->cpu_times = NULL;
   ui->item_cache = NULL;

   _ui = _evisum_config = NULL;

   _config_load(ui);

   /* UI content creation */
   _ui_tabs_add(parent, ui);
   _ui_tab_system_add(ui);
   _ui_process_panel_add(ui);
   _ui_tab_cpu_add(ui);
   _ui_tab_memory_add(ui);
   _ui_tab_disk_add(ui);
   _ui_tab_misc_add(ui);

   _item_cache_init(ui);

   return ui;
}

Ui *
ui_add(Evas_Object *parent)
{
   eina_lock_new(&_lock);

   /* Create our user interface. */
   Ui *ui = _ui = _ui_init(parent);
   if (!ui) return NULL;

   /* Start polling our data */
   _ui_launch(ui);

   return ui;
}

