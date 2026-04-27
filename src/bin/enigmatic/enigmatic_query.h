#ifndef ENIGMATIC_QUERY_H
#define ENIGMATIC_QUERY_H

#include <Ecore.h>
#include <Ecore_Con.h>

typedef struct
{
   Ecore_Con_Server    *srv;
   Ecore_Timer         *timer;
   const char          *command;
   char                *response;
} Enigmatic_Query;

Eina_Bool
enigmatic_query_send(const char *command);

#endif
