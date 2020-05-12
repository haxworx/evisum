#include "ui_disk.h"
#include "../system/disks.h"

void
ui_tab_disk_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->disk_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(ui->content, ui->disk_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->disk_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.5);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, 0.5);
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
}

static void
_ui_disk_add(Ui *ui, const char *path, const char *mount, unsigned long total, unsigned long used)
{
   Evas_Object *box, *frame, *progress, *label;
   double ratio, value;

   box = elm_box_add(ui->disk_activity);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_show(box);

   label = elm_label_add(box);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_text_set(label,
                      eina_slstr_printf(_("<subtitle>%s</subtitle><br><bigger>mounted at %s</bigger><br>%s of %s"),
                      path, mount, evisum_size_format(used), evisum_size_format(total)));
   evas_object_show(label);
   elm_box_pack_end(box, label);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(progress, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   ratio = total / 100.0;
   value = used / ratio;

   if (used == 0 && total == 0)
     elm_progressbar_value_set(progress, 1.0);
   else
     elm_progressbar_value_set(progress, value / 100.0);

   frame = elm_frame_add(ui->misc_activity);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set(frame, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_object_style_set(frame, "pad_large");
   evas_object_show(frame);

   elm_box_pack_end(box, progress);
   elm_object_content_set(frame, box);
   elm_box_pack_end(ui->disk_activity, frame);
}

void
ui_tab_disk_update(Ui *ui)
{
   Eina_List *disks;
   char *path;
   unsigned long total, used;

   if (!ui->disk_visible)
     return;

   elm_box_clear(ui->disk_activity);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        char *mount = disk_mount_point_get(path);
        if (mount)
          {
             if (disk_usage_get(mount, &total, &used))
               {
                  _ui_disk_add(ui, path, mount, total, used);
               }
             free(mount);
          }
        free(path);
     }
   if (disks)
     free(disks);
}

