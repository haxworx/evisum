#ifndef __UI_CACHE_H__
#define __UI_CACHE_H__

#include <Eina.h>
#include <Evas.h>

typedef struct _Evisum_Ui_Cache {
   Eina_List   *items;
   Evas_Object *parent;
   Evas_Object *(*item_create_cb)(Evas_Object *);
} Evisum_Ui_Cache;

typedef struct _Item_Cache {
   Evas_Object *obj;
   Eina_Bool    used;
} Item_Cache;

Evisum_Ui_Cache *
evisum_ui_item_cache_new(Evas_Object *parent, Evas_Object *(create_cb)(Evas_Object *), int size);

Item_Cache *
evisum_ui_item_cache_item_get(Evisum_Ui_Cache *cache);

Eina_Bool
evisum_ui_item_cache_item_release(Evisum_Ui_Cache *cache, Evas_Object *obj);

void
evisum_ui_item_cache_free(Evisum_Ui_Cache *cache);


#endif