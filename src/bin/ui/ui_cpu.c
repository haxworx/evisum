#include "ui_cpu.h"

void
ui_tab_cpu_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *label, *scroller;
   Evas_Object *progress;
   unsigned int cpu_count;

   parent = ui->content;

   ui->cpu_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->cpu_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->cpu_activity = hbox = elm_box_add(box);
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

   cpu_count = system_cpu_online_count_get();

   for (int i = 0; i < cpu_count; i++)
     {
        if (i == 0)
          {
             frame = elm_frame_add(box);
             evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
             evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_show(frame);
             elm_object_style_set(frame, "pad_large");
             elm_box_pack_end(box, frame);

             label = elm_label_add(box);
             evas_object_size_hint_align_set(label, EVAS_HINT_FILL, 0);
             evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             elm_object_text_set(label, eina_slstr_printf(_("<subtitle>CPUs</subtitle><br><bigger>Total of %d CPUs</bigger>"), cpu_count));
             evas_object_show(label);
             elm_box_pack_end(box, label);

             ui->temp_label = label = elm_label_add(box);
             evas_object_size_hint_align_set(label, EVAS_HINT_FILL, 0);
             evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_show(label);
             elm_box_pack_end(box, label);
          }

        frame = elm_frame_add(box);
        evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, 0);
        evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_show(frame);
        elm_object_style_set(frame, "pad_large");

        progress = elm_progressbar_add(frame);
        evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        elm_progressbar_span_size_set(progress, 1.0);
        elm_progressbar_unit_format_set(progress, "%1.2f%%");
        evas_object_show(progress);
        elm_progressbar_value_set(progress, 0.0);

        elm_object_content_set(frame, progress);
        elm_box_pack_end(box, frame);

        ui->cpu_list = eina_list_append(ui->cpu_list, progress);
     }

   elm_box_pack_end(ui->cpu_activity, box);
}

void
ui_tab_cpu_update(Ui *ui, Sys_Info *sysinfo)
{
   Eina_List *l;
   Evas_Object *pb;
   int i = 0;

   if (!ui->cpu_visible)
     return;

   if (sysinfo->temperature != INVALID_TEMP)
     elm_object_text_set(ui->temp_label, eina_slstr_printf(_("Core at (%d °C)"), sysinfo->temperature));

   EINA_LIST_FOREACH(ui->cpu_list, l, pb)
     {
        elm_progressbar_value_set(pb, sysinfo->cores[i]->percent / 100);
        ++i;
     }
}

