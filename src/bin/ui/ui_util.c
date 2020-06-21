#include "ui_util.h"
#include <Elementary.h>
#include "config.h"

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tbl, *rect, *button, *label;

   tbl = elm_table_add(parent);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tbl, FILL, FILL);

   rect = evas_object_rectangle_add(tbl);
   evas_object_size_hint_weight_set(rect, EXPAND, EXPAND);
   evas_object_size_hint_align_set(rect, FILL, FILL);
   evas_object_size_hint_min_set(rect,
                   TAB_BTN_WIDTH * elm_config_scale_get(),
                   TAB_BTN_HEIGHT * elm_config_scale_get());

   button = elm_button_add(parent);
   evas_object_size_hint_weight_set(button, EXPAND, EXPAND);
   evas_object_size_hint_align_set(button, FILL, FILL);
   evas_object_show(button);
   evas_object_smart_callback_add(button, "clicked", clicked_cb, data);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   evas_object_size_hint_align_set(label, FILL, FILL);
   evas_object_show(label);
   elm_object_text_set(label,
                   eina_slstr_printf("<bigger>%s</bigger>", text));
   elm_layout_content_set(button, "elm.swallow.content", label);

   elm_table_pack(tbl, rect, 0, 0, 1, 1);
   elm_table_pack(tbl, button, 0, 0, 1, 1);

   if (alias)
     *alias = button;

   return tbl;
}

const char *
evisum_size_format(unsigned long long bytes)
{
   const char *s, *unit = "BKMGTPEZY";
   unsigned long powi = 1;
   unsigned long long value;
   unsigned int precision = 2, powj = 1;

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       powi *= 1024;
       ++unit;
       if (unit[1] == '\0') break;
     }

   if (*unit == 'B') precision = 0;

   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }

   s = eina_slstr_printf("%1.*f %c", precision, (double) value / powi, *unit);

   return s;
}

static char *
_path_append(const char *path, const char *file)
{
   char *concat;
   int len;
   char separator = '/';

   len = strlen(path) + strlen(file) + 2;
   concat = malloc(len * sizeof(char));
   snprintf(concat, len, "%s%c%s", path, separator, file);

   return concat;
}

const char *
evisum_icon_path_get(const char *name)
{
   char *path;
   const char *icon_path, *directory = PACKAGE_DATA_DIR "/images";
   icon_path = name;

   path = _path_append(directory, eina_slstr_printf("%s.png", name));
   if (path)
     {
        if (ecore_file_exists(path))
          icon_path = eina_slstr_printf("%s", path);

        free(path);
     }

   return icon_path;
}

void
evisum_ui_textblock_font_size_set(Evas_Object *tb, int new_size)
{
   Evas_Textblock_Style *ts;

   if (!tb) return;

   ts = evas_textblock_style_new();

   evas_textblock_style_set(ts,
                 eina_slstr_printf("DEFAULT='font_size=%d'", new_size));
   evas_object_textblock_style_user_push(tb, ts);
}

int
evisum_ui_textblock_font_size_get(Evas_Object *tb)
{
   const char *style;
   char *p, *p1;
   Evas_Textblock_Style *ts;
   int size = 0;

   if (!tb) return size;

   ts = evas_object_textblock_style_get(tb);
   if (!ts) return size;

   style = evas_textblock_style_get(ts);
   if (!style && !style[0]) return size;

   p = strdup(style);

   p1 = strstr(p, "font_size=");
   if (p1)
     {
        p1 += 10;
        size = atoi(p1);
     }

   free(p);

   return size;
}
