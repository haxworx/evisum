#include "next/machine.h"
#include <Ecore.h>

static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;

int
main(int argc, char **argv)
{
   Eina_List *l;
   Cpu_Core **cores;
   Battery *bat;
   Sensor *sensor;
   Network_Interface *iface;

   ecore_init();

   int ncpu = 0;
   cores = system_cpu_usage_delayed_get(&ncpu, 1000000);
   for (int i = 0; i < ncpu; i++)
     printf("core %i = %1.2f%\n", cores[i]->id, cores[i]->percent);

   batteries = batteries_find();
   EINA_LIST_FOREACH(batteries, l, bat)
     {
        battery_check(bat);
        printf("battery %s (%s) => %1.2f\n", bat->name, bat->vendor, bat->percent);
     }
   EINA_LIST_FREE(batteries, bat)
     battery_free(bat);

   printf("POWER %i\n", power_ac());

   sensors = sensors_find();
   EINA_LIST_FREE(sensors, sensor)
     {
        if (sensor_check(sensor))
          printf("sensor %s = %1.2f\n", sensor->name, sensor->value);
        sensor_free(sensor);
     }

   network_interfaces = network_interfaces_find();
   EINA_LIST_FREE(network_interfaces, iface)
     {
        printf("name => %s => %i %i\n", iface->name, iface->total_in, iface->total_out);
        free(iface);
     }

   ecore_shutdown();

   return 0;
}
