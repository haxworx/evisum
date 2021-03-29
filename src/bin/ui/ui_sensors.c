#include "ui_sensors.h"
#include "system/machine.h"

typedef struct
{
   Ecore_Thread           *thread;
   Eina_List              *sensors;
   Eina_List              *batteries;

   Evas_Object            *glist;
   Elm_Genlist_Item_Class *itc;

   sensor_t               *sensor;
   Evas_Object            *thermal_pb;
   Evas_Object            *power_ic;
   Eina_Bool               skip_wait;

   Evisum_Ui              *ui;
} Win_Data;

typedef struct
{
   Evas_Object *pb;
} Bat;

typedef struct
{
   power_t   power;
   double    thermal_temp;
   Eina_Bool thermal_valid;
} Sensor_Win_Data;

static void
_name_set(char *buf, size_t len, sensor_t *s)
{
   if (!s->child_name)
     snprintf(buf, len, "%s", s->name);
   else
     snprintf(buf, len, "%s (%s)", s->name, s->child_name);
}

static int
_sort_cb(const void *p1, const void *p2)
{
   sensor_t *s1, *s2;
   char buf[64], buf2[64];

   s1 = (sensor_t *) p1; s2 = (sensor_t *) p2;

   _name_set(buf, sizeof(buf), s1);
   _name_set(buf2, sizeof(buf2), s2);

   return strcmp(buf, buf2);
}

static void
_sensors_refresh(Win_Data *wd)
{
   sensor_t **sensors;
   int n;

   sensors = system_sensors_thermal_get(&n);
   if (!sensors) return;

   for (int i = 0; i < n; i++)
     wd->sensors = eina_list_append(wd->sensors, sensors[i]);

   free(sensors);

   wd->sensors = eina_list_sort(wd->sensors, n, _sort_cb);

   elm_genlist_clear(wd->glist);
}

static void
_sensors_update(void *data, Ecore_Thread *thread)
{
   Win_Data *wd = data;

   Sensor_Win_Data *msg = malloc(sizeof(Win_Data));
   if (!msg) return;

   while (!ecore_thread_check(thread))
     {
        system_power_state_get(&msg->power);
        if (wd->sensor)
          {
             if (!system_sensor_thermal_get(wd->sensor))
               msg->thermal_valid = 0;
             else
               {
                  msg->thermal_valid = 1;
                  msg->thermal_temp = wd->sensor->value;
               }
          }
        ecore_thread_feedback(thread, msg);
        for (int i = 0; i < 8; i++)
          {
             if (wd->skip_wait || ecore_thread_check(thread)) break;
             usleep(250000);
          }
	wd->skip_wait = 0;
     }
   free(msg);
}

static void
_sensors_update_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata)
{
   Sensor_Win_Data *msg;
   Win_Data *wd;
   sensor_t *s;
   Eina_List *l;
   int i = 0;

   msg = msgdata;
   wd = data;

   EINA_LIST_FREE(wd->sensors, s)
     elm_genlist_item_append(wd->glist, wd->itc, s,
                             NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
   if (msg->thermal_valid && wd->sensor)
     {
        elm_progressbar_value_set(wd->thermal_pb, msg->thermal_temp / 100);
        elm_object_tooltip_text_set(wd->thermal_pb, wd->sensor->name);
     }

   if (wd->power_ic)
     {
        if (msg->power.have_ac)
          elm_icon_standard_set(wd->power_ic, evisum_icon_path_get("on"));
        else
          elm_icon_standard_set(wd->power_ic, evisum_icon_path_get("off"));
        evas_object_show(wd->power_ic);
     }
   l = eina_list_nth_list(wd->batteries, 0);
   while (l && msg->power.battery_count)
     {
        if (msg->power.batteries[i]->present)
          {
              Bat *bat = eina_list_data_get(l);
              elm_object_tooltip_text_set(bat->pb,
                                          eina_slstr_printf("%s (%s)",
                                          msg->power.batteries[i]->vendor,
                                          msg->power.batteries[i]->model));
              double perc = (double) msg->power.batteries[i]->percent / 100;
              elm_progressbar_value_set(bat->pb, perc);
          }
        l = eina_list_next(l);
        i++;
     }
   system_power_state_free(&msg->power);
}

static void
_item_del(void *data, Evas_Object *obj)
{
   sensor_t *s = data;

   system_sensor_thermal_free(s);
}

static void
_glist_item_pressed_cb(void *data, Evas_Object *obj, void *event_info)
{
   Win_Data *wd;
   Elm_Object_Item *it;
   sensor_t *s;
   char buf[64];

   wd = data;
   it = event_info;

   wd->sensor = s = elm_object_item_data_get(it);
   _name_set(buf, sizeof(buf), s);
   elm_object_text_set(obj, buf);
   wd->skip_wait = 1;
}

static char *
_text_get(void *data, Evas_Object *obj, const char *part)
{
   sensor_t *s;
   char buf[64];

   if (strcmp(part, "elm.text")) return NULL;

   s = data;

   _name_set(buf, sizeof(buf), s);

   return strdup(buf);
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Win_Data *wd;

   wd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(wd->ui->sensors.win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   evas_object_geometry_get(obj, &ui->sensors.x, &ui->sensors.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Bat *bat;
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(wd->thread);
   ecore_thread_wait(wd->thread, 0.5);
   ui->sensors.win = NULL;
   EINA_LIST_FREE(wd->batteries, bat)
     free(bat);

   elm_genlist_item_class_free(wd->itc);
   free(wd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   evas_object_geometry_get(obj, NULL, NULL, &ui->sensors.width, &ui->sensors.height);
}

void
ui_sensors_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *content, *tbl, *bx, *fr;
   Evas_Object *glist, *pb;
   Evas_Object *ic;
   power_t power;
   int j = 0, i = 0;

   if (ui->sensors.win)
     {
        elm_win_raise(ui->sensors.win);
        return;
     }

   Win_Data *wd = calloc(1, sizeof(Win_Data));
   if (!wd) return;
   wd->ui = ui;

   ui->sensors.win = win = elm_win_util_standard_add("evisum", _("Sensors"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, wd);

   content = elm_box_add(win);
   evas_object_size_hint_weight_set(content, EXPAND, EXPAND);
   evas_object_size_hint_align_set(content, FILL, FILL);
   evas_object_show(content);
   elm_object_content_set(win, content);

   system_power_state_get(&power);

   if (power.battery_count)
     {
        fr = elm_frame_add(win);
        evas_object_size_hint_weight_set(fr, EXPAND, 0);
        evas_object_size_hint_align_set(fr, FILL, FILL);
        elm_object_text_set(fr, _("Power"));

        bx = elm_box_add(win);
        evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
        evas_object_size_hint_align_set(bx, FILL, FILL);
        evas_object_show(bx);

        tbl = elm_table_add(win);
        evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
        evas_object_size_hint_align_set(tbl, FILL, FILL);
        elm_box_pack_end(bx, tbl);
        evas_object_show(tbl);
        elm_object_content_set(fr, bx);
        elm_box_pack_end(content, fr);
        evas_object_show(fr);
     }
   for (i = 0; i < power.battery_count; i++)
     {
        if (!power.batteries[i]->present) continue;

        Bat *bat = calloc(1, sizeof(Bat));
        if (!bat) return;

        if (!i)
          {
             wd->power_ic = ic = elm_icon_add(win);
             evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
             evas_object_size_hint_align_set(ic, 1.0, 0.5);
             elm_table_pack(tbl, ic, 0, j, 1, 1);
          }
        pb = elm_progressbar_add(win);
        evas_object_size_hint_weight_set(pb, EXPAND, 0);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        bat->pb = pb;
        elm_table_pack(tbl, pb, 1, j++, 1, 1);
        evas_object_show(pb);

        wd->batteries = eina_list_append(wd->batteries, bat);
     }

   system_power_state_free(&power);

   wd->thermal_pb = pb = elm_progressbar_add(win);
   evas_object_size_hint_weight_set(pb, EXPAND, 0);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   elm_progressbar_unit_format_set(pb, "%1.1f°C");

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   wd->glist = glist = elm_genlist_add(win);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   elm_object_text_set(glist, _("Select..."));
   elm_genlist_multi_select_set(glist, 0);
   evas_object_smart_callback_add(glist, "selected", _glist_item_pressed_cb, wd);
   elm_object_focus_allow_set(glist, 0);

   elm_box_pack_end(bx, glist);
   evas_object_show(glist);
   elm_box_pack_end(bx, pb);
   evas_object_show(pb);

   fr = elm_frame_add(win);
   elm_object_text_set(fr, _("Thermal"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_content_set(fr, bx);

   elm_box_pack_end(content, fr);
   evas_object_show(fr);

   wd->itc = elm_genlist_item_class_new();
   wd->itc->item_style = "no_icon";
   wd->itc->func.content_get = NULL;
   wd->itc->func.text_get = _text_get;
   wd->itc->func.filter_get = NULL;
   wd->itc->func.del = _item_del;

   if ((ui->sensors.width > 0) && (ui->sensors.height > 0))
     evas_object_resize(win, ui->sensors.width, ui->sensors.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);

   if ((ui->sensors.x > 0) && (ui->sensors.y > 0))
     evas_object_move(win, ui->sensors.x, ui->sensors.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);

   _sensors_refresh(wd);

   wd->thread = ecore_thread_feedback_run(_sensors_update,
                                          _sensors_update_feedback_cb,
                                          NULL,
                                          NULL,
                                          wd, 1);
}

