#include "cpu_basic.h"
#include "../../background/evisum_background.h"
#include "ui/evisum_ui_graph.h"

#define CORE_TILE_SIZE  128
#define CORE_MIN_SIZE   16
#define POLL_USEC       33333

typedef struct {
    int cpu_count;
    int *cpu_order;
    int *cpu_smooth;
    Evas_Object *container;
    Eina_List *objects;
    Eina_List *pads;
    int cols;
    int rows;
    Eina_Bool cpu_freq;
    int freq_min;
    int freq_max;
} Ext;

static void
_core_times_main_cb(void *data, Ecore_Thread *thread) {
    int ncpu;
    uint64_t seq = 0;
    Evisum_Ui_Cpu_Visual_Data *pd = data;
    Ext *ext = pd->ext;

    if (!system_cpu_frequency_min_max_get(&ext->freq_min, &ext->freq_max)) ext->cpu_freq = 1;

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
                if (ext->cpu_freq) core->freq = cores[id]->freq;
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

    pd = data;
    ext = pd->ext;
    cores = msgdata;

    int tile = CORE_TILE_SIZE;
    if (ext->cols > 0 && ext->rows > 0) {
        Evas_Coord tw = 0, th = 0;
        int side, fit_w, fit_h;

        evas_object_geometry_get(ext->container, NULL, NULL, &tw, &th);
        side = (tw < th) ? tw : th;
        if (side > 0) {
            fit_w = side / ext->cols;
            fit_h = side / ext->rows;
            tile = (fit_w < fit_h) ? fit_w : fit_h;
            if (tile > CORE_TILE_SIZE) tile = CORE_TILE_SIZE;
            if (tile < CORE_MIN_SIZE) tile = CORE_MIN_SIZE;
        }
    }
    for (int i = 0; i < ext->cpu_count; i++) {
        Core *core = &cores[i];
        int percent = core->percent;
        int prev = ext->cpu_smooth[i];
        if (prev >= 0) percent = ((prev * 3) + percent) / 4;
        if (percent < 0) percent = 0;
        else if (percent > 100)
            percent = 100;
        ext->cpu_smooth[i] = percent;
        Evas_Object *pad = eina_list_nth(ext->pads, i);
        Evas_Object *rec = eina_list_nth(ext->objects, i);
        int c = cpu_colormap[percent];
        int size = CORE_MIN_SIZE;

        if (ext->cpu_freq) {
            int v = core->freq - ext->freq_min;
            int d = ext->freq_max - ext->freq_min;
            if (d > 0) {
                v = (100 * v) / d;
                if (v < 0) v = 0;
                else if (v > 100) v = 100;
                size = CORE_MIN_SIZE + ((v * (tile - CORE_MIN_SIZE)) / 100);
            }
        } else {
            size = CORE_MIN_SIZE + ((percent * (tile - CORE_MIN_SIZE)) / 100);
        }

        if (size > tile) size = tile;
        evas_object_size_hint_min_set(pad, tile, tile);
        evas_object_size_hint_max_set(pad, tile, tile);
        evas_object_size_hint_max_set(rec, size, size);
        evas_object_size_hint_min_set(rec, size, size);
        evas_object_color_set(rec, RVAL(c), GVAL(c), BVAL(c), AVAL(c));
    }

    free(cores);
}

static void
_cb_free(void *data) {
    Ext *ext = data;

    eina_list_free(ext->objects);
    eina_list_free(ext->pads);

    free(ext->cpu_smooth);
    free(ext->cpu_order);
    free(ext);
}

static int
_row_fit(int n) {
    double i, f, value = sqrt(n);
    f = modf(value, &i);
    if (EINA_DBL_EQ(f, 0.0)) return value;
    return value + 1;
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

Evisum_Ui_Cpu_Visual_Data *
evisum_ui_cpu_visual_basic(Evas_Object *parent_bx) {
    Evas_Object *container, *grid, *fr, *legend_tbl, *colors, *lb, *bg, *desc_fr;
    Ext *ext;
    uint8_t text_r, text_g, text_b;
    char text_hex[7];

    Evisum_Ui_Cpu_Visual_Data *pd = calloc(1, sizeof(Evisum_Ui_Cpu_Visual_Data));
    if (!pd) return NULL;

    pd->ext = ext = calloc(1, sizeof(Ext));
    EINA_SAFETY_ON_NULL_RETURN_VAL(ext, NULL);
    pd->ext_free_cb = _cb_free;

    /* Populate lookup table to match id with topology core id */
    ext->cpu_count = system_cpu_count_get();
    ext->cpu_order = malloc((ext->cpu_count) * sizeof(int));
    ext->cpu_smooth = malloc((ext->cpu_count) * sizeof(int));
    for (int i = 0; i < ext->cpu_count; i++) ext->cpu_order[i] = i;
    if (!ext->cpu_order || !ext->cpu_smooth) {
        free(ext->cpu_order);
        free(ext->cpu_smooth);
        free(ext);
        free(pd);
        return NULL;
    }
    for (int i = 0; i < ext->cpu_count; i++) ext->cpu_smooth[i] = -1;
    system_cpu_topology_get(ext->cpu_order, ext->cpu_count);

    container = elm_table_add(parent_bx);
    evas_object_size_hint_weight_set(container, EXPAND, EXPAND);
    evas_object_size_hint_align_set(container, FILL, FILL);
    evas_object_show(container);
    ext->container = container;

    bg = evas_object_rectangle_add(evas_object_evas_get(container));
    evisum_ui_graph_bg_set(bg);
    evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bg, FILL, FILL);
    elm_table_pack(container, bg, 0, 0, 1, 1);
    evas_object_show(bg);

    grid = elm_table_add(container);
    elm_table_padding_set(grid, 0, 0);
    elm_table_align_set(grid, 0.5, 0.5);
    evas_object_size_hint_weight_set(grid, 0.0, 0.0);
    evas_object_size_hint_align_set(grid, 0.5, 0.5);
    elm_table_pack(container, grid, 0, 0, 1, 1);
    evas_object_show(grid);

    int row = 0, col = 0, w = _row_fit(ext->cpu_count);
    int rows = (ext->cpu_count + w - 1) / w;
    ext->cols = w;
    ext->rows = rows;

    evisum_cpu_default_colors_get(&text_r, &text_g, &text_b, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL);
    snprintf(text_hex, sizeof(text_hex), "%02x%02x%02x", text_r, text_g, text_b);

    for (int i = 0; i < ext->cpu_count; i++) {
        row = i / w;
        col = i % w;
        Evas_Object *pad = evas_object_rectangle_add(evas_object_evas_get(grid));
        evas_object_color_set(pad, 0, 0, 0, 0);
        evas_object_size_hint_min_set(pad, CORE_TILE_SIZE, CORE_TILE_SIZE);
        evas_object_size_hint_max_set(pad, CORE_TILE_SIZE, CORE_TILE_SIZE);
        elm_table_pack(grid, pad, col, row, 1, 1);
        evas_object_show(pad);
        ext->pads = eina_list_append(ext->pads, pad);

        Evas_Object *rec = evas_object_rectangle_add(evas_object_evas_get(grid));
        evas_object_size_hint_weight_set(rec, 0.0, 0.0);
        evas_object_size_hint_align_set(rec, 0.5, 0.5);
        evas_object_size_hint_min_set(rec, CORE_MIN_SIZE, CORE_MIN_SIZE);
        evas_object_size_hint_max_set(rec, CORE_MIN_SIZE, CORE_MIN_SIZE);
        elm_table_pack(grid, rec, col, row, 1, 1);
        evas_object_show(rec);
        ext->objects = eina_list_append(ext->objects, rec);
    }

    evas_object_size_hint_min_set(grid, w * CORE_TILE_SIZE, rows * CORE_TILE_SIZE);
    elm_box_pack_end(parent_bx, container);

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
    elm_object_text_set(lb, _("<align=left>Size shows activity level (frequency when available, otherwise load). "
                              "Color and the 0%-100% legend show CPU usage percentage.</align>"));
    evas_object_size_hint_align_set(lb, 0.0, 0.5);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    elm_object_content_set(desc_fr, lb);
    evas_object_show(lb);

    pd->thread = ecore_thread_feedback_run(_core_times_main_cb, _core_times_feedback_cb, NULL, NULL, pd, 1);
    return pd;
}
