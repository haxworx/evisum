#include "cpu_default.h"

#define BAR_HEIGHT    3
#define COLORS_HEIGHT 32
#define CORES_MANY    16

typedef struct {
   Evas_Object    *obj;
   Evas_Object    *bg;
   Evas_Object    *colors;
   int             cpu_count;

   Eina_Bool       show_cpufreq;
   // Have cpu scaling
   Eina_Bool       cpu_freq;
   int             freq_min;
   int             freq_max;

   Eina_Bool       show_cputemp;
   // Have temp readings
   Eina_Bool       cpu_temp;
   int             temp_min;
   int             temp_max;

   Eina_List      *explainers;
   int            *cpu_order;
   Eina_Bool       confused;
} Ext;

static void
_core_times_main_cb(void *data, Ecore_Thread *thread)
{
   Ui_Cpu_Data *pd = data;
   int ncpu;
   Ext *ext = pd->ext;

   if (!system_cpu_frequency_min_max_get(&ext->freq_min, &ext->freq_max))
     ext->cpu_freq = 1;

   system_cpu_temperature_min_max_get(&ext->temp_min, &ext->temp_max);
   if ((system_cpu_n_temperature_get(0)) != -1)
     ext->cpu_temp = 1;

   while (!ecore_thread_check(thread))
     {
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 100000);
        Core *cores_out = calloc(ncpu, sizeof(Core));

        if (cores_out)
          {
             for (int n = 0; n < ncpu; n++)
               {
                  int id = ext->cpu_order[n];
                  Core *core = &(cores_out[n]);
                  core->id = id;
                  core->percent = cores[id]->percent;
                  if (ext->cpu_freq)
                    core->freq = system_cpu_n_frequency_get(id);
                  if (ext->cpu_temp)
                    core->temp = system_cpu_n_temperature_get(id);
                  free(cores[id]);
               }
             ecore_thread_feedback(thread, cores_out);
          }
        free(cores);
     }
}

static void
_update(Ui_Cpu_Data *pd, Core *cores)
{
   Evas_Object *obj;
   unsigned int *pixels, *pix;
   Evas_Coord x, y, w, h;
   int iw, stride;
   Eina_Bool clear = 0;
   Ext *ext = pd->ext;
   obj = ext->obj;

   evas_object_geometry_get(obj, &x, &y, &w, &h);
   evas_object_image_size_get(obj, &iw, NULL);
   // if image pixel size doesn't match geom - we need to resize, so set
   // new size and mark it for clearing when we fill
   if (iw != w)
     {
        evas_object_image_size_set(obj, w, ext->cpu_count * 3);
        clear = 1;
     }

   // get pixel data ptr
   pixels = evas_object_image_data_get(obj, 1);
   if (!pixels) return;
   // get stride (# of bytes per line)
   stride = evas_object_image_stride_get(obj);

   for (y = 0; y < ext->cpu_count; y++)
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
        if ((ext->show_cpufreq) && (ext->cpu_freq))
          {
             int v = core->freq - ext->freq_min;
             int d = ext->freq_max - ext->freq_min;

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

        if (ext->show_cputemp && ext->cpu_temp)
          {
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             pix[x] = temp_colormap[core->temp & 0xff];
          }

        if (!ext->show_cpufreq)
          {
             // no freq show - then just repeat cpu usage color
             pix = &(pixels[((y * 3) + 1) * (stride / 4)]);
             pix[x] = c1;
          }
        if (!ext->show_cputemp)
          {
             pix = &(pixels[((y * 3) + 2) * (stride / 4)]);
             pix[x] = c1;
          }
     }
   // hand back pixel data ptr so evas knows we are done with it
   evas_object_image_data_set(obj, pixels);
   // now update region for all pixels in the image at the end as we
   // changed everything
   evas_object_image_data_update_add(obj, 0, 0, w, ext->cpu_count * 3);
}

typedef struct
{
   Evas_Object *lb;
   Evas_Object *rec;
} Explainer;

static void
_explain(Ui_Cpu_Data *pd, Core *cores)
{
   Eina_Strbuf *buf;
   Explainer *exp;
   Ext *ext;
   Evas_Object *lb, *rec;

   ext = pd->ext;

   if (!ext->explainers) return;

   buf = eina_strbuf_new();

   for (int i = 0; i < ext->cpu_count; i++)
     {
        Core *core = &(cores[i]);
        exp = eina_list_nth(ext->explainers, i);

        lb = exp->lb;
        rec = exp->rec;
        if (!ext->confused)
          {
             evas_object_hide(rec);
             evas_object_hide(lb);
          }
        else
          {
             eina_strbuf_append_printf(buf, "<b><color=#fff>%i%% ", core->percent);
             if (ext->cpu_freq)
               eina_strbuf_append_printf(buf, "%1.1fGHz ", (double) core->freq / 1000000);
             if (ext->cpu_temp)
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
   Ui_Cpu_Data *pd;
   Core *cores;
   Ext *ext;
   static Eina_Bool was_confused = 0;

   pd = data;
   ext = pd->ext;
   cores = msgdata;

   _update(pd, cores);

   if (ext->confused || was_confused)
     {
        _explain(pd, cores);
        was_confused = 1;
     }

   free(cores);
}

static void
_cb_free(void *data)
{
   Explainer *exp;
   Ext *ext = data;

   EINA_LIST_FREE(ext->explainers, exp)
     free(exp);

   free(ext->cpu_order);

   free(ext);
}

static void
_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Ui_Cpu_Data *pd = data;
   Ext *ext = pd->ext;

   ext->show_cpufreq = elm_check_state_get(obj);
}

static void
_temp_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Ui_Cpu_Data *pd = data;
   Ext *ext = pd->ext;

   ext->show_cputemp = elm_check_state_get(obj);
}

static void
_confused_check_changed_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                           void *event_info EINA_UNUSED)
{
   Ui_Cpu_Data *pd = data;
   Ext *ext = pd->ext;

   ext->confused = elm_check_state_get(obj);
}

static void
_colors_fill(Evas_Object *colors)
{
   // fill a 3 pixel high (and 100 wide) image with 3 grpdients matching
   // the colormaps we calculated as a legend
   int x, stride;
   unsigned int *pixels;

   evas_object_image_size_set(colors, 101, 3);
   pixels = evas_object_image_data_get(colors, 1);
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

Ui_Cpu_Data *
cpu_visual_default(Evas_Object *parent_box)
{
   Evas_Object *tbl, *tbl2, *box, *obj, *ic, *lb, *rec;
   Evas_Object *fr, *bx, *hbx, *colors, *check;
   Ext *ext;
   int *cpu_order;
   int i, f;
   char buf[128];
   Eina_Bool show_icons = 1;

   Ui_Cpu_Data *pd = calloc(1, sizeof(Ui_Cpu_Data));
   if (!pd) return NULL;

   pd->ext = ext = calloc(1, sizeof(Ext));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ext, NULL);

   ext->cpu_count = system_cpu_count_get();
   if (!system_cpu_frequency_min_max_get(&ext->freq_min, &ext->freq_max))
     ext->cpu_freq = 1;

   system_cpu_temperature_min_max_get(&ext->temp_min, &ext->temp_max);
   if ((system_cpu_n_temperature_get(0)) != -1)
     ext->cpu_temp = 1;

   ext->cpu_order = cpu_order = malloc((ext->cpu_count) * sizeof(int));
   for (i = 0; i < ext->cpu_count; i++)
     cpu_order[i] = i;
   system_cpu_topology_get(cpu_order, ext->cpu_count);

   box = parent_box;

   tbl = elm_table_add(box);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_size_hint_weight_set(tbl, EXPAND, EXPAND);
   evas_object_show(tbl);

   obj = evas_object_image_add(evas_object_evas_get(box));
   evas_object_size_hint_align_set(obj, FILL, FILL);
   evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
   evas_object_image_smooth_scale_set(obj, 0);
   evas_object_image_filled_set(obj, 1);
   evas_object_image_alpha_set(obj, 0);
   evas_object_show(obj);

   elm_table_pack(tbl, obj, 0, 0, 5, ext->cpu_count);

   if (ext->cpu_count > CORES_MANY)
     show_icons = 0;

   if (show_icons)
     {
        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_size_hint_align_set(rec, FILL, FILL);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        evas_object_color_set(rec, 0, 0, 0, 64);
        evas_object_show(rec);
        elm_table_pack(tbl, rec, 0, 0, 4, ext->cpu_count);
     }

   for (i = 0; show_icons && (i < ext->cpu_count); i++)
     {
        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 0, i, 1, 1);

        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(24), ELM_SCALE_SIZE(24));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 1, i, 1, 1);

        ic = elm_icon_add(box);
        elm_icon_standard_set(ic, evisum_icon_path_get("cpu"));
        evas_object_size_hint_align_set(ic, FILL, FILL);
        evas_object_size_hint_weight_set(ic, 0.0, EXPAND);
        elm_table_pack(tbl, ic, 1, i, 1, 1);
        evas_object_show(ic);

        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(8), ELM_SCALE_SIZE(8));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 2, i, 1, 1);

        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_color_set(rec, 0, 0, 0, 0);
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
        evas_object_size_hint_weight_set(rec, 0.0, EXPAND);
        elm_table_pack(tbl, rec, 3, i, 1, 1);

        lb = elm_label_add(box);
        snprintf(buf, sizeof(buf), "<b><color=#fff>%i</></>", cpu_order[i]);
        elm_object_text_set(lb, buf);
        evas_object_size_hint_align_set(lb, 0.0, 0.5);
        evas_object_size_hint_weight_set(lb, 0.0, EXPAND);
        elm_table_pack(tbl, lb, 3, i, 1, 1);
        evas_object_show(lb);

        // Begin explainer label overlay.

        tbl2 = elm_table_add(box);
        evas_object_size_hint_align_set(tbl2, 0.7, 0.5);
        evas_object_size_hint_weight_set(tbl2, EXPAND, EXPAND);
        evas_object_show(tbl2);

        rec = evas_object_rectangle_add(evas_object_evas_get(box));
        evas_object_color_set(rec, 0, 0, 0, 128);
        evas_object_size_hint_align_set(rec, FILL, FILL);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        elm_table_pack(tbl2, rec, 0, 0, 1, 1);

        lb = elm_label_add(box);
        evas_object_size_hint_align_set(lb, FILL, FILL);
        evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
        elm_object_text_set(lb, buf);
        elm_table_pack(tbl2, lb, 0, 0, 1, 1);
        elm_table_pack(tbl, tbl2, 4, i, 1, 1);

        Explainer *exp = malloc(sizeof(Explainer));
        exp->rec = rec;
        exp->lb = lb;

        ext->explainers = eina_list_append(ext->explainers, exp);
     }

   // Callback to free anything extra we pass around via the ext pointer in (Ui_Cpu_Data *).
   pd->ext_free_cb = _cb_free;

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
   evas_object_image_smooth_scale_set(colors, 0);
   evas_object_image_filled_set(colors, 1);
   evas_object_image_alpha_set(colors, 0);
   _colors_fill(colors);
   elm_table_pack(tbl, colors, 0, 0, 2, 3);
   evas_object_show(colors);

   lb = elm_label_add(box);
   elm_object_text_set(lb, "<b><color=#fff> 0%</></>");
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 0, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(box);
   f = (ext->freq_min + 500) / 1000;
   if (f < 1000)
     snprintf(buf, sizeof(buf), "<b><color=#fff> %iMHz</></>", f);
   else
     snprintf(buf, sizeof(buf), "<b><color=#fff> %1.1fGHz</></>", ((double)f + 0.05) / 1000.0);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 1, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(box);
   elm_object_text_set(lb, "<b><color=#fff>100%</></>");
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 0, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(box);
   f = (ext->freq_max + 500) / 1000;
   if (f < 1000)
     snprintf(buf, sizeof(buf), "<b><color=#fff>%iMHz</></>", f);
   else
     snprintf(buf, sizeof(buf), "<b><color=#fff>%1.1fGHz</></>", ((double)f + 0.05) / 1000.0);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 1, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(box);
   snprintf(buf, sizeof(buf), "<b><color=#fff> %i°C</></>", ext->temp_min);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 0, 2, 1, 1);
   evas_object_show(lb);

   lb = elm_label_add(box);
   snprintf(buf, sizeof(buf), "<b><color=#fff>%i°C</></>", ext->temp_max);
   elm_object_text_set(lb, buf);
   evas_object_size_hint_align_set(lb, 0.99, 0.5);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_table_pack(tbl, lb, 1, 2, 1, 1);
   evas_object_show(lb);

   elm_box_pack_end(box, fr);

   fr = elm_frame_add(box);
   elm_frame_autocollapse_set(fr, 1);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_show(fr);
   elm_frame_collapse_set(fr, 0);
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
   if (!ext->cpu_freq) elm_object_disabled_set(check, 1);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);
   evas_object_smart_callback_add(check, "changed", _check_changed_cb, pd);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Overlay CPU temperatures?"));
   if (!ext->cpu_temp) elm_object_disabled_set(check, 1);
   evas_object_smart_callback_add(check, "changed", _temp_check_changed_cb, pd);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);

   check = elm_check_add(fr);
   evas_object_size_hint_align_set(check, FILL, FILL);
   evas_object_size_hint_weight_set(check, EXPAND, 0);
   elm_object_text_set(check, _("Confused?"));
   evas_object_smart_callback_add(check, "changed", _confused_check_changed_cb, pd);
   evas_object_show(check);
   elm_box_pack_end(hbx, check);

   ext->obj = obj;
   ext->colors = colors;

   // min size of cpu color graph to show all cores.
   evas_object_size_hint_min_set
     (obj, 100, (BAR_HEIGHT * ext->cpu_count) * elm_config_scale_get());

   // run a feedback thread that sends feedback to the mainloop
   pd->thread = ecore_thread_feedback_run(_core_times_main_cb,
                                           _core_times_feedback_cb,
                                           NULL,
                                           NULL,
                                           pd, 1);
   return pd;
}

