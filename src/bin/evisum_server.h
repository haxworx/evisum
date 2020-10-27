#ifndef EVISUM_SERVER_H
#define EVISUM_SERVER_H

#include <Eina.h>
#include "evisum_actions.h"

Eina_Bool
evisum_server_init(void *data);

void
evisum_server_shutdown(void);

Eina_Bool
evisum_server_client_add(Evisum_Action action, int pid);

#endif
