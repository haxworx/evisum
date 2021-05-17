#include "next/machine.h"
#include <Ecore.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;

int
main(int argc, char **argv)
{
   ecore_init();

   puts("CORES:");

   Cpu_Core **cores;

   int ncpu = 0;
   cores = system_cpu_usage_delayed_get(&ncpu, 1000000);
   for (int i = 0; i < ncpu; i++)
     {
        printf("core %i = %1.2f%%\n", cores[i]->id, cores[i]->percent);
        free(cores[i]);
     }
   free(cores);

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
