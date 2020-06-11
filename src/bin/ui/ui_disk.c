#include "ui_disk.h"
#include "../system/disks.h"

void
ui_tab_disk_add(Ui *ui)
{
   Evas_Object *parent, *box, *hbox, *frame, *scroller;

   parent = ui->content;

   ui->disk_view = box = elm_box_add(parent);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   elm_table_pack(ui->content, ui->disk_view, 0, 1, 1, 1);
   evas_object_hide(box);

   ui->disk_activity = hbox = elm_box_add(box);
   evas_object_size_hint_weight_set(hbox, EXPAND, 0.5);
   evas_object_size_hint_align_set(hbox, FILL, 0.5);
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
}

static void
_ui_disk_add(Ui *ui, Filesystem_Info *inf)
{
   Evas_Object *box, *frame, *progress, *label;
   const char *type;
   double ratio, value;

   type = inf->type_name;
   if (!type)
     type = filesystem_name_by_id(inf->type);
   if (!type) type = "unknown";

   box = elm_box_add(ui->disk_activity);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_show(box);

   label = elm_label_add(box);
   evas_object_size_hint_align_set(label, FILL, FILL);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   elm_object_text_set(label,
                   eina_slstr_printf(_(
                   "<subtitle>%s</subtitle><br><bigger>mounted at %s (%s)</bigger>"
                   "<br>%s of %s"),
                   inf->path, inf->mount, type, evisum_size_format(inf->usage.used),
                   evisum_size_format(inf->usage.total)));
   evas_object_show(label);
   elm_box_pack_end(box, label);

   progress = elm_progressbar_add(box);
   evas_object_size_hint_align_set(progress, FILL, FILL);
   evas_object_size_hint_weight_set(progress, EXPAND, EXPAND);
   elm_progressbar_span_size_set(progress, 1.0);
   evas_object_show(progress);

   ratio = inf->usage.total / 100.0;
   value = inf->usage.used / ratio;

   if (inf->usage.used == 0 && inf->usage.total == 0)
     elm_progressbar_value_set(progress, 1.0);
   else
     elm_progressbar_value_set(progress, value / 100.0);

   frame = elm_frame_add(ui->misc_activity);
   evas_object_size_hint_align_set(frame, FILL, FILL);
   evas_object_size_hint_weight_set(frame, EXPAND, EXPAND);
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
   Eina_Bool zfs_mounted = EINA_FALSE;

   if (!ui->disk_visible)
     return;

   // FIXME
   elm_box_clear(ui->disk_activity);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        Filesystem_Info *fs = filesystem_info_get(path);
        if (fs)
          {
             // ZFS usage may have changed during application's lifetime.
             // Keep track of ZFS mounts else we may report bogus use
             // of memory.
             if (fs->type == filesystem_id_by_name("ZFS"))
               zfs_mounted = EINA_TRUE;

             _ui_disk_add(ui, fs);
             filesystem_info_free(fs);
          }
        free(path);
     }

   // Need to keep track of ZFS mounts. If we have no mounts
   // then memory usage should not take into account the ZFS
   // ARC memory allocation.
   if (!zfs_mounted) ui->zfs_mounted = EINA_FALSE;
}

