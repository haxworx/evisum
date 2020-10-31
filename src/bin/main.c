/*
 * Copyright 2018-2019. Alastair Roy Poole <netstar@gmail.com>
 *
 * See COPYING file for details.
 */

#include "config.h"
#include "evisum_config.h"
#include "evisum_server.h"
#include "ui/ui.h"

int
main(int argc, char **argv)
{
   Ui *ui;
   int i, pid = -1;
   Evisum_Action action = EVISUM_ACTION_DEFAULT;

   for (i = 0; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help") || !strcasecmp(argv[i], "-v")))
          {
             printf("(c) 2018-2020 Alastair Roy Poole <netstar@gmail.com>\n");
             printf("Evisum version: %s\n", PACKAGE_VERSION);
             exit(0);
          }
        else if (!strcmp(argv[i], "-c"))
          action = EVISUM_ACTION_CPU;
        else if (!strcmp(argv[i], "-m"))
          action = EVISUM_ACTION_MEM;
        else if (!strcmp(argv[i], "-d"))
          action = EVISUM_ACTION_STORAGE;
        else if (!strcmp(argv[i], "-s"))
          action = EVISUM_ACTION_SENSORS;
        else if (!strcmp(argv[i], "-p") && i < (argc -1))
          {
             action = EVISUM_ACTION_PROCESS;
             pid = atoi(argv[i+1]);
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

   if (evisum_server_client_add(action, pid))
     return 0;

   ui = evisum_ui_init();
   if (!ui) return 1;

   evisum_server_init(ui);
   evisum_ui_activate(ui, action, pid);

   ecore_main_loop_begin();

   evisum_ui_del(ui);
   evisum_server_shutdown();

   elm_shutdown();
   config_shutdown();
   ecore_shutdown();
   eina_shutdown();

   return 0;
}

