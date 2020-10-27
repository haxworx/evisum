#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Con.h>
#include "evisum_server.h"
#include "src/bin/ui/ui.h"
#include "src/bin/ui/ui_cpu.h"
#include "src/bin/ui/ui_memory.h"
#include "src/bin/ui/ui_disk.h"
#include "src/bin/ui/ui_sensors.h"

#define LISTEN_SOCKET_NAME "evisum_server"

typedef struct _Evisum_Server {
   Ecore_Event_Handler *handler;
   Ecore_Con_Server    *srv;
} Evisum_Server;

static void *_evisum_server = NULL;

static Eina_Bool
_evisum_server_server_client_connect_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Con_Event_Client_Data *ev;
   Evisum_Action *action;
   Ui *ui;

   ev = event;
   action = ev->data;
   ui = data;

   evisum_ui_activate(ui, *action);

   ecore_con_client_del(ev->client);

   return ECORE_CALLBACK_RENEW;
}

void
evisum_server_shutdown(void)
{
   Evisum_Server *server = _evisum_server;
   if (!server) return;

   ecore_event_handler_del(server->handler);
   ecore_con_server_del(server->srv);
   free(server);
}

Eina_Bool
evisum_server_init(void *data)
{
   Ui *ui = data;
   Evisum_Server *server = calloc(1, sizeof(Evisum_Server));
   if (!server) return EINA_FALSE;

   server->srv = ecore_con_server_add(ECORE_CON_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
   if (!server->srv) return EINA_FALSE;

   server->handler = ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _evisum_server_server_client_connect_cb, ui);
   _evisum_server = server;

   return EINA_TRUE;
}

typedef struct _Evisum_Server_Client {
   Ecore_Con_Server *srv;
   Evisum_Action     action;
   Eina_Bool         success;
} Evisum_Server_Client;

static Eina_Bool
_evisum_server_client_closed_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_Con_Event_Server_Del *ev;
   Evisum_Server_Client *client = data;

   ev = event;

   if (client->srv != ev->server) return ECORE_CALLBACK_RENEW;

   client->success = EINA_TRUE;

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_evisum_server_client_check_timer_cb(void *data EINA_UNUSED)
{
   Evisum_Server_Client *client;
   static double total = 0.0;

   client = data;
   total += 0.1;

   if (total < 1.0)
     return ECORE_CALLBACK_RENEW;

   free(client);

   ecore_main_loop_quit();

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_evisum_server_client_connect_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_Con_Event_Server_Add *ev;
   Ecore_Con_Server *srv;
   Evisum_Server_Client *client;

   ev = event;
   srv = ev->server;
   client = data;

   if (client->srv != srv) return ECORE_CALLBACK_RENEW;

   ecore_con_server_send(srv, &client->action, sizeof(Evisum_Action));
   ecore_con_server_flush(srv);

   return ECORE_CALLBACK_DONE;
}

Eina_Bool
evisum_server_client_add(Evisum_Action action)
{
   Evisum_Server_Client *client;
   Ecore_Con_Server *srv = ecore_con_server_connect(ECORE_CON_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
   if (!srv)
     {
        return EINA_FALSE;
     }

   client = calloc(1, sizeof(Evisum_Server_Client));
   if (!client) return EINA_FALSE;

   client->action = action;
   client->srv = srv;

   ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ADD, _evisum_server_client_connect_cb, client);
   ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DEL, _evisum_server_client_closed_cb, client);
   ecore_timer_add(0.1, _evisum_server_client_check_timer_cb, client);

   return EINA_TRUE;
}

