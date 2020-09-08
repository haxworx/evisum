#include "ui_cpu.h"

#define COLOR_FG 0xff2f99ff
#define COLOR_BG 0xff202020

typedef enum {
    COLOR_0   =  0xff57bb8a,
    COLOR_5   =  0xff63b682,
    COLOR_10  =  0xff73b87e,
    COLOR_15  =  0xff84bb7b,
    COLOR_20  =  0xff94bd77,
    COLOR_25  =  0xffa4c073,
    COLOR_30  =  0xffb0be6e,
    COLOR_35  =  0xffc4c56d,
    COLOR_40  =  0xffd4c86a,
    COLOR_45  =  0xffe2c965,
    COLOR_50  =  0xfff5ce62,
    COLOR_55  =  0xfff3c563,
    COLOR_60  =  0xffe9b861,
    COLOR_65  =  0xffe6ad61,
    COLOR_70  =  0xffecac67,
    COLOR_75  =  0xffe9a268,
    COLOR_80  =  0xffe79a69,
    COLOR_85  =  0xffe5926b,
    COLOR_90  =  0xffe2886c,
    COLOR_95  =  0xffe0816d,
    COLOR_100 =  0xffdd776e,
} Colors;

static int
_core_color(int percent)
{
   if (percent >= 100) return COLOR_100;
   if (percent >= 95) return COLOR_95;
   if (percent >= 90) return COLOR_90;
   if (percent >= 85) return COLOR_85;
   if (percent >= 80) return COLOR_80;
   if (percent >= 75) return COLOR_75;
   if (percent >= 70) return COLOR_70;
   if (percent >= 65) return COLOR_65;
   if (percent >= 60) return COLOR_60;
   if (percent >= 55) return COLOR_55;
   if (percent >= 50) return COLOR_50;
   if (percent >= 45) return COLOR_45;
   if (percent >= 40) return COLOR_40;
   if (percent >= 35) return COLOR_35;
   if (percent >= 30) return COLOR_30;
   if (percent >= 25) return COLOR_25;
   if (percent >= 20) return COLOR_20;
   if (percent >= 15) return COLOR_15;
   if (percent >= 10) return COLOR_10;
   if (percent >= 5) return COLOR_5;

   return COLOR_0;
}

typedef struct {
   Ecore_Animator *animator;
   Ui             *ui;

   Evas_Object    *bg;
   Evas_Object    *line;
   Evas_Object    *obj;

   Evas_Object    *guide;
   Evas_Object    *colors;

   Eina_Bool       enabled;
   Eina_Bool       redraw;

   int             cpu_count;
   Eina_List      *cores;

   // Have cpu scaling
   Eina_Bool       cpu_freq;
   int             freq_min;
   int             freq_max;

   int             pos;
   double          step;
} Animate;

typedef struct
{
   int id;
   int freq;
   int percent;
} Core;

static int
_core_alpha(int percent, int fr, int fr_max, int fr_min)
{
   double fade;
   int r, g, b, a;
   int rng, n;
   int color;

   if (fr)
     {
        rng = fr_max - fr_min;
        n = fr - fr_min;
        fade = (100.0 - ((n * 100) / rng)) / 100.0;
     }

   color = _core_color(percent);
   r = (color >> 16) & 0xff;
   g = (color >> 8) & 0xff;
   b = (color & 0xff);
   a = 0xff;

   if (fr)
     {
        r += (percent / 100.0) * 0xff;
        if (r >= 255) r = 255;
        b += fade * 0xff;
        if (b >= 255) b = 255;
     }

   color = (a << 24) + (r << 16) + (g << 8) + b;

   return color;
}

static void
_core_times_cb(void *data, Ecore_Thread *thread)
{
   Animate *ad;
   int ncpu;

   ad = data;

   if (!system_cpu_frequency_min_max_get(&ad->freq_min, &ad->freq_max))
     ad->cpu_freq = EINA_TRUE;

   while (!ecore_thread_check(thread))
     {
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 100000);
        for (int n = 0; n < ncpu; n++)
          {
             // Copy our core state data to the animator.
             Core *core = eina_list_nth(ad->cores, n);
             if (core)
               {
                  core->percent = cores[n]->percent;
                  if (ad->cpu_freq)
                    core->freq = system_cpu_n_frequency_get(n);
               }
             free(cores[n]);
          }
        free(cores);
     }
}

static void
_win_del_cb(void *data, Evas_Object *obj,
                    void *event_info EINA_UNUSED)
{
   Animate *ad = data;
   Ui *ui = ad->ui;
   Core *core;

   ecore_thread_cancel(ui->thread_cpu);
   ecore_thread_wait(ui->thread_cpu, 0.1);

   EINA_LIST_FREE(ad->cores, core)
     {
        free(core);
     }

   free(ad);

   evas_object_del(obj);
   ui->win_cpu = NULL;
}

static void
_reset(Animate *ad)
{
   ad->pos = ad->step = 0;
}

static Eina_Bool
_bg_fill(Animate *ad)
{
   uint32_t *pixels;
   Evas_Coord x, y, w, h;

   if (ecore_thread_check(ad->ui->thread_cpu)) return EINA_FALSE;

   evas_object_geometry_get(ad->bg, NULL, NULL, &w, &h);
   pixels = evas_object_image_data_get(ad->obj, EINA_TRUE);
   if (!pixels) return EINA_FALSE;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             *(pixels++) = COLOR_BG;
          }
     }
   ad->redraw = EINA_FALSE;
   return EINA_TRUE;
}

static Eina_Bool
_animate(void *data)
{
   uint32_t *pixels;
   Evas_Object *line, *obj, *bg;
   Evas_Coord x, y, w, h;
   Animate *ad = data;

   if (ecore_thread_check(ad->ui->thread_cpu)) return EINA_FALSE;

   bg = ad->bg; line = ad->line; obj = ad->obj;

   evas_object_geometry_get(bg, &x, &y, &w, &h);
   evas_object_image_size_set(obj, w, h);

   evas_object_move(line, x + w - ad->pos, y);
   evas_object_resize(line, 1, h);
   if (ad->enabled)
     evas_object_show(line);
   else
     evas_object_hide(line);

   if (ad->redraw)
     {
        _bg_fill(ad);
        return EINA_TRUE;
     }

   pixels = evas_object_image_data_get(obj, EINA_TRUE);
   if (!pixels) return EINA_TRUE;

   Core *core = NULL;

   int block = h / ad->cpu_count;

   for (y = 0; ad->enabled && y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             int n = y / block;
             core = eina_list_nth(ad->cores, n);
             if (!core) core = eina_list_nth(ad->cores, n - 1);
             if (core && x == (w - ad->pos))
               {
                 if (ad->cpu_freq)
                   *(pixels) = _core_alpha(core->percent, core->freq, ad->freq_min, ad->freq_max);
                 else
                   *(pixels) = _core_color(core->percent);
               }
             pixels++;
          }
     }

   ad->step += (double) (w * ecore_animator_frametime_get()) / 60.0;
   ad->pos = ad->step;

   if (ad->pos >= w)
     _reset(ad);

   return EINA_TRUE;
}

static void
_anim_resize_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Evas_Coord w, h, x, y;
   uint32_t *pixels;
   Animate *ad = data;

   ad->redraw = EINA_TRUE;
   _reset(ad);
   evas_object_hide(ad->line);

   h = 15;
   evas_object_size_hint_min_set(ad->colors, -1, h);
   evas_object_geometry_get(ad->bg, NULL, NULL, &w, NULL);
   evas_object_image_size_set(ad->colors, w, h);
   evas_object_image_data_update_add(ad->colors, 0, 0, w, h);

   pixels = evas_object_image_data_get(ad->colors, EINA_TRUE);
   if (!pixels) return;

   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             int n = (100.0 / w) * x;
             *(pixels++) = _core_alpha(n, 0, 0, 0);
          }
     }
}

static void
_anim_move_cb(void *data, Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   evas_object_hide(ad->line);
}

static void
_graph(Ui *ui, Evas_Object *parent)
{
   Evas_Object *frame, *tbl, *box, *bg, *obj, *line;
   Evas_Object *fr, *hbx, *bx, *guide, *colors;

   box = parent;

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_show(frame);
   elm_object_text_set(frame, _("CPU % / Freq"));

   tbl = elm_table_add(box);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_show(tbl);

   bg = evas_object_rectangle_add(evas_object_evas_get(box));
   evas_object_size_hint_align_set(bg, FILL, FILL);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   evas_object_show(bg);

   line = evas_object_rectangle_add(evas_object_evas_get(bg));
   evas_object_size_hint_align_set(line, FILL, FILL);
   evas_object_size_hint_weight_set(line, EXPAND, EXPAND);
   evas_object_color_set(line, 255, 47, 153, 255);
   evas_object_size_hint_max_set(line, 1, -1);
   evas_object_show(line);

   obj = evas_object_image_add(evas_object_evas_get(bg));
   evas_object_size_hint_align_set(obj, FILL, FILL);
   evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
   evas_object_image_filled_set(obj, EINA_TRUE);
   evas_object_image_colorspace_set(obj, EVAS_COLORSPACE_ARGB8888);
   evas_object_show(obj);

   elm_table_pack(tbl, bg, 0, 0, 1, 1);
   elm_table_pack(tbl, obj, 0, 0, 1, 1);
   elm_table_pack(tbl, line, 0, 0, 1, 1);

   bx = elm_box_add(box);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_show(bx);
   elm_box_pack_end(bx, tbl);

   hbx = elm_box_add(box);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   elm_box_horizontal_set(hbx, 1);
   evas_object_show(hbx);

   fr = elm_frame_add(box);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_show(fr);
   elm_object_text_set(fr, _("Increasing %"));
   elm_object_content_set(fr, hbx);

   colors = evas_object_image_add(evas_object_evas_get(fr));
   evas_object_size_hint_align_set(colors, FILL, FILL);
   evas_object_size_hint_weight_set(colors, EXPAND, EXPAND);
   evas_object_image_filled_set(colors, EINA_TRUE);
   evas_object_image_colorspace_set(colors, EVAS_COLORSPACE_ARGB8888);
   evas_object_show(colors);
   elm_box_pack_end(hbx, colors);

   elm_object_content_set(frame, bx);

   elm_box_pack_end(box, frame);
   elm_box_pack_end(box, fr);

   Animate *ad = calloc(1, sizeof(Animate));
   if (!ad) return;

   ad->bg = bg;
   ad->obj = obj;
   ad->line = line;
   ad->enabled = EINA_TRUE;
   ad->ui = ui;
   ad->redraw = EINA_TRUE;
   ad->cpu_count = system_cpu_online_count_get();

   ad->colors = colors;

   for (int i = 0; i < ad->cpu_count; i++)
     {
        Core *core = calloc(1, sizeof(Core));
        if (core)
          {
             core->id = i;
             ad->cores = eina_list_append(ad->cores, core);
          }
     }

   evas_object_size_hint_min_set(bg, 1, 2 * ad->cpu_count);

   evas_object_smart_callback_add(tbl, "resize", _anim_resize_cb, ad);
   evas_object_smart_callback_add(tbl, "move", _anim_move_cb, ad);
   evas_object_smart_callback_add(ui->win_cpu, "delete,request", _win_del_cb, ad);

   ad->animator = ecore_animator_add(_animate, ad);

   ui->thread_cpu = ecore_thread_run(_core_times_cb, NULL, NULL, ad);
}

void
ui_win_cpu_add(Ui *ui)
{
   Evas_Object *win, *box, *scroller;

   if (ui->win_cpu) return;

   ui->win_cpu = win = elm_win_util_dialog_add(ui->win, "evisum",
                   _("CPU Usage"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, evisum_ui_effects_enabled_get());

   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   box = elm_box_add(win);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   _graph(ui, box);

   elm_object_content_set(scroller, box);
   elm_object_content_set(win, scroller);

   evisum_child_window_show(ui->win, win);
}

