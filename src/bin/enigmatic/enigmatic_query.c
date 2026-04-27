#include "config.h"
#include "Enigmatic.h"
#include "enigmatic_query.h"

void
enigmatic_query_init(void)
{
   ecore_init();
   ecore_con_init();
}

void
enigmatic_query_shutdown(void)
{
   ecore_con_shutdown();
   ecore_shutdown();
}

static Eina_Bool
_enigmatic_query_done_cb(void *data, int type, void *event)
{
   Ecore_Con_Event_Server_Del *ev;
   Enigmatic_Query *query = data;

   ev = event;

   if (query->srv != ev->server) return ECORE_CALLBACK_RENEW;

   ecore_main_loop_quit();

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_enigmatic_query_data_cb(void *data, int type, void *event)
{
   Ecore_Con_Server *srv;
   Ecore_Con_Event_Server_Data *ev;
   Enigmatic_Query *query = data;

   ev = event;
   srv = ev->server;

   if (query->srv != srv) return ECORE_CALLBACK_RENEW;

   if (ev->size)
     query->response = strdup(ev->data);

   ecore_main_loop_quit();

   return ECORE_CALLBACK_DONE;
}

static Eina_Bool
cb_query_timeout(void *data)
{
   Enigmatic_Query *query = data;

   ecore_main_loop_quit();

   query->timer = NULL;

   return 0;
}

static Eina_Bool
_enigmatic_query_connect_cb(void *data, int type, void *event)
{
   Ecore_Con_Event_Server_Add *ev;
   Ecore_Con_Server *srv;
   Enigmatic_Query *query;

   ev = event;
   srv = ev->server;
   query = data;

   if (query->srv != srv) return ECORE_CALLBACK_RENEW;

   ecore_con_server_send(srv, query->command, strlen(query->command) + 1);
   ecore_con_server_flush(srv);

   query->timer = ecore_timer_add(1.0, cb_query_timeout, query);

   return ECORE_CALLBACK_DONE;
}

Eina_Bool
enigmatic_query_send(const char *command)
{
   Enigmatic_Query *query;
   Ecore_Event_Handler *handlers[4];
   Eina_Bool ok = 0;
   int n = 0;

   enigmatic_query_init();

   query = calloc(1, sizeof(Enigmatic_Query));
   EINA_SAFETY_ON_NULL_RETURN_VAL(query, 0);

   query->srv = ecore_con_server_connect(ECORE_CON_LOCAL_USER, PACKAGE, 0, NULL);
   if (!query->srv) return 0;

   query->command = command;

   handlers[n++] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ADD,
                                           _enigmatic_query_connect_cb, query);
   handlers[n++] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DEL,
                                           _enigmatic_query_done_cb, query);
   handlers[n++] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_ERROR,
                                           _enigmatic_query_done_cb, query);
   handlers[n++] = ecore_event_handler_add(ECORE_CON_EVENT_SERVER_DATA,
                                           _enigmatic_query_data_cb, query);

   ecore_main_loop_begin();

   for (int i = 0; i < n; i++)
     ecore_event_handler_del(handlers[i]);

   if (query->timer)
     ecore_timer_del(query->timer);

   if (query->response)
     {
        printf("%s\n", query->response);
        free(query->response);
        ok = 1;
     }

   free(query);

   enigmatic_query_shutdown();

   return ok;
}

