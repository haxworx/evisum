#include "system/machine.h"
#include "network_interfaces.h"
#include "uid.h"
#include "enigmatic_log.h"

typedef struct
{
   uint64_t in;
   uint64_t out;
   uint32_t sample_time;
} Network_Interface_Raw;

static Eina_Hash *_network_interface_raw_cache;

static Eina_Hash *
_network_interface_raw_cache_get(void)
{
   if (!_network_interface_raw_cache)
     _network_interface_raw_cache = eina_hash_string_superfast_new(free);

   return _network_interface_raw_cache;
}

static int64_t
_network_interface_elapsed_get(Enigmatic *enigmatic, Network_Interface_Raw *raw)
{
   int64_t elapsed = 0;

   if ((raw) && (enigmatic->poll_time > raw->sample_time))
     elapsed = enigmatic->poll_time - raw->sample_time;
   if (elapsed <= 0)
     elapsed = enigmatic->interval;
   if (elapsed <= 0)
     elapsed = 1;

   return elapsed;
}

static void
_network_interface_rates_update(Enigmatic *enigmatic, Network_Interface *iface)
{
   Eina_Hash *cache;
   Network_Interface_Raw *raw;
   uint64_t raw_in, raw_out;
   int64_t elapsed;

   raw_in = iface->total_in;
   raw_out = iface->total_out;
   iface->in = 0;
   iface->out = 0;

   cache = _network_interface_raw_cache_get();
   if (!cache) return;

   raw = eina_hash_find(cache, iface->name);
   if (!raw)
     {
        raw = calloc(1, sizeof(Network_Interface_Raw));
        if (!raw) return;

        raw->in = raw_in;
        raw->out = raw_out;
        raw->sample_time = enigmatic->poll_time;
        if (!eina_hash_add(cache, iface->name, raw))
          free(raw);
        return;
     }

   elapsed = _network_interface_elapsed_get(enigmatic, raw);
   if (raw_in >= raw->in)
     iface->in = (raw_in - raw->in) / elapsed;
   if (raw_out >= raw->out)
     iface->out = (raw_out - raw->out) / elapsed;

   raw->in = raw_in;
   raw->out = raw_out;
   raw->sample_time = enigmatic->poll_time;
}

void
enigmatic_monitor_network_interfaces_shutdown(void)
{
   if (!_network_interface_raw_cache) return;

   eina_hash_free(_network_interface_raw_cache);
   _network_interface_raw_cache = NULL;
}

static void
cb_network_interface_free(void *data)
{
   Network_Interface *iface = data;

   DEBUG("del %s", iface->name);

   free(iface);
}

static int
cb_network_interfaces_cmp(const void *a, const void *b)
{
   Network_Interface *iface1, *iface2;

   iface1 = (Network_Interface *) a;
   iface2 = (Network_Interface *) b;

   return strcmp(iface1->name, iface2->name);
}

static void
network_interfaces_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL;
   void *d = NULL;
   Network_Interface *iface;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        iface = d;
        ordered = eina_list_append(ordered, iface);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_network_interfaces_cmp);

   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = NETWORK;
   msg.number = n;
   enigmatic_log_list_write(enigmatic, EVENT_MESSAGE, msg, ordered, sizeof(Network_Interface));
   eina_list_free(ordered);
}

Eina_Bool
enigmatic_monitor_network_interfaces(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l, *network_interfaces;
   Network_Interface *iface, *iface2;
   Eina_Bool changed = 0;

   network_interfaces = network_interfaces_find();

   if (!*cache_hash)
     {
        *cache_hash = eina_hash_string_superfast_new(cb_network_interface_free);
        EINA_LIST_FOREACH(network_interfaces, l, iface)
          {
             iface2 = malloc(sizeof(Network_Interface));
             if (iface2)
               {
                  DEBUG("iface add: %s", iface->name);
                  memcpy(iface2, iface, sizeof(Network_Interface));
                  _network_interface_rates_update(enigmatic, iface2);
                  iface2->unique_id = unique_id_find(&enigmatic->unique_ids);
                  eina_hash_add(*cache_hash, iface->name, iface2);
               }
          }
     }

   if (enigmatic->broadcast)
     {
        network_interfaces_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Network_Interface *iface2 = d;
        Eina_Bool found = 0;
        EINA_LIST_FOREACH(network_interfaces, l, iface)
          {
             if (!strcmp(iface2->name, iface->name))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, iface2);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, iface)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = NETWORK;
        msg.number = iface->unique_id;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        unique_id_release(&enigmatic->unique_ids, iface->unique_id);
        if (_network_interface_raw_cache)
          eina_hash_del(_network_interface_raw_cache, iface->name, NULL);
        eina_hash_del(*cache_hash, iface->name, NULL);
     }

   EINA_LIST_FREE(network_interfaces, iface)
     {
        iface2 = eina_hash_find(*cache_hash, iface->name);
        if (!iface2)
          {
             iface->unique_id = unique_id_find(&enigmatic->unique_ids);
             _network_interface_rates_update(enigmatic, iface);

             Message msg;
             msg.type = MESG_ADD;
             msg.object_type = NETWORK;
             msg.number = 1;
             enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, iface, sizeof(Network_Interface));

             DEBUG("iface add: %s", iface->name);
             eina_hash_add(*cache_hash, iface->name, iface);
             continue;
          }

        Message msg;
        msg.type = MESG_MOD;

        _network_interface_rates_update(enigmatic, iface);

        if (iface2->total_in != iface->total_in)
          {
             msg.object_type = NETWORK_INCOMING;
             msg.number = iface2->unique_id;
             enigmatic_log_diff(enigmatic, msg, iface->total_in - iface2->total_in);

             DEBUG("%s in :%i", iface2->name, (int) iface->total_in - (int) iface2->total_in);
             changed = 1;
          }

        if (iface2->total_out != iface->total_out)
          {
             msg.object_type = NETWORK_OUTGOING;
             msg.number = iface2->unique_id;
             enigmatic_log_diff(enigmatic, msg, iface->total_out - iface2->total_out);

             DEBUG("%s out :%i", iface2->name, (int) iface->total_out - (int) iface2->total_out);
             changed = 1;
          }
        if (iface2->in != iface->in)
          {
             msg.object_type = NETWORK_INCOMING_RATE;
             msg.number = iface2->unique_id;
             enigmatic_log_diff(enigmatic, msg, (int64_t) iface->in - (int64_t) iface2->in);

             DEBUG("%s in rate :%i", iface2->name, (int) iface->in - (int) iface2->in);
             changed = 1;
          }

        if (iface2->out != iface->out)
          {
             msg.object_type = NETWORK_OUTGOING_RATE;
             msg.number = iface2->unique_id;
             enigmatic_log_diff(enigmatic, msg, (int64_t) iface->out - (int64_t) iface2->out);

             DEBUG("%s out rate :%i", iface2->name, (int) iface->out - (int) iface2->out);
             changed = 1;
          }
        iface2->total_in = iface->total_in;
        iface2->total_out = iface->total_out;
        iface2->in = iface->in;
        iface2->out = iface->out;
        free(iface);
     }

   return changed;
}
