#include "system/machine.h"
#include "power.h"
#include "enigmatic_log.h"

static Eina_Bool     power_ac_state = 0;
static Ecore_Thread *thread = NULL;
static Eina_Lock     power_lock;

static void
power_refresh(Enigmatic *enigmatic, Eina_Bool *ac)
{
   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = POWER;
   msg.number = 1;
   enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, ac, sizeof(Eina_Bool));
}

static void
power_thread(void *data EINA_UNUSED, Ecore_Thread *thread)
{
#if (EFL_VERSION_MAJOR >= 1 && EFL_VERSION_MINOR >= 26)
   ecore_thread_name_set(thread, "powermon");
#endif
   while (!ecore_thread_check(thread))
     {
        eina_lock_take(&power_lock);
        power_ac_state = power_ac_present();
        eina_lock_release(&power_lock);

        for (int i = 0; i < 20; i++)
          {
             if (ecore_thread_check(thread)) break;
             usleep(50000);
          }
     }
}

void
enigmatic_monitor_power_init(Enigmatic *enigmatic)
{
   eina_lock_new(&power_lock);
   power_ac_state = power_ac_present();
   enigmatic->power_thread = thread = ecore_thread_run(power_thread, NULL, NULL, NULL);
}

void
enigmatic_monitor_power_shutdown(void)
{
   eina_lock_take(&power_lock);
   eina_lock_release(&power_lock);
   eina_lock_free(&power_lock);
}

Eina_Bool
enigmatic_monitor_power(Enigmatic *enigmatic, Eina_Bool *ac_prev)
{
   Eina_Bool ac, changed = 0;

   if (eina_lock_take_try(&power_lock) != EINA_LOCK_SUCCEED) return 0;

   ac = power_ac_state;

   if (enigmatic->broadcast)
     {
        power_refresh(enigmatic, &ac);
        if (*ac_prev != ac) changed = 1;
        *ac_prev = ac;
        eina_lock_release(&power_lock);
        return changed;
     }
   if (ac != *ac_prev)
     {
        DEBUG("power %i", ac);
        Message msg;
        msg.type = MESG_MOD;
        msg.object_type = POWER_VALUE;
        Change change = CHANGE_I8;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &ac, 1);
        *ac_prev = ac;
        changed = 1;
     }
   eina_lock_release(&power_lock);
   return changed;
}

