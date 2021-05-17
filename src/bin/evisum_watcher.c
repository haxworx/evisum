#include "next/machine.h"
#include <Ecore.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;
static Eina_List   *cores = NULL;

int
main(int argc, char **argv)
{
   Eina_List *l;
   Cpu_Core *core;
   ecore_init();

   puts("CORES:");

   cores = cores_find();
   for (int i = 0; i < 10; i++)
     {
        cores_check(cores);
        EINA_LIST_FOREACH(cores, l, core)
          {
             printf("core %i = %1.2f%%\n", core->id, core->percent);
          }
	usleep(1000000);
     }
   EINA_LIST_FREE(cores, core)
     free(core);

   puts("BATTERIES:");
   Battery *bat;

   batteries = batteries_find();
   EINA_LIST_FREE(batteries, bat)
     {
        battery_check(bat);
        printf("battery %s (%s) => %1.2f\n", bat->model, bat->vendor, bat->percent);
        battery_free(bat);
     }

   printf("POWER: %i\n", power_ac_check());

   puts("SENSORS:");
   Sensor *sensor;

   sensors = sensors_find();
   EINA_LIST_FREE(sensors, sensor)
     {
        if (sensor_check(sensor))
          printf("sensor %s (%s) = %1.1f", sensor->name, sensor->child_name, sensor->value);
        if (sensor->type == FANRPM)
          printf("RPM\n");
        else if (sensor->type == THERMAL)
          printf("C\n");
        else
          {
             printf(" - UNHANDLED!\n");
             exit(1);
          }
        sensor_free(sensor);
     }

   puts("NETWORK:");

   Network_Interface *iface;

   network_interfaces = network_interfaces_find();
   EINA_LIST_FREE(network_interfaces, iface)
     {
        printf("name => %s => %"PRIi64" %"PRIi64"\n", iface->name, iface->total_in, iface->total_out);
        free(iface);
     }

   ecore_shutdown();

   return 0;
}
