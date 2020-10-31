#include "ui_util.h"
#include "ui.h"
#include <Elementary.h>
#include "config.h"

#define ARRAY_SIZE(n) sizeof(n) / sizeof(n[0])

static Eina_Bool _effects_enabled = EINA_FALSE;
static Eina_Bool _backgrounds_enabled = EINA_FALSE;
static Eina_Hash *_icon_cache = NULL;

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
                     const char *icon, Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tbl, *rect, *button, *label, *hbx, *ic;

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

   hbx = elm_box_add(parent);
   elm_box_horizontal_set(hbx, EINA_TRUE);
   evas_object_size_hint_weight_set(hbx, 0.0, EXPAND);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_show(hbx);

   ic = elm_icon_add(parent);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
   evas_object_size_hint_align_set(ic, FILL, FILL);
   evas_object_show(ic);

   elm_box_pack_end(hbx, ic);

   label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, 1.0, EXPAND);
   evas_object_size_hint_align_set(label, FILL, FILL);
   evas_object_show(label);
   elm_object_text_set(label,
                   eina_slstr_printf("%s", text));

   elm_box_pack_end(hbx, label);
   elm_layout_content_set(button, "elm.swallow.content", hbx);

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

void
_icon_cache_free_cb(void *data)
{
   char *ic_name = data;

   free(ic_name);
}

void
evisum_icon_cache_init(void)
{
   _icon_cache = eina_hash_string_superfast_new(_icon_cache_free_cb);
}

void
evisum_icon_cache_shutdown(void)
{
   eina_hash_free(_icon_cache);
}

const char *
evisum_icon_cache_find(const char *cmd)
{
   Efreet_Desktop *e;
   const char *name;
   char *exists;

   exists = eina_hash_find(_icon_cache, cmd);
   if (exists) return exists;

   if (!strncmp(cmd, "enlightenment", 13)) return "e";

   e = efreet_util_desktop_exec_find(cmd);
   if (!e)
     return "application";

   if (e->icon)
     name = e->icon;
   else
     name = cmd;

   eina_hash_add(_icon_cache, cmd, strdup(name));

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

typedef struct {
   Ui             *ui;
   Evas_Object    *label;
   Evas_Object    *win;
   Evas_Object    *bg;
   Evas_Object    *im;
   Ecore_Animator *animator;
   int             pos;
   int             pos2;
   Eina_Bool       im_upwards;
} Animate_Data;

static void
_win_del_cb(void *data, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Animate_Data *ad;
   Ui *ui;

   ad = data;
   ui = ad->ui;

   if (ad->animator)
     ecore_animator_del(ad->animator);

   free(ad);

   evas_object_del(ui->win_about);
   ui->win_about = NULL;
}

static void
_about_resize_cb(void *data, Evas_Object *obj EINA_UNUSED,
                 void *event_info EINA_UNUSED)
{
   Animate_Data *ad = data;

   evas_object_hide(ad->label);
}

static Eina_Bool
about_anim(void *data)
{
   Animate_Data *ad;
   Evas_Coord w, h, ow, oh, x;
   Evas_Coord ix, iy, iw, ih;
   static Eina_Bool begin = 0;
   time_t t = time(NULL);

   ad = data;

   evas_object_geometry_get(ad->bg, NULL, NULL,  &w, &h);
   if (w <= 0 || h <= 0) return EINA_TRUE;
   evas_object_geometry_get(ad->label, &x, NULL, &ow, &oh);
   evas_object_move(ad->label, x, ad->pos);
   evas_object_show(ad->label);

   ad->pos--;

   if (ad->pos <= -oh) ad->pos = h;

   if (!(t % 20)) begin = 1;

   if (!begin) return EINA_TRUE;

   evas_object_geometry_get(ad->im, &ix, &iy, &iw, &ih);

   if (ad->im_upwards)
     ad->pos2 -= 1;
   else
     ad->pos2 += 5;

   evas_object_move(ad->im, ix, ad->pos2);
   evas_object_show(ad->im);

   if (ad->pos2 < ih)
     {
        ad->im_upwards = 0;
     }
   else if (ad->pos2 > h + ih)
     {
        ad->pos2 = (h + ih);
        ad->im_upwards = 1;
        srand(t);
        evas_object_move(ad->im, rand() % w, ad->pos2);
        evas_object_hide(ad->im);
        begin = 0;
     }

   return EINA_TRUE;
}

void
evisum_about_window_show(void *data)
{
   Ui *ui;
   Animate_Data *about;
   Evas_Object *win, *bg, *tbl, *version, *label, *btn, *im;
   Evas_Object *hbx, *rec, *br;
   Evas_Coord x, y, w, h;
   Evas_Coord iw, ih;
   const char *copyright =
      "<font color=#ffffff>"
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
      "SOFTWARE.<br></></>";

   ui = data;

   if (ui->win_about) return;

   ui->win_about = win = elm_win_add(ui->win, "evisum", ELM_WIN_DIALOG_BASIC);
   elm_win_title_set(win, _("About"));

   /* All that moves */

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   elm_bg_file_set(bg, evisum_icon_path_get("ladyhand"), NULL);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);
   evas_object_size_hint_min_set(bg, 320 * elm_config_scale_get(),
                   320 * elm_config_scale_get());

   tbl = elm_table_add(win);
   evas_object_show(tbl);
   elm_win_resize_object_add(tbl, win);
   elm_table_align_set(tbl, 0, 0);

   label = elm_label_add(win);
   evas_object_size_hint_align_set(label, 0.0, 0.5);
   evas_object_size_hint_weight_set(label, EXPAND, 0);
   elm_object_text_set(label, copyright);

   evas_object_geometry_get(win, &x, &y, &w, &h);

   im = evas_object_image_filled_add(evas_object_evas_get(bg));
   evas_object_image_file_set(im, evisum_icon_path_get("lovethisdogharvey"), NULL);
   evas_object_image_size_get(im, &iw, &ih);
   evas_object_size_hint_min_set(im, iw / 2, ih / 2);
   evas_object_resize(im, iw / 2, ih / 2);
   evas_object_move(im, iw / 3, h + ih + ih);
   evas_object_pass_events_set(im, 1);

   about = malloc(sizeof(Animate_Data));
   about->win = win;
   about->bg = bg;
   about->label = label;
   about->pos = elm_config_scale_get() * 320;
   about->ui = ui;
   about->im = im;
   about->pos2 = h + ih + ih;
   about->im_upwards = 1;
   about->animator = ecore_animator_add(about_anim, about);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, about);
   evas_object_smart_callback_add(win, "resize", _about_resize_cb, about);

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
   evas_object_size_hint_min_set(rec, 92, 1);

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
   evas_object_size_hint_min_set(rec, 320 * elm_config_scale_get(), 64);
   evas_object_color_set(rec, 0, 0, 0, 128);
   evas_object_show(rec);

   evas_object_smart_callback_add(btn, "clicked", _win_del_cb, about);

   elm_table_pack(tbl, label, 0, 1, 1, 1);
   elm_table_pack(tbl, im, 0, 1, 1, 1);
   elm_table_pack(tbl, rec, 0, 0, 1, 1);
   elm_table_pack(tbl, hbx, 0, 0, 1, 1);
   elm_object_content_set(win, tbl);

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
evisum_ui_background_random_add(Evas_Object *win, Eina_Bool enabled)
{
   Evas_Object *bg;
   int i;
   char *images[] = { "sky_01", "sky_02", "sky_03", "sky_04"  };

   if (!enabled) return NULL;

   srand(time(NULL));

   i = rand() % ARRAY_SIZE(images);

   bg = elm_bg_add(win);
   elm_bg_file_set(bg, evisum_image_path_get(images[i]), NULL);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   return bg;
}

Evas_Object *
evisum_ui_background_add(Evas_Object *win, Eina_Bool enabled)
{
   Evas_Object *bg;

   if (!enabled) return NULL;

   bg = elm_bg_add(win);
   elm_bg_file_set(bg, evisum_image_path_get("sky_03"), NULL);
   evas_object_size_hint_align_set(bg, FILL, FILL);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_show(bg);

   return bg;
}

Eina_Bool
evisum_ui_backgrounds_enabled_get(void)
{
   return _backgrounds_enabled;
}

void
evisum_ui_backgrounds_enabled_set(Eina_Bool enabled)
{
   _backgrounds_enabled = enabled;
}

Eina_Bool
evisum_ui_effects_enabled_get(void)
{
   return _effects_enabled;
}

void
evisum_ui_effects_enabled_set(Eina_Bool enabled)
{
   _effects_enabled = enabled;
}

typedef struct
{
   Ui          *ui;
   int          pos;
   Evas_Object *im;
   Evas_Object *bolt;
} Animation;

static Eina_Bool
_anim_clouds(void *data)
{
   Ui *ui;
   Animation *anim;
   Evas_Coord ww, wh, iw, ih;
   int cpu;
   time_t t;
   static int bolt = 0;

   anim = data;
   ui = anim->ui;

   evas_object_geometry_get(ui->win, NULL, NULL, &ww, &wh);
   if (ww <= 0 || wh <= 0) return EINA_TRUE;
   evas_object_image_size_get(anim->im, &iw, &ih);

   if (ww > iw) iw = ww;

   cpu = (ui->cpu_usage / 10) > 0 ? ui->cpu_usage / 10  :  1;

   evas_object_resize(anim->im, iw, wh);
   evas_object_image_fill_set(anim->im, anim->pos, -50, iw, ih);
   anim->pos += cpu;

   t = time(NULL);

   if (cpu >= 6 && !bolt)
     {
        if (cpu == 6 && !(t % 16)) bolt++;
        else if (cpu == 7 && !(t % 8)) bolt++;
        else if (cpu == 8 && !(t % 4)) bolt++;
        else if (cpu == 9 && !(t % 2)) bolt++;
        else if (cpu == 10) bolt++;
     }

   if (bolt)
     {
        struct timespec ts;

        if (bolt++ == 1)
          {
             clock_gettime(CLOCK_REALTIME, &ts);
             srand(ts.tv_nsec);
             evas_object_image_size_get(anim->bolt, &iw, &ih);
             evas_object_move(anim->bolt, -(rand() % iw), -(rand() % (ih / 4)));
          }

        if (bolt > 20)
          {
             evas_object_hide(anim->bolt);
             bolt = 0;
          }
        else if (!(bolt % 2))
          evas_object_show(anim->bolt);
        else
          evas_object_hide(anim->bolt);
     }

   if (anim->pos >= iw)
     anim->pos = 0;

   return ECORE_CALLBACK_RENEW;
}

void
evisum_ui_animate(void *data)
{
   Animation *anim;
   Ui *ui;
   Evas_Object *im;
   Evas_Coord iw, ih, ww, wh;

   ui = data;

   anim = calloc(1, sizeof(Animation));
   if (!anim) return;

   anim->ui = ui;

   evas_object_geometry_get(ui->win, NULL, NULL, &ww, &wh);

   anim->bolt = im = evas_object_image_filled_add(evas_object_evas_get(ui->win));
   evas_object_pass_events_set(im, 1);
   evas_object_image_file_set(im, evisum_icon_path_get("bolt"), NULL);
   evas_object_image_size_get(im, &iw, &ih);
   evas_object_size_hint_min_set(im, iw, ih);
   evas_object_resize(im, iw, ih);

   anim->im = im = evas_object_image_add(evas_object_evas_get(ui->win));
   evas_object_image_file_set(im, evisum_icon_path_get("clo"), NULL);
   evas_object_image_size_get(im, &iw, &ih);
   evas_object_image_fill_set(im, 0, -50, iw, wh);
   evas_object_resize(im, iw, wh);
   evas_object_move(im, 0, 0);
   evas_object_pass_events_set(im, 1);
   evas_object_show(im);

   ui->animator = ecore_animator_add(_anim_clouds, anim);
}

