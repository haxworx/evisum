#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Con.h>
#include "evisum_server.h"
#include "src/bin/ui/evisum_ui.h"
#include "evisum_config.h"

#define LISTEN_SOCKET_NAME "evisum_server"
#define WANTED "bonjour monde"

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
   Evisum_Ui *ui;
   int *pid;

   ev = event;
   action = ev->data;
   ui = data;

   pid = ev->data + sizeof(int);

   ecore_con_client_send(ev->client, WANTED, strlen(WANTED));
   ecore_con_client_flush(ev->client);

   evisum_ui_activate(ui, *action, *pid);

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
   Evisum_Ui *ui = data;
   Evisum_Server *server = calloc(1, sizeof(Evisum_Server));
   if (!server) return 0;

   server->srv = ecore_con_server_add(ECORE_CON_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
   if (!server->srv) return 0;

   server->handler = ecore_event_handler_add(ECORE_CON_EVENT_CLIENT_DATA, _evisum_server_server_client_connect_cb, ui);
   _evisum_server = server;

   return 1;
}

typedef struct _Evisum_Server_Client {
   Ecore_Con_Server     *srv;
   Evisum_Action        action;
   int                  pid;
   Eina_Bool            success;
} Evisum_Server_Client;

static Eina_Bool
_evisum_server_client_done_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_Con_Event_Server_Del *ev;
   Evisum_Server_Client *client = data;

   ev = event;

   if (client->srv != ev->server) return ECORE_CALLBACK_RENEW;

   ecore_main_loop_quit();

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_evisum_server_client_data_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ecore_Con_Server *srv;
   Ecore_Con_Event_Server_Data *ev;
   Evisum_Server_Client *client = data;

   ev = event;
   srv = ev->server;

   if (client->srv != srv) return ECORE_CALLBACK_RENEW;

   client->success = 1;
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
   ecore_con_server_send(srv, &client->pid, sizeof(int));
   ecore_con_server_flush(srv);

   return ECORE_CALLBACK_DONE;
}

Eina_Bool
evisum_server_client_add(Evisum_Action action, int pid)
{
   Evisum_Server_Client *client;
   Ecore_Event_Handler  *handler[4];
   Eina_Bool ok;

   Ecore_Con_Server *srv = ecore_con_server_connect(ECORE_CON_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
   if (!srv)
     return 0;

   client = calloc(1, sizeof(Evisum_Server_Client));
   if (!client) return 0;

   client->action = action;
   client->pid = pid;
   client->srv = srv;

   handler[0] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ADD,
                                        _evisum_server_client_connect_cb, client);
   handler[1] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DEL,
                                        _evisum_server_client_done_cb, client);
   handler[2] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ERROR,
                                        _evisum_server_client_done_cb, client);
   handler[3] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DATA,
                                        _evisum_server_client_data_cb, client);

   ecore_main_loop_begin();

   ecore_event_handler_del(handler[0]);
   ecore_event_handler_del(handler[1]);
   ecore_event_handler_del(handler[2]);
   ecore_event_handler_del(handler[3]);

   ok = client->success;
   free(client);

   return ok;
}

