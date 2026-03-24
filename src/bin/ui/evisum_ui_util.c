#include "evisum_ui_util.h"
#include "evisum_ui.h"
#include <Elementary.h>
#include "config.h"

#define ARRAY_SIZE(n) sizeof(n) / sizeof(n[0])
#define EVISUM_THEME_FILE PACKAGE_DATA_DIR "/themes/evisum.edj"
#define EVISUM_THEME_ICON_GROUP_PREFIX "evisum/icons/"
#define EVISUM_ABOUT_BG_WIDTH 560
#define EVISUM_ABOUT_BG_HEIGHT 540

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
   evisum_ui_icon_set(ic, icon);
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
evisum_size_format(unsigned long long bytes, Eina_Bool simple)
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

   if (simple)
     return eina_slstr_printf("%1.*f %c", precision, (double) value / powi, units[i][0]);

   return eina_slstr_printf("%1.*f %s", precision, (double) value / powi, units[i]);
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
     {
        Proc_Info *pproc = proc_info_by_pid(proc->ppid);

        if (!pproc) return "application";
        const char *command = evisum_icon_cache_find(icon_cache, pproc);
        proc_info_free(pproc);
        return command;
     }

   if (e->icon)
     name = e->icon;
   else
     name = cmd;

   eina_hash_add(icon_cache, cmd, strdup(name));

   efreet_desktop_free(e);

   return name;
}

const char *
evisum_ui_icon_name_get(const char *name)
{
   const char *group;

   if (!name) return NULL;

   group = eina_slstr_printf(EVISUM_THEME_ICON_GROUP_PREFIX "%s", name);
   if (edje_file_group_exists(EVISUM_THEME_FILE, group))
     return group;

   return name;
}

void
evisum_ui_icon_set(Evas_Object *ic, const char *name)
{
   const char *icon;
   Evas_Object *img;

   if (!ic || !name) return;

   icon = evisum_ui_icon_name_get(name);
   if (!icon) return;

   if (!strncmp(icon, EVISUM_THEME_ICON_GROUP_PREFIX,
                strlen(EVISUM_THEME_ICON_GROUP_PREFIX)))
     {
        if (elm_image_file_set(ic, EVISUM_THEME_FILE, icon))
          {
             elm_image_resizable_set(ic, EINA_TRUE, EINA_TRUE);
             elm_image_smooth_set(ic, EINA_TRUE);
             evas_object_color_set(ic, 255, 255, 255, 255);
             img = elm_image_object_get(ic);
             if (img)
               {
                  const char *type = evas_object_type_get(img);
                  evas_object_color_set(img, 255, 255, 255, 255);
                  if (type && !strcmp(type, "image"))
                    evas_object_image_alpha_set(img, EINA_TRUE);
               }
             return;
          }
     }

   elm_icon_standard_set(ic, icon);
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
   int             w;
   int             h;
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
   Evas_Coord w, h;

   evas_object_geometry_get(ad->win, NULL, NULL, &w, &h);
   if ((w != ad->w) || (h != ad->h))
     evas_object_resize(ad->win, ad->w, ad->h);
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

   if (ad->im)
     {
        evas_object_move(ad->im, ELM_SCALE_SIZE(4), h - ELM_SCALE_SIZE(64));
        evas_object_show(ad->im);
     }

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
   Evas_Object *win, *bg, *tb, *version, *lb, *btn;
   Evas_Object *hbx, *rec, *pad, *pad_left, *pad_right;
   Evas_Object *hdr_bg;
   int about_w, about_h, bg_w, bg_h, hdr_h;
   const char *msg[] = {
      "The greatest of all time...",
      "Remember to take your medication!",
      "Choose love!",
      "Schizophrenia!!!",
      "I endorse this message!",
      "Hack the planet!",
      "Remember what you need to carry!",
      "Well done my son."
   };
   const char *copyright =
      "<font color=#ffffff>"
      "<align=center>"
      "<b>"
      "Copyright &copy; 2019-2021, 2024-2026. Alastair Poole &lt;alastair.poole@pm.me&gt;<br>"
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
   about_w = EVISUM_ABOUT_BG_WIDTH;
   about_h = EVISUM_ABOUT_BG_HEIGHT;
   hdr_h = ELM_SCALE_SIZE(BTN_HEIGHT + 16);

   if (ui->win_about)
     {
        elm_win_raise(ui->win_about);
        return;
     }

   ui->win_about = win = elm_win_util_dialog_add(ui->proc.win, "evisum", _("About"));
   elm_win_autodel_set(win, 1);
   elm_win_size_base_set(win, about_w, about_h);
   evas_object_size_hint_min_set(win, about_w, about_h);
   evas_object_size_hint_max_set(win, about_w, about_h);
   evas_object_resize(win, about_w, about_h);
   elm_win_center(win, 1, 1);
   elm_win_title_set(win, _("About"));

   /* All that moves */

   bg_w = about_w + (about_w / 10);
   bg_h = about_h + (about_h / 10);

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, 0.0, 0.0);
   evas_object_size_hint_align_set(bg, 0.5, 0.5);
   elm_bg_option_set(bg, ELM_BG_OPTION_STRETCH);
   elm_bg_file_set(bg, EVISUM_THEME_FILE, evisum_ui_icon_name_get("ladyhand"));
   elm_win_resize_object_add(win, bg);
    evas_object_show(bg);

   evas_object_size_hint_min_set(bg, bg_w, bg_h);
   evas_object_size_hint_max_set(bg, bg_w, bg_h);

   tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
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

   about = malloc(sizeof(Animate_Data));
   about->win = win;
   about->bg = bg;
   about->obj = pad;
   about->pos = ELM_SCALE_SIZE(400);
   about->ui = ui;
   about->im = NULL;
   about->w = about_w;
   about->h = about_h;
   about->animator = ecore_animator_add(about_anim, about);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, about);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _about_resize_cb, about);

   /* Version overlay */

   hbx = elm_box_add(win);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, FILL, 0.0);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_min_set(hbx, about_w, hdr_h);
   evas_object_show(hbx);

   version = elm_label_add(win);
   evas_object_size_hint_weight_set(version, 0, 0);
   evas_object_size_hint_align_set(version, 0.0, 0.5);
   evas_object_show(version);
   elm_object_text_set(version,
                   eina_slstr_printf("<font color=#ffffff><big>%s</>",
                   PACKAGE_VERSION));

   btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, 0);
   evas_object_size_hint_align_set(btn, 1.0, 0.5);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_object_text_set(btn, _("Close"));
   evas_object_show(btn);

   rec = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
   evas_object_color_set(rec, 0, 0, 0, 0);
   evas_object_show(rec);

   pad_left = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(pad_left, FILL, FILL);
   evas_object_size_hint_weight_set(pad_left, 0, EXPAND);
   evas_object_size_hint_min_set(pad_left, ELM_SCALE_SIZE(8), 1);
   evas_object_color_set(pad_left, 0, 0, 0, 0);
   evas_object_show(pad_left);

   pad_right = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(pad_right, FILL, FILL);
   evas_object_size_hint_weight_set(pad_right, 0, EXPAND);
   evas_object_size_hint_min_set(pad_right, ELM_SCALE_SIZE(8), 1);
   evas_object_color_set(pad_right, 0, 0, 0, 0);
   evas_object_show(pad_right);

   elm_box_pack_end(hbx, pad_left);
   elm_box_pack_end(hbx, version);
   elm_box_pack_end(hbx, rec);
   elm_box_pack_end(hbx, btn);
   elm_box_pack_end(hbx, pad_right);

   hdr_bg = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_align_set(hdr_bg, FILL, 0.0);
   evas_object_size_hint_weight_set(hdr_bg, EXPAND, 0);
   evas_object_size_hint_min_set(hdr_bg, about_w, 1);//hdr_h * 2);
   evas_object_color_set(hdr_bg, 0, 0, 0, 128);
   evas_object_show(hdr_bg);

   evas_object_smart_callback_add(btn, "clicked", _btn_close_cb, about);

   elm_object_part_content_set(bg, "elm.swallow.content", pad);
   elm_table_pack(tb, bg, 0, 0, 1, 1);
   elm_table_pack(tb, hdr_bg, 0, 0, 1, 1);
   elm_table_pack(tb, hbx, 0, 0, 1, 1);
   elm_object_content_set(win, tb);
   evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, about);

   evas_object_show(win);
}

const char *
evisum_image_path_get(const char *name)
{
   const char *group;

   if (!name) return NULL;

   group = eina_slstr_printf(EVISUM_THEME_ICON_GROUP_PREFIX "%s", name);
   if (edje_file_group_exists(EVISUM_THEME_FILE, group))
     return group;

   return NULL;
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
