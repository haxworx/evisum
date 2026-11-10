#include "evisum_ui_cache.h"

/* Genlist contents may be destroyed before the cache's deferred cleanup. */
static void
_cached_object_deleted_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                          void *event_info EINA_UNUSED)
{
   Evisum_Ui_Cache *cache = data;
   Eina_List *l;
   Item_Cache *it;

   EINA_LIST_FOREACH (cache->active, l, it)
     if (it->obj == obj)
       {
          cache->active = eina_list_remove_list(cache->active, l);
          free(it);
          return;
       }
   EINA_LIST_FOREACH (cache->inactive, l, it)
     if (it->obj == obj)
       {
          cache->inactive = eina_list_remove_list(cache->inactive, l);
          free(it);
          return;
       }
   cache->pending = eina_list_remove(cache->pending, obj);
}

static Evas_Object *
_cached_object_create(Evisum_Ui_Cache *cache)
{
   Evas_Object *obj = cache->item_create_cb(cache->parent);
   if (obj) evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _cached_object_deleted_cb, cache);
   return obj;
}

static void
_cached_object_delete(Evisum_Ui_Cache *cache, Evas_Object *obj)
{
   if (!obj) return;
   evas_object_event_callback_del_full(obj, EVAS_CALLBACK_DEL, _cached_object_deleted_cb, cache);
   evas_object_del(obj);
}


Evisum_Ui_Cache *
evisum_ui_item_cache_new(Evas_Object *parent, Evas_Object *(create_cb) (Evas_Object *), int size)
{
   Evisum_Ui_Cache *cache = calloc(1, sizeof(Evisum_Ui_Cache));
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
             it->obj = _cached_object_create(cache);
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

   EINA_LIST_FOREACH_SAFE (cache->active, l, l_next, it)
     {
        if (!it) continue;
        int found = 0;
        EINA_LIST_FOREACH (objs, l2, o)
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
             evas_object_event_callback_del_full(it->obj, EVAS_CALLBACK_DEL, _cached_object_deleted_cb, cache);
             free(it);
          }
     }
}

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache)
{
   Eina_List *l, *l_next;
   Item_Cache *it;

   EINA_LIST_FOREACH_SAFE (cache->inactive, l, l_next, it)
     {
        cache->inactive = eina_list_remove_list(cache->inactive, l);
        cache->active = eina_list_prepend(cache->active, it);
        return it;
     }

   it = calloc(1, sizeof(Item_Cache));
   if (it)
     {
        it->obj = _cached_object_create(cache);
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

   EINA_LIST_FREE (cache->active, it)
     {
        cache->pending = eina_list_append(cache->pending, it->obj);
        free(it);
     }
   EINA_LIST_FREE (cache->inactive, it)
     {
        cache->pending = eina_list_append(cache->pending, it->obj);
        free(it);
     }
   for (int i = 0; i < cache->size; i++)
     {
        it = calloc(1, sizeof(Item_Cache));
        if (it)
          {
             it->obj = _cached_object_create(cache);
             cache->inactive = eina_list_prepend(cache->inactive, it);
          }
     }
   evisum_ui_item_cache_pending_del(cache);
}

// Delete N at a time and pass on until empty.
static Eina_Bool
_pending_triggered_cb(void *data)
{

   Evisum_Ui_Cache *cache = data;
   int i = 0;

   while (cache->pending && i++ < 20)
     {
        Evas_Object *obj = eina_list_data_get(cache->pending);
        cache->pending = eina_list_remove_list(cache->pending, cache->pending);
        _cached_object_delete(cache, obj);
     }
   if (cache->pending) return ECORE_CALLBACK_RENEW;

   void (*done_cb)(void *) = cache->pending_done_cb;
   void *done_data = cache->data;
   cache->pending_timer = NULL;
   cache->pending_done_cb = NULL;
   cache->data = NULL;
   /* The completion callback may reset or free the cache. */
   if (done_cb) done_cb(done_data);
   return ECORE_CALLBACK_CANCEL;
}

void
evisum_ui_item_cache_pending_del(Evisum_Ui_Cache *cache)
{
   if (!cache->pending_timer) cache->pending_timer = ecore_timer_add(0.5, _pending_triggered_cb, cache);
}

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj)
{
   Item_Cache *it;
   Eina_List *l, *l_next;
   Eina_Bool released = 0;

   if (!cache->active) return 0;
   int n = eina_list_count(cache->inactive);

   EINA_LIST_FOREACH_SAFE (cache->active, l, l_next, it)
     {
        if (it->obj == obj)
          {
             cache->active = eina_list_remove_list(cache->active, l);
             if (n > 10)
               {
                  _cached_object_delete(cache, it->obj);
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
   Evas_Object *obj;

   if (!cache) return;
   if (cache->pending_timer)
     {
        ecore_timer_del(cache->pending_timer);
        cache->pending_done_cb = NULL;
        cache->data = NULL;
     }

   /* The cache owns rows, never the genlist that supplied their parent. */
   EINA_LIST_FREE (cache->active, it)
     {
        _cached_object_delete(cache, it->obj);
        free(it);
     }
   EINA_LIST_FREE (cache->inactive, it)
     {
        _cached_object_delete(cache, it->obj);
        free(it);
     }
   EINA_LIST_FREE (cache->pending, obj) _cached_object_delete(cache, obj);

   free(cache);
}
