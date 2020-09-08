#include "ui_cpu.h"

#define COLOR_FG 0xff2f99ff
#define COLOR_BG 0xff202020


#define COLOR_HIGH     0xff26f226
#define COLOR_MID_HIGH 0xfff2f226
#define COLOR_MID      0xffe21212
#define COLOR_MID_LOW  0xff471292
#define COLOR_LOW      0xff2f99ff

typedef struct {
   Ui          *ui;
   int          cpu_id;

   Evas_Object *bg;
   Evas_Object *line;
   Evas_Object *obj;
   Evas_Object *btn;

   Eina_Bool    enabled;
   Eina_Bool    redraw;

   int          freq;
   int          freq_min;
   int          freq_max;

   int          pos;
   double       value;
   double       step;
} Animate_Data;

typedef struct {
   Ecore_Animator *animator;
   Animate_Data   *ad;
   double         *value;
   Evas_Object    *pb;
   Evas_Object    *lbl;
} Progress;

static void
loop_reset(Animate_Data *ad)
{
   ad->pos = ad->step = 0;
}

static Eina_Bool
_bg_fill(Animate_Data *ad)
{
   uint32_t *pixels;
   Evas_Coord x, y, w, h;

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

static int
_color_rng(int fr, int fr_min, int fr_max)
{
   int rng, n;

   rng = fr_max - fr_min;
   n = fr - fr_min;
   n = (n * 10) / rng;

   if (n > 8) return COLOR_HIGH;
   if (n > 6) return COLOR_MID_HIGH;
   if (n > 4) return COLOR_MID;
   if (n > 2) return COLOR_MID_LOW;

   return COLOR_LOW;
}

static int
_color(Animate_Data *ad)
{
   if (ad->freq != -1 && ad->freq_min && ad->freq_max)
     {
        return _color_rng(ad->freq, ad->freq_min, ad->freq_max);
     }

   return COLOR_FG;
}

static Eina_Bool
animate(void *data)
{
   uint32_t *pixels;
   Evas_Object *line, *obj, *bg;
   Evas_Coord x, y, w, h, fill_y;
   double value;
   Animate_Data *ad = data;

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

   value = ad->value > 0 ? ad->value : 1.0;

   fill_y = h - (int) ((double)(h / 100.0) * value);

   for (y = 0; ad->enabled && y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             if (x == (w - ad->pos))
               {
                   *(pixels) = COLOR_BG;
               }
             if ((x == (w - ad->pos)) && (y >= fill_y))
               {
                  if (y % 2)
                    *(pixels) = _color(ad);
               }
             pixels++;
          }
     }

   ad->step += (double) (w * ecore_animator_frametime_get()) / 60.0;
   ad->pos = ad->step;

   if (ad->pos >= w)
     loop_reset(ad);

   return EINA_TRUE;
}

static void
_anim_resize_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Animate_Data *ad = data;

   ad->redraw = EINA_TRUE;
   loop_reset(ad);

   evas_object_hide(ad->line);
}

static void
_anim_move_cb(void *data, Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   Animate_Data *ad = data;

   evas_object_hide(ad->line);
}

static void
_btn_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Evas_Object *rect;
   Animate_Data *ad = data;

   ad->enabled = !ad->enabled;

   rect = elm_object_part_content_get(ad->btn, "elm.swallow.content");
   if (!ad->enabled)
     evas_object_color_set(rect, 0, 0, 0, 0);
   else
     evas_object_color_set(rect, 47, 153, 255, 255);
}

static void
_core_times_cb(void *data, Ecore_Thread *thread)
{
   Progress *progress;
   cpu_core_t **cores;
   Eina_List *l;
   Ui *ui;
   int ncpu, min = 0, max = 0;
   Eina_Bool cpu_freq = EINA_FALSE;

   ui = data;

   if (!system_cpu_frequency_min_max_get(&min, &max))
     cpu_freq = EINA_TRUE;

   for (int i = 0; !ecore_thread_check(thread); i = 0)
     {
        cores = system_cpu_usage_get(&ncpu);
        EINA_LIST_FOREACH(ui->cpu_list, l, progress)
          {
             *progress->value = cores[i]->percent;
             ecore_thread_main_loop_begin();

             if (cpu_freq)
               {
                  int freq = system_cpu_n_frequency_get(progress->ad->cpu_id);

                  if (freq > 1000000)
                    elm_object_text_set(progress->lbl, eina_slstr_printf("%1.1f GHz", (double) freq / 1000000.0));
                  else
                    elm_object_text_set(progress->lbl, eina_slstr_printf("%d MHz",  freq / 1000));

                  progress->ad->freq = freq;
                  progress->ad->freq_min = min;
                  progress->ad->freq_max = max;
               }
             elm_progressbar_value_set(progress->pb, cores[i]->percent / 100);
             ecore_thread_main_loop_end();
             free(cores[i++]);
          }
        free(cores);
     }
}

static void
_win_del_cb(void *data, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Progress *progress;
   Ui *ui = data;

   ecore_thread_cancel(ui->thread_cpu);

   EINA_LIST_FREE(ui->cpu_list, progress)
     {
        ecore_animator_del(progress->animator);
        free(progress->ad);
        free(progress);
     }

   ecore_thread_wait(ui->thread_cpu, 0.1);
   evas_object_del(obj);
   ui->win_cpu = NULL;
}

void
_simple(Ui *ui, Evas_Object *parent)
{
   Evas_Object *frame, *bg, *line, *pb, *tbl, *cbox, *sbox, *lbl, *lbox, *btn, *rect;
   Evas_Object *obj;
   Evas_Object *box = parent;

   int cpu_count = system_cpu_online_count_get();
   for (int i = 0; i < cpu_count; i++)
     {
        lbox = elm_box_add(box);
        evas_object_size_hint_align_set(lbox, FILL, FILL);
        evas_object_size_hint_weight_set(lbox, EXPAND, EXPAND);
        evas_object_show(lbox);
        elm_box_horizontal_set(lbox, EINA_TRUE);

        cbox = elm_box_add(box);
        evas_object_size_hint_align_set(cbox, FILL, FILL);
        evas_object_size_hint_weight_set(cbox, 0.1, EXPAND);
        evas_object_show(cbox);

        sbox = elm_box_add(box);
        evas_object_size_hint_align_set(sbox, FILL, FILL);
        evas_object_size_hint_weight_set(sbox, EXPAND, EXPAND);
        elm_box_horizontal_set(sbox, EINA_TRUE);
        evas_object_show(sbox);

        btn = elm_button_add(box);
        evas_object_show(btn);

        rect = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_min_set(rect, 16 * elm_config_scale_get(),
                        16 * elm_config_scale_get());
        evas_object_color_set(rect, 47, 153, 255, 255);
        evas_object_show(rect);
        elm_object_part_content_set(btn, "elm.swallow.content", rect);

        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, FILL, FILL);
        evas_object_size_hint_weight_set(frame, 0, EXPAND);
        evas_object_show(frame);
        elm_object_style_set(frame, "pad_small");

        pb = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        evas_object_size_hint_weight_set(pb, 0.2, EXPAND);
        elm_progressbar_span_size_set(pb, 1.0);
        elm_progressbar_unit_format_set(pb, "%1.2f%%");
        evas_object_show(pb);

        lbl = elm_label_add(box);
        evas_object_size_hint_align_set(lbl, 0.5, 0.0);
        evas_object_size_hint_weight_set(lbl, EXPAND, EXPAND);
        evas_object_show(lbl);

        elm_box_pack_end(sbox, btn);
        elm_box_pack_end(sbox, pb);
        elm_box_pack_end(cbox, sbox);
        elm_box_pack_end(cbox, lbl);
        elm_box_pack_end(lbox, cbox);

        tbl = elm_table_add(box);
        evas_object_size_hint_align_set(tbl, FILL, FILL);
        evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
        evas_object_show(tbl);

        bg = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_align_set(bg, FILL, FILL);
        evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
        evas_object_show(bg);
        evas_object_size_hint_min_set(bg, 1, 60);

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

        elm_table_pack(tbl, bg, 0, 1, 1, 1);
        elm_table_pack(tbl, obj, 0, 1, 1, 1);
        elm_table_pack(tbl, line, 0, 1, 1, 1);

        elm_box_pack_end(lbox, tbl);
        elm_object_content_set(frame, lbox);
        elm_box_pack_end(box, frame);

        Animate_Data *ad = calloc(1, sizeof(Animate_Data));
        if (!ad) return;

        ad->bg = bg;
        ad->obj = obj;
        ad->line = line;
        ad->enabled = EINA_TRUE;
        ad->btn = btn;
        ad->cpu_id = i;
        ad->ui = ui;
        ad->redraw = EINA_TRUE;

        evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, ad);
        evas_object_smart_callback_add(tbl, "resize", _anim_resize_cb, ad);
        evas_object_smart_callback_add(tbl, "move", _anim_move_cb, ad);

        Progress *progress = calloc(1, sizeof(Progress));
        if (progress)
          {
             progress->pb = pb;
             progress->lbl = lbl;
             progress->value = &ad->value;
             progress->animator = ecore_animator_add(animate, ad);
             progress->ad = ad;

             ui->cpu_list = eina_list_append(ui->cpu_list, progress);
          }
     }

   evas_object_smart_callback_add(ui->win, "delete,request", _win_del_cb, ui);

   ui->thread_cpu = ecore_thread_run(_core_times_cb, NULL, NULL, ui);
}

typedef struct {
   Ecore_Animator *animator;
   Ui             *ui;

   Evas_Object    *bg;
   Evas_Object    *line;
   Evas_Object    *obj;

   Eina_Bool       enabled;
   Eina_Bool       redraw;

   int             cpu_count;
   Eina_List      *cores;

   int             pos;
   double          step;
} Animate;

typedef struct
{
   int id;
   int freq;
   int percent;
} Core;

static void
_core_times_complex_cb(void *data, Ecore_Thread *thread)
{
   cpu_core_t **cores;
   Animate *ad;
   int ncpu, min = 0, max = 0;
   Eina_Bool cpu_freq = EINA_FALSE;

   ad = data;

   if (!system_cpu_frequency_min_max_get(&min, &max))
     cpu_freq = EINA_TRUE;

   while (!ecore_thread_check(thread))
     {
        cores = system_cpu_usage_get(&ncpu);
        for (int n = 0; n < ncpu; n++)
          {
             Core *core = eina_list_nth(ad->cores, n);
             core->percent = cores[n]->percent;
             if (cpu_freq)
               {
                  core->freq = system_cpu_n_frequency_get(n);
                  printf("%d => %d\n", (int) core->percent, core->freq);
               }
             free(cores[n]);
          }
        printf("\n\n");
        free(cores);
     }
}

static void
_win_complex_del_cb(void *data, Evas_Object *obj,
                    void *event_info EINA_UNUSED)
{
   Animate *ad = data;
   Ui *ui = ad->ui;
   Core *core;

   ecore_thread_cancel(ui->thread_cpu);

   EINA_LIST_FREE(ad->cores, core)
     {
        free(core);
     }

   ecore_animator_del(ad->animator);
   free(ad);

   ecore_thread_wait(ui->thread_cpu, 0.1);
   evas_object_del(obj);
   ui->win_cpu = NULL;
}

static void
_complex_reset(Animate *ad)
{
   ad->pos = ad->step = 0;
}

static Eina_Bool
_bg_complex_fill(Animate *ad)
{
   uint32_t *pixels;
   Evas_Coord x, y, w, h;

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
_animate_complex(void *data)
{
   uint32_t *pixels;
   Evas_Object *line, *obj, *bg;
   Evas_Coord x, y, w, h, fill_y;
   double value;
   Animate *ad = data;

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
        _bg_complex_fill(ad);
        return EINA_TRUE;
     }

   pixels = evas_object_image_data_get(obj, EINA_TRUE);
   if (!pixels) return EINA_TRUE;

   value = 20 ; //ad->value > 0 ? ad->value : 1.0;

   fill_y = h - (int) ((double)(h / 100.0) * value);

   for (y = 0; ad->enabled && y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             if (x == (w - ad->pos))
               {
                   *(pixels) = COLOR_BG;
               }
             if ((x == (w - ad->pos)) && (y >= fill_y))
               {
                  if (y % 2)
                    *(pixels) = COLOR_FG;
               }
             pixels++;
          }
     }

   ad->step += (double) (w * ecore_animator_frametime_get()) / 60.0;
   ad->pos = ad->step;

   if (ad->pos >= w)
     _complex_reset(ad);

   return EINA_TRUE;
}

static void
_anim_complex_resize_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   ad->redraw = EINA_TRUE;
   _complex_reset(ad);

   evas_object_hide(ad->line);
}

static void
_anim_complex_move_cb(void *data, Evas_Object *obj EINA_UNUSED,
              void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   evas_object_hide(ad->line);
}

static void
_complex(Ui *ui, Evas_Object *parent)
{
   Evas_Object *frame, *tbl, *box, *bg, *obj, *line;

   box = parent;

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_small");

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

   elm_table_pack(tbl, bg, 0, 1, 1, 1);
   elm_table_pack(tbl, obj, 0, 1, 1, 1);
   elm_table_pack(tbl, line, 0, 1, 1, 1);
   elm_object_content_set(frame, tbl);

   elm_box_pack_end(box, frame);

   Animate *ad = calloc(1, sizeof(Animate));
   if (!ad) return;

   ad->bg = bg;
   ad->obj = obj;
   ad->line = line;
   ad->enabled = EINA_TRUE;
   ad->ui = ui;
   ad->redraw = EINA_TRUE;
   ad->cpu_count = system_cpu_online_count_get();

   for (int i = 0; i < ad->cpu_count; i++)
     {
        Core *core = calloc(1, sizeof(Core));
        if (core)
          {
             core->id = i;
             ad->cores = eina_list_append(ad->cores, core);
          }
     }

   evas_object_smart_callback_add(tbl, "resize", _anim_complex_resize_cb, ad);
   evas_object_smart_callback_add(tbl, "move", _anim_complex_move_cb, ad);
   evas_object_smart_callback_add(ui->win, "delete,request", _win_complex_del_cb, ad);

   ad->animator = ecore_animator_add(_animate_complex, ad);

   ui->thread_cpu = ecore_thread_run(_core_times_complex_cb, NULL, NULL, ad);
}

void
ui_win_cpu_add(Ui *ui)
{
   Evas_Object *win, *box, *scroller;
   int cpu_count;

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

   cpu_count = system_cpu_online_count_get();
   if (cpu_count)
     _simple(ui, box);
    //_complex(ui, box);

   elm_object_content_set(scroller, box);
   elm_object_content_set(win, scroller);

   evisum_child_window_show(ui->win, win);
}

