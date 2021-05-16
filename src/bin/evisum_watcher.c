#include "next/machine.h"

static Eina_List   *cores = NULL;
static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;

int
main(int argc, char **argv)
{
   Eina_List *l;
   Battery *bat;

   ecore_init();

   batteries = batteries_find();
   EINA_LIST_FOREACH(batteries, l, bat)
     {
        battery_check(bat);
        printf("battery %s (%s) => %1.2f\n", bat->name, bat->vendor, bat->percent);
     }
   EINA_LIST_FREE(batteries, bat)
     battery_free(bat);

   ecore_shutdown();

   return 0;
}
