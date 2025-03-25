#include "evisum_ui_cpu.h"
#include "config.h"

// Templates for visualisations.
#include "visuals/visuals.x"

// config for colors/sizing
static void
_win_mouse_move_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Coord w, h;
   Evas_Event_Mouse_Move *ev;
   Ui_Cpu_Data *pd = data;

   ev = event_info;
   evas_object_geometry_get(obj, NULL, NULL, &w, &h);

   if ((ev->cur.canvas.x >= (w - 128)) && (ev->cur.canvas.y <= 128))
     {
       if (!pd->btn_visible)
         {
            elm_object_signal_emit(pd->btn_menu, "menu,show", "evisum/menu");
            pd->btn_visible = 1;
         }
     }
   else if ((pd->btn_visible) && (!pd->menu))
    {
       elm_object_signal_emit(pd->btn_menu, "menu,hide", "evisum/menu");
       pd->btn_visible = 0;
    }
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Evisum_Ui *ui;

   ui = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     {
        evas_object_del(ui->cpu.win);
     }
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Ui_Cpu_Data *pd = data;

   ui = pd->ui;
   if (!pd->menu)
     pd->menu = evisum_ui_main_menu_create(ui, ui->cpu.win, obj);
   else
     {
        evas_object_del(pd->menu);
        pd->menu = NULL;
     }
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, NULL, NULL, &ui->cpu.width, &ui->cpu.height);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   evas_object_geometry_get(obj, &ui->cpu.x, &ui->cpu.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui_Cpu_Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);

   if (pd->ext_free_cb)
      pd->ext_free_cb(pd->ext);

   free(pd);
   ui->cpu.win = NULL;
}

Eina_List *
evisum_ui_cpu_visuals_get(void)
{
   Eina_List *l = NULL;

   for (int i = 0; (i < sizeof(visualizations) / sizeof(Cpu_Visual)); i++)
     {
        l = eina_list_append(l, strdup(visualizations[i].name));
     }

   return l;
}

Cpu_Visual *
evisum_ui_cpu_visual_by_name(const char *name)
{
   for (int i = 0; (i < sizeof(visualizations) / sizeof(Cpu_Visual)); i++)
     {
        if (!strcmp(name, visualizations[i].name))
          return &visualizations[i];
     }
   return NULL;
}

void
evisum_ui_cpu_win_restart(Evisum_Ui *ui)
{
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_NONE);
   evas_object_del(ui->cpu.win);
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);
   evisum_ui_cpu_win_add(ui);
}

void
evisum_ui_cpu_win_add(Evisum_Ui *ui)
{
   Ui_Cpu_Data *pd;
   Evas_Object *win, *box, *scr, *btn, *ic;
   Evas_Object *tb;
   Elm_Layout *lay;
   Cpu_Visual *vis;

   if (ui->cpu.win)
     {
        elm_win_raise(ui->cpu.win);
        return;
     }

   ui->cpu.win = win = elm_win_util_standard_add("evisum",
                   _("CPU Activity"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   scr = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scr, FILL, FILL);
   elm_scroller_policy_set(scr,
                           ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scr);

   tb = elm_table_add(win);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_show(tb);

   box = elm_box_add(win);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   elm_table_pack(tb, box, 0, 0, 1, 1);

   vis = evisum_ui_cpu_visual_by_name(ui->cpu.visual);
   if (!vis)
     {
        fprintf(stderr, "FATAL: unknown CPU visual (check your config)\n");
        exit(1);
     }
   pd = vis->func(box);
   pd->win = win;
   pd->ui = ui;

   elm_object_content_set(scr, tb);
   elm_object_content_set(win, scr);

   btn = elm_button_add(win);
   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get("menu"));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);
   elm_object_focus_allow_set(btn, 0);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   evas_object_smart_callback_add(btn, "clicked", _btn_menu_clicked_cb, pd);

   pd->btn_menu = lay = elm_layout_add(win);
   evas_object_size_hint_weight_set(lay, 1.0, 1.0);
   evas_object_size_hint_align_set(lay, 0.99, 0.01);
   elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
   elm_layout_content_set(lay, "evisum/menu", btn);
   elm_table_pack(tb, lay, 0, 0, 1, 1);
   evas_object_show(lay);

   evas_object_event_callback_add(scr, EVAS_CALLBACK_MOUSE_MOVE, _win_mouse_move_cb, pd);
   evas_object_event_callback_add(scr, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE,  _win_move_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);

   if ((ui->cpu.width > 0) && (ui->cpu.height > 0))
     evas_object_resize(win, ui->cpu.width, ui->cpu.height);
   else
     evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.8), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT * 1.2));

   if ((ui->cpu.x > 0) && (ui->cpu.y > 0))
     evas_object_move(win, ui->cpu.x, ui->cpu.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);
}

