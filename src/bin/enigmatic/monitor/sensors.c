#include "system/machine.h"
#include "sensors.h"
#include "uid.h"
#include "enigmatic_log.h"

static Eina_List     *sensors = NULL;
static Ecore_Thread  *thread = NULL;
static Eina_Lock      sensors_lock;

void
sensor_key(char *buf, size_t len, Sensor *sensor)
{
   snprintf(buf, len, "%s.%s", sensor->name, sensor->child_name);
}

static void
cb_sensor_free(void *data)
{
   Sensor *sensor = data;
   char key[1024];
   sensor_key(key, sizeof(key), sensor);

   DEBUG("del %s", key);

   free(sensor);
}

static int
cb_sensor_cmp(const void *a, const void *b)
{
   Sensor *s1, *s2;
   char buf1[512], buf2[512];

   s1 = (Sensor *) a;
   s2 = (Sensor *) b;

   snprintf(buf1, sizeof(buf1), "%s.%s", s1->name, s1->child_name);
   snprintf(buf2, sizeof(buf2), "%s.%s", s2->name, s2->child_name);

   return strcmp(buf1, buf2);
}

static void
sensors_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL;
   void *d = NULL;
   Sensor *sensor;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        sensor = d;
        ordered = eina_list_append(ordered, sensor);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_sensor_cmp);

   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = SENSOR;
   msg.number = n;
   enigmatic_log_list_write(enigmatic, EVENT_MESSAGE, msg, ordered, sizeof(Sensor));
   eina_list_free(ordered);
}

static void
sensors_thread(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   Sensor *sensor;
   uint32_t it = 0;

#if (EFL_VERSION_MAJOR >= 1 && EFL_VERSION_MINOR >= 26)
   ecore_thread_name_set(thread, "sensemon");
#endif

   while (!ecore_thread_check(thread))
     {
        eina_lock_take(&sensors_lock);
        if ((it) && (!(it % 3)))
          {
             EINA_LIST_FREE(sensors, sensor)
               free(sensor);
             sensors = sensors_find();
          }
        sensors_update(sensors);
        eina_lock_release(&sensors_lock);

        for (int i = 0; i < 20; i++)
          {
             if (ecore_thread_check(thread)) break;
             usleep(50000);
          }
        it++;
     }
   EINA_LIST_FREE(sensors, sensor)
     free(sensor);
}

void
enigmatic_monitor_sensors_init(void)
{
   eina_lock_new(&sensors_lock);

   sensors = sensors_find();
   sensors_update(sensors);
}

void
enigmatic_monitor_sensors_shutdown(void)
{
   eina_lock_take(&sensors_lock);
   eina_lock_release(&sensors_lock);
   eina_lock_free(&sensors_lock);
}

Eina_Bool
enigmatic_monitor_sensors(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l;
   Sensor *sensor, *s;
   char key[1024];
   Eina_Bool changed = 0;

   if (eina_lock_take_try(&sensors_lock) != EINA_LOCK_SUCCEED) return 0;

   if (!*cache_hash)
     {
        *cache_hash = eina_hash_string_superfast_new(cb_sensor_free);
        EINA_LIST_FOREACH(sensors, l, sensor)
          {
             s = malloc(sizeof(Sensor));
             if (s)
               {
                  memcpy(s, sensor, sizeof(Sensor));
                  sensor_key(key, sizeof(key), sensor);
                  DEBUG("sensor %s added", key);
                  s->unique_id = unique_id_find(&enigmatic->unique_ids);
                  eina_hash_add(*cache_hash, key, s);
               }
          }
        enigmatic->sensors_thread = thread = ecore_thread_run(sensors_thread, NULL, NULL, NULL);
     }

   if (enigmatic->broadcast)
     {
        sensors_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Sensor *s = d;
        Eina_Bool found = 0;
        char key2[1024];
        EINA_LIST_FOREACH(sensors, l, sensor)
          {
             sensor_key(key, sizeof(key), sensor);
             sensor_key(key2, sizeof(key2), s);
             if (!strcmp(key2, key))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, s);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, sensor)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = SENSOR;
        msg.number = sensor->unique_id;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        sensor_key(key, sizeof(key), sensor);
        unique_id_release(&enigmatic->unique_ids, sensor->unique_id);
        eina_hash_del(*cache_hash, key, NULL);
     }

   EINA_LIST_FOREACH(sensors, l, sensor)
     {
        sensor_key(key, sizeof(key), sensor);
        s = eina_hash_find(*cache_hash, key);
        if (!s)
          {
             Sensor *new_sensor = calloc(1, sizeof(Sensor));
             if (new_sensor)
               {
                  memcpy(new_sensor, sensor, sizeof(Sensor));
                  new_sensor->unique_id = unique_id_find(&enigmatic->unique_ids);

                  Message msg;
                  msg.type = MESG_ADD;
                  msg.object_type = SENSOR;
                  msg.number = 1;
                  enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, new_sensor, sizeof(Sensor));

                  DEBUG("sensor %s added", key);
                  eina_hash_add(*cache_hash, key, new_sensor);
               }
             continue;
          }

        if (!EINA_DBL_EQ(s->value, sensor->value))
          {
             Message msg;
             Change change;
             msg.type = MESG_MOD;
             change = CHANGE_FLOAT;
             msg.object_type = SENSOR_VALUE;
             msg.number = s->unique_id;
             float diff = sensor->value - s->value;
             enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
             enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
             enigmatic_log_write(enigmatic, (char *) &diff, sizeof(float));
             DEBUG("%s :%i", key, (int) sensor->value - (int) s->value);
             changed = 1;
          }
        s->value = sensor->value;
     }

   eina_lock_release(&sensors_lock);

   return changed;
}

