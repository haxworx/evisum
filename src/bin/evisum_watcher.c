#include <Eina.h>
#include <Ecore.h>
#include "next/machine.h"

static Eina_Thread *background_thread = NULL;
static Eina_List   *cores = NULL;
static Eina_List   *batteries = NULL;
static Eina_List   *sensors = NULL;
static Eina_List   *network_interfaces = NULL;

#define SECOND 1000000
#define INTERVAL 16
#define SLEEP_DURATION SECOND / INTERVAL
#define CHECK_EVERY_SECOND(n) ((!n) || (!(n % INTERVAL))) 

static void
_cb_background_thread(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   int64_t poll_count = 0;

   while (!ecore_thread_check(thread))
     {
        if (CHECK_EVERY_SECOND(poll_count)) {}

        usleep(SLEEP_DURATION);
	poll_count++;
     }
}

int
main(int argc, char **argv)
{
   ecore_init();

   background_thread = ecore_thread_run(_cb_background_thread, NULL, NULL, NULL);

   ecore_main_loop_begin();

   ecore_shutdown();

   return 0;
}
