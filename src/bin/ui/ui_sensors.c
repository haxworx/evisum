#include "ui_sensors.h"
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
          fmt = _("<color=#fff>%s (plugged in)</>");
        else
          fmt = _("<color=#fff>%s</>");

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
   Eina_Strbuf *name;
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

        name = eina_strbuf_new();
        eina_strbuf_append(name, snsr->name);
        if (snsr->child_name)
          eina_strbuf_append_printf(name, " (%s)", snsr->child_name);

        elm_object_text_set(label, eina_slstr_printf("<color=#fff>%s</>",
                        eina_strbuf_string_get(name)));
        eina_strbuf_free(name);

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
        elm_progressbar_unit_format_set(pb, "%1.1f°C");
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
_sensors_free(power_t *power, sensor_t **sensors, int sensor_count)
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
        if (snsr->child_name)
          free(snsr->child_name);
        free(snsr);
     }
   if (sensors)
     free(sensors);
}

static Eina_Bool
_sensors_update(void *data)
{
   Ui *ui;
   power_t power;
   sensor_t **sensors;
   int sensor_count = 0;
   Evas_Object *box, *frame;

   ui = data;

   elm_box_clear(ui->sensors.box);

   box = elm_box_add(ui->sensors.box);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

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

   _sensors_free(&power, sensors, sensor_count);

   frame = elm_frame_add(ui->sensors.box);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   elm_object_style_set(frame, "pad_huge");
   evas_object_show(frame);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->sensors.box, frame);

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->sensors.timer)
     ecore_timer_del(ui->sensors.timer);
   ui->sensors.timer = NULL;

   evas_object_del(obj);
   ui->sensors.win = NULL;
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   evisum_ui_config_save(ui);
}

void
ui_win_sensors_add(Ui *ui)
{
   Evas_Object *win, *box, *hbox, *frame, *scroller;
   Evas_Object *table, *border, *rect;
   Evas_Coord x = 0, y = 0;

   if (ui->sensors.win)
     {
        elm_win_raise(ui->sensors.win);
        return;
     }

   ui->sensors.win = win = elm_win_util_standard_add("evisum", _("Sensors"));
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, (evisum_ui_effects_enabled_get() ||
                                   evisum_ui_backgrounds_enabled_get()));
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, ui);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_hide(box);

   ui->sensors.box = hbox = elm_box_add(box);
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

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, ui);

   if (ui->sensors.width > 0 && ui->sensors.height > 0)
     evas_object_resize(win, ui->sensors.width, ui->sensors.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);

   if (ui->win)
     evas_object_geometry_get(ui->win, &x, &y, NULL, NULL);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 20);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);

   _sensors_update(ui);

   ui->sensors.timer = ecore_timer_add(ui->settings.poll_delay, _sensors_update, ui);
}

