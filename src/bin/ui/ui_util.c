#include "ui_util.h"
#include "ui.h"
#include <Elementary.h>
#include "config.h"

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text,
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
                   eina_slstr_printf("%s", text));
   elm_layout_content_set(button, "elm.swallow.content", label);

   elm_table_pack(tbl, rect, 0, 0, 1, 1);
   elm_table_pack(tbl, button, 0, 0, 1, 1);

   if (alias)
     *alias = button;

   return tbl;
}

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
                   BTN_WIDTH * elm_config_scale_get(),
                   BTN_HEIGHT * elm_config_scale_get());

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
                   eina_slstr_printf("%s", text));
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

void
evisum_child_window_show(Evas_Object *parent, Evas_Object *win)
{
   Evas_Coord x, y, w, h;

   evas_object_resize(win, UI_CHILD_WIN_WIDTH, UI_CHILD_WIN_HEIGHT);
   evas_object_geometry_get(parent, &x, &y, &w, &h);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 10);
   else
     elm_win_center(win, EINA_TRUE, EINA_TRUE);

   evas_object_show(win);
}

static Ecore_Animator *_animator = NULL;

typedef struct {
   int pos;
   Evas_Object *label;
   Evas_Object *version;
   Evas_Object *bg;
} Animate_Data;

static Animate_Data about_data;

static void
_win_del_cb(void *data, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Evas_Object *win;
   Ui *ui;

   win = data;

   if (_animator)
     ecore_animator_del(_animator);
   _animator = NULL;
   ui = evas_object_data_get(win, "ui");
   if (!ui) return;
   evas_object_del(win);
   ui->win_about = NULL;
}

static Eina_Bool
about_anim(void *data)
{
   Animate_Data *ad;
   Evas_Coord w, h, ow, oh, x;

   ad = data;

   evas_object_geometry_get(ad->bg, NULL, NULL, &w, &h);
   evas_object_geometry_get(ad->label, &x, NULL, &ow, &oh);
   evas_object_move(ad->label, x, ad->pos);

   ad->pos--;
   if (ad->pos <= -oh) ad->pos = h;

   return EINA_TRUE;
}

void
evisum_about_window_show(void *data)
{
   Ui *ui;
   Evas_Object *win, *bg, *box, *version, *label, *btn;
   const char *copyright =
      "<font color=#ffffff>"
      "<small>"
      "<b>"
      "Copyright &copy; 2019-2020 Alastair Poole &lt;netstar@gmail.com&gt;<br>"
      "<br>"
      "</b>"
      "Permission to use, copy, modify, and distribute this software <br>"
      "for any purpose with or without fee is hereby granted, provided <br>"
      "that the above copyright notice and this permission notice appear <br>"
      "in all copies. <br>"
      "<br>"
      "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL <br>"
      "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED <br>"
      "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL <br>"
      "THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR <br>"
      "CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING <br>"
      "FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF <br>"
      "CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT <br>"
      "OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS <br>"
      "SOFTWARE.<br>"
      "</small>";

   ui = data;

   if (ui->win_about) return;

   ui->win_about = win = elm_win_add(ui->win, "evisum", ELM_WIN_DIALOG_BASIC);
   elm_win_title_set(win, "About Evisum");
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, win);
   evas_object_data_set(win, "ui", ui);

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   elm_bg_file_set(bg, evisum_icon_path_get("ladyhand"), NULL);
   elm_win_resize_object_add(win, bg);
   evas_object_color_set(bg, 160, 160, 160, 255);
   evas_object_show(bg);
   evas_object_size_hint_min_set(bg, 320 * elm_config_scale_get(),
                   320 * elm_config_scale_get());

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_color_set(box, 255, 255, 255, 255);
   evas_object_show(box);

   label = elm_label_add(win);
   evas_object_show(label);
   elm_object_text_set(label, copyright);

   version = elm_label_add(win);
   evas_object_show(version);
   elm_object_text_set(version,
                   eina_slstr_printf("<font color=#ffffff><b>%s</b>",
                   PACKAGE_VERSION));

   btn = elm_button_add(win);
   evas_object_size_hint_align_set(btn, 0.5, 0.9);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   elm_object_text_set(btn, _("Okay!"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _win_del_cb, win);

   memset(&about_data, 0, sizeof(Animate_Data));
   about_data.bg = bg;
   about_data.label = label;
   about_data.version = version;
   about_data.pos = elm_config_scale_get() * 320;
   _animator = ecore_animator_add(about_anim, &about_data);

   elm_box_pack_end(box, version);
   elm_box_pack_end(box, label);
   elm_box_pack_end(box, btn);
   elm_object_content_set(win, box);
   evas_object_show(win);
}
