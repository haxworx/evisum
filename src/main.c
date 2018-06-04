/* Copyright 2018. Alastair Poole <netstar@gmail.com>
   See LICENSE file for details.
*/

#include "process.h"
#include "system.h"
#include "ui.h"

static void
_win_del_cb(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_del(obj);
   ecore_main_loop_quit();
}

static Evas_Object *
_win_add(void)
{
   Evas_Object *win, *icon;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   win = elm_win_util_standard_add("esysinfo", "esysinfo");
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "system-preferences");
   elm_win_icon_object_set(win, icon);

   evas_object_resize(win, 768 * elm_config_scale_get(), 420 * elm_config_scale_get());
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, NULL);

   elm_win_title_set(win, "System Information");

   return win;
}

int
main(int argc, char **argv)
{
   Evas_Object *win;

   eina_init();
   ecore_init();
   elm_init(argc, argv);

   win = _win_add();
   ui_add(win);

   elm_win_center(win, EINA_TRUE, EINA_TRUE);
   evas_object_show(win);

   ecore_main_loop_begin();

   eina_shutdown();
   ecore_shutdown();
   elm_shutdown();

   return 0;
}

