/* 
 * Copyright 2018-2019. Alastair Poole <netstar@gmail.com>
 *
 * http://haxlab.org.
 *
 * See LICENSE file for details.
 */

#include "config.h"
#include "ui.h"

static void
_win_del_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   exit(0);
}

static Ui *
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
   elm_win_title_set(win, "System Activity");
   elm_win_center(win, EINA_TRUE, EINA_TRUE);

   ui = ui_add(win);
   if (!ui)
     return NULL;

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evas_object_show(win);

   return ui;
}

int
main(int argc, char **argv)
{
   Ui *ui;

   eina_init();
   ecore_init();
   elm_init(argc, argv);

   ui = _win_add();
   if (ui)
     {
        ecore_main_loop_begin();
        free(ui);
     }

   elm_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

