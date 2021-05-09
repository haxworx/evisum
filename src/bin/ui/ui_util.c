#include "ui_util.h"
#include "evisum_ui.h"
#include <Elementary.h>
#include "config.h"

#define ARRAY_SIZE(n) sizeof(n) / sizeof(n[0])

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                  Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tb, *rect, *btn, *lb;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);

   rect = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_weight_set(rect, EXPAND, EXPAND);
   evas_object_size_hint_align_set(rect, FILL, FILL);
   evas_object_size_hint_min_set(rect,
                   TAB_BTN_WIDTH * elm_config_scale_get(),
                   TAB_BTN_HEIGHT * elm_config_scale_get());

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

   lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   evas_object_show(lb);
   elm_object_text_set(lb,
                   eina_slstr_printf("%s", text));
   elm_layout_content_set(btn, "elm.swallow.content", lb);

   elm_table_pack(tb, rect, 0, 0, 1, 1);
   elm_table_pack(tb, btn, 0, 0, 1, 1);

   if (alias)
     *alias = btn;

   return tb;
}

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text,
                     const char *icon, Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tb, *rect, *btn, *lb, *hbx, *ic;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);

   rect = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rect, ELM_SCALE_SIZE(BTN_WIDTH),
                                 ELM_SCALE_SIZE(BTN_HEIGHT));

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

   hbx = elm_box_add(parent);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_weight_set(hbx, 0.0, EXPAND);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_show(hbx);

   ic = elm_icon_add(parent);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
   evas_object_size_hint_align_set(ic, FILL, FILL);
   evas_object_show(ic);

   elm_box_pack_end(hbx, ic);

   lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, 1.0, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   evas_object_show(lb);
   elm_object_text_set(lb,
                   eina_slstr_printf("%s", text));

   elm_box_pack_end(hbx, lb);
   elm_layout_content_set(btn, "elm.swallow.content", hbx);

   elm_table_pack(tb, rect, 0, 0, 1, 1);
   elm_table_pack(tb, btn, 0, 0, 1, 1);

   if (alias)
     *alias = btn;

   return tb;
}

const char *
evisum_size_format(unsigned long long bytes)
{
   unsigned long powi = 1;
   unsigned long long value;
   unsigned int precision = 2, powj = 1;
   int i = 0;
   static const char *units[8] = {
      _("B"), _("KiB"), _("MiB"), _("GiB"),
      _("TiB"), _("PiB"), _("EiB"), _("ZiB"),
   };

   value = bytes;
   while (value > 1024)
     {
       if ((value / 1024) < powi) break;
       powi *= 1024;
       ++i;
       if (i == 7) break;
     }

   if (!i) precision = 0;

   while (precision > 0)
     {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
     }

   return eina_slstr_printf("%1.*f %s", precision, (double) value / powi, units[i]);
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

void
_icon_cache_free_cb(void *data)
{
   char *ic_name = data;

   free(ic_name);
}

Eina_Hash *
evisum_icon_cache_new(void)
{
   Eina_Hash *icon_cache;

   icon_cache = eina_hash_string_superfast_new(_icon_cache_free_cb);

   return icon_cache;
}

void
evisum_icon_cache_del(Eina_Hash *icon_cache)
{
   eina_hash_free(icon_cache);
}

const char *
evisum_icon_cache_find(Eina_Hash *icon_cache, const Proc_Info *proc)
{
   Efreet_Desktop *e;
   const char *name, *cmd;
   char *exists;

   if (proc->is_kernel)
#if defined(__linux__)
     return "linux";
#else
     return "freebsd";
#endif

   cmd = proc->command;

   exists = eina_hash_find(icon_cache, cmd);
   if (exists) return exists;

   if (!strncmp(cmd, "enlightenment", 13)) return "e";

   e = efreet_util_desktop_exec_find(cmd);
   if (!e)
     return "application";

   if (e->icon)
     name = e->icon;
   else
     name = cmd;

   eina_hash_add(icon_cache, cmd, strdup(name));

   efreet_desktop_free(e);

   return name;
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
                 eina_slstr_printf("font='monospace' DEFAULT='font_size=%d'", new_size));
   evas_object_textblock_style_user_push(tb, ts);

   evas_textblock_style_free(ts);
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

typedef struct {
   Evisum_Ui      *ui;
   Evas_Object    *obj;
   Evas_Object    *win;
   Evas_Object    *bg;
   Evas_Object    *im;
   Ecore_Animator *animator;
   double          pos;
} Animate_Data;

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Animate_Data *ad;
   Evisum_Ui *ui;

   ad = data;
   ui = ad->ui;

   if (ad->animator)
     ecore_animator_del(ad->animator);

   free(ad);

   evas_object_del(ui->win_about);
   ui->win_about = NULL;
}

static void
_btn_close_cb(void *data EINA_UNUSED, Evas_Object *obj,
              void *event_info EINA_UNUSED)
{
   Animate_Data *ad;
   Evisum_Ui *ui;

   ad = data;
   ui = ad->ui;

   evas_object_del(ui->win_about);
}

static void
_about_resize_cb(void *data, Evas *e, Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   Animate_Data *ad = data;

   evas_object_hide(ad->obj);
}

static Eina_Bool
about_anim(void *data)
{
   Animate_Data *ad;
   Evas_Coord w, h, oh;
   ad = data;

   evas_object_geometry_get(ad->bg, NULL, NULL,  &w, &h);
   if (w <= 0 || h <= 0) return 1;
   evas_object_geometry_get(ad->obj, NULL, NULL, NULL, &oh);
   evas_object_move(ad->obj, 0, ad->pos);
   if (ad->pos <= h)
     evas_object_show(ad->obj);

   evas_object_move(ad->im, ELM_SCALE_SIZE(4), h - ELM_SCALE_SIZE(64));
   evas_object_show(ad->im);

   ad->pos -= 0.5;

   if (ad->pos <= -oh)
     {
        ad->pos =  h;
     }

   return 1;
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Animate_Data *ad;

   ad = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(ad->ui->win_about);
}

void
evisum_about_window_show(void *data)
{
   Evisum_Ui *ui;
   Animate_Data *about;
   Evas_Object *win, *bg, *tb, *version, *lb, *btn, *im;
   Evas_Object *hbx, *rec, *pad, *br;
   Evas_Coord x, y, w, h;
   Evas_Coord iw, ih;
   const char *msg[] = {
      "monitor like it's 1999...",
      "works for me!",
      "killed by a Turtle!",
      "logged in, base gone!",
      "pancakes!",
   };
   const char *copyright =
      "<font color=#ffffff>"
      "<align=center>"
      "<b>"
      "Copyright &copy; 2019-2021 Alastair Poole &lt;netstar@gmail.com&gt;<br>"
      "<br>"
      "</b>"
      "</>"
      "Permission to use, copy, modify, and distribute this software "
      "for any purpose with or without fee is hereby granted, provided "
      "that the above copyright notice and this permission notice appear "
      "in all copies.<br>"
      "<br>"
      "THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL "
      "WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED "
      "WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL "
      "THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR "
      "CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING "
      "FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF "
      "CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT "
      "OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS "
      "SOFTWARE.<br><br><br><br><br><br><br><br><br><br><br>"
      "<align=center>%s</></></>";

   ui = data;

   if (ui->win_about)
     {
        elm_win_raise(ui->win_about);
        return;
     }

   ui->win_about = win = elm_win_util_standard_add("evisum", "evisum");
   elm_win_autodel_set(win, 1);
   elm_win_center(win, 1, 1);
   elm_win_title_set(win, _("About"));

   /* All that moves */

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   elm_bg_file_set(bg, evisum_icon_path_get("ladyhand"), NULL);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);
   evas_object_size_hint_min_set(bg, ELM_SCALE_SIZE(320),
                                 ELM_SCALE_SIZE(400));
   evas_object_size_hint_max_set(bg, ELM_SCALE_SIZE(320),
                                 ELM_SCALE_SIZE(400));

   tb = elm_table_add(win);
   evas_object_show(tb);
   elm_win_resize_object_add(win, tb);
   elm_table_align_set(tb, 0, 0);

   pad = elm_frame_add(win);
   evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
   evas_object_size_hint_weight_set(pad, FILL, FILL);
   elm_object_style_set(pad, "pad_medium");

   lb = elm_entry_add(win);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   elm_entry_single_line_set(lb, 0);
   elm_object_focus_allow_set(lb, 0);
   elm_entry_scrollable_set(lb, 0);
   elm_entry_editable_set(lb, 0);
   elm_entry_select_allow_set(lb, 0);
   evas_object_size_hint_weight_set(lb, 0, 0);
   srand(time(NULL));
   elm_object_text_set(lb, eina_slstr_printf(copyright,
                                             msg[rand() % ARRAY_SIZE(msg)]));
   evas_object_show(lb);
   elm_object_content_set(pad, lb);

   evas_object_geometry_get(win, &x, &y, &w, &h);

   im = evas_object_image_filled_add(evas_object_evas_get(bg));
   evas_object_image_file_set(im, evisum_icon_path_get("lovethisdogharvey"), NULL);
   evas_object_image_size_get(im, &iw, &ih);
   evas_object_size_hint_min_set(im, ELM_SCALE_SIZE(iw) * 0.5, ELM_SCALE_SIZE(ih) * 0.5);
   evas_object_resize(im, ELM_SCALE_SIZE(iw) * 0.5, ELM_SCALE_SIZE(ih) * 0.5);
   evas_object_show(im);
   evas_object_pass_events_set(im, 1);

   about = malloc(sizeof(Animate_Data));
   about->win = win;
   about->bg = bg;
   about->obj = pad;
   about->pos = ELM_SCALE_SIZE(400);
   about->ui = ui;
   about->im = im;
   about->animator = ecore_animator_add(about_anim, about);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, about);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _about_resize_cb, about);

   /* Version overlay */

   hbx = elm_box_add(win);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, FILL, 0.5);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_show(hbx);

   version = elm_label_add(win);
   evas_object_size_hint_weight_set(version, EXPAND, 0);
   evas_object_size_hint_align_set(version, 0.1, 0.5);
   evas_object_show(version);
   elm_object_text_set(version,
                   eina_slstr_printf("<font color=#ffffff><big>%s</>",
                   PACKAGE_VERSION));

   br = elm_table_add(win);
   evas_object_size_hint_weight_set(br, EXPAND, 0);
   evas_object_size_hint_align_set(br, 0.9, 0.5);
   evas_object_show(br);

   rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_WIDTH), 1);

   btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("Close"));
   evas_object_show(btn);
   elm_table_pack(br, btn, 0, 0, 1, 1);
   elm_table_pack(br, rec, 0, 0, 1, 1);

   rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);

   elm_box_pack_end(hbx, version);
   elm_box_pack_end(hbx, rec);
   elm_box_pack_end(hbx, br);

   rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(320), ELM_SCALE_SIZE(64));
   evas_object_color_set(rec, 0, 0, 0, 128);
   evas_object_show(rec);

   evas_object_smart_callback_add(btn, "clicked", _btn_close_cb, about);

   elm_object_part_content_set(bg, "elm.swallow.content", pad);
   elm_table_pack(tb, im, 0, 1, 1, 1);
   elm_table_pack(tb, rec, 0, 0, 1, 1);
   elm_table_pack(tb, hbx, 0, 0, 1, 1);
   elm_object_content_set(win, tb);
   evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, about);

   evas_object_show(win);
}

const char *
evisum_image_path_get(const char *name)
{
   char *path;
   const char *icon_path = NULL, *directory = PACKAGE_DATA_DIR "/images";

   path = _path_append(directory, eina_slstr_printf("%s.jpg", name));
   if (path)
     {
        if (ecore_file_exists(path))
          icon_path = eina_slstr_printf("%s", path);

        free(path);
     }

   return icon_path;
}

Evas_Object *
evisum_ui_background_add(Evas_Object *win)
{
   Evas_Object *bg;

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_data_set(win, "bg", bg);
   evas_object_show(bg);

   return bg;
}

void
evisum_ui_icon_size_set(Evas_Object *ic, int size)
{
   evas_object_size_hint_min_set(ic, size, size);
   evas_object_size_hint_max_set(ic, size, size);
}
