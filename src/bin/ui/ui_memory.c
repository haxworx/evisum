#include "ui_memory.h"

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, 0.1, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, eina_slstr_printf("<bigger>%s</bigger>",text));
   evas_object_show(label);

   return label;
}

void
ui_tab_memory_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *progress, *scroller;
   Evas_Object *label, *table;

   parent = ui->content;

   ui->mem_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->mem_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->mem_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(hbox);

   frame = elm_frame_add(box);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_small");
   evas_object_show(frame);

   scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(scroller, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_scroller_policy_set(scroller, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);
   elm_object_content_set(scroller, hbox);
   elm_object_content_set(frame, scroller);
   elm_box_pack_end(box, frame);

   box = elm_box_add(ui->content);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_medium");
   elm_box_pack_end(box, frame);

   ui->title_mem = label = elm_label_add(parent);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(label, _("<subtitle>Memory</subtitle>"));
   evas_object_show(label);
   elm_box_pack_end(box, label);

   frame = elm_frame_add(box);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(frame);
   elm_object_style_set(frame, "pad_large");
   elm_box_pack_end(box, frame);

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_padding_set(table, 0, 20 * elm_config_scale_get());
   evas_object_show(table);

   label = _label_mem(box, _("Used"));

   ui->progress_mem_used = progress = elm_progressbar_add(table);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 0, 1, 1);
   elm_table_pack(table, progress, 1, 0, 1, 1);

   label = _label_mem(box, _("Cached"));

   ui->progress_mem_cached = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 1, 1, 1);
   elm_table_pack(table, progress, 1, 1, 1, 1);

   label = _label_mem(box, _("Buffered"));

   ui->progress_mem_buffered = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 2, 1, 1);
   elm_table_pack(table, progress, 1, 2, 1, 1);

   label = _label_mem(box, _("Shared"));

   ui->progress_mem_shared = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 3, 1, 1);
   elm_table_pack(table, progress, 1, 3, 1, 1);

   label = _label_mem(box, _("Swapped"));

   ui->progress_mem_swap = progress = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(progress);

   elm_table_pack(table, label, 0, 4, 1, 1);
   elm_table_pack(table, progress, 1, 4, 1, 1);

   frame = elm_frame_add(ui->mem_activity);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_style_set(frame, "pad_large");
   evas_object_show(frame);

   elm_box_pack_end(box, table);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->mem_activity, frame);
}

void
ui_tab_memory_update(Ui *ui, results_t *results)
{
   Evas_Object *progress;
   double ratio, value;

   if (!ui->mem_visible)
     return;

   progress = ui->progress_mem_used;
   ratio = results->memory.total / 100.0;
   value = results->memory.used / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s (%1.0f &#37;)",
                                   evisum_size_format(results->memory.used << 10),
                                   evisum_size_format(results->memory.total << 10),
                                   value));

   progress = ui->progress_mem_cached;
   ratio = results->memory.total / 100.0;
   value = results->memory.cached / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s (%1.0f &#37;)",
                                   evisum_size_format(results->memory.cached << 10),
                                   evisum_size_format(results->memory.total << 10),
                                   value));

   progress = ui->progress_mem_buffered;
   ratio = results->memory.total / 100.0;
   value = results->memory.buffered / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s (%1.0f &#37;)",
                                   evisum_size_format(results->memory.buffered << 10),
                                   evisum_size_format(results->memory.total << 10),
                                   value));

   progress = ui->progress_mem_shared;
   ratio = results->memory.total / 100.0;
   value = results->memory.shared / ratio;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s (%1.0f &#37;)",
                                   evisum_size_format(results->memory.shared << 10),
                                   evisum_size_format(results->memory.total << 10),
                                   value));

   progress = ui->progress_mem_swap;
   if (results->memory.swap_total)
     {
        ratio = results->memory.swap_total / 100.0;
        value = results->memory.swap_used / ratio;
     }
   else value = 0.0;
   elm_progressbar_value_set(progress, value / 100);
   elm_progressbar_unit_format_set(progress, eina_slstr_printf("%s / %s (%1.0f &#37;)",
                                   evisum_size_format(results->memory.swap_used << 10),
                                   evisum_size_format(results->memory.swap_total << 10),
                                   value));
}

