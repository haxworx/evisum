#ifndef __UI_UTIL_H__
#define __UI_UTIL_H__

#include <Evas.h>

#define FILL EVAS_HINT_FILL
#define EXPAND EVAS_HINT_EXPAND

#define TAB_BTN_WIDTH  96
#define TAB_BTN_HEIGHT 32
#define BTN_WIDTH      80
#define BTN_HEIGHT     24

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


#endif