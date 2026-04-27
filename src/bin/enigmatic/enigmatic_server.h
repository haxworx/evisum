#ifndef ENIGMATIC_SERVER_H
#define ENIGMATIC_SERVER_H
#include "Enigmatic.h"
#include <Ecore.h>
#include <Ecore_Con.h>

typedef struct
{
   Ecore_Event_Handler *handler;
   Ecore_Con_Server    *srv;
   Enigmatic           *enigmatic;
} Enigmatic_Server;

void
enigmatic_server_init(Enigmatic *enigmatic);

void
enigmatic_server_shutdown(Enigmatic *enigmatic);

#endif
