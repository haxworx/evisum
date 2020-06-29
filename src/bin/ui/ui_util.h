#ifndef __UI_UTIL_H__
#define __UI_UTIL_H__

#include <Evas.h>

#define FILL EVAS_HINT_FILL
#define EXPAND EVAS_HINT_EXPAND

#define TAB_BTN_WIDTH  96
#define TAB_BTN_HEIGHT 32
#define BTN_WIDTH      80
#define BTN_HEIGHT     24

#define UI_CHILD_WIN_WIDTH  360
#define UI_CHILD_WIN_HEIGHT 360
#define MISC_MAX_WIDTH      350
#define MISC_MIN_WIDTH      340

#define TEXT_PAD "   "

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                Evas_Smart_Cb clicked_cb, void *data);

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                Evas_Smart_Cb clicked_cb, void *data);

const char *
evisum_size_format(unsigned long long bytes);

const char *
evisum_icon_path_get(const char *name);

int
evisum_ui_textblock_font_size_get(Evas_Object *tb);

void
evisum_ui_textblock_font_size_set(Evas_Object *tb, int new_size);

void
evisum_child_window_show(Evas_Object *parent, Evas_Object *win);

void
evisum_about_window_show(void *data);

#endif
