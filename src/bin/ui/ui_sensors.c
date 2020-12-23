#include "ui_sensors.h"
#include "system/machine.h"

static Eina_Lock _lock;

typedef struct
{
   Eina_List              *sensors;
   Eina_List              *batteries;

   Evas_Object            *combobox;
   Elm_Genlist_Item_Class *itc;
   Elm_Object_Item        *selected_it;

   sensor_t               *sensor;

   Evas_Object            *power_fr;
   Evas_Object            *thermal_pb;

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

   elm_genlist_clear(pd->combobox);
}

static void
_sensors_update(void *data, Ecore_Thread *thread)
{
   Ui_Data *pd = data;

   Data *msg = malloc(sizeof(Data));
   if (!msg) return;

   while (!ecore_thread_check(thread))
     {
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

        system_power_state_get(&msg->power);

        ecore_thread_feedback(thread, msg);

        if (ecore_thread_check(thread)) break;

        usleep(1000000);
     }

   free(msg);
}

static void
_sensors_update_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata)
{
   sensor_t *s;
   Ui_Data *pd = data;
   Eina_List *l;
   Data *msg = msgdata;

   EINA_LIST_FREE(pd->sensors, s)
     elm_genlist_item_append(pd->combobox, pd->itc, s,
                             NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
   if (msg->thermal_valid)
     elm_progressbar_value_set(pd->thermal_pb, msg->thermal_temp / 100);

   l = eina_list_nth_list(pd->batteries, 0);
   if (l)
     {
        if (msg->power.have_ac)
          elm_object_text_set(pd->power_fr, _("Power (AC)"));
        else
          elm_object_text_set(pd->power_fr, _("Power"));
     }
   for (int i = 0; l && msg->power.battery_count; i++)
     {
        if (msg->power.batteries[i]->present)
          {
              Bat *bat = eina_list_data_get(l);
              double perc = (double) msg->power.batteries[i]->percent / 100;
              elm_progressbar_value_set(bat->pb, perc);
          }
        l = eina_list_next(l);
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
_combo_expanded_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *it;
   Ui_Data *pd = data;

   if (pd->selected_it)
     it = pd->selected_it;
   else
     it = elm_genlist_selected_item_get(obj);

   if (it)
     elm_genlist_item_selected_set(it, 1);
}

static void
_combo_item_pressed_cb(void *data, Evas_Object *obj, void *event_info)
{
   Ui_Data *pd;
   Elm_Object_Item *it;
   sensor_t *s;
   char buf[64];

   pd = data;
   it = pd->selected_it = event_info;

   pd->sensor = s = elm_object_item_data_get(it);
   _name_set(buf, sizeof(buf), s);
   elm_object_text_set(obj, buf);

   elm_combobox_hover_end(obj);
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
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Bat *bat;
   Ui_Data *pd = data;
   Ui *ui = pd->ui;

   ecore_thread_cancel(ui->sensors.thread);
   ecore_thread_wait(ui->sensors.thread, 0.5);
   ui->sensors.thread = NULL;

   evas_object_del(obj);
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
ui_win_sensors_add(Ui *ui)
{
   Evas_Object *win, *content, *bx, *tbl, *fr;
   Evas_Object *combo, *rec, *lb, *ic, *pb;
   Elm_Genlist_Item_Class *itc;
   power_t power;
   Evas_Coord x = 0, y = 0;

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
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);

   content = elm_box_add(win);
   evas_object_size_hint_weight_set(content, EXPAND, EXPAND);
   evas_object_size_hint_align_set(content, FILL, FILL);
   evas_object_show(content);
   elm_object_content_set(win, content);

   system_power_state_get(&power);
   if (power.battery_count)
     {
        pd->power_fr = fr = elm_frame_add(win);
        evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
        evas_object_size_hint_align_set(fr, FILL, FILL);
        elm_object_text_set(fr, _("Power"));
        evas_object_show(fr);

        bx = elm_box_add(win);
        evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
        evas_object_size_hint_align_set(bx, FILL, FILL);
        evas_object_show(bx);
        elm_object_content_set(fr, bx);
        elm_box_pack_end(content, fr);
     }

   for (int i = 0; i < power.battery_count; i++)
     {
        if (!power.batteries[i]->present) continue;

        Bat *bat = calloc(1, sizeof(Bat));
        if (!bat) return;

        tbl = elm_table_add(win);
        evas_object_size_hint_weight_set(tbl, EXPAND, 0.5);
        evas_object_size_hint_align_set(tbl, FILL, FILL);
        evas_object_show(tbl);
        elm_box_pack_end(bx, tbl);

        rec = evas_object_rectangle_add(evas_object_evas_get(win));
        evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(48));
        elm_table_pack(tbl, rec, 0, 0, 1, 1);

        ic = elm_icon_add(win);
        elm_icon_standard_set(ic, evisum_icon_path_get("battery"));
        evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(32), ELM_SCALE_SIZE(32));
        evas_object_show(ic);
        elm_table_pack(tbl, ic, 0, 0, 1, 1);
        elm_object_content_set(fr, bx);
        elm_box_pack_end(content, fr);

        lb = elm_label_add(win);
        elm_object_text_set(lb, eina_slstr_printf("<small>%s</>", power.batteries[i]->name));
        evas_object_size_hint_weight_set(lb, EXPAND, 0);
        evas_object_size_hint_align_set(lb, 0.5, 0.5);
        evas_object_show(lb);

        pb = elm_progressbar_add(win);
        evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        evas_object_show(pb);
        bat->pb = pb;

        elm_table_pack(tbl, pb, 1, 0, 1, 1);
        elm_table_pack(tbl, lb, 1, 0, 1, 1);

        elm_box_pack_end(bx, tbl);

        pd->batteries = eina_list_append(pd->batteries, bat);
     }

   system_power_state_free(&power);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_text_set(fr, _("Thermal"));
   evas_object_show(fr);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   pd->combobox = combo = elm_combobox_add(win);
   evas_object_size_hint_weight_set(combo, EXPAND, 0);
   evas_object_size_hint_align_set(combo, FILL, FILL);
   elm_object_text_set(combo, _("Select..."));
   elm_genlist_multi_select_set(combo, 0);
   evas_object_smart_callback_add(combo, "item,pressed", _combo_item_pressed_cb, pd);
   evas_object_smart_callback_add(combo, "expanded", _combo_expanded_cb, pd);
   evas_object_show(combo);
   elm_box_pack_end(bx, combo);

   tbl = elm_table_add(win);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   elm_table_align_set(tbl, 0, 0.5);
   evas_object_show(tbl);
   elm_box_pack_end(bx, tbl);

   rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(48));
   elm_table_pack(tbl, rec, 0, 0, 1, 1);

   ic = elm_icon_add(win);
   elm_icon_standard_set(ic, evisum_icon_path_get("sensor"));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(32), ELM_SCALE_SIZE(32));
   evas_object_show(ic);
   elm_table_pack(tbl, ic, 0, 0, 1, 1);
   elm_object_content_set(fr, bx);
   elm_box_pack_end(content, fr);

   pd->thermal_pb = pb = elm_progressbar_add(win);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   elm_progressbar_unit_format_set(pb, "%1.1f°C");
   evas_object_show(pb);
   elm_table_pack(tbl, pb, 1, 0, 1, 1);

   pd->itc = itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.content_get = _content_get;
   itc->func.text_get = NULL;
   itc->func.filter_get = NULL;
   itc->func.del = _item_del;

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

   _sensors_refresh(pd);

   ui->sensors.thread = ecore_thread_feedback_run(_sensors_update,
                                                  _sensors_update_feedback_cb,
                                                  NULL, NULL, pd, EINA_TRUE);
}

