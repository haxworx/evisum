#include "Enigmatic_Client.h"
#include <Elementary.h>

#define NOISY 0

static void
cb_event_change(Enigmatic_Client *client, Snapshot *s, void *data)
{
   char buf[32];
   struct tm tm_out;
   time_t t = (time_t) s->time;

   localtime_r((time_t *) &t, &tm_out);
   strftime(buf, sizeof(buf) - 1, "%Y-%m-%d %H:%M:%S", &tm_out);

   printf("%s\n", buf);

   Eina_List *l;
   Cpu_Core *core;

   EINA_LIST_FOREACH(s->cores, l, core)
     printf("%s\t=> %i%% => %i => %iC\n", core->name, core->percent, core->freq, core->temp);

   printf("Total: %"PRIu64" Used: %"PRIu64" Buffered: %"PRIu64" Cached: %"PRIu64" Shared: %"PRIu64" Swap Total %"PRIu64" Swap Used: %"PRIu64"\n",
          (s->meminfo.total >> 10), (s->meminfo.used >> 10), (s->meminfo.buffered >> 10),
          (s->meminfo.cached >> 10), (s->meminfo.shared >> 10), (s->meminfo.swap_total >> 10), (s->meminfo.swap_used >> 10));
   for (int i = 0; i < s->meminfo.video_count; i++)
     printf("Video: (%i) Total: %"PRIu64" Used: %"PRIu64"\n", i, s->meminfo.video[i].total, s->meminfo.video[i].used);

   Sensor *sensor;
   EINA_LIST_FOREACH(s->sensors, l, sensor)
     {
        printf("%s.%s %1.1f", sensor->name, sensor->child_name, sensor->value);
        if (sensor->type == THERMAL) printf("C\n");
        else printf("RPM\n");
     }

   Battery *battery;
   EINA_LIST_FOREACH(s->batteries, l, battery)
     printf("%s => %1.1f%% => %i of %i\n", battery->name, battery->percent, battery->charge_current, battery->charge_full);

   printf("AC: [%i]\n", s->power);

   Network_Interface *iface;
   EINA_LIST_FOREACH(s->network_interfaces, l, iface)
     printf("%s => %"PRIu64" => %"PRIu64"\n", iface->name, iface->total_in, iface->total_out);

   File_System *fs;
   EINA_LIST_FOREACH(s->file_systems, l, fs)
     printf("%s on %s => %"PRIu64" of %"PRIu64"\n", fs->mount, fs->path, (fs->usage.used >> 10), (fs->usage.total >> 10));

#if NOISY
   Proc_Info_Log *proc;
   EINA_LIST_FOREACH(s->processes, l, proc)
     printf("pid: %i cmd %s pri: %i nthreads: %i mem: %"PRIu64" cpu: %i%%\n",
            proc->pid, proc->path, proc->priority, proc->numthreads, proc->mem_size >> 10, proc->cpu_usage);
#endif
}

static void
cb_cpu_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   printf("add %s => %i\n", core->name, core->id);
}

static void
cb_cpu_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Cpu_Core *core = event->data;
   printf("del %s => %i\n", core->name, core->id);
}

static void
cb_battery_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Battery *bat = event->data;
   printf("add %s\n", bat->name);
}

static void
cb_battery_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Battery *bat = event->data;
   printf("del %s\n", bat->name);
}

static void
cb_power_supply_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Eina_Bool *power = event->data;
   printf("add [AC] %i\n", *power);
}

static void
cb_power_supply_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Eina_Bool *power = event->data;
   printf("del [AC] %i\n", *power);
}

static void
cb_sensor_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Sensor *sensor = event->data;
   printf("add %s.%s\n", sensor->name, sensor->child_name);
}

static void
cb_sensor_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Sensor *sensor = event->data;
   printf("del %s.%s\n", sensor->name, sensor->child_name);
}

static void
cb_network_iface_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Network_Interface *iface = event->data;
   printf("add %s\n", iface->name);
}

static void
cb_network_iface_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Network_Interface *iface = event->data;
   printf("del %s\n", iface->name);
}

static void
cb_file_system_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   File_System *fs = event->data;
   printf("add %s => %s\n", fs->mount, fs->path);
}

static void
cb_file_system_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   File_System *fs = event->data;
   printf("del %s => %s\n", fs->mount, fs->path);
}

static void
cb_process_add(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Proc_Info_Log *proc = event->data;
   printf("add %i => %s\n", proc->pid, proc->command);
}

static void
cb_process_del(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   Proc_Info_Log *proc = event->data;
   printf("del %i => %s\n", proc->pid, proc->command);
}

static void
cb_recording_delay(Enigmatic_Client *client, Enigmatic_Client_Event *event, void *data)
{
   int *delay = event->data;
   printf("gap %i => total %i secs\n", event->time, *delay);
}

static void
cb_event_change_init(Enigmatic_Client *client, Snapshot *s, void *data)
{
   enigmatic_client_event_callback_add(client, EVENT_CPU_ADD, cb_cpu_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_CPU_DEL, cb_cpu_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_BATTERY_ADD, cb_battery_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_BATTERY_DEL, cb_battery_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_POWER_SUPPLY_ADD, cb_power_supply_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_POWER_SUPPLY_DEL, cb_power_supply_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_SENSOR_ADD, cb_sensor_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_SENSOR_DEL, cb_sensor_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_NETWORK_IFACE_ADD, cb_network_iface_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_NETWORK_IFACE_DEL, cb_network_iface_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_FILE_SYSTEM_ADD, cb_file_system_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_FILE_SYSTEM_DEL, cb_file_system_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_PROCESS_ADD, cb_process_add, NULL);
   enigmatic_client_event_callback_add(client, EVENT_PROCESS_DEL, cb_process_del, NULL);
   enigmatic_client_event_callback_add(client, EVENT_RECORD_DELAY, cb_recording_delay, NULL);
}

static void
follow(void)
{
   if (!enigmatic_launch()) return;

   Enigmatic_Client *client = enigmatic_client_open();
   EINA_SAFETY_ON_NULL_RETURN(client);

   enigmatic_client_monitor_add(client, cb_event_change_init, cb_event_change, NULL);

   ecore_main_loop_begin();

   enigmatic_client_del(client);
}

static void
history(void)
{
   Enigmatic_Client *client = enigmatic_client_add();
   EINA_SAFETY_ON_NULL_RETURN(client);

   cb_event_change_init(client, NULL, NULL);
   enigmatic_client_snapshot_callback_set(client, cb_event_change, NULL);
   enigmatic_client_replay_time_start_set(client, time(NULL) - (2 * 3600));
   enigmatic_client_replay_time_end_set(client, time(NULL));
   enigmatic_client_replay(client);

   enigmatic_client_del(client);
}

int
elm_main(int argc, char **argv)
{
   if ((argc == 2) && (!strcasecmp(argv[1], "-F")))
     follow();
   else
     history();
   return 0;
}

ELM_MAIN();
