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
   cache->pending = NULL;
   cache->pending_timer = NULL;

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
             free(it);
          }
     }
}

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache)
{
   Eina_List *l, *l_next;
   Item_Cache *it;
   Eina_Bool clean;
   int i = 0, n = eina_list_count(cache->inactive);

   clean = (n > 10) ? 1 : 0;

   EINA_LIST_FOREACH_SAFE(cache->inactive, l, l_next, it)
     {
        cache->inactive = eina_list_remove_list(cache->inactive, l);
        if ((clean) && (i < 8))
          {
             cache->pending = eina_list_prepend(cache->pending, it->obj);
             free(it);
             i++;
          }
        else
         {
            cache->active = eina_list_prepend(cache->active, it);
            break;
         }
        it = NULL;
     }

   if (clean)
     evisum_ui_item_cache_pending_del(cache);
   if (it) return it;

   it = calloc(1, sizeof(Item_Cache));
   if (it)
     {
        it->obj = cache->item_create_cb(cache->parent);
        cache->active = eina_list_prepend(cache->active, it);
     }

   return it;
}

void
evisum_ui_item_cache_reset(Evisum_Ui_Cache *cache, void (*done_cb)(void *data), void *data)
{
   Item_Cache *it;

   cache->pending_done_cb = done_cb;
   cache->data = data;

   EINA_LIST_FREE(cache->active, it)
     {
        cache->pending = eina_list_append(cache->pending, it->obj);
        free(it);
     }
   EINA_LIST_FREE(cache->inactive, it)
     {
        cache->pending = eina_list_append(cache->pending, it->obj);
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
   evisum_ui_item_cache_pending_del(cache);
}

// Delete N at a time and pass on until empty.
static Eina_Bool
_pending_triggered_cb(void *data)
{

   Eina_List *l, *l_next;
   Evas_Object *o;
   Evisum_Ui_Cache *cache;
   int n, i = 0;

   cache = data;

   n = eina_list_count(cache->pending);
   EINA_LIST_FOREACH_SAFE(cache->pending, l, l_next, o)
     {
        cache->pending = eina_list_remove_list(cache->pending, l);
        evas_object_del(o);
        i++; n--;
        if (i == 20) break;
     }
   if (n)
     return 1;

   if (cache->pending_done_cb)
     cache->pending_done_cb(cache->data);

   cache->pending_timer = NULL;
   return 0;
}

void
evisum_ui_item_cache_pending_del(Evisum_Ui_Cache *cache)
{
   if (!cache->pending_timer)
     cache->pending_timer = ecore_timer_add(0.5, _pending_triggered_cb, cache);
}

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj)
{
   Item_Cache *it;
   Eina_List *l, *l_next;
   Eina_Bool released = 0;

   if (!cache->active) return 0;
   int n = eina_list_count(cache->inactive);

   EINA_LIST_FOREACH_SAFE(cache->active, l, l_next, it)
     {
        if (it->obj == obj)
          {
             cache->active = eina_list_remove_list(cache->active, l);
             if (n > 10)
               {
                  evas_object_del(it->obj);
                  free(it);
               }
             else
               {
                  evas_object_hide(it->obj);
                  cache->inactive = eina_list_prepend(cache->inactive, it);
               }
             released = 1;
             break;
          }
     }

   return released;
}

void
evisum_ui_item_cache_free(Evisum_Ui_Cache *cache)
{
   Item_Cache *it;

   if (cache->pending_timer)
     {
        ecore_timer_del(cache->pending_timer);
        cache->pending_done_cb = NULL;
        cache->data = NULL;
     }

   if (cache->parent)
     evas_object_del(cache->parent);

   EINA_LIST_FREE(cache->active, it)
     free(it);
   EINA_LIST_FREE(cache->inactive, it)
     free(it);
   eina_list_free(cache->pending);

   free(cache);
}
