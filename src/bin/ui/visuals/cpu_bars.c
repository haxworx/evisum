#include "cpu_bars.h"
#include "../../background/evisum_background.h"
#include "ui/evisum_ui_graph.h"

#define BAR_WIDTH 16
#define POLL_USEC 33333

typedef struct {
    int cpu_count;
    int *cpu_order;
    int *cpu_smooth;
    Eina_List *objects;
    Evas_Object *tb;
    Evas_Object *bg;
} Ext;

static void
_bar_gradient_apply(Evas_Object *rec, unsigned int top, unsigned int bottom) {
    Evas_Map *map;

    map = evas_map_new(4);
    if (!map) {
        evas_object_map_enable_set(rec, EINA_FALSE);
        evas_object_color_set(rec, RVAL(top), GVAL(top), BVAL(top), AVAL(top));
        return;
    }

    evas_map_util_points_populate_from_object(map, rec);
    evas_map_point_color_set(map, 0, RVAL(top), GVAL(top), BVAL(top), AVAL(top));
    evas_map_point_color_set(map, 1, RVAL(top), GVAL(top), BVAL(top), AVAL(top));
    evas_map_point_color_set(map, 2, RVAL(bottom), GVAL(bottom), BVAL(bottom), AVAL(bottom));
    evas_map_point_color_set(map, 3, RVAL(bottom), GVAL(bottom), BVAL(bottom), AVAL(bottom));
    evas_object_map_set(rec, map);
    evas_object_map_enable_set(rec, EINA_TRUE);
    evas_map_free(map);
}

static void
_cpu_percent_colors_fill(Evas_Object *colors) {
    unsigned int *pixels;
    int stride;

    evas_object_image_size_set(colors, 101, 1);
    pixels = evas_object_image_data_get(colors, 1);
    if (!pixels) return;
    stride = evas_object_image_stride_get(colors) / 4;

    for (int x = 0; x <= 100; x++) pixels[x] = cpu_colormap[x];

    evas_object_image_data_set(colors, pixels);
    evas_object_image_data_update_add(colors, 0, 0, stride, 1);
}

static void
_core_times_main_cb(void *data, Ecore_Thread *thread) {
    int ncpu;
    uint64_t seq = 0;
    Evisum_Ui_Cpu_Visual_Data *pd = data;
    Ext *ext = pd->ext;

    ecore_thread_name_set(thread, "cpu");

    while (!ecore_thread_check(thread)) {
        if (!evisum_background_update_wait(&seq)) continue;
        Cpu_Core **cores = system_cpu_state_get(&ncpu);
        if (!cores || ncpu <= 0) continue;
        Core *cores_out = calloc(ncpu, sizeof(Core));

        if (cores_out) {
            for (int n = 0; n < ncpu; n++) {
                int id = ext->cpu_order[n];
                Core *core = &(cores_out[n]);
                core->id = id;
                core->percent = cores[id]->percent;
            }
            ecore_thread_feedback(thread, cores_out);
        }
        free(cores);
    }
}

static void
_core_times_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata) {
    Evisum_Ui_Cpu_Visual_Data *pd;
    Core *cores;
    Ext *ext;
    Evas_Coord oh, ow, ox, oy;
    int step = 0;

    pd = data;
    ext = pd->ext;
    cores = msgdata;

    evas_object_geometry_get(ext->tb, NULL, &oy, &ow, &oh);
    evas_object_geometry_get(pd->win, NULL, NULL, NULL, &oh);

    step = (oh / 100);

    for (int i = 0; i < ext->cpu_count; i++) {
        Core *core = &cores[i];
        Evas_Object *rec = eina_list_nth(ext->objects, i);
        int percent = core->percent;
        int prev = ext->cpu_smooth[i];
        if (prev >= 0) percent = ((prev * 3) + percent) / 4;
        if (percent < 0) percent = 0;
        else if (percent > 100)
            percent = 100;
        ext->cpu_smooth[i] = percent;
        evas_object_geometry_get(rec, &ox, NULL, NULL, NULL);
        unsigned int c_top = cpu_colormap[percent];
        unsigned int c_bottom = cpu_colormap[0];
        int h = percent * step;
        if (!h) h = 1;
        evas_object_resize(rec, ELM_SCALE_SIZE(BAR_WIDTH), ELM_SCALE_SIZE(h));
        evas_object_move(rec, ox, oy - ELM_SCALE_SIZE(h));
        _bar_gradient_apply(rec, c_top, c_bottom);
        elm_table_align_set(ext->tb, 0.0, 1.0);
    }

    free(cores);
}

static void
_cb_free(void *data) {
    Ext *ext = data;

    eina_list_free(ext->objects);

    free(ext->cpu_smooth);
    free(ext->cpu_order);
    free(ext);
}

Evisum_Ui_Cpu_Visual_Data *
evisum_ui_cpu_visual_bars(Evas_Object *parent_bx) {
    Evas_Object *tb, *rec, *container, *fr, *legend_tbl, *colors, *lb, *bg, *desc_fr;
    Ext *ext;
    uint8_t text_r, text_g, text_b;
    char text_hex[7];
    int i;

    Evisum_Ui_Cpu_Visual_Data *pd = calloc(1, sizeof(Evisum_Ui_Cpu_Visual_Data));
    if (!pd) return NULL;

    pd->ext = ext = calloc(1, sizeof(Ext));
    EINA_SAFETY_ON_NULL_RETURN_VAL(ext, NULL);
    pd->ext_free_cb = _cb_free;

    /* Populate lookup table to match id with topology core id */
    ext->cpu_count = system_cpu_count_get();
    ext->cpu_order = malloc((ext->cpu_count) * sizeof(int));
    ext->cpu_smooth = malloc((ext->cpu_count) * sizeof(int));
    for (i = 0; i < ext->cpu_count; i++) ext->cpu_order[i] = i;
    if (!ext->cpu_order || !ext->cpu_smooth) {
        free(ext->cpu_order);
        free(ext->cpu_smooth);
        free(ext);
        free(pd);
        return NULL;
    }
    for (i = 0; i < ext->cpu_count; i++) ext->cpu_smooth[i] = -1;
    system_cpu_topology_get(ext->cpu_order, ext->cpu_count);

    container = elm_table_add(parent_bx);
    evas_object_size_hint_weight_set(container, EXPAND, EXPAND);
    evas_object_size_hint_align_set(container, FILL, FILL);
    evas_object_show(container);

    bg = evas_object_rectangle_add(evas_object_evas_get(container));
    evisum_ui_graph_bg_set(bg);
    evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bg, FILL, FILL);
    elm_table_pack(container, bg, 0, 0, 1, 1);
    evas_object_show(bg);

    ext->tb = tb = elm_table_add(container);
    elm_table_padding_set(tb, ELM_SCALE_SIZE(2), ELM_SCALE_SIZE(2));
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, 0.5, 1.0);
    elm_table_pack(container, tb, 0, 0, 1, 1);
    evas_object_show(tb);

    for (i = 0; i < ext->cpu_count; i++) {
        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BAR_WIDTH), 1);
        elm_table_pack(tb, rec, i, 0, 1, 1);
        evas_object_show(rec);
        ext->objects = eina_list_append(ext->objects, rec);
    }

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
    evas_object_size_hint_align_set(rec, FILL, FILL);
    elm_table_pack(tb, rec, 0, 0, i, 2);

    elm_box_pack_end(parent_bx, container);

    evisum_cpu_default_colors_get(&text_r, &text_g, &text_b, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL);
    snprintf(text_hex, sizeof(text_hex), "%02x%02x%02x", text_r, text_g, text_b);

    fr = elm_frame_add(parent_bx);
    evas_object_size_hint_align_set(fr, FILL, FILL);
    evas_object_size_hint_weight_set(fr, EXPAND, 0.0);
    elm_object_text_set(fr, _("Legend"));
    evas_object_show(fr);
    elm_box_pack_end(parent_bx, fr);

    legend_tbl = elm_table_add(fr);
    evas_object_size_hint_align_set(legend_tbl, FILL, FILL);
    evas_object_size_hint_weight_set(legend_tbl, EXPAND, EXPAND);
    evas_object_show(legend_tbl);
    elm_object_content_set(fr, legend_tbl);

    colors = evas_object_image_add(evas_object_evas_get(fr));
    evas_object_size_hint_min_set(colors, ELM_SCALE_SIZE(100), ELM_SCALE_SIZE(20));
    evas_object_size_hint_align_set(colors, FILL, FILL);
    evas_object_size_hint_weight_set(colors, EXPAND, EXPAND);
    evas_object_image_smooth_scale_set(colors, 0);
    evas_object_image_filled_set(colors, 1);
    evas_object_image_alpha_set(colors, 0);
    _cpu_percent_colors_fill(colors);
    elm_table_pack(legend_tbl, colors, 0, 0, 2, 1);
    evas_object_show(colors);

    lb = elm_label_add(fr);
    elm_object_text_set(lb, eina_slstr_printf("<b><color=#%s>0%%</></>", text_hex));
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    elm_table_pack(legend_tbl, lb, 0, 0, 1, 1);
    evas_object_show(lb);

    lb = elm_label_add(fr);
    elm_object_text_set(lb, eina_slstr_printf("<b><color=#%s>100%%</></>", text_hex));
    evas_object_size_hint_align_set(lb, 1.0, 0.5);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    elm_table_pack(legend_tbl, lb, 1, 0, 1, 1);
    evas_object_show(lb);

    desc_fr = elm_frame_add(parent_bx);
    elm_object_text_set(desc_fr, _("Description"));
    evas_object_size_hint_align_set(desc_fr, FILL, FILL);
    evas_object_size_hint_weight_set(desc_fr, EXPAND, 0.0);
    elm_box_pack_end(parent_bx, desc_fr);
    evas_object_show(desc_fr);

    lb = elm_label_add(desc_fr);
    elm_label_line_wrap_set(lb, ELM_WRAP_WORD);
    elm_object_text_set(lb, _("<align=left>This view shows CPU usage only.</align>"));
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    elm_object_content_set(desc_fr, lb);
    evas_object_show(lb);

    pd->thread = ecore_thread_feedback_run(_core_times_main_cb, _core_times_feedback_cb, NULL, NULL, pd, 1);
    return pd;
}
