#include "ui_cpu.h"

typedef struct {
   short id;
   short percent;
   unsigned int freq;
} Core;

typedef struct {
   Ui             *ui;

   Evas_Object    *bg;
   Evas_Object    *obj;

   Evas_Object    *colors;

   int             cpu_count;

   Eina_Bool       show_cpufreq;
   // Have cpu scaling
   Eina_Bool       cpu_freq;
   int             freq_min;
   int             freq_max;
} Animate;

typedef struct _Color_Point {
   unsigned int val;
   unsigned int color;
} Color_Point;

// config for colors/sizing
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 }, // 0
   {  33, 0xff2030a0 }, // 1
   {  66, 0xffa040a0 }, // 2
   { 100, 0xffff9040 }, // 3
   { 256, 0xffff9040 }  // overflow to avoid if's
};
static const Color_Point freq_colormap_in[] = {
   {   0, 0x00000000 }, // 0
   {  50, 0x00105020 }, // 1
   { 100, 0x002060c0 }, // 2
   { 256, 0x002060c0 }  // overflow to avoid if's
};
#define BAR_HEIGHT 2
#define COLORS_HEIGHT 20

// stored colormap tables
static unsigned int cpu_colormap[256];
static unsigned int freq_colormap[256];

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
                  Core *core = &(cores_out[n]);
                  core->id = n;
                  core->percent = cores[n]->percent;
                  if (ad->cpu_freq)
                    core->freq = system_cpu_n_frequency_get(n);
                  free(cores[n]);
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
        evas_object_image_size_set(obj, w, ad->cpu_count);
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
        unsigned int a, r, g, b;
        unsigned int a2, r2, g2, b2;

        // our pix ptr is the pixel row and y is both y pixel coord and core
        pix = &(pixels[y * (stride / 4)]);
        if (clear)
          {
             // clear/fill with 0 value from colormap
             for (x = 0; x < (w - 1); x++) pix[x] = cpu_colormap[0];
          }
        else
          {
             // scroll pixels 1 to the left
             for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
          }
        // final pixel on end of each row... set it to a new value
        // get color from cpu colormap and decompose to rgb
        c1 = cpu_colormap[core->percent & 0xff];
        a = AVAL(c1);
        r = RVAL(c1);
        g = GVAL(c1);
        b = BVAL(c1);
        // if we overlay freq, then we will do argb addition of freq color
        // and cpu percent usage color. designed to affect mostly different
        // rgb channels so you can see when freq goes up but not cpu % or
        // vice-versa
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
                  a2 = AVAL(c2);
                  r2 = RVAL(c2);
                  g2 = GVAL(c2);
                  b2 = BVAL(c2);
                  // do argb add and then limit arb to max of 0xff since
                  // the add can overflow 0xff
                  a += a2;
                  r += r2;
                  g += g2;
                  b += b2;
                  if (a > 0xff) a = 0xff;
                  if (r > 0xff) r = 0xff;
                  if (g > 0xff) g = 0xff;
                  if (b > 0xff) b = 0xff;
               }
          }
        // last pixel == resulting pixel color
        pix[x] = ARGB(a, r, g, b);
     }
   // hand back pixel data ptr so evas knows we are done with it
   evas_object_image_data_set(obj, pixels);
   // now add update region for all pixels in the image at the end as we
   // changed everything
   evas_object_image_data_update_add(obj, 0, 0, w, ad->cpu_count);
}

static void
_core_times_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   // when the thread sends feedback to mainloop, the feedback is cpu and freq
   // stat info from the feedback thread, so update based on that info then
   // free it as we don't need it anyway - producer+consumer model
   Animate *ad = data;
   Core *cores = msgdata;
   _update(ad, cores);
   free(cores);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Animate *ad = data;
   Ui *ui = ad->ui;

   // on deletion of window, cancel thread, free animate data and set cpu
   // dialog handle to null
   ecore_thread_cancel(ui->thread_cpu);
   ecore_thread_wait(ui->thread_cpu, 0.5);
   free(ad);
   ui->win_cpu = NULL;
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Animate *ad = data;
   // sho freq overlay check changed
   ad->show_cpufreq = elm_check_state_get(obj);
}

static void
_colors_fill(Evas_Object *colors)
{
   // fill a 2 pixel high (and 100 wide) image with 2 gradients matching
   // the colormaps we calculated as a legend
   int x, stride;
   unsigned int *pixels;

   evas_object_image_size_set(colors, 101, 2);
   pixels = evas_object_image_data_get(colors, EINA_TRUE);
   if (!pixels) return;
   stride = evas_object_image_stride_get(colors);
   // cpu percent (first row)
   for (x = 0; x <= 100; x++) pixels[x] = cpu_colormap[x];
   // cpu freq (next row)
   for (x = 0; x <= 100; x++) pixels[x + (stride / 4)] = freq_colormap[x];
   evas_object_image_data_set(colors, pixels);
   evas_object_image_data_update_add(colors, 0, 0, 101, 1);
}

static void
_graph(Ui *ui, Evas_Object *parent)
{
   Evas_Object *frame, *tbl, *box, *obj;
   Evas_Object *fr, *hbx, *bx, *colors, *check;
   int n;

   // init colormaps from a small # of points
   _color_init(cpu_colormap_in, 4, cpu_colormap);
   _color_init(freq_colormap_in, 3, freq_colormap);

   box = parent;

   n = system_cpu_online_count_get();

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_show(frame);
   if (n > 1)
     elm_object_text_set(frame, eina_slstr_printf(_("%d CPU Cores Available"), n));
   else
     elm_object_text_set(frame, _("ONE CPU CORE...MAKE IT COUNT!!!"));

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

   elm_table_pack(tbl, obj, 0, 0, 1, 1);

   bx = elm_box_add(box);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_show(bx);
   elm_box_pack_end(bx, tbl);

   // Set the main content.
   elm_object_content_set(frame, bx);
   elm_box_pack_end(box, frame);

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
   evas_object_size_hint_min_set
     (colors, 100, COLORS_HEIGHT * elm_config_scale_get());
   evas_object_size_hint_align_set(colors, FILL, FILL);
   evas_object_size_hint_weight_set(colors, EXPAND, EXPAND);
   evas_object_image_smooth_scale_set(colors, EINA_FALSE);
   evas_object_image_filled_set(colors, EINA_TRUE);
   evas_object_image_alpha_set(colors, EINA_FALSE);
   _colors_fill(colors);
   evas_object_show(colors);
   elm_box_pack_end(hbx, colors);

   elm_box_pack_end(box, fr);

   fr = elm_frame_add(box);
   elm_frame_autocollapse_set(fr, EINA_TRUE);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_show(fr);
   elm_object_text_set(fr, _("Options"));
   elm_box_pack_end(box, fr);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Overlay CPU frequency?"));
   evas_object_show(check);
   elm_object_content_set(fr, check);

   Animate *ad = calloc(1, sizeof(Animate));
   if (!ad) return;

   ad->obj = obj;
   ad->ui = ui;
   ad->cpu_count = system_cpu_online_count_get();

   ad->colors = colors;

   // min size ofr cpu color graph to show all cores.
   evas_object_size_hint_min_set
     (obj, 100, (BAR_HEIGHT * ad->cpu_count) * elm_config_scale_get());

   evas_object_smart_callback_add(check, "changed", _check_changed_cb, ad);
   // since win is on auto-delete, just listen for when it is deleted,
   // whatever the cause/reason
   evas_object_event_callback_add(ui->win_cpu, EVAS_CALLBACK_DEL, _win_del_cb, ad);

   // run a feedback thread that sends feedback to the mainloop
   ui->thread_cpu = ecore_thread_feedback_run(_core_times_main_cb,
                                              _core_times_feedback_cb,
                                              NULL,
                                              NULL,
                                              ad, EINA_TRUE);
}

void
ui_win_cpu_add(Ui *ui)
{
   Evas_Object *win, *box, *scroller;

   if (ui->win_cpu) return;

   ui->win_cpu = win = elm_win_util_standard_add("evisum",
                   _("CPU Usage"));
   elm_win_autodel_set(win, EINA_TRUE);
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

