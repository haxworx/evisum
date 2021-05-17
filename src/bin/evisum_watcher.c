#include "next/machine.h"
#include <Ecore.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;
static Eina_List   *cores = NULL;

static int
test(void)
{
   Eina_List *l;
   Cpu_Core *core;
   ecore_init();

   puts("CORES:");

   printf("total: %i online %i\n", cores_count(), cores_online_count());

   cores = cores_find();
   for (int i = 0; i < 10; i++)
     {
        cores_update(cores);
        EINA_LIST_FOREACH(cores, l, core)
          {
             printf("core %i = %1.2f%%\t%1.2fMHz %iC\n", core->id, core->percent,
                    (double) core_id_frequency(core->id) / 1000, core_id_temperature(core->id));
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
        battery_update(bat);
        printf("battery %s (%s) => %1.2f\n", bat->model, bat->vendor, bat->percent);
        battery_free(bat);
     }

   printf("POWER: %i\n", power_ac_present());

   puts("SENSORS:");
   Sensor *sensor;

   sensors = sensors_find();
   EINA_LIST_FREE(sensors, sensor)
     {
        if (sensor_update(sensor))
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

int main(int argc, char **argv)
{
   return test();
}
