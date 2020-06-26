#include "ui_memory.h"

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

void
ui_tab_memory_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *pb;
   Evas_Object *scroller, *border, *rect, *label, *table;

   parent = ui->content;

   ui->mem_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_table_pack(ui->content, ui->mem_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->mem_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   elm_table_padding_set(table, 0, 20 * elm_config_scale_get());
   evas_object_show(table);

   label = _label_mem(box, _("Used"));
   ui->progress_mem_used = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 0, 1, 1);
   elm_table_pack(table, pb, 1, 0, 1, 1);

   label = _label_mem(box, _("Cached"));
   ui->progress_mem_cached = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 1, 1, 1);
   elm_table_pack(table, pb, 1, 1, 1, 1);

   label = _label_mem(box, _("Buffered"));
   ui->progress_mem_buffered = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 2, 1, 1);
   elm_table_pack(table, pb, 1, 2, 1, 1);

   label = _label_mem(box, _("Shared"));
   ui->progress_mem_shared = pb = _progress_add(table);
   elm_table_pack(table, label, 0, 3, 1, 1);
   elm_table_pack(table, pb, 1, 3, 1, 1);

   label = _label_mem(box, _("Swapped"));
   ui->progress_mem_swap = pb = _progress_add(frame);
   elm_table_pack(table, label, 0, 4, 1, 1);
   elm_table_pack(table, pb, 1, 4, 1, 1);

   border = elm_frame_add(parent);
   elm_object_style_set(border, "pad_small");
   evas_object_size_hint_weight_set(border, EXPAND, EXPAND);
   evas_object_size_hint_align_set(border, FILL, FILL);
   evas_object_show(border);
   elm_object_content_set(border, table);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);

   rect = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_size_hint_max_set(rect, MISC_MAX_WIDTH, -1);
   evas_object_size_hint_min_set(rect, MISC_MIN_WIDTH, 1);

   elm_table_pack(table, rect, 0, 0, 1, 1);
   elm_table_pack(table, border, 0, 0, 1, 1);

   elm_object_content_set(scroller, table);
   elm_object_content_set(frame, scroller);
   elm_box_pack_end(ui->mem_view, frame);
}

void
ui_tab_memory_update(Ui *ui, Sys_Info *info)
{
   Evas_Object *pb;
   double ratio, value;

   if (!ui->mem_visible)
     return;

   if (ui->zfs_mounted)
     info->memory.used += info->memory.zfs_arc_used;

   pb = ui->progress_mem_used;
   ratio = info->memory.total / 100.0;
   value = info->memory.used / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.used),
                   evisum_size_format(info->memory.total)));

   pb = ui->progress_mem_cached;
   ratio = info->memory.total / 100.0;
   value = info->memory.cached / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.cached),
                   evisum_size_format(info->memory.total)));

   pb = ui->progress_mem_buffered;
   ratio = info->memory.total / 100.0;
   value = info->memory.buffered / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.buffered),
                   evisum_size_format(info->memory.total)));

   pb = ui->progress_mem_shared;
   ratio = info->memory.total / 100.0;
   value = info->memory.shared / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.shared),
                   evisum_size_format(info->memory.total)));

   pb = ui->progress_mem_swap;
   if (info->memory.swap_total)
     {
        ratio = info->memory.swap_total / 100.0;
        value = info->memory.swap_used / ratio;
     }
   else value = 0.0;

   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(info->memory.swap_used),
                   evisum_size_format(info->memory.swap_total)));
}

