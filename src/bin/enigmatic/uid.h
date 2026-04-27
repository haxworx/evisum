#ifndef UNIQUE_IDS_H
#define UNIQUE_IDS_H

#include <Eina.h>

typedef struct
{
   int       id;
   Eina_Bool available;
} uid;

int
unique_id_find(Eina_List **list);

void
unique_id_release(Eina_List **list, int id);

#endif
