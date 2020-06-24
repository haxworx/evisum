#include "ui_cpu.h"

#define COLOR_FG 0xff2f99ff
#define COLOR_BG 0xff323232

typedef struct {
   Evas_Object *bg;
   Evas_Object *line;
   Evas_Object *obj;
   Evas_Object *btn;
   Ui          *ui;
   Eina_Bool    enabled;
   int          pos;
   int          cpu_id;
   double       value;
   double       step;
} Animation;

typedef struct {
   double      *value;
   Evas_Object *pb;
} Progress;

static void
graph_clear(Evas_Object *obj, Evas_Coord w, Evas_Coord h)
{
   uint32_t *pixels;
   Evas_Coord x, y;

   pixels = evas_object_image_data_get(obj, EINA_TRUE);
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             *(pixels++) = COLOR_BG;
          }
    }
   evas_object_image_data_update_add(obj, 0, 0, w, h);
}

static Eina_Bool
animator(void *data EINA_UNUSED)
{
   uint32_t *pixels;
   Evas_Object *line, *obj, *bg;
   Evas_Coord x, y, w, h;
   Animation *anim = data;

   if (!anim->ui->cpu_visible) return EINA_TRUE;

   bg = anim->bg; line = anim->line; obj = anim->obj;

   evas_object_geometry_get(bg,  &x, &y, &w, &h);
   evas_object_move(line, x + w - anim->pos, y);
   evas_object_resize(line, 1, h);

   if (anim->enabled)
     evas_object_show(line);
   else
     evas_object_hide(line);

   evas_object_image_size_set(obj, w, h);

   pixels = evas_object_image_data_get(obj, EINA_TRUE);

   int fill_y = h - (int) ((double)(h / 100.0) * anim->value);

   for (y = 0; y < h; y++)
     {
       if (!anim->enabled)
         break;
       for (x = 0; x < w; x++)
        {
           if  ((x >= (w - anim->pos)) && y > fill_y)
             {
                 if (y % 2)
                   *(pixels) = COLOR_FG;
                 else
                   *(pixels) = COLOR_BG;
             }
            else
              *(pixels) = COLOR_BG;

            pixels++;
         }
     }
   // XXX FPS
   anim->step += (double) w / (60 * 60);
   anim->pos = anim->step;

   if (anim->pos >= w)
     {
        graph_clear(obj, w, h);
        anim->pos = anim->step = 0;
     }

   return EINA_TRUE;
}

static void
_btn_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Evas_Object *rect;
   Animation *anim = data;

   anim->enabled = !anim->enabled;

   rect = elm_object_part_content_get(anim->btn, "elm.swallow.content");
   if (!anim->enabled)
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
   int ncpu, i;

   ui = data;
   while (1)
     {
        if (ecore_thread_check(thread))
          break;
        if (!ui->cpu_visible)
          {
             usleep(1000000);
             continue;
          }
        i = 0;
        cores = system_cpu_usage_get(&ncpu);
        EINA_LIST_FOREACH(ui->cpu_list, l, progress)
          {
             if (!cores || !cores[i])
               {
                  ++i;
                  continue;
               }
             *progress->value = cores[i]->percent;
             ecore_thread_main_loop_begin();
             elm_progressbar_value_set(progress->pb, cores[i++]->percent / 100);
             ecore_thread_main_loop_end();
          }
        for (i = 0; i < ncpu; i++)
          free(cores[i]);
        free(cores);
     }
}

void
ui_tab_cpu_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *scroller, *frame;
   Evas_Object *pb, *tbl, *lbox, *btn, *rect;
   Evas_Object *bg, *line, *obj;
   unsigned int cpu_count;

   parent = ui->content;

   ui->cpu_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_table_pack(ui->content, ui->cpu_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->cpu_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);
   elm_box_pack_end(box, scroller);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, 0.1, EXPAND);
   evas_object_show(box);

   cpu_count = system_cpu_online_count_get();
   for (int i = 0; i < cpu_count; i++)
     {
        lbox = elm_box_add(ui->content);
        evas_object_size_hint_align_set(lbox, FILL, FILL);
        evas_object_size_hint_weight_set(lbox, 0.1, EXPAND);
        evas_object_show(lbox);
        elm_box_horizontal_set(lbox, EINA_TRUE);

        btn = elm_button_add(box);
        evas_object_show(btn);

        rect = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_min_set(rect, 16, 16);
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
        elm_progressbar_value_set(pb, 0.0);

        elm_box_pack_end(lbox, pb);
        elm_box_pack_end(lbox, btn);

        Progress *progress = calloc(1, sizeof(Progress));
        progress->value = calloc(1, sizeof(float));
        progress->pb = pb;

        tbl = elm_table_add(box);
        evas_object_size_hint_align_set(tbl, FILL, FILL);
        evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
        evas_object_show(tbl);

        bg = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_align_set(bg, FILL, FILL);
        evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
        evas_object_size_hint_min_set(bg, 1, 80);

        line = evas_object_rectangle_add(evas_object_evas_get(bg));
        evas_object_size_hint_align_set(line, FILL, FILL);
        evas_object_size_hint_weight_set(line, EXPAND, EXPAND);
        evas_object_color_set(line, 255, 47, 153, 255);
        evas_object_show(line);

        obj = evas_object_image_add(evas_object_evas_get(bg));
        evas_object_size_hint_align_set(obj, FILL, FILL);
        evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
        evas_object_image_filled_set(obj, EINA_TRUE);
        evas_object_show(obj);

        Animation *anim = calloc(1, sizeof(Animation));
        anim->bg = bg;
        anim->obj = obj;
        anim->line = line;
        anim->enabled = EINA_TRUE;
        anim->btn = btn;
        anim->cpu_id = i;
        anim->ui = ui;

        progress->value = &anim->value;
        evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, anim);
        ecore_animator_add(animator, anim);

        elm_table_pack(tbl, bg, 0, 1, 1, 1);
        elm_table_pack(tbl, obj, 0, 1, 1, 1);
        elm_table_pack(tbl, line, 0, 1, 1, 1);
        elm_box_pack_end(lbox, tbl);
        elm_object_content_set(frame, lbox);
        elm_box_pack_end(box, frame);

        ui->cpu_list = eina_list_append(ui->cpu_list, progress);
     }

   ui->thread_cpu = ecore_thread_run(_core_times_cb, NULL, NULL, ui);

   elm_box_pack_end(hbox, box);
}

