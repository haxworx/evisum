#include "ui_cache.h"

Evisum_Ui_Cache *
evisum_ui_item_cache_new(Evas_Object *parent, Evas_Object *(create_cb)(Evas_Object *), int size)
{
   Evisum_Ui_Cache *cache = malloc(sizeof(Evisum_Ui_Cache));
   if (!cache) return NULL;

   cache->parent = parent;
   cache->item_create_cb = create_cb;
   cache->items = NULL;

   for (int i = 0; i < size; i++)
     {
        Item_Cache *it = calloc(1, sizeof(Item_Cache));
        if (it)
          {
             it->obj = cache->item_create_cb(parent);
             cache->items = eina_list_append(cache->items, it);
          }
     }

   return cache;
}

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache)
{
   Eina_List *l;
   Item_Cache *it;

   EINA_LIST_FOREACH(cache->items, l, it)
     {
        if (it->used == 0)
          {
             it->used = 1;
             return it;
          }
     }

   it = calloc(1, sizeof(Item_Cache));
   if (it)
     {
        it->obj = cache->item_create_cb(cache->parent);
        it->used = 1;
        cache->items = eina_list_append(cache->items, it);
     }

   return it;
}

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj)
{
   Item_Cache *it;
   Eina_List *l;
   Eina_Bool released = EINA_FALSE;

   EINA_LIST_FOREACH(cache->items, l, it)
     {
        if (it->obj == obj)
          {
             it->used = 0;
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

   EINA_LIST_FREE(cache->items, it)
     {
        free(it);
     }

   eina_list_free(cache->items);

   free(cache);
}
