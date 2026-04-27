#include "system/machine.h"
#include "cores.h"
#include "uid.h"
#include "enigmatic_log.h"

static void
cb_core_free(void *data)
{
   Cpu_Core *core = data;

   DEBUG("del %s", core->name);

   free(core);
}

static int
cb_core_cmp(const void *a, const void *b)
{
   Cpu_Core *c1, *c2;

   c1 = (Cpu_Core *) a;
   c2 = (Cpu_Core *) b;

   return c1->id - c2->id;
}

static void
cores_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL;
   void *d = NULL;
   Cpu_Core *core;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        core = d;
        ordered = eina_list_append(ordered, core);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);

   ordered = eina_list_sort(ordered, n, cb_core_cmp);

   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = CPU_CORE;
   msg.number = n;
   enigmatic_log_list_write(enigmatic, EVENT_MESSAGE, msg, ordered, sizeof(Cpu_Core));
   eina_list_free(ordered);
}

Eina_Bool
enigmatic_monitor_cores(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l;
   Cpu_Core *c, *core;
   Eina_Bool changed = 0;
   Eina_List *cores;

   cores = cores_find();
   cores_update(cores);

   if (!*cache_hash)
     {
        *cache_hash = eina_hash_string_superfast_new(cb_core_free);

        EINA_LIST_FOREACH(cores, l, core)
          {
             c = malloc(sizeof(Cpu_Core));
             if (c)
               {
                  memcpy(c, core, sizeof(Cpu_Core));
                  c->unique_id = unique_id_find(&enigmatic->unique_ids);
                  DEBUG("core %s added", c->name);
                  eina_hash_add(*cache_hash, c->name, c);
               }
          }
     }

   if (enigmatic->broadcast)
     {
        cores_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Cpu_Core *c = d;
        Eina_Bool found = 0;
        EINA_LIST_FOREACH(cores, l, core)
          {
             if (!strcmp(c->name, core->name))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, c);
     }
   eina_iterator_free(it);

   if (purge) changed = 1;

   EINA_LIST_FREE(purge, core)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = CPU_CORE;
        msg.number = core->unique_id;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        unique_id_release(&enigmatic->unique_ids, core->unique_id);
        eina_hash_del(*cache_hash, core->name, NULL);
     }

   double ratio;
   int diff_total, diff_idle;
   int percent;
   unsigned long used;

   EINA_LIST_FREE(cores, core)
     {
        c = eina_hash_find(*cache_hash, core->name);
        if (!c)
          {
             core->unique_id = unique_id_find(&enigmatic->unique_ids);

             Message msg;
             msg.type = MESG_ADD;
             msg.object_type = CPU_CORE;
             msg.number = 1;
             enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, core, sizeof(Cpu_Core));

             DEBUG("core %s added", core->name);

             eina_hash_add(*cache_hash, core->name, core);
             changed = 1;
             continue;
          }
        diff_total = (core->total - c->total) / enigmatic->interval;
        diff_idle = (core->idle - c->idle) / enigmatic->interval;
        ratio = diff_total / 100.0;
        used = diff_total - diff_idle;
        percent = used / ratio;

        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;

        core->percent = percent;

        if (c->percent != core->percent)
          {
             Message msg;
             msg.type = MESG_MOD;
             msg.object_type = CPU_CORE_PERC;
             msg.number = c->unique_id;

             enigmatic_log_diff(enigmatic, msg, core->percent - c->percent);

             c->percent = core->percent;
          }

        if (c->temp != core->temp)
          {
             Message msg;
             msg.type = MESG_MOD;
             msg.object_type = CPU_CORE_TEMP;
             msg.number = c->unique_id;

             enigmatic_log_diff(enigmatic, msg, core->temp - c->temp);

             c->temp = core->temp;
          }

        if (c->freq != core->freq)
          {
             Message msg;
             msg.type = MESG_MOD;
             msg.object_type = CPU_CORE_FREQ;
             msg.number = c->unique_id;

             enigmatic_log_diff(enigmatic, msg, core->freq - c->freq);

             c->freq = core->freq;
          }

        c->idle = core->idle;
        c->total = core->total;

        DEBUG("%s (%i) => %i%% => %i => %iC", c->name, c->unique_id, c->percent, c->freq, c->temp);

        free(core);
     }

   return changed;
}

