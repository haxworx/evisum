#include "evisum_ui_memory.h"
#include "evisum_ui_graph.h"
#include "evisum_ui_colors.h"
#include "system/filesystems.h"
#include "config.h"

#include <Elementary.h>

typedef struct
{
   char key[64];
   char name[64];

   uint8_t color_r;
   uint8_t color_g;
   uint8_t color_b;

   double history[120];
   int    history_count;

   uint64_t used;
   uint64_t total;
   Eina_Bool enabled;

   Evas_Object *legend_row;
   Evas_Object *legend_label;
   Evas_Object *legend_pb;
} Memory_Series;

typedef struct
{
   Ecore_Thread *thread;
   Evas_Object  *win;
   Evas_Object  *main_menu;
   Elm_Layout   *btn_menu;
   Evas_Object  *graph_bg;
   Evas_Object  *graph_img;
   Evas_Object  *legend_tb;
   Eina_Bool     btn_visible;

   Memory_Series series[5 + MEM_VIDEO_CARD_MAX];
   int           series_count;

   Evisum_Ui    *ui;
} Win_Data;

enum
{
   SERIES_USED = 0,
   SERIES_CACHED,
   SERIES_BUFFERED,
   SERIES_SHARED,
   SERIES_SWAP,
   SERIES_VIDEO_BASE
};

#define MEM_GRAPH_SAMPLES 120
#define WIN_WIDTH 420
#define WIN_HEIGHT 360

static const Evisum_Ui_Graph_Layer _mem_layers[] = {
   { -0.6, 0.24 },
   {  0.6, 0.24 },
   {  0.0, 0.92 },
};

static void
_legend_repack(Win_Data *wd)
{
   int row = 0;

   if (!wd->legend_tb)
     return;

   elm_table_clear(wd->legend_tb, 0);

   for (int i = 0; i < wd->series_count; i++)
     {
        Memory_Series *entry = &wd->series[i];

        if (!entry->enabled || !entry->legend_row || !entry->legend_pb)
          {
             if (entry->legend_row) evas_object_hide(entry->legend_row);
             if (entry->legend_pb) evas_object_hide(entry->legend_pb);
             continue;
          }

        elm_table_pack(wd->legend_tb, entry->legend_row, 0, row, 1, 1);
        elm_table_pack(wd->legend_tb, entry->legend_pb, 1, row, 1, 1);
        evas_object_show(entry->legend_row);
        evas_object_show(entry->legend_pb);
        row++;
     }
}

static void
_series_history_add(Memory_Series *entry, double value)
{
   if (entry->history_count < MEM_GRAPH_SAMPLES)
     entry->history[entry->history_count++] = value;
   else
     {
        memmove(&entry->history[0], &entry->history[1],
                sizeof(double) * (MEM_GRAPH_SAMPLES - 1));
        entry->history[MEM_GRAPH_SAMPLES - 1] = value;
     }
}

static void
_series_legend_add(Win_Data *wd, Memory_Series *entry)
{
   Evas_Object *left, *swatch, *lb, *pb;
   Evas *evas;

   if (!wd->legend_tb || entry->legend_row)
     return;

   left = elm_box_add(wd->legend_tb);
   elm_box_horizontal_set(left, EINA_TRUE);
   elm_box_padding_set(left, 4 * elm_config_scale_get(), 0);
   evas_object_size_hint_weight_set(left, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(left, EVAS_HINT_FILL, 0.5);
   evas_object_show(left);

   evas = evas_object_evas_get(left);
   swatch = evas_object_rectangle_add(evas);
   evas_object_color_set(swatch, entry->color_r, entry->color_g, entry->color_b, 255);
   evas_object_size_hint_min_set(swatch,
                                 12 * elm_config_scale_get(),
                                 12 * elm_config_scale_get());
   evas_object_size_hint_align_set(swatch, 0.0, 0.5);
   elm_box_pack_end(left, swatch);
   evas_object_show(swatch);

   lb = elm_label_add(left);
   evas_object_size_hint_weight_set(lb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(lb, 0.0, 0.5);
   elm_object_text_set(lb, entry->name);
   elm_box_pack_end(left, lb);
   evas_object_show(lb);

   pb = elm_progressbar_add(wd->legend_tb);
   elm_object_text_set(pb, NULL);
   elm_progressbar_span_size_set(pb, ELM_SCALE_SIZE(220));
   elm_progressbar_unit_format_set(pb, "0 B / 0 B");
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
   evas_object_show(pb);

   entry->legend_row = left;
   entry->legend_label = lb;
   entry->legend_pb = pb;
}

static void
_series_legend_update(Memory_Series *entry)
{
   double value = 0.0;

   if (!entry->legend_pb || !entry->legend_label)
     return;

   elm_object_text_set(entry->legend_label, entry->name);

   if (entry->total)
     {
        value = (double) entry->used / (double) entry->total;
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;
        elm_object_disabled_set(entry->legend_pb, 0);
     }
   else
     {
        elm_object_disabled_set(entry->legend_pb, 1);
     }

   elm_progressbar_value_set(entry->legend_pb, value);
   elm_progressbar_unit_format_set(entry->legend_pb,
                                   eina_slstr_printf("%s / %s",
                                                     evisum_size_format(entry->used, 0),
                                                     evisum_size_format(entry->total, 0)));
}

static void
_graph_redraw(Win_Data *wd)
{
   Evisum_Ui_Graph_Series series[5 + MEM_VIDEO_CARD_MAX];
   int nseries = 0;

   for (int i = 0; i < wd->series_count; i++)
     {
        Memory_Series *entry = &wd->series[i];

        if (!entry->enabled || (entry->history_count < 2))
          continue;

        series[nseries].history = entry->history;
        series[nseries].history_count = entry->history_count;
        series[nseries].color_r = entry->color_r;
        series[nseries].color_g = entry->color_g;
        series[nseries].color_b = entry->color_b;
        nseries++;
     }

   evisum_ui_graph_draw(wd->graph_bg, wd->graph_img,
                        MEM_GRAPH_SAMPLES, 100.0,
                        series, nseries,
                        _mem_layers, EINA_C_ARRAY_LENGTH(_mem_layers));
}

static void
_graph_bg_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   _graph_redraw(wd);
}

static void
_series_init(Win_Data *wd)
{
   Memory_Series *entry;

   wd->series_count = SERIES_VIDEO_BASE + MEM_VIDEO_CARD_MAX;

   entry = &wd->series[SERIES_USED];
   snprintf(entry->key, sizeof(entry->key), "mem_used");
   snprintf(entry->name, sizeof(entry->name), "%s", _("Used"));

   entry = &wd->series[SERIES_CACHED];
   snprintf(entry->key, sizeof(entry->key), "mem_cached");
   snprintf(entry->name, sizeof(entry->name), "%s", _("Cached"));

   entry = &wd->series[SERIES_BUFFERED];
   snprintf(entry->key, sizeof(entry->key), "mem_buffered");
   snprintf(entry->name, sizeof(entry->name), "%s", _("Buffered"));

   entry = &wd->series[SERIES_SHARED];
   snprintf(entry->key, sizeof(entry->key), "mem_shared");
   snprintf(entry->name, sizeof(entry->name), "%s", _("Shared"));

   entry = &wd->series[SERIES_SWAP];
   snprintf(entry->key, sizeof(entry->key), "mem_swap");
   snprintf(entry->name, sizeof(entry->name), "%s", _("Swapped"));

   for (int i = 0; i < MEM_VIDEO_CARD_MAX; i++)
     {
        entry = &wd->series[SERIES_VIDEO_BASE + i];
        snprintf(entry->key, sizeof(entry->key), "mem_video_%d", i);
        snprintf(entry->name, sizeof(entry->name), "%s %d", _("Video"), i + 1);
     }

   for (int i = 0; i < wd->series_count; i++)
     evisum_graph_color_get(wd->series[i].key,
                            &wd->series[i].color_r,
                            &wd->series[i].color_g,
                            &wd->series[i].color_b);
}

static void
_series_usage_set(Win_Data *wd, int idx, uint64_t used, uint64_t total, Eina_Bool enabled)
{
   Memory_Series *entry = &wd->series[idx];
   double used_percent = 0.0;

   entry->enabled = enabled;
   entry->used = used;
   entry->total = total;

   if (!enabled)
     return;

   if (total)
     {
        used_percent = ((double) used / (double) total) * 100.0;
        if (used_percent < 0.0) used_percent = 0.0;
        if (used_percent > 100.0) used_percent = 100.0;
     }

   _series_history_add(entry, used_percent);
   _series_legend_add(wd, entry);
   _series_legend_update(entry);
}

static void
_series_update_from_memory(Win_Data *wd, meminfo_t *memory)
{
   _series_usage_set(wd, SERIES_USED, memory->used, memory->total, EINA_TRUE);
   _series_usage_set(wd, SERIES_CACHED, memory->cached, memory->total, EINA_TRUE);
   _series_usage_set(wd, SERIES_BUFFERED, memory->buffered, memory->total, EINA_TRUE);
   _series_usage_set(wd, SERIES_SHARED, memory->shared, memory->total, EINA_TRUE);
   _series_usage_set(wd, SERIES_SWAP, memory->swap_used, memory->swap_total,
                     memory->swap_total > 0);

   for (int i = 0; i < MEM_VIDEO_CARD_MAX; i++)
     {
        if (i < (int) memory->video_count)
          {
             _series_usage_set(wd, SERIES_VIDEO_BASE + i,
                               memory->video[i].used,
                               memory->video[i].total,
                               memory->video[i].total > 0);
          }
        else
          {
             _series_usage_set(wd, SERIES_VIDEO_BASE + i, 0, 0, EINA_FALSE);
          }
     }

   _legend_repack(wd);
   _graph_redraw(wd);
}

static void
_mem_usage_main_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   static meminfo_t memory;

   ecore_thread_name_set(thread, "memory");

   while (!ecore_thread_check(thread))
     {
        memset(&memory, 0, sizeof(memory));
        system_memory_usage_get(&memory);
        if (file_system_in_use("ZFS"))
          memory.used += memory.zfs_arc_used;

        ecore_thread_feedback(thread, &memory);
        for (int i = 0; i < 8; i++)
          {
             if (ecore_thread_check(thread))
               break;
             usleep(125000);
          }
     }
}

static void
_mem_usage_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Win_Data *wd = data;
   meminfo_t *memory = msgdata;

   _series_update_from_memory(wd, memory);
}

static void
_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   wd->main_menu = NULL;
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (!wd->main_menu)
     {
        wd->main_menu = evisum_ui_main_menu_create(wd->ui, wd->win, obj);
        evas_object_smart_callback_add(wd->main_menu, "dismissed",
                                       _main_menu_dismissed_cb, wd);
     }
   else
     {
        evas_object_del(wd->main_menu);
        wd->main_menu = NULL;
     }
}

static void
_win_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Win_Data *wd = data;
   Evas_Event_Mouse_Move *ev = event_info;
   Evas_Coord gx, gy, gw, gh;
   Eina_Bool on_hotzone = EINA_FALSE;

   if (!wd->graph_bg || !wd->btn_menu || !ev)
     return;

   evas_object_geometry_get(wd->graph_bg, &gx, &gy, &gw, &gh);
   if ((ev->cur.canvas.x >= (gx + gw - 128)) &&
       (ev->cur.canvas.x <= (gx + gw)) &&
       (ev->cur.canvas.y >= gy) &&
       (ev->cur.canvas.y <= (gy + 128)))
     on_hotzone = EINA_TRUE;

   if (on_hotzone)
     {
        if (!wd->btn_visible)
          {
             elm_object_signal_emit(wd->btn_menu, "menu,show", "evisum/menu");
             wd->btn_visible = 1;
          }
     }
   else if ((wd->btn_visible) && (!wd->main_menu))
     {
        elm_object_signal_emit(wd->btn_menu, "menu,hide", "evisum/menu");
        wd->btn_visible = 0;
     }
}

static void
_win_key_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Win_Data *wd = data;
   Evas_Event_Key_Down *ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(wd->win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   evas_object_geometry_get(obj, &ui->mem.x, &ui->mem.y, NULL, NULL);
}

static void
_win_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   _graph_redraw(wd);
   evas_object_geometry_get(obj, NULL, NULL, &ui->mem.width, &ui->mem.height);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(wd->thread);
   ecore_thread_wait(wd->thread, 0.5);

   if (wd->main_menu)
     evas_object_del(wd->main_menu);

   ui->mem.win = NULL;
   free(wd);
}

void
evisum_ui_mem_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *tb, *graph_tb, *legend_fr;
   Evas_Object *btn, *ic;
   Elm_Layout *lay;
   Evas *evas;

   if (ui->mem.win)
     {
        elm_win_raise(ui->mem.win);
        return;
     }

   ui->mem.win = win = elm_win_util_standard_add("evisum", _("Memory Usage"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evas = evas_object_evas_get(win);

   Win_Data *wd = calloc(1, sizeof(Win_Data));
   if (!wd)
     {
        evas_object_del(win);
        ui->mem.win = NULL;
        return;
     }

   wd->win = win;
   wd->ui = ui;
   _series_init(wd);

   tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   graph_tb = elm_table_add(win);
   evas_object_size_hint_weight_set(graph_tb, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(graph_tb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(graph_tb, 420 * elm_config_scale_get(),
                                 260 * elm_config_scale_get());
   elm_table_pack(tb, graph_tb, 0, 0, 1, 1);
   evas_object_show(graph_tb);

   wd->graph_bg = evas_object_rectangle_add(evas);
   evas_object_color_set(wd->graph_bg, 32, 32, 32, 255);
   evas_object_size_hint_weight_set(wd->graph_bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(wd->graph_bg, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(graph_tb, wd->graph_bg, 0, 0, 1, 1);
   evas_object_show(wd->graph_bg);
   evas_object_event_callback_add(wd->graph_bg, EVAS_CALLBACK_RESIZE, _graph_bg_resize_cb, wd);
   evas_object_event_callback_add(wd->graph_bg, EVAS_CALLBACK_MOVE, _graph_bg_resize_cb, wd);

   wd->graph_img = evas_object_image_filled_add(evas);
   evas_object_image_alpha_set(wd->graph_img, EINA_FALSE);
   evas_object_size_hint_weight_set(wd->graph_img, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(wd->graph_img, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(graph_tb, wd->graph_img, 0, 0, 1, 1);
   evas_object_show(wd->graph_img);
   evas_object_stack_above(wd->graph_img, wd->graph_bg);

   btn = elm_button_add(win);
   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, "menu");
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);
   elm_object_focus_allow_set(btn, 0);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   evas_object_smart_callback_add(btn, "clicked", _btn_menu_clicked_cb, wd);

   wd->btn_menu = lay = elm_layout_add(win);
   evas_object_size_hint_weight_set(lay, 1.0, 1.0);
   evas_object_size_hint_align_set(lay, 0.99, 0.01);
   elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
   elm_layout_content_set(lay, "evisum/menu", btn);
   elm_table_pack(graph_tb, lay, 0, 0, 1, 1);
   evas_object_show(lay);

   legend_fr = elm_frame_add(win);
   elm_object_text_set(legend_fr, _("Memory Types"));
   evas_object_size_hint_weight_set(legend_fr, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(legend_fr, EVAS_HINT_FILL, 0.0);
   elm_table_pack(tb, legend_fr, 0, 1, 1, 1);
   evas_object_show(legend_fr);

   wd->legend_tb = elm_table_add(legend_fr);
   elm_table_padding_set(wd->legend_tb, 8 * elm_config_scale_get(),
                         2 * elm_config_scale_get());
   evas_object_size_hint_weight_set(wd->legend_tb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(wd->legend_tb, EVAS_HINT_FILL, 0.0);
   elm_object_content_set(legend_fr, wd->legend_tb);
   evas_object_show(wd->legend_tb);

   elm_object_content_set(win, tb);

   if ((ui->mem.width > 0) && (ui->mem.height > 0))
     evas_object_resize(win, ui->mem.width, ui->mem.height);
   else
     evas_object_resize(win, WIN_WIDTH, WIN_HEIGHT);

   if ((ui->mem.x > 0) && (ui->mem.y > 0))
     evas_object_move(win, ui->mem.x, ui->mem.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOUSE_MOVE, _win_mouse_move_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, wd);
   evas_object_show(win);

   wd->thread = ecore_thread_feedback_run(_mem_usage_main_cb,
                                          _mem_usage_feedback_cb,
                                          NULL,
                                          NULL,
                                          wd, 1);
}
