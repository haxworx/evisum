#include "ui_misc.h"
#include "system/machine.h"

static Eina_Bool
_battery_usage_add(Evas_Object *box, power_t *power)
{
   Evas_Object *frame, *vbox, *hbox, *pb, *ic, *label;
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
          fmt = _("<big>%s (plugged in) </big>");
        else
          fmt = _("<big>%s</big>");

        elm_object_text_set(label, eina_slstr_printf(fmt,
                        power->batteries[i]->name));
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

        pb = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
        elm_progressbar_span_size_set(pb, 1.0);
        elm_progressbar_unit_format_set(pb, "%1.0f%%");
        elm_progressbar_value_set(pb,
                        (double) power->batteries[i]->percent / 100);
        evas_object_show(pb);

        elm_box_pack_end(hbox, pb);
        elm_box_pack_end(vbox, hbox);
        elm_object_content_set(frame, vbox);
        elm_box_pack_end(box, frame);
     }

   return !!power->battery_count;
}

static Eina_Bool
_sensor_usage_add(Evas_Object *box, sensor_t **sensors, int count)
{
   Evas_Object *frame, *vbox, *hbox, *pb, *ic, *label;
   sensor_t *snsr;

   for (int i = 0; i < count; i++)
     {
        snsr = sensors[i];

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

        elm_object_text_set(label, eina_slstr_printf("<big>%s</big>",
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

        pb = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
        elm_progressbar_span_size_set(pb, 1.0);
        elm_progressbar_unit_format_set(pb, "%1.0f°C");
        elm_progressbar_value_set(pb,
                        (double) snsr->value / 100);
        evas_object_show(pb);

        elm_box_pack_end(hbox, pb);
        elm_box_pack_end(vbox, hbox);
        elm_object_content_set(frame, vbox);
        elm_box_pack_end(box, frame);
     }

   return count;
}

static char *
_network_transfer_format(double rate)
{
   const char *unit = "B/s";

   if (rate > 1073741824)
     {
        rate /= 1073741824;
        unit = "GB/s";
     }
   else if (rate > 1048576 && rate <= 1073741824)
     {
        rate /= 1048576;
        unit = "MB/s";
     }
   else if (rate > 1024 && rate <= 1048576)
     {
        rate /= 1024;
        unit = "KB/s";
     }

   return strdup(eina_slstr_printf("%.2f %s", rate, unit));
}

static void
_network_usage_add(Ui *ui, Evas_Object *box, uint64_t bytes, Eina_Bool incoming)
{
   Evas_Object *frame, *vbox, *hbox, *label, *pb, *ic;
   char *tmp;

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
   if (incoming)
     elm_object_text_set(label, _("<big>Network Incoming</big>"));
   else
     elm_object_text_set(label, _("<big>Network Outgoing</big>"));

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

   pb = elm_progressbar_add(box);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   evas_object_show(pb);
   elm_box_pack_end(hbox, pb);
   elm_box_pack_end(vbox, hbox);

   tmp = _network_transfer_format(bytes);
   if (tmp)
     {
        elm_progressbar_unit_format_set(pb, tmp);
        free(tmp);
     }

   if (bytes)
     {
        if (incoming)
          {
             if (ui->incoming_max < bytes)
               ui->incoming_max = bytes;
             elm_progressbar_value_set(pb, (double) bytes / ui->incoming_max);
          }
        else
          {
             if (ui->outgoing_max < bytes)
               ui->outgoing_max = bytes;
             elm_progressbar_value_set(pb, (double) bytes / ui->outgoing_max);
          }
     }

   elm_object_content_set(frame, vbox);
   elm_box_pack_end(box, frame);
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

static void
_misc_free(power_t *power, sensor_t **sensors, int sensor_count)
{
   for (int i = 0; i < power->battery_count; i++)
     {
        if (power->batteries[i]->name)
          free(power->batteries[i]->name);
#if defined(__OpenBSD__)
        if (power->batteries[i]->mibs)
          free(power->batteries[i]->mibs);
#endif
        free(power->batteries[i]);
     }
   if (power->batteries)
     free(power->batteries);

   for (int i = 0; i < sensor_count; i++)
     {
        sensor_t *snsr = sensors[i];
        if (snsr->name)
          free(snsr->name);
        free(snsr);
     }
   if (sensors)
     free(sensors);
}

static Eina_Bool
_misc_update(void *data)
{
   Ui *ui;
   power_t power;
   sensor_t **sensors;
   int sensor_count = 0;
   Evas_Object *box, *frame;

   ui = data;

   elm_box_clear(ui->misc_activity);

   box = elm_box_add(ui->misc_activity);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   _network_usage_add(ui, box, ui->network_usage.incoming, EINA_TRUE);
   _network_usage_add(ui, box, ui->network_usage.outgoing, EINA_FALSE);
   _separator_add(box);

   memset(&power, 0, sizeof(power));
   system_power_state_get(&power);

   if (_battery_usage_add(box, &power))
     _separator_add(box);

   sensors = system_sensors_thermal_get(&sensor_count);
   if (sensors)
     {
        _sensor_usage_add(box, sensors, sensor_count);
        _separator_add(box);
     }

   _misc_free(&power, sensors, sensor_count);

   frame = elm_frame_add(ui->misc_activity);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   elm_object_style_set(frame, "pad_huge");
   evas_object_show(frame);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->misc_activity, frame);

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->timer_misc)
     ecore_timer_del(ui->timer_misc);
   ui->timer_misc = NULL;

   evas_object_del(obj);
   ui->misc_visible = EINA_FALSE;
}

void
ui_win_misc_add(Ui *ui)
{
   Evas_Object *win, *box, *hbox, *frame, *scroller;
   Evas_Object *table, *border, *rect;

   if (ui->misc_visible) return;
   ui->misc_visible = EINA_TRUE;

   win = elm_win_util_standard_add("evisum", _("Stuff"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_hide(box);

   ui->misc_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   elm_object_style_set(frame, "pad_small");
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_show(frame);

   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   border = elm_frame_add(box);
   elm_object_style_set(border, "pad_small");
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   evas_object_show(border);
   elm_object_content_set(border, hbox);

   table = elm_table_add(win);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);

   rect = evas_object_rectangle_add(evas_object_rectangle_add(win));
   evas_object_size_hint_max_set(rect, MISC_MAX_WIDTH, -1);
   evas_object_size_hint_min_set(rect, MISC_MIN_WIDTH, 1);

   elm_table_pack(table, rect, 0, 0, 1, 1);
   elm_table_pack(table, border, 0, 0, 1, 1);

   elm_object_content_set(scroller, table);
   elm_object_content_set(frame,scroller);
   elm_box_pack_end(box, frame);
   elm_object_content_set(win, box);

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evisum_child_window_show(ui->win, win);

   _misc_update(ui);

   ui->timer_misc = ecore_timer_add(3.0, _misc_update, ui);
}

