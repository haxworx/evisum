/*
 * Copyright 2018-2019. Alastair Roy Poole <netstar@gmail.com>
 *
 * See COPYING file for details.
 */

#define DEVELOPMENT 1

#include "config.h"
#include "evisum_config.h"
#include "ui/ui.h"

#if defined(DEVELOPMENT)
# include "system/machine.h"
# include "system/process.h"
# include "system/disks.h"
# include "system/filesystems.h"
#endif

static void
_win_del_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evisum_ui_shutdown(ui);
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
   evas_object_resize(win, EVISUM_WIN_WIDTH * elm_config_scale_get(),
                   EVISUM_WIN_HEIGHT * elm_config_scale_get());
   elm_win_title_set(win, _("EFL System Monitor"));
   elm_win_center(win, EINA_TRUE, EINA_TRUE);

   ui = evisum_ui_add(win);
   if (!ui)
     return NULL;

   ui->state.shutdown_now = EINA_TRUE;

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evas_object_show(win);

   return ui;
}

#if defined(DEVELOPMENT)
static void
_test(int all)
{
   Sys_Info *inf;
   Eina_List *procs, *disks;
   Proc_Info *proc;
   File_System *fs;
   char *path, *mount;

   printf("Starting testing\n");
   inf = system_info_all_get();
   int cpu_count = system_cpu_count_get();
   for (int i = 0; i < cpu_count; i++)
     {
        int temp = system_cpu_n_temperature_get(i);
        if (temp != -1) printf(" cpu %d temp %d C\n", i, temp);
     }

   system_info_all_free(inf);

   if (!all) goto out;

   eina_init();
   ecore_init();

   procs = proc_info_all_get();
   EINA_LIST_FREE(procs, proc)
     proc_info_free(proc);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        mount = disk_mount_point_get(path);
        if (mount)
          {
             fs = file_system_info_get(mount);
             if (fs)
               file_system_info_free(fs);
             free(mount);
          }
        free(path);
     }

   ecore_shutdown();
   eina_shutdown();
out:
   printf("Ending testing\n");
}
#endif

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
             printf("(c) 2018-2020 Alastair Roy Poole <netstar@gmail.com>\n");
             printf("Evisum version: %s\n", PACKAGE_VERSION);
             exit(0);
          }
#if defined(DEVELOPMENT)
        else if (!strcmp(argv[i], "-t"))
          {
             _test(1);
             exit(0);
          }
        else if (!strcmp(argv[i], "-T"))
          {
             _test(0);
             exit(0);
          }
#endif
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
        evisum_ui_del(ui);
     }

   elm_shutdown();
   config_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

