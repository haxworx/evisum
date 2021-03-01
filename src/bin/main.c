/*
 * Copyright 2018-2021. Alastair Roy Poole <netstar@gmail.com>
 *
 * See COPYING file for details.
 */

#include "config.h"
#include "evisum_config.h"
#include "evisum_server.h"
#include "ui/ui.h"

static Eina_Bool
_shutdown_cb(void *data, int type, void *event EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->cpu.win) evas_object_del(ui->cpu.win);
   if (ui->mem.win) evas_object_del(ui->mem.win);
   if (ui->disk.win) evas_object_del(ui->disk.win);
   if (ui->sensors.win) evas_object_del(ui->sensors.win);
   if (ui->proc.win) evas_object_del(ui->proc.win);

   return 0;
}

static void
_signals(Ui *ui)
{
   ui->handler_sig = ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, _shutdown_cb, ui);
}

int
elm_main(int argc, char **argv)
{
   Ui *ui;
   int i, pid = -1;
   Evisum_Action action = EVISUM_ACTION_DEFAULT;

   for (i = 0; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help") || !strcasecmp(argv[i], "-v")))
          {
             printf("Usage: evisum [OPTIONS]\n"
                    "   Where OPTIONS can be one of\n"
                    "      -c\n"
                    "        Launch CPU view.\n"
                    "      -m\n"
                    "        Launch memory view.\n"
                    "      -d\n"
                    "        Launch storage view.\n"
                    "      -s\n"
                    "        Launch sensors view.\n"
                    "      -p <pid>\n"
                    "        Launch process view for pid.\n"
                    "      -h | -help | --help\n"
                    "        This menu.\n"
                    "   No arguments will launch the process explorer.\n");
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

#if 0
   int n;
   net_iface_t **ifaces = system_network_ifaces_get(&n);
   for (int i = 0; i < n; i++) {
       printf("%s\n", ifaces[i]->name);
       free(ifaces[i]);
   }
   if (ifaces) free(ifaces);
   exit(1);
#endif
   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

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

   _signals(ui);

   evisum_server_init(ui);
   evisum_ui_activate(ui, action, pid);

   ecore_main_loop_begin();

   ecore_event_handler_del(ui->handler_sig);
   evisum_ui_shutdown(ui);
   evisum_server_shutdown();

   return 0;
}

ELM_MAIN()
