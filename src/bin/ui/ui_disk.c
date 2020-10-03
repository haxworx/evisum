#include "ui_disk.h"
#include "../system/disks.h"

Eina_Hash *_mounted;

typedef struct _Item_Disk
{
   Evas_Object *parent;
   Evas_Object *pb;
   Evas_Object *lbl;

   char        *path;
} Item_Disk;

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
_ui_item_disk_update(Item_Disk *item, File_System *inf)
{
   char *usage;
   double ratio, value;
   Evas_Object *pb = item->pb;

   usage = _file_system_usage_format(inf);
   if (usage)
     {
        elm_progressbar_unit_format_set(pb, usage);
        free(usage);
     }

   ratio = inf->usage.total / 100.0;
   value = inf->usage.used / ratio;

   if (inf->usage.used != inf->usage.total)
     elm_progressbar_value_set(pb, value / 100.0);
   else
     elm_progressbar_value_set(pb, 1.0);
}

static Item_Disk *
_ui_item_disk_add(Ui *ui, File_System *inf)
{
   Evas_Object *frame, *vbox, *hbox, *pb, *ic, *label;
   Evas_Object *parent;
   const char *type;

   type = inf->type_name;
   if (!type)
     type = file_system_name_by_id(inf->type);
   if (!type) type = "unknown";

   parent = ui->disk.box;

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

   Item_Disk *it = malloc(sizeof(Item_Disk));
   if (it)
     {
        it->parent = frame;
        it->pb = pb;
        it->lbl = label;
        it->path = strdup(inf->path);
        _ui_item_disk_update(it, inf);
     }
   return it;
}

static void
_hash_free_cb(void *data)
{
   Item_Disk *it = data;
   if (it->path)
     free(it->path);
   free(it);
}

static Eina_Bool
_ignore_path(char *path)
{
   if (!strcmp(path, "devfs"))
     return 1;
   return 0;
}

static Eina_Bool
_disks_poll_timer_cb(void *data)
{
   Ui *ui;
   Eina_List *disks;
   char *path;
   Item_Disk *item;
   File_System *fs;

   ui = data;

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        if (_ignore_path(path))
          {
             free(path);
             continue;
          }
        fs = file_system_info_get(path);
        if (fs)
          {
             if ((item = eina_hash_find(_mounted, eina_slstr_printf("%s:%s", fs->path, fs->mount))))
               _ui_item_disk_update(item, fs);
             else
               {
                  item = _ui_item_disk_add(ui, fs);
                  eina_hash_add(_mounted, eina_slstr_printf("%s:%s", fs->path, fs->mount), item);
                  elm_box_pack_end(ui->disk.box, item->parent);
               }
             file_system_info_free(fs);
          }
        free(path);
     }

   void *d;
   Eina_Iterator *it = eina_hash_iterator_data_new(_mounted);

   while (eina_iterator_next(it, &d))
     {
        item = d;
        fs = file_system_info_get(item->path);
        if (fs)
          file_system_info_free(fs);
        else
          {
             elm_box_unpack(ui->disk.box, item->parent);
             evas_object_del(item->parent);
             eina_hash_del(_mounted, NULL, item);
          }
     }
   eina_iterator_free(it);
   elm_box_recalculate(ui->disk.box);

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (ui->disk.timer)
     ecore_timer_del(ui->disk.timer);
   ui->disk.timer = NULL;

   eina_hash_free(_mounted);
   _mounted = NULL;

   evas_object_del(obj);
   ui->disk.win = NULL;
}

void
ui_win_disk_add(Ui *ui)
{
   Evas_Object *win, *box, *vbox, *scroller;
   Evas_Object *table, *rect;

   if (ui->disk.win)
     {
        elm_win_raise(ui->disk.win);
        return;
     }

   _mounted = eina_hash_string_superfast_new(_hash_free_cb);

   ui->disk.win = win = elm_win_util_standard_add("evisum",
                   _("Storage"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, evisum_ui_effects_enabled_get());

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);

   ui->disk.box = vbox = elm_box_add(win);
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

   _disks_poll_timer_cb(ui);

   ui->disk.timer = ecore_timer_add(3.0, _disks_poll_timer_cb, ui);
}

