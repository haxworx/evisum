/* 
 * Copyright 2018-2019. Alastair Poole <netstar@gmail.com>
 * See LICENSE file for details.
 */

#define VERSION "0.2.6"

#include "process.h"
#include "system.h"
#include "ui.h"

static void
_win_del_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   exit(0);
}

static Eina_Bool
_win_add(void)
{
   Ui *ui;
   Evas_Object *win, *icon;

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   win = elm_win_util_standard_add("evisum", "evisum");
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "evisum");
   elm_win_icon_object_set(win, icon);
   evas_object_resize(win, EVISUM_SIZE_WIDTH * elm_config_scale_get(), EVISUM_SIZE_HEIGHT * elm_config_scale_get());
   elm_win_title_set(win, "System Status");
   elm_win_center(win, EINA_TRUE, EINA_TRUE);

   ui = ui_add(win);
   if (!ui)
     return EINA_FALSE;

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evas_object_show(win);

   return EINA_TRUE;
}

int
main(int argc, char **argv)
{
   eina_init();
   ecore_init();
   elm_init(argc, argv);

   if (_win_add())
     ecore_main_loop_begin();

   elm_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

