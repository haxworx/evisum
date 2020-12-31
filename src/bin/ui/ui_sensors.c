#include "ui_sensors.h"
#include "system/machine.h"

typedef struct
{
   Ecore_Thread           *thread;
   Eina_List              *sensors;
   Eina_List              *batteries;

   Evas_Object            *genlist;
   Elm_Genlist_Item_Class *itc;

   sensor_t               *sensor;

   Evas_Object            *thermal_pb;

   Evas_Object            *power_ic;
   Ui                     *ui;
} Ui_Data;

typedef struct {
   Evas_Object *pb;
} Bat;

typedef struct
{
   power_t   power;

   double    thermal_temp;
   Eina_Bool thermal_valid;
} Data;

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
_sensors_refresh(Ui_Data *pd)
{
   sensor_t **ss;
   int n;

   ss = system_sensors_thermal_get(&n);

   for (int i = 0; i < n; i++)
     pd->sensors = eina_list_append(pd->sensors, ss[i]);

   free(ss);

   pd->sensors = eina_list_sort(pd->sensors, n, _sort_cb);

   elm_genlist_clear(pd->genlist);
}

static void
_sensors_update(void *data, Ecore_Thread *thread)
{
   Ui_Data *pd = data;

   Data *msg = malloc(sizeof(Data));
   if (!msg) return;

   while (!ecore_thread_check(thread))
     {
        system_power_state_get(&msg->power);
        if (pd->sensor)
          {
             if (!system_sensor_thermal_get(pd->sensor))
               msg->thermal_valid = 0;
             else
               {
                  msg->thermal_valid = 1;
                  msg->thermal_temp = pd->sensor->value;
               }
          }
        ecore_thread_feedback(thread, msg);
        for (int i = 0; i < 8; i++)
          {
             if (ecore_thread_check(thread)) break;
             usleep(250000);
          }
     }
   free(msg);
}

static void
_sensors_update_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata)
{
   Data *msg;
   Ui_Data *pd;
   sensor_t *s;
   Eina_List *l;
   int i = 0;

   msg = msgdata;
   pd = data;

   EINA_LIST_FREE(pd->sensors, s)
     elm_genlist_item_append(pd->genlist, pd->itc, s,
                             NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
   if (msg->thermal_valid && pd->sensor)
     {
        elm_progressbar_value_set(pd->thermal_pb, msg->thermal_temp / 100);
        elm_object_tooltip_text_set(pd->thermal_pb, pd->sensor->name);
     }

   if (pd->power_ic)
     {
        if (msg->power.have_ac)
          elm_icon_standard_set(pd->power_ic, evisum_icon_path_get("on"));
        else
          elm_icon_standard_set(pd->power_ic, evisum_icon_path_get("off"));
        evas_object_show(pd->power_ic);
     }
   l = eina_list_nth_list(pd->batteries, 0);
   while (l && msg->power.battery_count)
     {
        if (msg->power.batteries[i]->present)
          {
              Bat *bat = eina_list_data_get(l);
              elm_object_tooltip_text_set(bat->pb,
                                          msg->power.batteries[i]->name);
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
_genlist_item_pressed_cb(void *data, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd;
   Elm_Object_Item *it;
   sensor_t *s;
   char buf[64];

   pd = data;
   it = event_info;

   pd->sensor = s = elm_object_item_data_get(it);
   _name_set(buf, sizeof(buf), s);
   elm_object_text_set(obj, buf);
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *part)
{
   Evas_Object *bx, *lb;
   sensor_t *s;
   char buf[64];

   if (strcmp(part, "elm.swallow.content")) return NULL;

   s = data;

   bx = elm_box_add(obj);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   lb = elm_label_add(obj);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(lb, 0.0, FILL);
   _name_set(buf, sizeof(buf), s);
   elm_object_text_set(lb, buf);
   evas_object_show(lb);

   elm_box_pack_end(bx, lb);

   return bx;
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Ui_Data *pd;

   pd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(pd->ui->sensors.win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ui_Data *pd;
   Ui *ui;
   Evas_Coord x = 0, y = 0;

   pd = data;
   ui = pd->ui;

   evas_object_geometry_get(obj, &x, &y, NULL, NULL);
   ui->sensors.x = x;
   ui->sensors.y = y;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Bat *bat;
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   evisum_ui_config_save(ui);

   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);
   ui->sensors.win = NULL;
   EINA_LIST_FREE(pd->batteries, bat)
     free(bat);

   elm_genlist_item_class_free(pd->itc);
   free(pd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd = data;

   evisum_ui_config_save(pd->ui);
}

void
ui_win_sensors_add(Ui *ui, Evas_Object *parent)
{
   Evas_Object *win, *tbl, *fr;
   Evas_Object *genlist, *pb;
   Evas_Object *ic;
   power_t power;
   Evas_Coord x = 0, y = 0;
   int j = 0, i = 0;

   if (ui->sensors.win)
     {
        elm_win_raise(ui->sensors.win);
        return;
     }

   Ui_Data *pd = calloc(1, sizeof(Ui_Data));
   if (!pd) return;
   pd->ui = ui;

   ui->sensors.win = win = elm_win_util_standard_add("evisum", _("Sensors"));
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win,
                                   evisum_ui_backgrounds_enabled_get());
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, pd);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_medium");
   evas_object_show(fr);
   elm_object_content_set(win, fr);

   system_power_state_get(&power);

   tbl = elm_table_add(win);
   evas_object_size_hint_weight_set(tbl, EXPAND, 0);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_show(tbl);
   elm_table_padding_set(tbl, 0, ELM_SCALE_SIZE(5));
   elm_object_content_set(fr, tbl);

   for (i = 0; i < power.battery_count; i++)
     {
        if (!power.batteries[i]->present) continue;

        Bat *bat = calloc(1, sizeof(Bat));
        if (!bat) return;

        if (!i)
          {
             pd->power_ic = ic = elm_icon_add(win);
             evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
             evas_object_size_hint_align_set(ic, 1.0, 0.5);
             elm_table_pack(tbl, ic, 0, j, 1, 1);
          }
        pb = elm_progressbar_add(win);
        evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        evas_object_show(pb);
        bat->pb = pb;
        elm_table_pack(tbl, pb, 1, j++, 1, 1);

        pd->batteries = eina_list_append(pd->batteries, bat);
     }

   system_power_state_free(&power);

   pd->thermal_pb = pb = elm_progressbar_add(win);
   evas_object_size_hint_weight_set(pb, EXPAND, 0);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   elm_progressbar_unit_format_set(pb, "%1.1f°C");
   evas_object_show(pb);

   elm_table_pack(tbl, pb, (i ? 1 : 0), j++, (i ? 1 : 2), 1);

   pd->genlist = genlist = elm_genlist_add(win);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   elm_object_text_set(genlist, _("Select..."));
   elm_genlist_multi_select_set(genlist, 0);
   evas_object_smart_callback_add(genlist, "selected", _genlist_item_pressed_cb, pd);
   elm_object_focus_allow_set(genlist, 0);
   evas_object_show(genlist);
   elm_table_pack(tbl, genlist, 0, j++, 2, 1);

   pd->itc = elm_genlist_item_class_new();
   pd->itc->item_style = "full";
   pd->itc->func.content_get = _content_get;
   pd->itc->func.text_get = NULL;
   pd->itc->func.filter_get = NULL;
   pd->itc->func.del = _item_del;

   if (ui->sensors.width > 0 && ui->sensors.height > 0)
     evas_object_resize(win, ui->sensors.width, ui->sensors.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);

   if (ui->sensors.x > 0 && ui->sensors.y > 0)
     evas_object_move(win, ui->sensors.x, ui->sensors.y);
   else
     {
        if (parent)
          evas_object_geometry_get(parent, &x, &y, NULL, NULL);
        if (x > 0 && y > 0)
          evas_object_move(win, x + 20, y + 20);
        else
          elm_win_center(win, 1, 1);
     }

   evas_object_show(win);

   _sensors_refresh(pd);

   pd->thread = ecore_thread_feedback_run(_sensors_update,
                                          _sensors_update_feedback_cb,
                                          NULL, NULL, pd, EINA_TRUE);
}

