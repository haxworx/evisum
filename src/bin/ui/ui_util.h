#ifndef __UI_UTIL_H__
#define __UI_UTIL_H__

#include <Evas.h>
#include "system/process.h"

#define FILL EVAS_HINT_FILL
#define EXPAND EVAS_HINT_EXPAND

#define TAB_BTN_WIDTH  74
#define TAB_BTN_HEIGHT 2
#define BTN_WIDTH      54
#define BTN_HEIGHT     1

#define UI_CHILD_WIN_WIDTH  360
#define UI_CHILD_WIN_HEIGHT 360

void
evisum_icon_cache_init(void);

void
evisum_icon_cache_shutdown(void);

const char *
evisum_icon_cache_find(const Proc_Info *proc);

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                  Evas_Smart_Cb clicked_cb, void *data);

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                     const char *icon, Evas_Smart_Cb clicked_cb, void *data);

const char *
evisum_size_format(unsigned long long bytes);

const char *
evisum_icon_path_get(const char *name);

const char *
evisum_image_path_get(const char *name);

Evas_Object *
evisum_ui_background_add(Evas_Object *win, Eina_Bool enabled);

Evas_Object *
evisum_ui_background_random_add(Evas_Object *win, Eina_Bool enabled);

void
evisum_ui_backgrounds_enabled_set(Eina_Bool enabled);

Eina_Bool
evisum_ui_backgrounds_enabled_get(void);

int
evisum_ui_textblock_font_size_get(Evas_Object *tb);

void
evisum_ui_textblock_font_size_set(Evas_Object *tb, int new_size);

void
evisum_about_window_show(void *data);

#endif
