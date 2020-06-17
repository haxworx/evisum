#include "ui_misc.h"

static Eina_Bool
_battery_usage_add(Evas_Object *box, power_t *power)
{
   Evas_Object *frame, *vbox, *hbox, *progress, *ic, *label;
   const char *fmt;

   for (int i = 0; i < power->battery_count; i++)
     {
        if (!power->batteries[i]->present)
          continue;

        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, FILL, FILL);
        evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
        elm_object_style_set(frame, "pad_small");
        evas_object_show(frame);

        vbox = elm_box_add(box);
        evas_object_size_hint_align_set(vbox, FILL, FILL);
        evas_object_size_hint_weight_set(vbox, EXPAND, EXPAND);
        evas_object_show(vbox);

        label = elm_label_add(box);
        evas_object_size_hint_align_set(label, 1.0, FILL);
        evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
        evas_object_show(label);
        elm_box_pack_end(vbox, label);

        if (power->have_ac && i == 0)
          fmt = _("<bigger>%s (plugged in) </bigger>");
        else
          fmt = _("<bigger>%s</bigger>");

        elm_object_text_set(label, eina_slstr_printf(fmt,
                        power->battery_names[i]));
        hbox = elm_box_add(box);
        evas_object_size_hint_align_set(hbox, FILL, FILL);
        evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
        elm_box_horizontal_set(hbox, EINA_TRUE);
        evas_object_show(hbox);

        ic = elm_image_add(box);
        elm_image_file_set(ic, evisum_icon_path_get("battery"), NULL);
        evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(),
                        32 * elm_config_scale_get());
        evas_object_show(ic);
        elm_box_pack_end(hbox, ic);

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, FILL, FILL);
        evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.0f%%");
        elm_progressbar_value_set(progress,
                        (double) power->batteries[i]->percent / 100);
        evas_object_show(progress);

        elm_box_pack_end(hbox, progress);
        elm_box_pack_end(vbox, hbox);
        elm_object_content_set(frame, vbox);
        elm_box_pack_end(box, frame);
     }

   return !!power->battery_count;
}

static Eina_Bool
_sensor_usage_add(Evas_Object *box, Sys_Info *sysinfo)
{
   Evas_Object *frame, *vbox, *hbox, *progress, *ic, *label;
   sensor_t *snsr;

   for (int i = 0; i < sysinfo->sensor_count; i++)
     {
        snsr = sysinfo->sensors[i];

        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, FILL, FILL);
        evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
        elm_object_style_set(frame, "pad_small");
        evas_object_show(frame);

        vbox = elm_box_add(box);
        evas_object_size_hint_align_set(vbox, FILL, FILL);
        evas_object_size_hint_weight_set(vbox, EXPAND, EXPAND);
        evas_object_show(vbox);

        label = elm_label_add(box);
        evas_object_size_hint_align_set(label, 1.0, FILL);
        evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
        evas_object_show(label);
        elm_box_pack_end(vbox, label);

        elm_object_text_set(label, eina_slstr_printf("<bigger>%s</bigger>",
                        snsr->name));

        hbox = elm_box_add(box);
        evas_object_size_hint_align_set(hbox, FILL, FILL);
        evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
        elm_box_horizontal_set(hbox, EINA_TRUE);
        evas_object_show(hbox);

        ic = elm_image_add(box);
        elm_image_file_set(ic, evisum_icon_path_get("sensor"), NULL);
        evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(),
                        32 * elm_config_scale_get());
        evas_object_show(ic);
        elm_box_pack_end(hbox, ic);

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, FILL, FILL);
        evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.0f°C");
        elm_progressbar_value_set(progress,
                        (double) snsr->value / 100);
        evas_object_show(progress);

        elm_box_pack_end(hbox, progress);
        elm_box_pack_end(vbox, hbox);
        elm_object_content_set(frame, vbox);
        elm_box_pack_end(box, frame);
     }

   return !!sysinfo->sensor_count;
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

static void
_network_usage_add(Ui *ui, Evas_Object *box, uint64_t bytes, Eina_Bool incoming)
{
   Evas_Object *vbox, *hbox, *label, *progress, *ic;
   char *tmp;

   vbox = elm_box_add(box);
   evas_object_size_hint_align_set(vbox, FILL, FILL);
   evas_object_size_hint_weight_set(vbox, EXPAND, EXPAND);
   evas_object_show(vbox);

   label = elm_label_add(box);
   if (incoming)
     elm_object_text_set(label, _("<bigger>Network Incoming</bigger>"));
   else
     elm_object_text_set(label, _("<bigger>Network Outgoing</bigger>"));

   evas_object_size_hint_align_set(label, 1.0, FILL);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   evas_object_show(label);
   elm_box_pack_end(vbox, label);

   hbox = elm_box_add(box);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ic = elm_image_add(box);
   elm_image_file_set(ic, evisum_icon_path_get("network"), NULL);
   evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(),
                   32 * elm_config_scale_get());
   evas_object_show(ic);
   elm_box_pack_end(hbox, ic);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, FILL, FILL);
   evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);
   elm_box_pack_end(hbox, progress);
   elm_box_pack_end(vbox, hbox);

   tmp = _network_transfer_format(bytes);
   if (tmp)
     {
        elm_progressbar_unit_format_set(progress, tmp);
        free(tmp);
     }

   if (bytes)
     {
        if (incoming)
          {
             if (ui->incoming_max < bytes)
               ui->incoming_max = bytes;
             elm_progressbar_value_set(progress,
                             (double) bytes / ui->incoming_max);
          }
        else
          {
             if (ui->outgoing_max < bytes)
               ui->outgoing_max = bytes;
             elm_progressbar_value_set(progress,
                             (double) bytes / ui->outgoing_max);
          }
     }

   elm_box_pack_end(box, vbox);
}

static void
_separator_add(Evas_Object *box)
{
   Evas_Object *hbox, *sep;

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_show(hbox);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_show(sep);

   elm_box_pack_end(hbox, sep);
   elm_box_pack_end(box, hbox);
}

void
ui_tab_misc_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->misc_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_table_pack(ui->content, ui->misc_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->misc_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   elm_object_style_set(frame, "pad_small");
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);
}

void
ui_tab_misc_update(Ui *ui, Sys_Info *sysinfo)
{
   Evas_Object *box, *frame;

   if (!ui->misc_visible)
     return;

   elm_box_clear(ui->misc_activity);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   if (_battery_usage_add(box, &sysinfo->power))
     _separator_add(box);
   if (_sensor_usage_add(box, sysinfo))
     _separator_add(box);
   _network_usage_add(ui, box, sysinfo->incoming, EINA_TRUE);
   _network_usage_add(ui, box, sysinfo->outgoing, EINA_FALSE);

   frame = elm_frame_add(ui->misc_activity);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   elm_object_style_set(frame, "pad_medium");
   evas_object_show(frame);
   elm_object_content_set(frame, box);

   elm_box_pack_end(ui->misc_activity, frame);
}


