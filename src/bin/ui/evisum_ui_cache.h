#ifndef __EVISUM_UI_CACHE_H__
#define __EVISUM_UI_CACHE_H__

#include <Eina.h>
#include <Evas.h>
#include <Ecore.h>

typedef struct _Evisum_Ui_Cache {
   Eina_List   *inactive;
   Eina_List   *active;
   Evas_Object *parent;
   Evas_Object *(*item_create_cb)(Evas_Object *);
   int          size;
   void        (*pending_done_cb)(void *);
   void        *data;
   Ecore_Timer *pending_timer;
   Eina_List   *pending;
} Evisum_Ui_Cache;

typedef struct _Item_Cache {
   Evas_Object *obj;
} Item_Cache;

Evisum_Ui_Cache *
evisum_ui_item_cache_new(Evas_Object *parent, Evas_Object *(create_cb)(Evas_Object *), int size);

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache);

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj);

void
evisum_ui_item_cache_free(Evisum_Ui_Cache *cache);

void
evisum_ui_item_cache_reset(Evisum_Ui_Cache *cache, void (*done_cb)(void *data), void *data);

void
evisum_ui_item_cache_steal(Evisum_Ui_Cache *cache, Eina_List *objs);

void
evisum_ui_item_cache_pending_del(Evisum_Ui_Cache *cache);

#endif
