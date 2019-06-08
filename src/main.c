/* Copyright 2018. Alastair Poole <netstar@gmail.com>
   See LICENSE file for details.
 */

#define VERSION "0.2.0"

#include "process.h"
#include "system.h"
#include "ui.h"

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ui *ui;

   ui = data;

   ui_shutdown(ui);
}

static Evas_Object *
_win_add(void)
{
   Ui *ui;
   Evas_Object *win, *icon;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   win = elm_win_util_standard_add("evisum", "evisum");
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "evisum");
   elm_win_icon_object_set(win, icon);
   evas_object_resize(win, 400 * elm_config_scale_get(), 450 * elm_config_scale_get());
   elm_win_title_set(win, "System Information");
   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_show(win);

   ui = ui_add(win);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);

   return win;
}

int
main(int argc, char **argv)
{
   eina_init();
   ecore_init();
   elm_init(argc, argv);

   _win_add();

   ecore_main_loop_begin();

   elm_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

