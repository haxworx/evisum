#include "ui_cpu.h"

typedef struct {
   short id;
   short percent;
   unsigned int freq;
   unsigned int temp;
} Core;

typedef struct {
   Ui             *ui;

   Ecore_Thread   *thread;

   Evas_Object    *win;
   Evas_Object    *menu;
   Evas_Object    *btn_menu;
   Evas_Object    *bg;
   Evas_Object    *obj;

   Evas_Object    *colors;

   int             cpu_count;

   int            *cpu_order;

   Eina_Bool       show_cpufreq;
   // Have cpu scaling
   Eina_Bool       cpu_freq;
   int             freq_min;
   int             freq_max;

   Eina_Bool       show_cputemp;
   // Have temp readings.
   Eina_Bool       cpu_temp;
   int             temp_min;
   int             temp_max;

   Eina_Bool       confused;
   Eina_List      *explainers;
} Animate;

typedef struct _Color_Point {
   unsigned int val;
   unsigned int color;
} Color_Point;

// config for colors/sizing
#define COLOR_CPU_NUM 5
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 }, // 0
   {  25, 0xff2030a0 }, // 1
   {  50, 0xffa040a0 }, // 2
   {  75, 0xffff9040 }, // 3
   { 100, 0xffffffff }, // 4
   { 256, 0xffffffff }  // overflow to avoid if's
};
#define COLOR_FREQ_NUM 4
static const Color_Point freq_colormap_in[] = {
   {   0, 0xff202020 }, // 0
   {  33, 0xff285020 }, // 1
   {  67, 0xff30a060 }, // 2
   { 100, 0xffa0ff80 }, // 3
   { 256, 0xffa0ff80 }  // overflow to avoid if's
};

#define COLOR_TEMP_NUM 5
static const Color_Point temp_colormap_in[] = {
   {  0,  0xff57bb8a }, // 0
   {  25, 0xffa4c073 },
   {  50, 0xfff5ce62 },
   {  75, 0xffe9a268 },
   { 100, 0xffdd776e },
   { 256, 0xffdd776e }
};
#define BAR_HEIGHT 2
#define COLORS_HEIGHT 60

// stored colormap tables
static unsigned int cpu_colormap[256];
static unsigned int freq_colormap[256];
static unsigned int temp_colormap[256];

// handy macros to access argb values from pixels
#define AVAL(x) (((x) >> 24) & 0xff)
#define RVAL(x) (((x) >> 16) & 0xff)
#define GVAL(x) (((x) >>  8) & 0xff)
#define BVAL(x) (((x)      ) & 0xff)
#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

static void
_color_init(const Color_Point *col_in, unsigned int n, unsigned int *col)
{
   unsigned int pos, interp, val, dist, d;
   unsigned int a, r, g, b;
   unsigned int a1, r1, g1, b1, v1;
   unsigned int a2, r2, g2, b2, v2;

   // wal colormap_in until colormap table is full
   for (pos = 0, val = 0; pos < n; pos++)
     {
        // get first color and value position
        v1 = col_in[pos].val;
        a1 = AVAL(col_in[pos].color);
        r1 = RVAL(col_in[pos].color);
        g1 = GVAL(col_in[pos].color);
        b1 = BVAL(col_in[pos].color);
        // get second color and valuje position
        v2 = col_in[pos + 1].val;
        a2 = AVAL(col_in[pos + 1].color);
        r2 = RVAL(col_in[pos + 1].color);
        g2 = GVAL(col_in[pos + 1].color);
        b2 = BVAL(col_in[pos + 1].color);
        // get distance between values (how many entires to fill)
        dist = v2 - v1;
        // walk over the span of colors from point a to point b
        for (interp = v1; interp < v2; interp++)
          {
             // distance from starting point
             d = interp - v1;
             // calculate linear interpolation between start and given d
             a = ((d * a2) + ((dist - d) * a1)) / dist;
             r = ((d * r2) + ((dist - d) * r1)) / dist;
             g = ((d * g2) + ((dist - d) * g1)) / dist;
             b = ((d * b2) + ((dist - d) * b1)) / dist;
             // write out resulting color value
             col[val] = ARGB(a, r, g, b);
             val++;
          }
     }
}

static void
_core_times_main_cb(void *data, Ecore_Thread *thread)
{
   // this runs in a dedicated thread in the bg sleeping, collecting info
   // on usage then sending as feedback to mainloop
   Animate *ad = data;
   int ncpu;

   // when we begin to run - get min and max freq. note - no locks so
   // not that great... writing to data in thread that mainloop thread
   // will read
   if (!system_cpu_frequency_min_max_get(&ad->freq_min, &ad->freq_max))
     ad->cpu_freq = EINA_TRUE;

   system_cpu_temperature_min_max_get(&ad->temp_min, &ad->temp_max);
   if ((system_cpu_n_temperature_get(0)) != -1)
     ad->cpu_temp = EINA_TRUE;

   // while this thread has not been canceled
   while (!ecore_thread_check(thread))
     {
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 100000);
        Core *cores_out = calloc(ncpu, sizeof(Core));

        // producer-consumer moduel. this thread produces data and sends as
        // feedback to mainloop to consume
        if (cores_out)
          {
             for (int n = 0; n < ncpu; n++)
               {
                  // Copy our core state data to mainloop
                  int id = ad->cpu_order[n];
                  Core *core = &(cores_out[n]);
                  core->id = id;
                  core->percent = cores[id]->percent;
                  if (ad->cpu_freq)
                    core->freq = system_cpu_n_frequency_get(id);
                  if (ad->cpu_temp)
                    core->temp = system_cpu_n_temperature_get(id);
                  free(cores[id]);
               }
             ecore_thread_feedback(thread, cores_out);
          }
        free(cores);
     }
}

static void
_update(Animate *ad, Core *cores)
{
   Evas_Object *obj = ad->obj;
   unsigned int *pixels, *pix;
   Evas_Coord x, y, w, h;
   int iw, stride;
   Eina_Bool clear = EINA_FALSE;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_image_size_get(obj, &iw, NULL);
   // if image pixel size doesn't match geom - we need to resize, so set
   // new size and mark it for clearing when we fill
   if (iw != w)
     {
        evas_object_image_size_set(obj, w, ad->cpu_count * 3);
        clear = EINA_TRUE;
     }

   // get pixel data ptr
   pixels = evas_object_image_data_get(obj, EINA_TRUE);
   if (!pixels) return;
   // get stride (# of bytes per line)
   stride = evas_object_image_stride_get(obj);

   // go throuhg al the cpu cores
   for (y = 0; y < ad->cpu_count; y++)
     {
        Core *core = &(cores[y]);
        unsigned int c1, c2;

        // our pix ptr is the pixel row and y is both y pixel coord and core
        if (clear)
          {
             // clear/fill with 0 value from colormap
             pix = &(pixels[(y * 3) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = cpu_colormap[0];
             pix = &(pixels[((y * 3) + 1) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = freq_colormap[0];
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = cpu_colormap[0];
          }
        else
          {
             // scroll pixels 1 to the left
             pix = &(pixels[(y * 3) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
             pix = &(pixels[((y * 3) + 1) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
          }
        // final pixel on end of each row... set it to a new value
        // get color from cpu colormap
        // last pixel == resulting pixel color
        c1 = cpu_colormap[core->percent & 0xff];
        pix = &(pixels[(y * 3) * (stride / 4)]);
        pix[x] = c1;
        // 2nd row of pixles for freq
        if ((ad->show_cpufreq) && (ad->cpu_freq))
          {
             int v = core->freq - ad->freq_min;
             int d = ad->freq_max - ad->freq_min;

             // if there is a difference between min and max ... a range
             if (d > 0)
               {
                  v = (100 * v) / d;
                  if (v < 0) v = 0;
                  else if (v > 100) v = 100;
                  // v now is 0->100 as a percentage of possible frequency
                  // the cpu can do
                  c2 = freq_colormap[v & 0xff];
               }
             else c2 = freq_colormap[0];
             pix = &(pixels[((y * 3) + 1) * (stride / 4)]);
             pix[x] = c2;
          }

        if (ad->show_cputemp && ad->cpu_temp)
          {
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             pix[x] = temp_colormap[core->temp & 0xff];
          }

        if (!ad->show_cpufreq)
          {
             // no freq show - then just repeat cpu usage color
             pix = &(pixels[((y * 3) + 1) * (stride / 4)]);
             pix[x] = c1;
          }
        if (!ad->show_cputemp)
          {
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             pix[x] = c1;
          }
     }
   // hand back pixel data ptr so evas knows we are done with it
   evas_object_image_data_set(obj, pixels);
   // now add update region for all pixels in the image at the end as we
   // changed everything
   evas_object_image_data_update_add(obj, 0, 0, w, ad->cpu_count * 3);
}

typedef struct
{
   Evas_Object *lb;
   Evas_Object *rec;
} Explainer;

static void
_explain(Animate *ad, Core *cores)
{
   Eina_Strbuf *buf;
   Explainer *exp;
   Evas_Object *lb, *rec;

   buf = eina_strbuf_new();

   for (int i = 0; i < ad->cpu_count; i++)
     {
        Core *core = &(cores[i]);
        exp = eina_list_nth(ad->explainers, i);
        lb = exp->lb;
        rec = exp->rec;
        if (!ad->confused)
          {
             evas_object_hide(rec);
             evas_object_hide(lb);
          }
        else
          {
             eina_strbuf_append_printf(buf, "<b><color=#fff>%i%% ", core->percent);
             if (ad->cpu_freq)
               eina_strbuf_append_printf(buf, "%1.1fGHz ", (double) core->freq / 1000000);
             if (ad->cpu_temp)
               eina_strbuf_append_printf(buf, "%i°C", core->temp);
             eina_strbuf_append(buf, "</></>");

             elm_object_text_set(lb, eina_strbuf_string_get(buf));
             eina_strbuf_reset(buf);
             evas_object_show(rec);
             evas_object_show(lb);
          }
     }
   eina_strbuf_free(buf);
}

static void
_core_times_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Animate *ad;
   Core *cores;
   static Eina_Bool was_confused = 0;

   ad = data;
   cores = msgdata;

   _update(ad, cores);

   if (ad->confused || was_confused)
     {
        _explain(ad, cores);
        was_confused = 1;
     }

   free(cores);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Animate *ad;
   Ui *ui;
   Evas_Coord x = 0, y = 0;

   ad = data;
   ui = ad->ui;

   evas_object_geometry_get(obj, &x, &y, NULL, NULL);
   ui->cpu.x = x;
   ui->cpu.y = y;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Explainer *exp;
   Animate *ad = data;
   Ui *ui = ad->ui;

   evisum_ui_config_save(ui);

   ecore_thread_cancel(ad->thread);
   ecore_thread_wait(ad->thread, 0.5);

   EINA_LIST_FREE(ad->explainers, exp)
     {
        free(exp);
     }

   ad->explainers = NULL;
   free(ad->cpu_order);
   free(ad);
   ui->cpu.win = NULL;
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   ad->show_cpufreq = elm_check_state_get(obj);
}

static void
_temp_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   ad->show_cputemp = elm_check_state_get(obj);
}

static void
_confused_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                           void *event_info EINA_UNUSED)
{
   Animate *ad = data;

   ad->confused = elm_check_state_get(obj);
}

static void
_colors_fill(Evas_Object *colors)
{
   // fill a 3 pixel high (and 100 wide) image with 3 gradients matching
   // the colormaps we calculated as a legend
   int x, stride;
   unsigned int *pixels;

   evas_object_image_size_set(colors, 101, 3);
   pixels = evas_object_image_data_get(colors, EINA_TRUE);
   if (!pixels) return;
   stride = evas_object_image_stride_get(colors);
   // cpu percent (first row)
   for (x = 0; x <= 100; x++) pixels[x] = cpu_colormap[x];
   // cpu freq (next row)
   for (x = 0; x <= 100; x++) pixels[x + (stride / 4)] = freq_colormap[x];
   // cpu temp (next row)
   for (x = 0; x <= 100; x++) pixels[x + (stride / 2)] = temp_colormap[x];

   evas_object_image_data_set(colors, pixels);
   evas_object_image_data_update_add(colors, 0, 0, 101, 1);
}

static void
_win_mouse_move_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Coord w, h;
   Evas_Event_Mouse_Move *ev;
   Animate *ad = data;

   ev = event_info;
   evas_object_geometry_get(obj, NULL, NULL, &w, &h);

   if (ev->cur.output.x >= (w - 128) && ev->cur.output.y <= 128)
     evas_object_show(ad->btn_menu);
   else
     evas_object_hide(ad->btn_menu);
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Animate *ad;

   ad = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(ad->ui->cpu.win);
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Ui *ui;
   Animate *ad = data;

   ui = ad->ui;
   if (!ad->menu)
     ad->menu = evisum_ui_main_menu_create(ui, ui->cpu.win, obj);
   else
     {
        evas_object_del(ad->menu);
        ad->menu = NULL;
     }
}

static Animate *
_graph(Ui *ui, Evas_Object *parent)
{
   Evas_Object *tbl, *tbl2, *box, *obj, *ic, *lb, *rec;
   Evas_Object *fr, *bx, *hbx, *colors, *check, *btn;
   int i, f;
   char buf[128];

   Animate *ad = calloc(1, sizeof(Animate));
   if (!ad) return NULL;

   ad->win = ui->cpu.win;
   ad->cpu_count = system_cpu_count_get();
   if (!system_cpu_frequency_min_max_get(&ad->freq_min, &ad->freq_max))
     ad->cpu_freq = EINA_TRUE;

   system_cpu_temperature_min_max_get(&ad->temp_min, &ad->temp_max);
   if ((system_cpu_n_temperature_get(0)) != -1)
     ad->cpu_temp = EINA_TRUE;

   ad->cpu_order = malloc((ad->cpu_count) * sizeof(int));
   for (i = 0; i < ad->cpu_count; i++)
     ad->cpu_order[i] = i;
   system_cpu_topology_get(ad->cpu_order, ad->cpu_count);

   // init colormaps from a small # of points
   _color_init(cpu_colormap_in, COLOR_CPU_NUM, cpu_colormap);
   _color_init(freq_colormap_in, COLOR_FREQ_NUM, freq_colormap);
   _color_init(temp_colormap_in, COLOR_TEMP_NUM, temp_colormap);

   box = parent;

   tbl = elm_table_add(box);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_show(tbl);

   obj = evas_object_image_add(evas_object_evas_get(parent));
   evas_object_size_hint_align_set(obj, FILL, FILL);
   evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
   evas_object_image_smooth_scale_set(obj, EINA_FALSE);
   evas_object_image_filled_set(obj, EINA_TRUE);
   evas_object_image_alpha_set(obj, EINA_FALSE);
   evas_object_show(obj);

   elm_table_pack(tbl, obj, 0, 0, 5, ad->cpu_count);

   rec = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_size_hint_align_set(rec, FILL, FILL);
   evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
   evas_object_color_set(rec, 0, 0, 0, 64);
   evas_object_show(rec);
   elm_table_pack(tbl, rec, 0, 0, 4, ad->cpu_count);

   for (i = 0; i < ad->cpu_count; i++)
     {
        rec = evas_object_rectangle_add(evas_object_evas_get(parent));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 0, i, 1, 1);

        rec = evas_object_rectangle_add(evas_object_evas_get(parent));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(24), ELM_SCALE_SIZE(24));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 1, i, 1, 1);

        ic = elm_icon_add(parent);
        elm_icon_standard_set(ic, evisum_icon_path_get("cpu"));
        evas_object_size_hint_align_set(ic, FILL, FILL);
        evas_object_size_hint_weight_set(ic, 0.0, EXPAND);
        elm_table_pack(tbl, ic, 1, i, 1, 1);
        evas_object_show(ic);

        rec = evas_object_rectangle_add(evas_object_evas_get(parent));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 2, i, 1, 1);

        rec = evas_object_rectangle_add(evas_object_evas_get(parent));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 3, i, 1, 1);

        lb = elm_label_add(parent);
        snprintf(buf, sizeof(buf), "<b><color=#fff>%i</></>", ad->cpu_order[i]);
        elm_object_text_set(lb, buf);
        evas_object_size_hint_align_set(lb, 0.0, 0.5);
        evas_object_size_hint_weight_set(lb, 0.0, EXPAND);
        elm_table_pack(tbl, lb, 3, i, 1, 1);
        evas_object_show(lb);

        // Begin explainer label overlay.

        tbl2 = elm_table_add(parent);
        evas_object_size_hint_align_set(tbl2, 0.7, 0.5);
        evas_object_size_hint_weight_set(tbl2, EXPAND, EXPAND);
        evas_object_show(tbl2);

        rec = evas_object_rectangle_add(evas_object_evas_get(parent));
        evas_object_color_set(rec, 0, 0, 0, 128);
        evas_object_size_hint_align_set(rec, FILL, FILL);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        elm_table_pack(tbl2, rec, 0, 0, 1, 1);

        lb = elm_label_add(parent);
        evas_object_size_hint_align_set(lb, FILL, FILL);
        evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
        elm_object_text_set(lb, buf);
        elm_table_pack(tbl2, lb, 0, 0, 1, 1);
        elm_table_pack(tbl, tbl2, 4, i, 1, 1);

        Explainer *exp = malloc(sizeof(Explainer));
        exp->rec = rec;
        exp->lb = lb;

        ad->explainers = eina_list_append(ad->explainers, exp);
     }

   ad->btn_menu = btn = elm_button_add(parent);
   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get("menu"));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), 1);
   evas_object_show(ic);
   evas_object_size_hint_weight_set(btn, 1.0, 1.0);
   evas_object_size_hint_align_set(btn, 1.0, 0);
   evas_object_smart_callback_add(btn, "clicked", _btn_menu_clicked_cb, ad);
   elm_table_pack(tbl, btn, 0, 0, 5, ad->cpu_count);

   bx = elm_box_add(box);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_show(bx);
   elm_box_pack_end(bx, tbl);

   // Set the main content.
   elm_box_pack_end(box, bx);

   tbl = elm_table_add(box);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_size_hint_weight_set(tbl, EXPAND, 0);
   evas_object_show(tbl);

   fr = elm_frame_add(box);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_show(fr);
   elm_object_text_set(fr, _("Legend"));
   elm_object_content_set(fr, tbl);

   colors = evas_object_image_add(evas_object_evas_get(fr));
   evas_object_size_hint_min_set
     (colors, 100, COLORS_HEIGHT * elm_config_scale_get());
   evas_object_size_hint_align_set(colors, FILL, FILL);
   evas_object_size_hint_weight_set(colors, EXPAND, EXPAND);
   evas_object_image_smooth_scale_set(colors, EINA_FALSE);
   evas_object_image_filled_set(colors, EINA_TRUE);
   evas_object_image_alpha_set(colors, EINA_FALSE);
   _colors_fill(colors);
   elm_table_pack(tbl, colors, 0, 0, 2, 3);
   evas_object_show(colors);

   lb = elm_label_add(parent);
   elm_object_text_set(lb, "<b><color=#fff> 0%</></>");
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 0, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(parent);
   f = (ad->freq_min + 500) / 1000;
   if (f < 1000)
     snprintf(buf, sizeof(buf), "<b><color=#fff> %iMHz</></>", f);
   else
     snprintf(buf, sizeof(buf), "<b><color=#fff> %1.1fGHz</></>", ((double)f + 0.05) / 1000.0);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 1, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(parent);
   elm_object_text_set(lb, "<b><color=#fff>100%</></>");
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 0, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(parent);
   f = (ad->freq_max + 500) / 1000;
   if (f < 1000)
     snprintf(buf, sizeof(buf), "<b><color=#fff>%iMHz</></>", f);
   else
     snprintf(buf, sizeof(buf), "<b><color=#fff>%1.1fGHz</></>", ((double)f + 0.05) / 1000.0);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 1, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(parent);
   snprintf(buf, sizeof(buf), "<b><color=#fff> %i°C</></>", ad->temp_min);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 2, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(parent);
   snprintf(buf, sizeof(buf), "<b><color=#fff>%i°C</></>", ad->temp_max);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 2, 1, 1);
   evas_object_show(lb);

   elm_box_pack_end(box, fr);

   fr = elm_frame_add(box);
   elm_frame_autocollapse_set(fr, EINA_TRUE);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_show(fr);
   elm_frame_collapse_set(fr, EINA_TRUE);
   elm_object_text_set(fr, _("Options"));
   elm_box_pack_end(box, fr);

   hbx = elm_box_add(fr);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   elm_box_horizontal_set(hbx, 1);
   evas_object_show(hbx);
   elm_object_content_set(fr, hbx);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Overlay CPU frequency?"));
   if (!ad->cpu_freq) elm_object_disabled_set(check, 1);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);
   evas_object_smart_callback_add(check, "changed", _check_changed_cb, ad);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Overlay CPU temperatures?"));
   if (!ad->cpu_temp) elm_object_disabled_set(check, 1);
   evas_object_smart_callback_add(check, "changed", _temp_check_changed_cb, ad);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Confused?"));
   evas_object_smart_callback_add(check, "changed", _confused_check_changed_cb, ad);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);

   ad->obj = obj;
   ad->ui = ui;
   ad->colors = colors;

   // min size of cpu color graph to show all cores.
   evas_object_size_hint_min_set
     (obj, 100, (BAR_HEIGHT * ad->cpu_count) * elm_config_scale_get());

   evas_object_event_callback_add(ui->cpu.win, EVAS_CALLBACK_DEL, _win_del_cb, ad);
   evas_object_event_callback_add(ui->cpu.win, EVAS_CALLBACK_MOVE, _win_move_cb, ad);

   // run a feedback thread that sends feedback to the mainloop
   ad->thread = ecore_thread_feedback_run(_core_times_main_cb,
                                          _core_times_feedback_cb,
                                          NULL,
                                          NULL,
                                          ad, EINA_TRUE);
   return ad;
}

 static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   evisum_ui_config_save(ui);
}

void
ui_win_cpu_add(Ui *ui, Evas_Object *parent)
{
   Animate *ad;
   Evas_Object *win, *box, *scroller;
   Evas_Coord x = 0, y = 0;

   if (ui->cpu.win)
     {
        elm_win_raise(ui->cpu.win);
        return;
     }

   ui->cpu.win = win = elm_win_util_standard_add("evisum",
                   _("CPU Activity"));
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win,
                                   evisum_ui_backgrounds_enabled_get());

   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, ui);

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

   ad = _graph(ui, box);
   evas_object_event_callback_add(scroller, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, ad);
   evas_object_event_callback_add(scroller, EVAS_CALLBACK_MOUSE_MOVE, _win_mouse_move_cb, ad);
   elm_object_content_set(scroller, box);
   elm_object_content_set(win, scroller);

   if (ui->cpu.width > 0 && ui->cpu.height > 0)
     evas_object_resize(win, ui->cpu.width, ui->cpu.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH * 1.5, UI_CHILD_WIN_HEIGHT * 1.1);

   if (ui->cpu.x > 0 && ui->cpu.y > 0)
     evas_object_move(win, ui->cpu.x, ui->cpu.y);
   else
     {
        if (parent)
          evas_object_geometry_get(parent, &x, &y, NULL, NULL);
        if (x > 0 && y > 0)
          evas_object_move(win, x + 20, y + 20);
        else
         elm_win_center(win, 1, 1);
     }
   evas_object_show(win);
}

