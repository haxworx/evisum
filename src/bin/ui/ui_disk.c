#include "ui_disk.h"
#include "../system/disks.h"

static char *
_file_system_usage_format(File_System *inf)
{
   return strdup(eina_slstr_printf("%s / %s",
               evisum_size_format(inf->usage.used),
               evisum_size_format(inf->usage.total)));
}

static void
_separator_add(Evas_Object *box)
{
   Evas_Object *hbox, *sep;

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_show(hbox);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_show(sep);

   elm_box_pack_end(hbox, sep);
   elm_box_pack_end(box, hbox);
}

static void
_ui_disk_add(Ui *ui, File_System *inf)
{
   Evas_Object *frame, *vbox, *hbox, *pb, *ic, *label;
   Evas_Object *parent;
   const char *type;
   char *usage;
   double ratio, value;

   type = inf->type_name;
   if (!type)
     type = file_system_name_by_id(inf->type);
   if (!type) type = "unknown";

   parent = ui->disk_activity;

   frame = elm_frame_add(parent);
   evas_object_size_hint_align_set(frame, FILL, 0);
   evas_object_size_hint_weight_set(frame, EXPAND, 0);
   elm_object_style_set(frame, "pad_huge");
   evas_object_show(frame);

   vbox = elm_box_add(parent);
   evas_object_size_hint_align_set(vbox, FILL, FILL);
   evas_object_size_hint_weight_set(vbox, EXPAND, EXPAND);
   evas_object_show(vbox);

   label = elm_label_add(parent);
   evas_object_size_hint_align_set(label, 1.0, FILL);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   evas_object_show(label);
   elm_box_pack_end(vbox, label);

   elm_object_text_set(label, eina_slstr_printf("%s",
                   inf->mount));

   hbox = elm_box_add(parent);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_show(hbox);

   ic = elm_image_add(parent);
   elm_image_file_set(ic, evisum_icon_path_get("mount"), NULL);
   evas_object_size_hint_min_set(ic, 32 * elm_config_scale_get(),
                   32 * elm_config_scale_get());
   evas_object_show(ic);
   elm_box_pack_end(hbox, ic);


   pb = elm_progressbar_add(frame);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);
   evas_object_show(pb);

   usage = _file_system_usage_format(inf);
   if (usage)
     {
        elm_progressbar_unit_format_set(pb, usage);
        free(usage);
     }

   ratio = inf->usage.total / 100.0;
   value = inf->usage.used / ratio;

   if (inf->usage.used == 0 && inf->usage.total == 0)
     elm_progressbar_value_set(pb, 1.0);
   else
     elm_progressbar_value_set(pb, value / 100.0);

   label = elm_label_add(parent);
   evas_object_size_hint_align_set(label, 1.0, FILL);
   evas_object_size_hint_weight_set(label, EXPAND, EXPAND);
   evas_object_show(label);

   elm_object_text_set(label,
                   eina_slstr_printf("%s <b>(%s)</b>",
                   inf->path, type));

   elm_box_pack_end(vbox, label);
   elm_box_pack_end(hbox, pb);
   elm_box_pack_end(vbox, hbox);
   _separator_add(vbox);

   elm_object_content_set(frame, vbox);
   elm_box_pack_end(ui->disk_activity, frame);
}

static Eina_Bool
_disk_update(void *data)
{
   Ui *ui;
   Eina_List *disks;
   char *path;
   Eina_Bool zfs_mounted = EINA_FALSE;

   ui = data;

   elm_box_clear(ui->disk_activity);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        File_System *fs = file_system_info_get(path);
        if (fs)
          {
             if (fs->type == file_system_id_by_name("ZFS"))
               zfs_mounted = EINA_TRUE;

             _ui_disk_add(ui, fs);
             file_system_info_free(fs);
          }
        free(path);
     }

   ui->zfs_mounted = zfs_mounted;

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->timer_disk)
     ecore_timer_del(ui->timer_disk);
   ui->timer_disk = NULL;

   ui->disk_visible = EINA_FALSE;
   evas_object_del(obj);
}

void
ui_win_disk_add(Ui *ui)
{
   Evas_Object *win, *box, *vbox, *scroller;
   Evas_Object *table, *rect;

   if (ui->disk_visible) return;
   ui->disk_visible = EINA_TRUE;

   win = elm_win_util_standard_add("evisum", _("Storage"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);

   ui->disk_activity = vbox = elm_box_add(win);
   evas_object_size_hint_weight_set(vbox, EXPAND, 0.0);
   evas_object_size_hint_align_set(vbox, FILL, 0.5);
   evas_object_show(vbox);

   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   table = elm_table_add(win);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);

   rect = evas_object_rectangle_add(evas_object_rectangle_add(win));
   evas_object_size_hint_max_set(rect, MISC_MAX_WIDTH, -1);
   evas_object_size_hint_min_set(rect, MISC_MIN_WIDTH, 1);

   elm_table_pack(table, rect, 0, 0, 1, 1);
   elm_table_pack(table, vbox, 0, 0, 1, 1);

   elm_object_content_set(scroller, table);
   elm_box_pack_end(box, scroller);
   elm_object_content_set(win, box);

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evisum_child_window_show(ui->win, win);
   _disk_update(ui);

   ui->timer_disk = ecore_timer_add(3.0, _disk_update, ui);
}

