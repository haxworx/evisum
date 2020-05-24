/*
 * Copyright 2018-2019. Alastair Poole <netstar@gmail.com>
 *
 * See LICENSE file for details.
 */

#include "config.h"
#include "evisum_config.h"
#include "ui/ui.h"

static void
_win_del_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
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
   evas_object_resize(win, EVISUM_SIZE_WIDTH  * elm_config_scale_get(),
                           EVISUM_SIZE_HEIGHT * elm_config_scale_get());
   elm_win_title_set(win, _("EFL System Monitor"));
   elm_win_center(win, EINA_TRUE, EINA_TRUE);

   ui = evisum_ui_add(win);
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
   int i;

   for (i = 0; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help") || !strcasecmp(argv[i], "-v")))
          {
             printf("(c) 2018-2020 Alastair Poole <netstar@gmail.com>\n");
             printf("Evisum version: %s\n", PACKAGE_VERSION);
             exit(0);
          }
     }

   eina_init();
   ecore_init();
   config_init();
   elm_init(argc, argv);

#if ENABLE_NLS
   setlocale(LC_ALL, "");
   bindtextdomain(PACKAGE, LOCALEDIR);
   bind_textdomain_codeset(PACKAGE, "UTF-8");
   textdomain(PACKAGE);
#endif

   ui = _win_add();
   if (ui)
     {
        ecore_main_loop_begin();
        evisum_ui_shutdown(ui);
        free(ui);
     }

   elm_shutdown();
   config_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

