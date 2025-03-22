#ifndef __EVISUM_UI_UTIL_H__
#define __EVISUM_UI_UTIL_H__

#include <Evas.h>
#include "system/process.h"

#define FILL EVAS_HINT_FILL
#define EXPAND EVAS_HINT_EXPAND

#define TAB_BTN_WIDTH   72
#define TAB_BTN_HEIGHT  24
#define BTN_WIDTH       62
#define BTN_HEIGHT      24
#define LIST_BTN_HEIGHT 24

#define UI_CHILD_WIN_WIDTH  360
#define UI_CHILD_WIN_HEIGHT 360

Eina_Hash *
evisum_icon_cache_new(void);

void
evisum_icon_cache_del(Eina_Hash *icon_cache);

const char *
evisum_icon_cache_find(Eina_Hash *icon_cache, const Proc_Info *proc);

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                  Evas_Smart_Cb clicked_cb, void *data);

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                     const char *icon, Evas_Smart_Cb clicked_cb, void *data);

void
evisum_ui_icon_size_set(Evas_Object *ic, int size);

const char *
evisum_size_format(unsigned long long bytes, Eina_Bool simple);

const char *
evisum_icon_path_get(const char *name);

const char *
evisum_image_path_get(const char *name);

Evas_Object *
evisum_ui_background_add(Evas_Object *win);

int
evisum_ui_textblock_font_size_get(Evas_Object *tb);

void
evisum_ui_textblock_font_size_set(Evas_Object *tb, int new_size);

void
evisum_about_window_show(void *data);

#endif
