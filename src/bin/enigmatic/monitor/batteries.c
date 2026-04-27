#include "system/machine.h"
#include "batteries.h"
#include "uid.h"
#include "enigmatic_log.h"

static Eina_List *batteries = NULL;
static Eina_Lock batteries_lock;
static Ecore_Thread *thread = NULL;

static void
cb_battery_free(void *data)
{
   Battery *bat = data;

   DEBUG("del %s", bat->name);

   free(bat);
}

static int
cb_battery_cmp(const void *a, const void *b)
{
   Battery *bat1, *bat2;

   bat1 = (Battery *) a;
   bat2 = (Battery *) b;

   return strcmp(bat1->name, bat2->name);
}

static void
batteries_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL;
   void *d = NULL;
   Battery *bat;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        bat = d;
        ordered = eina_list_append(ordered, bat);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_battery_cmp);

   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = BATTERY;
   msg.number = n;
   enigmatic_log_list_write(enigmatic, EVENT_MESSAGE, msg, ordered, sizeof(Battery));
   eina_list_free(ordered);
}

static void
battery_thread(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   Battery *bat;
   uint32_t it = 0;

#if (EFL_VERSION_MAJOR >= 1 && EFL_VERSION_MINOR >= 26)
   ecore_thread_name_set(thread, "batmon");
#endif

   while (!ecore_thread_check(thread))
     {
        eina_lock_take(&batteries_lock);
        if ((it) && (!(it % 2)))
          {
             EINA_LIST_FREE(batteries, bat)
               free(bat);
             batteries = batteries_find();
          }
        batteries_update(batteries);
        eina_lock_release(&batteries_lock);

        for (int i = 0; i < 20; i++)
          {
             if (ecore_thread_check(thread)) break;
             usleep(50000);
          }
        it++;
     }
   EINA_LIST_FREE(batteries, bat)
     free(bat);
}

void
enigmatic_monitor_batteries_init(void)
{
   eina_lock_new(&batteries_lock);

   batteries = batteries_find();
   batteries_update(batteries);
}

void
enigmatic_monitor_batteries_shutdown(void)
{
   eina_lock_take(&batteries_lock);
   eina_lock_release(&batteries_lock);
   eina_lock_free(&batteries_lock);
}

Eina_Bool
enigmatic_monitor_batteries(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l;
   Battery *bat, *b;
   Eina_Bool changed = 0;

   if (eina_lock_take_try(&batteries_lock) != EINA_LOCK_SUCCEED) return 0;

   if (!*cache_hash)
     {
        *cache_hash = eina_hash_string_superfast_new(cb_battery_free);
        EINA_LIST_FOREACH(batteries, l, bat)
          {
             b = malloc(sizeof(Battery));
             if (b)
               {
                  DEBUG("battery add: %s", bat->name);

                  memcpy(b, bat, sizeof(Battery));
                  b->unique_id = unique_id_find(&enigmatic->unique_ids);
                  eina_hash_add(*cache_hash, b->name, b);
               }
          }
        enigmatic->battery_thread = thread = ecore_thread_run(battery_thread, NULL, NULL, NULL);
     }

   if (enigmatic->broadcast)
     {
        batteries_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Battery *b = d;
        Eina_Bool found = 0;
        EINA_LIST_FOREACH(batteries, l, bat)
          {
             if (!strcmp(b->name, bat->name))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, b);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, bat)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = BATTERY;
        msg.number = bat->unique_id;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        unique_id_release(&enigmatic->unique_ids, bat->unique_id);
        eina_hash_del(*cache_hash, bat->name, NULL);
     }
   EINA_LIST_FOREACH(batteries, l, bat)
     {
        b = eina_hash_find(*cache_hash, bat->name);
        if (!b)
          {
             Battery *new_bat = calloc(1, sizeof(Battery));
             if (new_bat)
               {
                  memcpy(new_bat, bat, sizeof(Battery));
                  new_bat->unique_id = unique_id_find(&enigmatic->unique_ids);

                  Message msg;
                  msg.type = MESG_ADD;
                  msg.object_type = BATTERY;
                  msg.number = 1;
                  enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, new_bat, sizeof(Battery));

                  DEBUG("battery add: %s", new_bat->name);

                  eina_hash_add(*cache_hash, bat->name, new_bat);
               }
             continue;
          }

        float diff;

        Message msg;
        msg.type = MESG_MOD;

        if (!EINA_DBL_EQ(b->percent, bat->percent))
          {
             Change change = CHANGE_FLOAT;
             msg.object_type = BATTERY_PERCENT;
             msg.number = b->unique_id;
             diff = bat->percent - b->percent;
             enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
             enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
             enigmatic_log_write(enigmatic, (char *) &diff, sizeof(float));

             DEBUG("%s :%i%%", b->name, (int) bat->percent - (int) b->percent);

             changed = 1;
          }

        if (b->charge_full != bat->charge_full)
          {
             Message msg;
             msg.type = MESG_MOD;
             msg.object_type = BATTERY_FULL;
             msg.number = b->unique_id;
             enigmatic_log_diff(enigmatic, msg, bat->charge_full - b->charge_full);

             DEBUG("%s (full) :%i", b->name, (int) bat->charge_full - (int) b->charge_full);
             changed = 1;
          }

        if (b->charge_current != bat->charge_current)
          {
             Message msg;
             msg.type = MESG_MOD;
             msg.object_type = BATTERY_CURRENT;
             msg.number = b->unique_id;
             enigmatic_log_diff(enigmatic, msg, bat->charge_current - b->charge_current);

             DEBUG("%s (current) :%i", b->name, (int) bat->charge_current - (int) b->charge_current);
             changed = 1;
          }
        b->percent = bat->percent;
        b->charge_full = bat->charge_full;
        b->charge_current = bat->charge_current;
     }

   eina_lock_release(&batteries_lock);

   return changed;
}

