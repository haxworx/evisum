#include "ui_memory.h"

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, 0.1, EXPAND);
   evas_object_size_hint_align_set(label, FILL, FILL);
   elm_object_text_set(label, eina_slstr_printf("<bigger>%s</bigger>",text));
   evas_object_show(label);

   return label;
}

static Evas_Object *
_progress_add(Evas_Object *parent)
{
   Evas_Object *progress = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(progress, FILL, FILL);
   evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   return progress;
}

void
ui_tab_memory_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *progress, *scroller;
   Evas_Object *label, *table;

   parent = ui->content;

   ui->mem_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_table_pack(ui->content, ui->mem_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->mem_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0);
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
   elm_object_content_set(scroller, hbox);
   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, FILL, 0);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_medium");
   elm_box_pack_end(box, frame);

   ui->title_mem = label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   evas_object_size_hint_align_set(label, FILL, FILL);
   elm_object_text_set(label, _("<subtitle>Memory</subtitle>"));
   evas_object_show(label);
   elm_box_pack_end(box, label);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, FILL, 0);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_large");
   elm_box_pack_end(box, frame);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   elm_table_padding_set(table, 0, 20 * elm_config_scale_get());
   evas_object_show(table);

   label = _label_mem(box, _("Used"));
   ui->progress_mem_used = progress = _progress_add(table);
   elm_table_pack(table, label, 0, 0, 1, 1);
   elm_table_pack(table, progress, 1, 0, 1, 1);

   label = _label_mem(box, _("Cached"));
   ui->progress_mem_cached = progress = _progress_add(table);
   elm_table_pack(table, label, 0, 1, 1, 1);
   elm_table_pack(table, progress, 1, 1, 1, 1);

   label = _label_mem(box, _("Buffered"));
   ui->progress_mem_buffered = progress = _progress_add(table);
   elm_table_pack(table, label, 0, 2, 1, 1);
   elm_table_pack(table, progress, 1, 2, 1, 1);

   label = _label_mem(box, _("Shared"));
   ui->progress_mem_shared = progress = _progress_add(table);
   elm_table_pack(table, label, 0, 3, 1, 1);
   elm_table_pack(table, progress, 1, 3, 1, 1);

   label = _label_mem(box, _("Swapped"));
   ui->progress_mem_swap = progress = _progress_add(frame);
   elm_table_pack(table, label, 0, 4, 1, 1);
   elm_table_pack(table, progress, 1, 4, 1, 1);

   frame = elm_frame_add(ui->mem_activity);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   elm_object_style_set(frame, "pad_large");
   evas_object_show(frame);

   elm_box_pack_end(box, table);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->mem_activity, frame);
}

void
ui_tab_memory_update(Ui *ui, Sys_Info *sysinfo)
{
   Evas_Object *progress;
   double ratio, value;

   if (!ui->mem_visible)
     return;

   if (ui->zfs_mounted)
     sysinfo->memory.used += sysinfo->memory.zfs_arc_used;

   progress = ui->progress_mem_used;
   ratio = sysinfo->memory.total / 100.0;
   value = sysinfo->memory.used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress,
                   eina_slstr_printf("%s / %s (%1.0f &#37;)",
                   evisum_size_format(sysinfo->memory.used),
                   evisum_size_format(sysinfo->memory.total),
                   value));

   progress = ui->progress_mem_cached;
   ratio = sysinfo->memory.total / 100.0;
   value = sysinfo->memory.cached / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress,
                   eina_slstr_printf("%s / %s (%1.0f &#37;)",
                   evisum_size_format(sysinfo->memory.cached),
                   evisum_size_format(sysinfo->memory.total),
                   value));

   progress = ui->progress_mem_buffered;
   ratio = sysinfo->memory.total / 100.0;
   value = sysinfo->memory.buffered / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress,
                   eina_slstr_printf("%s / %s (%1.0f &#37;)",
                   evisum_size_format(sysinfo->memory.buffered),
                   evisum_size_format(sysinfo->memory.total),
                   value));

   progress = ui->progress_mem_shared;
   ratio = sysinfo->memory.total / 100.0;
   value = sysinfo->memory.shared / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress,
                   eina_slstr_printf("%s / %s (%1.0f &#37;)",
                   evisum_size_format(sysinfo->memory.shared),
                   evisum_size_format(sysinfo->memory.total),
                   value));

   progress = ui->progress_mem_swap;
   if (sysinfo->memory.swap_total)
     {
        ratio = sysinfo->memory.swap_total / 100.0;
        value = sysinfo->memory.swap_used / ratio;
     }
   else value = 0.0;

   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress,
                   eina_slstr_printf("%s / %s (%1.0f &#37;)",
                   evisum_size_format(sysinfo->memory.swap_used),
                   evisum_size_format(sysinfo->memory.swap_total),
                   value));
}

