#include "system/machine.h"
#include "network_interfaces.h"
#include "uid.h"
#include "enigmatic_log.h"

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
        eina_hash_del(*cache_hash, iface->name, NULL);
     }

   EINA_LIST_FREE(network_interfaces, iface)
     {
        iface2 = eina_hash_find(*cache_hash, iface->name);
        if (!iface2)
          {
             iface->unique_id = unique_id_find(&enigmatic->unique_ids);

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
        iface2->total_in = iface->total_in;
        iface2->total_out = iface->total_out;
        free(iface);
     }

   return changed;
}

