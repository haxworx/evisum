#include "ui_cache.h"

Evisum_Ui_Cache *
evisum_ui_item_cache_new(Evas_Object *parent,
                         Evas_Object *(create_cb)(Evas_Object *), int size)
{
   Evisum_Ui_Cache *cache = malloc(sizeof(Evisum_Ui_Cache));
   if (!cache) return NULL;

   cache->parent = parent;
   cache->item_create_cb = create_cb;
   cache->inactive = cache->active = NULL;
   cache->size = size;

   for (int i = 0; i < size; i++)
     {
        Item_Cache *it = malloc(sizeof(Item_Cache));
        if (it)
          {
             it->obj = cache->item_create_cb(parent);
             cache->inactive = eina_list_prepend(cache->inactive, it);
          }
     }

   return cache;
}

void
evisum_ui_item_cache_steal(Evisum_Ui_Cache *cache, Eina_List *objs)
{
   Eina_List *l, *l_next, *l2;
   Item_Cache *it;
   Evas_Object *o;

   EINA_LIST_FOREACH_SAFE(cache->active, l, l_next, it)
     {
        int found = 0;
        EINA_LIST_FOREACH(objs, l2, o)
          {
             if (it->obj == o)
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          {
             cache->active = eina_list_remove_list(cache->active, l);
             evas_object_del(it->obj);
             free(it);
          }
     }
   if (eina_list_count(cache->inactive)) return;
   for (int i = 0; i < cache->size; i++)
     {
        Item_Cache *it = malloc(sizeof(Item_Cache));
        if (it)
          {
             it->obj = cache->item_create_cb(cache->parent);
             cache->inactive = eina_list_prepend(cache->inactive, it);
          }
     }
}

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache)
{
   Eina_List *l, *l_next;
   Item_Cache *it;

   EINA_LIST_FOREACH_SAFE(cache->inactive, l, l_next, it)
     {
        cache->inactive = eina_list_remove_list(cache->inactive, l);
        cache->active = eina_list_prepend(cache->active, it);
        return it;
     }

   it = calloc(1, sizeof(Item_Cache));
   if (it)
     {
        it->obj = cache->item_create_cb(cache->parent);
        cache->active = eina_list_prepend(cache->active, it);
     }

   return it;
}

void
evisum_ui_item_cache_reset(Evisum_Ui_Cache *cache)
{
   Item_Cache *it;

   EINA_LIST_FREE(cache->active, it)
     {
        free(it);
     }
   EINA_LIST_FREE(cache->inactive, it)
     {
        free(it);
     }
   for (int i = 0; i < cache->size; i++)
     {
        it = calloc(1, sizeof(Item_Cache));
        if (it)
          {
             it->obj = cache->item_create_cb(cache->parent);
             cache->inactive = eina_list_prepend(cache->inactive, it);
          }
     }
}

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj)
{
   Item_Cache *it;
   Eina_List *l, *l_next;
   Eina_Bool released = EINA_FALSE;

   if (!cache->active) return EINA_FALSE;
   int n = eina_list_count(cache->inactive);

   EINA_LIST_FOREACH_SAFE(cache->active, l, l_next, it)
     {
        if (it->obj == obj)
          {
             cache->active = eina_list_remove_list(cache->active, l);
             if (n >= 50)
               {
                  evas_object_del(it->obj);
                  free(it);
               }
             else
               {
                  evas_object_hide(it->obj);
                  cache->inactive = eina_list_prepend(cache->inactive, it);
               }
             released = EINA_TRUE;
             break;
          }
     }

   return released;
}

void
evisum_ui_item_cache_free(Evisum_Ui_Cache *cache)
{
   Item_Cache *it;

   evas_object_del(cache->parent);

   EINA_LIST_FREE(cache->active, it)
     free(it);
   EINA_LIST_FREE(cache->inactive, it)
     free(it);
   free(cache);
}
