#include <Eina.h>
#include <Ecore.h>
#include "next/machine.h"

static Ecore_Timer *background_timer;
static Eina_List   *cores = NULL;
static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;

static Eina_Bool
_cb_background_timer(void *data EINA_UNUSED)
{
   static int64_t poll_count = 0;
   static double t_prev = 0;
   double t;

   poll_count++;

   t = ecore_loop_time_get();

   if (t_prev) printf("%1.4f\n", t - t_prev);
   t_prev = t; 

   return 1;
}

int
main(int argc, char **argv)
{
   ecore_init();

   background_timer = ecore_timer_add(0.025, _cb_background_timer, NULL);

   ecore_main_loop_begin();

   ecore_shutdown();

   return 0;
}
