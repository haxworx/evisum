#include "evisum_ui_util.h"
#include "evisum_ui.h"

#include <Elementary.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"

#define ARRAY_SIZE(n)          (sizeof(n) / sizeof((n)[0]))
#define EVISUM_THEME_FILE      PACKAGE_DATA_DIR "/themes/evisum.edj"
#define EVISUM_ABOUT_BG_WIDTH  560
#define EVISUM_ABOUT_BG_HEIGHT 540

static const char *about_msg[] = { "The greatest of all time...",
                                   "Remember to take your medication!",
                                   "Choose love!",
                                   "Schizophrenia!!!",
                                   "I endorse this message!",
                                   "Hack the planet!",
                                   "Remember what you need to carry!",
				   "When Chuck Norris uses malloc, he frees memory",
                                   "Be kind to others",
                                   "Be patient with yourself",
                                   "Trust me",
                                   "Well done my son." };

static const char *about_text_fmt
        = "<font color=#ffffff>"
          "<align=center>"
          "<b>"
          "Copyright &copy; 2019-2021, 2024-2026. Alastair Poole &lt;netstar@netstar.im&gt;<br>"
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

static inline const char *
_about_random_msg(void) {
    return about_msg[rand() % ARRAY_SIZE(about_msg)];
}

typedef struct {
    Evisum_Ui *ui;
    Evas_Object *obj;
    Evas_Object *clip;
    Evas_Object *win;
    Evas_Object *bg;
    Evas_Object *im;
    Evas_Object *lb;
    Ecore_Animator *animator;
    double pos;
    double speed;
    double last_tick;
    int reset_hide_frames;
    int w;
    int h;
} Animate_Data;

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Animate_Data *ad = data;
    Evisum_Ui *ui = ad->ui;

    if (ad->animator) ecore_animator_del(ad->animator);
    if (ad->clip) evas_object_del(ad->clip);

    free(ad);

    evas_object_del(ui->win_about);
    ui->win_about = NULL;
}

static void
_btn_close_cb(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Animate_Data *ad = data;
    Evisum_Ui *ui = ad->ui;

    evas_object_del(ui->win_about);
    (void) obj;
}

static void
_about_resize_cb(void *data, Evas *e, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Animate_Data *ad = data;
    Evas_Coord w, h;

    evas_object_geometry_get(ad->win, NULL, NULL, &w, &h);
    if ((w != ad->w) || (h != ad->h)) evas_object_resize(ad->win, ad->w, ad->h);
    evas_object_hide(ad->obj);
    (void) e;
}

static Eina_Bool
about_anim(void *data) {
    Animate_Data *ad = data;
    double now, dt;
    Evas_Coord bx, by, bw, bh, ox, oy, ow, oh;

    evas_object_geometry_get(ad->bg, &bx, &by, &bw, &bh);
    evas_object_geometry_get(ad->obj, NULL, NULL, &ow, &oh);
    if (bw <= 0 || bh <= 0 || ow <= 0 || oh <= 0) {
        evas_object_hide(ad->obj);
        return 1;
    }
    if (ad->clip) {
        evas_object_move(ad->clip, bx, by);
        evas_object_resize(ad->clip, bw, bh);
    }

    if (ad->im) {
        evas_object_move(ad->im, ELM_SCALE_SIZE(4), bh - ELM_SCALE_SIZE(64));
        evas_object_show(ad->im);
    }

    now = ecore_loop_time_get();
    if (ad->last_tick <= 0.0) ad->last_tick = now;
    dt = now - ad->last_tick;
    ad->last_tick = now;

    if (dt < 0.0) dt = 0.0;
    if (dt > 0.1) dt = 0.1;

    if (ad->reset_hide_frames > 0) {
        ad->reset_hide_frames--;
        evas_object_move(ad->obj, bx, by + ad->pos);
        evas_object_hide(ad->obj);
        return 1;
    }

    ad->pos -= (ad->speed * dt);

    if (ad->pos <= -oh) {
        ad->pos = bh + ELM_SCALE_SIZE(2);
        if (ad->lb) elm_object_text_set(ad->lb, eina_slstr_printf(about_text_fmt, _about_random_msg()));
        ad->reset_hide_frames = 1;
        evas_object_move(ad->obj, bx, by + ad->pos);
        evas_object_hide(ad->obj);
        return 1;
    }

    evas_object_move(ad->obj, bx, by + ad->pos);
    evas_object_geometry_get(ad->obj, &ox, &oy, &ow, &oh);
    if ((ox < (bx + bw)) && ((ox + ow) > bx) && (oy < (by + bh)) && ((oy + oh) > by)) evas_object_show(ad->obj);
    else evas_object_hide(ad->obj);

    return 1;
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
    Evas_Event_Key_Down *ev = event_info;
    Animate_Data *ad = data;

    if (!ev || !ev->keyname) return;

    if (!strcmp(ev->keyname, "Escape")) evas_object_del(ad->ui->win_about);
    (void) e;
    (void) obj;
}

void
evisum_about_window_show(void *data) {
    Evisum_Ui *ui = data;
    Animate_Data *about;
    Evas_Object *win, *bg, *tb, *version, *lb, *btn;
    Evas_Object *hbx, *rec, *pad, *pad_left, *pad_right, *clip;
    Evas_Object *hdr_vbx, *pad_top;
    Evas_Object *hdr_bg;
    int about_w, about_h, bg_w, bg_h, hdr_h, about_pad, btn_h;

    about_w = EVISUM_ABOUT_BG_WIDTH;
    about_h = EVISUM_ABOUT_BG_HEIGHT;
    about_pad = ELM_SCALE_SIZE(16);
    btn_h = ELM_SCALE_SIZE(BTN_HEIGHT);
    hdr_h = btn_h + about_pad;

    if (ui->win_about) {
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
    evas_object_size_hint_weight_set(pad, 0.0, 0.0);
    evas_object_size_hint_align_set(pad, FILL, 0.0);
    evas_object_pass_events_set(pad, EINA_TRUE);
    elm_object_style_set(pad, "pad_medium");

    lb = elm_entry_add(win);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(lb, FILL, FILL);
    elm_entry_single_line_set(lb, 0);
    elm_object_focus_allow_set(lb, 0);
    elm_entry_scrollable_set(lb, 0);
    elm_entry_editable_set(lb, 0);
    elm_entry_select_allow_set(lb, 0);
    evas_object_pass_events_set(lb, EINA_TRUE);
    evas_object_size_hint_weight_set(lb, 0, 0);
    srand(time(NULL));
    elm_object_text_set(lb, eina_slstr_printf(about_text_fmt, _about_random_msg()));
    evas_object_show(lb);
    elm_object_content_set(pad, lb);
    evas_object_show(pad);

    clip = evas_object_rectangle_add(evas_object_evas_get(win));
    evas_object_pass_events_set(clip, EINA_TRUE);
    evas_object_hide(clip);
    evas_object_clip_set(pad, clip);

    about = malloc(sizeof(Animate_Data));
    if (!about) return;

    about->win = win;
    about->bg = bg;
    about->obj = pad;
    about->clip = clip;
    about->lb = lb;
    about->pos = ELM_SCALE_SIZE(400);
    about->speed = ELM_SCALE_SIZE(48);
    about->last_tick = 0.0;
    about->reset_hide_frames = 0;
    about->ui = ui;
    about->im = NULL;
    about->w = about_w;
    about->h = about_h;
    about->animator = ecore_animator_add(about_anim, about);
    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, about);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _about_resize_cb, about);

    hbx = elm_box_add(win);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_align_set(hbx, FILL, FILL);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);
    evas_object_size_hint_min_set(hbx, about_w, btn_h);
    evas_object_show(hbx);

    hdr_vbx = elm_box_add(win);
    evas_object_size_hint_align_set(hdr_vbx, FILL, 0.0);
    evas_object_size_hint_weight_set(hdr_vbx, EXPAND, 0);
    evas_object_size_hint_min_set(hdr_vbx, about_w, hdr_h);
    evas_object_show(hdr_vbx);

    version = elm_label_add(win);
    evas_object_size_hint_weight_set(version, 0, 0);
    evas_object_size_hint_align_set(version, 0.0, 0.5);
    evas_object_show(version);
    elm_object_text_set(version, eina_slstr_printf("<font color=#ffffff><big>%s</>", PACKAGE_VERSION));

    btn = elm_button_add(win);
    evas_object_size_hint_weight_set(btn, 0, 0);
    evas_object_size_hint_align_set(btn, 1.0, 0.5);
    evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), btn_h);
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
    evas_object_size_hint_min_set(pad_left, about_pad, 1);
    evas_object_color_set(pad_left, 0, 0, 0, 0);
    evas_object_show(pad_left);

    pad_right = evas_object_rectangle_add(evas_object_evas_get(win));
    evas_object_size_hint_align_set(pad_right, FILL, FILL);
    evas_object_size_hint_weight_set(pad_right, 0, EXPAND);
    evas_object_size_hint_min_set(pad_right, about_pad, 1);
    evas_object_color_set(pad_right, 0, 0, 0, 0);
    evas_object_show(pad_right);

    elm_box_pack_end(hbx, pad_left);
    elm_box_pack_end(hbx, version);
    elm_box_pack_end(hbx, rec);
    elm_box_pack_end(hbx, btn);
    elm_box_pack_end(hbx, pad_right);

    pad_top = evas_object_rectangle_add(evas_object_evas_get(win));
    evas_object_size_hint_align_set(pad_top, FILL, FILL);
    evas_object_size_hint_weight_set(pad_top, EXPAND, 0);
    evas_object_size_hint_min_set(pad_top, about_w, about_pad);
    evas_object_color_set(pad_top, 0, 0, 0, 0);
    evas_object_show(pad_top);

    elm_box_pack_end(hdr_vbx, pad_top);
    elm_box_pack_end(hdr_vbx, hbx);

    hdr_bg = evas_object_rectangle_add(evas_object_evas_get(win));
    evas_object_size_hint_align_set(hdr_bg, FILL, 0.0);
    evas_object_size_hint_weight_set(hdr_bg, EXPAND, 0);
    evas_object_size_hint_min_set(hdr_bg, about_w, 1);
    evas_object_color_set(hdr_bg, 0, 0, 0, 77);
    evas_object_show(hdr_bg);

    evas_object_smart_callback_add(btn, "clicked", _btn_close_cb, about);

    elm_table_pack(tb, bg, 0, 0, 1, 1);
    elm_table_pack(tb, pad, 0, 0, 1, 1);
    elm_table_pack(tb, hdr_bg, 0, 0, 1, 1);
    elm_table_pack(tb, hdr_vbx, 0, 0, 1, 1);
    elm_object_content_set(win, tb);
    evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, about);

    evas_object_show(win);
}
