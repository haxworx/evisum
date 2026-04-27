#include "config.h"
#include "enigmatic_server.h"
#include "Enigmatic.h"
#include <signal.h>

static Enigmatic_Server *server = NULL;

static Eina_Bool
_enigmatic_server_client_data_cb(void *data, int type, void *event)
{
   char *msg;
   Enigmatic_Server *server;
   Enigmatic *enigmatic;
   Interval interval;
   int sent = 0;
   Eina_Bool contentious_update = 0;
   Eina_Bool stop = 0;
   Ecore_Con_Event_Client_Data *ev = event;

   msg = ev->data;
   server = data;
   enigmatic = server->enigmatic;

   if (!strcmp(msg, "PING"))
     sent = ecore_con_client_send(ev->client, "PONG", 5);
   else if (!strcmp(msg, "interval-slow"))
     {
        contentious_update = 1;
        interval = INTERVAL_SLOW;
     }
   else if (!strcmp(msg, "interval-medium"))
     {
        contentious_update = 1;
        interval = INTERVAL_MEDIUM;
     }
   else if (!strcmp(msg, "interval-normal"))
     {
        contentious_update = 1;
        interval = INTERVAL_NORMAL;
     }
   else if (!strcmp(msg, "STOP"))
     {
        stop = 1;
        sent = ecore_con_client_send(ev->client, "STOPPING", 9);
     }

   if (contentious_update)
     {
        eina_lock_take(&enigmatic->update_lock);
        enigmatic->interval_update = interval;
        eina_lock_release(&enigmatic->update_lock);
        sent = ecore_con_client_send(ev->client, "OK", 3);
     }

   if (sent)
     ecore_con_client_flush(ev->client);

   if (stop)
     raise(SIGTERM);

   return ECORE_CALLBACK_RENEW;
}

void
enigmatic_server_init(Enigmatic *enigmatic)
{
   server = calloc(1, sizeof(Enigmatic_Server));
   EINA_SAFETY_ON_NULL_RETURN(server);

   server->enigmatic = enigmatic;

   ecore_con_init();

   server->srv = ecore_con_server_add(ECORE_CON_LOCAL_USER, PACKAGE, 0, NULL);
   if (!server->srv)
     ERROR("ecore_con_server_add");

   server->handler = ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _enigmatic_server_client_data_cb, server);
}

void
enigmatic_server_shutdown(Enigmatic *enigmatic)
{
   ecore_event_handler_del(server->handler);
   ecore_con_server_del(server->srv);
   free(server);

   ecore_con_shutdown();
}
