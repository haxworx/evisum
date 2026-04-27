#include "Enigmatic.h"
#include "enigmatic_config.h"
#include "monitor/monitor.h"
#include "enigmatic_server.h"
#include "enigmatic_query.h"
#include "enigmatic_log.h"

static int lock_fd = -1;

#define DEBUGTIME 0

static void
system_info_free(System_Info *info)
{
   eina_hash_free(info->cores);
   eina_hash_free(info->sensors);
   eina_hash_free(info->batteries);
   eina_hash_free(info->network_interfaces);
   eina_hash_free(info->file_systems);
   eina_hash_free(info->processes);
}

static Eina_Bool
cb_shutdown(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Signal_Exit *ev = event;
   Enigmatic *enigmatic = data;

   if ((!ev) || (!ev->terminate))
     return ECORE_CALLBACK_RENEW;

   if (enigmatic->thread)
     {
        ecore_thread_cancel(enigmatic->thread);
        ecore_thread_wait(enigmatic->thread, 0.5);
     }

   ecore_thread_cancel(enigmatic->battery_thread);
   ecore_thread_cancel(enigmatic->sensors_thread);
   ecore_thread_cancel(enigmatic->power_thread);

   ecore_thread_wait(enigmatic->battery_thread, 0.5);
   ecore_thread_wait(enigmatic->sensors_thread, 0.5);
   ecore_thread_wait(enigmatic->power_thread, 0.5);

   ecore_event_handler_del(enigmatic->handler);

   ecore_main_loop_quit();

   return ECORE_CALLBACK_DONE;
}

static void
enigmatic_system_monitor(void *data, Ecore_Thread *thread)
{
   System_Info *info;
   struct timespec ts;
   Enigmatic *enigmatic = data;

   enigmatic->info = info = calloc(1, sizeof(System_Info));
   EINA_SAFETY_ON_NULL_RETURN(enigmatic->info);

#if (EFL_VERSION_MAJOR >= 1 && EFL_VERSION_MINOR >= 26)
   ecore_thread_name_set(thread, "logger");
#endif

   while (!ecore_thread_check(thread))
     {
        clock_gettime(CLOCK_REALTIME, &ts);
        enigmatic->poll_time = ts.tv_sec;

        int64_t tdiff = ts.tv_nsec + (ts.tv_sec * 1000000000);

        if (enigmatic_log_rotate(enigmatic))
          enigmatic->broadcast = 1;

        if (enigmatic->broadcast)
          ENIGMATIC_LOG_HEADER(enigmatic, EVENT_BROADCAST);

        if (enigmatic->interval == INTERVAL_NORMAL)
          enigmatic_monitor_cores(enigmatic, &info->cores);

        if ((enigmatic->broadcast) || (!(enigmatic->poll_count % 10)))
          {
             if (enigmatic->interval != INTERVAL_NORMAL)
               enigmatic_monitor_cores(enigmatic, &info->cores);

             enigmatic_monitor_memory(enigmatic, &info->meminfo);
             enigmatic_monitor_sensors(enigmatic, &info->sensors);
             enigmatic_monitor_power(enigmatic, &info->power);
             enigmatic_monitor_batteries(enigmatic, &info->batteries);
             enigmatic_monitor_network_interfaces(enigmatic, &info->network_interfaces);
             enigmatic_monitor_file_systems(enigmatic, &info->file_systems);
             enigmatic_monitor_processes(enigmatic, &info->processes);

             ENIGMATIC_LOG_HEADER(enigmatic, EVENT_BLOCK_END);

             eina_lock_take(&enigmatic->update_lock);
             if (enigmatic->interval != enigmatic->interval_update)
               enigmatic->interval = enigmatic->interval_update;
             eina_lock_release(&enigmatic->update_lock);
          }

        // flush to disk.
        enigmatic_log_crush(enigmatic);

        enigmatic->broadcast = 0;
        if ((enigmatic->poll_count) && (!(enigmatic->poll_count % (enigmatic->device_refresh_interval / enigmatic->interval))))
          enigmatic->broadcast = 1;

        enigmatic->poll_count++;

        clock_gettime(CLOCK_REALTIME, &ts);
        int usecs = (((ts.tv_sec * 1000000000) + ts.tv_nsec) - tdiff) / 1000;
        if (usecs > 100000) usecs = 100000;

        usleep(((enigmatic->interval * 1000000) / 10) - usecs);
#if DEBUGTIME
        printf("usecs is %i\n", usecs);
        clock_gettime(CLOCK_REALTIME, &ts);
        printf("want %i00000 got %ld\n", enigmatic->interval, (((ts.tv_sec * 1000000000) + ts.tv_nsec) - tdiff) / 1000);
#endif
     }

   system_info_free(info);
   free(info);
}

static void
enigmatic_init(Enigmatic *enigmatic)
{
   eina_lock_new(&enigmatic->update_lock);
   enigmatic->pid = getpid();
   enigmatic_pidfile_create(enigmatic);

   enigmatic->device_refresh_interval = 900 * 10;
   enigmatic->log.hour = -1;
   enigmatic->interval = enigmatic->interval_update = INTERVAL_NORMAL;
   enigmatic->broadcast = 1;

   enigmatic_config_init();
   enigmatic->config = enigmatic_config_load();

   enigmatic_server_init(enigmatic);

   enigmatic_log_open(enigmatic);

   enigmatic_monitor_batteries_init();
   enigmatic_monitor_sensors_init();
   enigmatic_monitor_power_init(enigmatic);
}

static void
enigmatic_shutdown(Enigmatic *enigmatic)
{
   void *id;

   enigmatic_log_close(enigmatic);

   EINA_LIST_FREE(enigmatic->unique_ids, id)
     free(id);

   enigmatic_monitor_batteries_shutdown();
   enigmatic_monitor_sensors_shutdown();
   enigmatic_monitor_power_shutdown();

   enigmatic_server_shutdown(enigmatic);

   enigmatic_config_save(enigmatic->config);
   enigmatic_config_shutdown();

   enigmatic_pidfile_delete(enigmatic);
   eina_lock_free(&enigmatic->update_lock);
}

void
cb_exit(void)
{
   enigmatic_log_unlock(lock_fd);
}

void
usage(void)
{
   printf("%s [OPTIONS]\n"
          "Where OPTIONS can be one of: \n"
          "   -s                 Stop enigmatic daemon.\n"
          "   -p                 Ping enigmatic daemon.\n"
          "   --interval-normal  Set enigmatic daemon poll interval (normal).\n"
          "   --interval-medium  Set enigmatic daemon poll interval (medium).\n"
          "   --interval-slow    Set enigmatic daemon poll interval (slow).\n"
          "   -v | --version     Enigmatic version.\n"
          "   -h | --help        This menu.\n",
          PACKAGE);
   exit(0);
}

int main(int argc, char **argv)
{
   for (int i = 1; i < argc; i++)
     {
        if ((!strcasecmp(argv[i], "-h")) || (!strcasecmp(argv[i], "--help")))
          usage();
        else if ((!strcasecmp(argv[i], "-v")) || (!strcasecmp(argv[i], "--version")))
          enigmatic_about();
        else if (!strcmp(argv[i], "-p"))
          exit(!enigmatic_query_send("PING"));
        else if (!strcmp(argv[i], "--interval-normal"))
          exit(!enigmatic_query_send("interval-normal"));
        else if (!strcmp(argv[i], "--interval-medium"))
          exit(!enigmatic_query_send("interval-medium"));
        else if (!strcmp(argv[i], "--interval-slow"))
          exit(!enigmatic_query_send("interval-slow"));
        else if (!strcmp(argv[i], "-s"))
          {
             if (enigmatic_query_send("STOP"))
               exit(0);
             exit(!enigmatic_terminate());
          }
     }

   lock_fd = enigmatic_log_lock();
   atexit(cb_exit);

   ecore_init();

   Enigmatic *enigmatic = calloc(1, sizeof(Enigmatic));
   EINA_SAFETY_ON_NULL_RETURN_VAL(enigmatic, 1);
   enigmatic->lock_fd = lock_fd;

   enigmatic_init(enigmatic);

   enigmatic->handler = ecore_event_handler_add(ECORE_EVENT_SIGNAL_EXIT, cb_shutdown, enigmatic);

   enigmatic->thread = ecore_thread_run(enigmatic_system_monitor, NULL, NULL, enigmatic);

   ecore_main_loop_begin();

   enigmatic_shutdown(enigmatic);

   free(enigmatic);

   ecore_shutdown();

   return 0;
}
