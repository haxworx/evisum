#include "uid.h"

int
unique_id_find(Eina_List **list)
{
   Eina_List *l;
   uid *id;
   int next_available = 0;

   EINA_LIST_FOREACH(*list, l, id)
     {
        if (id->available)
          {
             id->available = 0;
             return id->id;
          }
        next_available = id->id + 1;
     }

   id = malloc(sizeof(uid));
   EINA_SAFETY_ON_NULL_RETURN_VAL(id, -1);

   id->id = next_available;
   id->available = 0;
   *list = eina_list_append(*list, id);

   return id->id;
}

void
unique_id_release(Eina_List **list, int id)
{
   Eina_List *l;
   uid *eid;

   EINA_LIST_FOREACH(*list, l, eid)
     {
        if (eid->id == id)
          {
             eid->available = 1;
             return;
          }
     }

   fprintf(stderr, "AINT NOBODY GOT TIME FOR THAT!\n");
   exit(2);
}

