#include "ui_cpu.h"

#define COLOR_FG 0xff2f99ff
#define COLOR_BG 0xff202020

typedef struct {
   Ui          *ui;
   int          cpu_id;

   Evas_Object *bg;
   Evas_Object *line;
   Evas_Object *obj;
   Evas_Object *btn;

   Eina_Bool    enabled;
   Eina_Bool    redraw;

   int          pos;
   double       value;
   double       step;
} Animate_Data;

typedef struct {
   Ecore_Animator *animator;
   Animate_Data   *anim_data;
   double         *value;
   Evas_Object    *pb;
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
                    *(pixels) = COLOR_FG;
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
   int ncpu;

   ui = data;

   for (int i = 0; !ecore_thread_check(thread); i = 0)
     {
        if (!ui->cpu_visible)
          {
             usleep(1000000);
             continue;
          }

        cores = system_cpu_usage_get(&ncpu);

        EINA_LIST_FOREACH(ui->cpu_list, l, progress)
          {
             *progress->value = cores[i]->percent;
             ecore_thread_main_loop_begin();
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

   evas_object_hide(obj);

   ecore_thread_cancel(ui->thread_cpu);
   ecore_thread_wait(ui->thread_cpu, 1.0);

   EINA_LIST_FREE(ui->cpu_list, progress)
     {
        ecore_animator_del(progress->animator);
        free(progress->anim_data);
        free(progress);
     }

   evas_object_del(obj);
   ui->cpu_visible = EINA_FALSE;
}

void
ui_win_cpu_add(Ui *ui)
{
   Evas_Object *win, *box, *hbox, *scroller, *frame;
   Evas_Object *pb, *tbl, *lbox, *btn, *rect;
   Evas_Object *bg, *line, *obj;
   int cpu_count;

   if (ui->cpu_visible) return;

   ui->cpu_visible = EINA_TRUE;

   win = elm_win_util_standard_add("evisum", _("CPU Usage"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   hbox = elm_box_add(win);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   box = elm_box_add(hbox);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, 0.1, EXPAND);
   evas_object_show(box);

   cpu_count = system_cpu_online_count_get();
   for (int i = 0; i < cpu_count; i++)
     {
        lbox = elm_box_add(box);
        evas_object_size_hint_align_set(lbox, FILL, FILL);
        evas_object_size_hint_weight_set(lbox, 0.1, EXPAND);
        evas_object_show(lbox);
        elm_box_horizontal_set(lbox, EINA_TRUE);

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
        evas_object_size_hint_weight_set(pb, 0.1, EXPAND);
        elm_progressbar_span_size_set(pb, 1.0);
        elm_progressbar_unit_format_set(pb, "%1.2f%%");
        evas_object_show(pb);

        elm_box_pack_end(lbox, btn);
        elm_box_pack_end(lbox, pb);

        tbl = elm_table_add(box);
        evas_object_size_hint_align_set(tbl, FILL, FILL);
        evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
        evas_object_show(tbl);

        bg = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_align_set(bg, FILL, FILL);
        evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
        evas_object_show(bg);
        evas_object_size_hint_min_set(bg, 1, 80);

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
             progress->value = &ad->value;
             progress->animator = ecore_animator_add(animate, ad);
             progress->anim_data = ad;

             ui->cpu_list = eina_list_append(ui->cpu_list, progress);
          }
     }

   ui->thread_cpu = ecore_thread_run(_core_times_cb, NULL, NULL, ui);

   elm_box_pack_end(hbox, box);
   elm_object_content_set(scroller, hbox);

   elm_object_content_set(win, scroller);
   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evisum_child_window_show(ui->win, win);
}

