#include "evisum_ui_util.h"
#include "evisum_ui.h"

#include <Elementary.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "config.h"

#define ARRAY_SIZE(n)          (sizeof(n) / sizeof((n)[0]))
#define EVISUM_ABOUT_BG_FILE   "images/ladyhand.png"
#define EVISUM_ABOUT_BG_FILE_ABS PACKAGE_DATA_DIR "/images/ladyhand.png"
#define EVISUM_THEME_FILE      PACKAGE_DATA_DIR "/themes/evisum.edj"
#define EVISUM_THEME_FILE_REL  "data/themes/evisum.edj"
#define EVISUM_THEME_GROUP     "evisum/about/ladyhand"
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

static Evas_Object *
_about_background_create(Evas_Object *parent) {
    Evas_Object *bg;
    Eina_Bool ok = EINA_FALSE;
    const char *data_dir = elm_app_data_dir_get();
    char buf[PATH_MAX];

    bg = elm_image_add(parent);
    elm_image_resizable_set(bg, EINA_TRUE, EINA_TRUE);
    elm_image_aspect_fixed_set(bg, EINA_TRUE);
    elm_image_fill_outside_set(bg, EINA_TRUE);

    ok = elm_image_file_set(bg, EVISUM_THEME_FILE, EVISUM_THEME_GROUP);
    if (!ok) ok = elm_image_file_set(bg, EVISUM_THEME_FILE_REL, EVISUM_THEME_GROUP);

    if (data_dir && data_dir[0]) {
        snprintf(buf, sizeof(buf), "%s/%s", data_dir, EVISUM_ABOUT_BG_FILE);
        if (!ok) ok = elm_image_file_set(bg, buf, NULL);
    }

    if (!ok) ok = elm_image_file_set(bg, EVISUM_ABOUT_BG_FILE_ABS, NULL);
    if (!ok) ok = elm_image_file_set(bg, "data/images/ladyhand.png", NULL);
    if (!ok) {
        evas_object_del(bg);
        bg = elm_bg_add(parent);
        evas_object_color_set(bg, 26, 26, 26, 255);
    }
    return bg;
}

static void
_about_delete_request_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    evas_object_del(data);
}

typedef struct {
    Evisum_Ui *ui;
    Evas_Object *scroller;
    Evas_Object *top_spacer;
    Evas_Object *bottom_spacer;
    Evas_Object *lb;
    Ecore_Animator *animator;
    double pos;
} Animate_Data;

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Animate_Data *ad = data;
    Evisum_Ui *ui = ad->ui;

    if (ad->animator) ecore_animator_del(ad->animator);

    free(ad);

    if (ui->win_about == obj) ui->win_about = NULL;
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

    if (!ad) return;
    elm_scroller_region_get(ad->scroller, NULL, NULL, &w, &h);
    if (h <= 0) return;

    evas_object_size_hint_min_set(ad->top_spacer, w, h);
    evas_object_size_hint_min_set(ad->bottom_spacer, w, h);
    ad->pos = 0.0;
    elm_scroller_region_show(ad->scroller, 0, 0, w, h);
    (void) e;
}

static Eina_Bool
about_anim(void *data) {
    Animate_Data *ad = data;
    Evas_Coord w, h, cw, ch;
    double max_y;

    if (!ad || !ad->scroller) return ECORE_CALLBACK_CANCEL;

    elm_scroller_region_get(ad->scroller, NULL, NULL, &w, &h);
    elm_scroller_child_size_get(ad->scroller, &cw, &ch);
    if (h <= 0 || ch <= h) return ECORE_CALLBACK_RENEW;

    max_y = ch - h;
    ad->pos += 24.0 * ecore_animator_frametime_get();
    if (ad->pos > max_y) {
        ad->pos = 0.0;
        if (ad->lb) elm_object_text_set(ad->lb, eina_slstr_printf(about_text_fmt, _about_random_msg()));
    }

    elm_scroller_region_show(ad->scroller, 0, (int) ad->pos, w, h);
    return ECORE_CALLBACK_RENEW;
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
    Evas_Object *win, *bg, *root, *header, *version, *lb, *btn;
    Evas_Object *hbx, *spacer, *pad_left, *pad_right, *scroller, *scroller_box;
    Evas_Object *top_spacer, *bottom_spacer;
    Evas_Object *pad_top;
    int about_w, about_h, hdr_h, about_pad, btn_h;

    about_w = ELM_SCALE_SIZE(EVISUM_ABOUT_BG_WIDTH);
    about_h = ELM_SCALE_SIZE(EVISUM_ABOUT_BG_HEIGHT);
    about_pad = ELM_SCALE_SIZE(16);
    btn_h = ELM_SCALE_SIZE(BTN_HEIGHT);
    hdr_h = btn_h + about_pad;

    if (ui->win_about) {
        elm_win_raise(ui->win_about);
        return;
    }

    ui->win_about = win = elm_win_add(ui->proc.win, "about", ELM_WIN_DIALOG_BASIC);
    elm_win_autodel_set(win, 1);
    evas_object_smart_callback_add(win, "delete,request", _about_delete_request_cb, win);
    evas_object_size_hint_min_set(win, about_w, about_h);
    evas_object_size_hint_max_set(win, about_w, about_h);
    elm_win_center(win, 1, 1);
    elm_win_title_set(win, _("About"));

    bg = _about_background_create(win);
    evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bg, FILL, FILL);
    elm_win_resize_object_add(win, bg);
    evas_object_show(bg);

    root = elm_box_add(win);
    elm_box_horizontal_set(root, EINA_FALSE);
    evas_object_size_hint_weight_set(root, EXPAND, EXPAND);
    evas_object_size_hint_align_set(root, FILL, FILL);
    elm_win_resize_object_add(win, root);
    evas_object_show(root);

    header = elm_box_add(win);
    elm_box_horizontal_set(header, EINA_FALSE);
    evas_object_size_hint_weight_set(header, EXPAND, 0.0);
    evas_object_size_hint_align_set(header, FILL, 0.0);
    evas_object_size_hint_min_set(header, about_w, hdr_h);
    evas_object_show(header);
    elm_box_pack_end(root, header);

    scroller = elm_scroller_add(win);
    elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_OFF);
    elm_scroller_bounce_set(scroller, EINA_FALSE, EINA_FALSE);
    elm_object_style_set(scroller, "transparent");
    evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
    evas_object_size_hint_align_set(scroller, FILL, FILL);
    evas_object_show(scroller);

    scroller_box = elm_box_add(scroller);
    evas_object_size_hint_weight_set(scroller_box, EXPAND, 0.0);
    evas_object_size_hint_align_set(scroller_box, FILL, 0.0);
    elm_object_content_set(scroller, scroller_box);
    evas_object_show(scroller_box);
    elm_box_pack_end(root, scroller);

    top_spacer = elm_box_add(scroller_box);
    evas_object_size_hint_weight_set(top_spacer, EXPAND, 0.0);
    evas_object_size_hint_align_set(top_spacer, FILL, 0.0);
    evas_object_size_hint_min_set(top_spacer, about_w, about_h);
    elm_box_pack_end(scroller_box, top_spacer);
    evas_object_show(top_spacer);

    lb = elm_entry_add(win);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(lb, FILL, FILL);
    elm_entry_single_line_set(lb, 0);
    elm_entry_line_wrap_set(lb, ELM_WRAP_MIXED);
    elm_object_focus_allow_set(lb, 0);
    elm_object_style_set(lb, "transparent");
    elm_entry_scrollable_set(lb, 0);
    elm_entry_editable_set(lb, 0);
    elm_entry_select_allow_set(lb, 0);
    evas_object_pass_events_set(lb, EINA_TRUE);
    evas_object_size_hint_weight_set(lb, EXPAND, 0.0);
    srand(time(NULL));
    elm_object_text_set(lb, eina_slstr_printf(about_text_fmt, _about_random_msg()));
    evas_object_show(lb);
    elm_box_pack_end(scroller_box, lb);

    bottom_spacer = elm_box_add(scroller_box);
    evas_object_size_hint_weight_set(bottom_spacer, EXPAND, 0.0);
    evas_object_size_hint_align_set(bottom_spacer, FILL, 0.0);
    evas_object_size_hint_min_set(bottom_spacer, about_w, about_h);
    elm_box_pack_end(scroller_box, bottom_spacer);
    evas_object_show(bottom_spacer);

    about = malloc(sizeof(Animate_Data));
    if (!about) return;

    about->scroller = scroller;
    about->top_spacer = top_spacer;
    about->bottom_spacer = bottom_spacer;
    about->lb = lb;
    about->pos = 0.0;
    about->ui = ui;
    about->animator = ecore_animator_add(about_anim, about);
    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, about);
    evas_object_event_callback_add(scroller, EVAS_CALLBACK_RESIZE, _about_resize_cb, about);

    hbx = elm_box_add(win);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_align_set(hbx, FILL, FILL);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);
    evas_object_size_hint_min_set(hbx, about_w, btn_h);
    evas_object_show(hbx);

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

    spacer = elm_box_add(win);
    evas_object_size_hint_align_set(spacer, FILL, FILL);
    evas_object_size_hint_weight_set(spacer, EXPAND, EXPAND);
    evas_object_show(spacer);

    pad_left = elm_box_add(win);
    evas_object_size_hint_align_set(pad_left, FILL, FILL);
    evas_object_size_hint_weight_set(pad_left, 0, EXPAND);
    evas_object_size_hint_min_set(pad_left, about_pad, 1);
    evas_object_show(pad_left);

    pad_right = elm_box_add(win);
    evas_object_size_hint_align_set(pad_right, FILL, FILL);
    evas_object_size_hint_weight_set(pad_right, 0, EXPAND);
    evas_object_size_hint_min_set(pad_right, about_pad, 1);
    evas_object_show(pad_right);

    elm_box_pack_end(hbx, pad_left);
    elm_box_pack_end(hbx, version);
    elm_box_pack_end(hbx, spacer);
    elm_box_pack_end(hbx, btn);
    elm_box_pack_end(hbx, pad_right);

    pad_top = elm_box_add(win);
    evas_object_size_hint_align_set(pad_top, FILL, FILL);
    evas_object_size_hint_weight_set(pad_top, EXPAND, 0);
    evas_object_size_hint_min_set(pad_top, about_w, about_pad);
    evas_object_show(pad_top);

    elm_box_pack_end(header, pad_top);
    elm_box_pack_end(header, hbx);

    evas_object_smart_callback_add(btn, "clicked", _btn_close_cb, about);

    evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, about);

    evas_object_resize(win, about_w, about_h);
    evas_object_show(win);
}
