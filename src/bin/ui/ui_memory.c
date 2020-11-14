#include "ui_memory.h"
#include "system/filesystems.h"

typedef struct  {
   Evas_Object  *used;
   Evas_Object  *cached;
   Evas_Object  *buffered;
   Evas_Object  *shared;
   Evas_Object  *swap;
   Evas_Object  *video[MEM_VIDEO_CARD_MAX];
} Widgets;

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, 0, EXPAND);
   evas_object_size_hint_align_set(label, FILL, FILL);
   elm_object_text_set(label, eina_slstr_printf("%s",text));
   evas_object_show(label);

   return label;
}

static Evas_Object *
_progress_add(Evas_Object *parent)
{
   Evas_Object *pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   evas_object_show(pb);

   return pb;
}

static Eina_Bool
_memory_update(void *data)
{
   Widgets *widgets;
   Evas_Object *pb;
   double ratio, value;
   meminfo_t memory;
   int i;

   memset(&memory, 0, sizeof(memory));
   system_memory_usage_get(&memory);

   widgets = data;

   if (file_system_in_use("ZFS"))
     memory.used += memory.zfs_arc_used;

   pb = widgets->used;
   ratio = memory.total / 100.0;
   value = memory.used / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory.used),
                   evisum_size_format(memory.total)));

   pb = widgets->cached;
   ratio = memory.total / 100.0;
   value = memory.cached / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory.cached),
                   evisum_size_format(memory.total)));

   pb = widgets->buffered;
   ratio = memory.total / 100.0;
   value = memory.buffered / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory.buffered),
                   evisum_size_format(memory.total)));

   pb = widgets->shared;
   ratio = memory.total / 100.0;
   value = memory.shared / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory.shared),
                   evisum_size_format(memory.total)));

   pb = widgets->swap;
   if (memory.swap_total)
     {
        elm_object_disabled_set(pb, EINA_FALSE);
        ratio = memory.swap_total / 100.0;
        value = memory.swap_used / ratio;
     }
   else
     {
        elm_object_disabled_set(pb, EINA_TRUE);
        value = 0.0;
     }

   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory.swap_used),
                   evisum_size_format(memory.swap_total)));

   for (i = 0; i < memory.video_count; i++)
     {
        pb = widgets->video[i];
        if (!pb) break;
        if (memory.video[i].total) elm_object_disabled_set(pb, EINA_FALSE);
        else elm_object_disabled_set(pb, EINA_TRUE);
        ratio = memory.video[i].total / 100.0;
        value = memory.video[i].used / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        evisum_size_format(memory.video[i].used),
                        evisum_size_format(memory.video[i].total)));
     }
   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Widgets *widgets;
   Ui *ui = data;

   ecore_timer_del(ui->mem.timer);
   ui->mem.timer = NULL;

   widgets = evas_object_data_get(obj, "widgets");
   if (widgets) free(widgets);

   evas_object_del(obj);
   ui->mem.win = NULL;
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   evisum_ui_config_save(ui);
}

void
ui_win_memory_add(Ui *ui)
{
   Evas_Object *win, *frame, *pb;
   Evas_Object *border, *rect, *label, *table;
   int i;
   Evas_Coord x = 0, y = 0;
   meminfo_t memory;

   if (ui->mem.win)
     {
        elm_win_raise(ui->mem.win);
        return;
     }

   Widgets *widgets = calloc(1, sizeof(Widgets));
   if (!widgets) return;

   memset(&memory, 0, sizeof(memory));
   system_memory_usage_get(&memory);

   ui->mem.win = win = elm_win_util_standard_add("evisum", _("Memory Usage"));
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, (evisum_ui_effects_enabled_get() ||
                                   evisum_ui_backgrounds_enabled_get()));
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, ui);

   frame = elm_frame_add(win);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   table = elm_table_add(win);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   elm_table_padding_set(table, 8, 2);
   evas_object_show(table);

   label = _label_mem(table, _("Used"));
   widgets->used = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 0, 1, 1);
   elm_table_pack(table, pb, 1, 0, 1, 1);

   label = _label_mem(table, _("Cached"));
   widgets->cached = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 1, 1, 1);
   elm_table_pack(table, pb, 1, 1, 1, 1);

   label = _label_mem(table, _("Buffered"));
   widgets->buffered = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 2, 1, 1);
   elm_table_pack(table, pb, 1, 2, 1, 1);

   label = _label_mem(table, _("Shared"));
   widgets->shared = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 3, 1, 1);
   elm_table_pack(table, pb, 1, 3, 1, 1);

   label = _label_mem(table, _("Swapped"));
   widgets->swap = pb = _progress_add(frame);
   elm_table_pack(table, label, 0, 4, 1, 1);
   elm_table_pack(table, pb, 1, 4, 1, 1);

   for (i = 0; i < memory.video_count; i++)
     {
        label = _label_mem(table, _("Video"));
        widgets->video[i] = pb = _progress_add(frame);
        elm_table_pack(table, label, 0, 5 + i, 1, 1);
        elm_table_pack(table, pb, 1, 5 + i, 1, 1);
     }

   border = elm_frame_add(win);
   elm_object_style_set(border, "pad_small");
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   evas_object_show(border);
   elm_object_content_set(border, table);

   table = elm_table_add(win);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);

   rect = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_min_set(rect, MISC_MIN_WIDTH, 1);

   elm_table_pack(table, rect, 0, 0, 1, 1);
   elm_table_pack(table, border, 0, 0, 1, 1);

   elm_object_content_set(frame, table);
   elm_object_content_set(win, frame);

   evas_object_data_set(win, "widgets", widgets);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, ui);

   if (ui->mem.width > 0 && ui->mem.height > 0)
     evas_object_resize(win, ui->mem.width, ui->mem.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH , UI_CHILD_WIN_HEIGHT / 2);

   if (ui->win)
     evas_object_geometry_get(ui->win, &x, &y, NULL, NULL);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 20);
   else
     elm_win_center(win, 1, 1);

   evas_object_show(win);

   evas_object_show(win);

   _memory_update(widgets);

   ui->mem.timer = ecore_timer_add(ui->settings.poll_delay, _memory_update, widgets);
}
