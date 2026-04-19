#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_Ipc.h>
#include "evisum_server.h"
#include "src/bin/ui/evisum_ui.h"
#include "evisum_config.h"

#define LISTEN_SOCKET_NAME "evisum_server"
#define WANTED             "bonjour monde"

typedef enum {
  EVISUM_MSG_CORE // core message domain - can add more
} Evisum_Msg_Major;

enum {
  EVISUM_MSG_HELLO, // hello from client to server
  EVISUM_MSG_HELLO_REPLY, // hello reply from server to client
} Evisum_Msg_Minor;

typedef struct _Evisum_Server {
    Ecore_Event_Handler *handler;
    Ecore_Ipc_Server *srv;
} Evisum_Server;

typedef struct _Evisum_Msg_Hello {
    Evisum_Action action;
    pid_t         pid;
} Evisum_Msg_Hello;

static void *_evisum_server = NULL;

static Eina_Bool
_evisum_server_server_client_data_cb(void *data EINA_UNUSED, int type EINA_UNUSED, void *event) {
    Ecore_Ipc_Event_Client_Data *ev = event;
    Evisum_Ui *ui = data;

    if ((ev->major == 0) && (ev->minor == 0) &&
        (ev->size == sizeof(Evisum_Msg_Hello))) {
      // if this is the hello message
      Evisum_Msg_Hello *hello = ev->data;

      evisum_ui_activate(ui, hello->action, hello->pid);
      ecore_ipc_client_send(ev->client,
                            EVISUM_MSG_CORE, EVISUM_MSG_HELLO_REPLY,
                            0, 0, 0, // ref, ref_to, response <- not used
                            WANTED, strlen(WANTED) + 1);
      // yes data can just be a string, but guaranteedd to be all there on
      // the other end. also send the nul byte at the string end to be nice
      // so it's terminated without needing a copy
    }


    return ECORE_CALLBACK_RENEW;
}

void
evisum_server_shutdown(void) {
    Evisum_Server *server = _evisum_server;
    if (!server) return;

    ecore_event_handler_del(server->handler);
    ecore_ipc_server_del(server->srv);
    free(server);
}

Eina_Bool
evisum_server_init(void *data) {
    Evisum_Ui *ui = data;
    Evisum_Server *server = calloc(1, sizeof(Evisum_Server));
    if (!server) return 0;

    server->srv = ecore_ipc_server_add(ECORE_IPC_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
    if (!server->srv) return 0;

    server->handler = ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DATA, _evisum_server_server_client_data_cb, ui);
    _evisum_server = server;

    return 1;
}

typedef struct _Evisum_Server_Client {
    Ecore_Ipc_Server *srv;
    Evisum_Action action;
    pid_t pid;
    Eina_Bool success;
} Evisum_Server_Client;

static Eina_Bool
_evisum_server_client_done_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED) {
    Ecore_Ipc_Event_Server_Del *ev;
    Evisum_Server_Client *client = data;

    ev = event;

    if (client->srv != ev->server) return ECORE_CALLBACK_RENEW;

    ecore_main_loop_quit();

    return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_evisum_server_client_data_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED) {
    Ecore_Ipc_Server *srv;
    Ecore_Ipc_Event_Server_Data *ev;
    Evisum_Server_Client *client = data;

    ev = event;
    srv = ev->server;

    if (client->srv != srv) return ECORE_CALLBACK_RENEW;

    client->success = 1;
    ecore_main_loop_quit();

    return ECORE_CALLBACK_DONE;
}

static Eina_Bool
_evisum_server_client_connect_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED) {
    Ecore_Ipc_Event_Server_Add *ev;
    Ecore_Ipc_Server *srv;
    Evisum_Server_Client *client;
    Evisum_Msg_Hello hello;

    ev = event;
    srv = ev->server;
    client = data;

    if (client->srv != srv) return ECORE_CALLBACK_RENEW;
    hello.action = client->action;
    hello.pid = client->pid;
    ecore_ipc_server_send(srv,
                          EVISUM_MSG_CORE, EVISUM_MSG_HELLO,
                          0, 0, 0, // ref, ref_to, response <- not used
                          &hello, sizeof(hello));

    return ECORE_CALLBACK_DONE;
}

Eina_Bool
evisum_server_client_add(Evisum_Action action, pid_t pid) {
    Evisum_Server_Client *client;
    Ecore_Event_Handler *handler[3];
    Eina_Bool ok;

    Ecore_Ipc_Server *srv = ecore_ipc_server_connect(ECORE_IPC_LOCAL_USER, LISTEN_SOCKET_NAME, 0, NULL);
    if (!srv) return 0;

    client = calloc(1, sizeof(Evisum_Server_Client));
    if (!client) return 0;

    client->action = action;
    client->pid = pid;
    client->srv = srv;

    handler[0] = ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_ADD, _evisum_server_client_connect_cb, client);
    handler[1] = ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DEL, _evisum_server_client_done_cb, client);
    handler[2] = ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DATA, _evisum_server_client_data_cb, client);

    ecore_main_loop_begin();

    ecore_event_handler_del(handler[0]);
    ecore_event_handler_del(handler[1]);
    ecore_event_handler_del(handler[2]);

    ok = client->success;
    free(client);

    return ok;
}
