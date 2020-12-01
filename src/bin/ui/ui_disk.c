#include "ui_disk.h"
#include "../system/filesystems.h"

typedef struct
{
   Evas_Object     *panes;
   Evas_Object     *btn_device;
   Evas_Object     *btn_mount;
   Evas_Object     *btn_fs;
   Evas_Object     *btn_usage;
   Evas_Object     *btn_used;
   Evas_Object     *btn_total;
   Evas_Object     *genlist;
   Evisum_Ui_Cache *cache;
   Ecore_Timer     *timer;
   int             (*sort_cb)(const void *, const void *);
   Eina_Bool        sort_reverse;
} Ui_Data;

static Eina_Lock _lock;

static Ui_Data *_private_data = NULL;

static void
_item_unrealized_cb(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Evas_Object *o;
   Eina_List *contents = NULL;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        evisum_ui_item_cache_item_release(_private_data->cache, o);
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   File_System *inf = data;

   file_system_info_free(inf);
}

static Evas_Object *
_item_column_add(Evas_Object *table, const char *text, int col)
{
   Evas_Object *rect, *lb;

   lb = elm_label_add(table);
   evas_object_data_set(table, text, lb);
   evas_object_show(lb);

   rect = evas_object_rectangle_add(table);
   evas_object_data_set(lb, "rect", rect);

   elm_table_pack(table, lb, col, 0, 1, 1);
   elm_table_pack(table, rect, col, 0, 1, 1);

   return lb;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *table, *lb, *pb;

   table = elm_table_add(parent);
   evas_object_size_hint_weight_set(table, EXPAND, EXPAND);
   evas_object_size_hint_align_set(table, FILL, FILL);
   evas_object_show(table);

   lb = _item_column_add(table, "device", 0);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(table, "mount", 1);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(table, "fs", 2);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(table, "used", 3);
   evas_object_size_hint_align_set(lb, 0.5, FILL);
   lb = _item_column_add(table, "total", 4);
   evas_object_size_hint_align_set(lb, 0.5, FILL);

   pb = elm_progressbar_add(table);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_data_set(table, "usage", pb);

   elm_table_pack(table, pb, 5, 0, 1, 1);

   return table;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Evas_Object *lb, *r, *pb;
   Evas_Coord w, ow;
   Ui_Data *pd;
   File_System *inf =  data;

   if (!inf) return NULL;
   if (strcmp(source, "elm.swallow.content")) return NULL;
   pd = evas_object_data_get(obj, "private");
   if (!pd) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(pd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(pd->btn_device, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "device");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->path));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_device, w, 1);
   r = evas_object_data_get(lb, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(pd->btn_mount, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "mount");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->mount));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_mount, w, 1);
   r = evas_object_data_get(lb, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(pd->btn_fs, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "fs");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->type_name));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_mount, w, 1);
   r = evas_object_data_get(lb, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(pd->btn_used, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "used");
   elm_object_text_set(lb, eina_slstr_printf("%s", evisum_size_format(inf->usage.used)));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_mount, ow, 1);
   r = evas_object_data_get(lb, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(pd->btn_total, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "total");
   elm_object_text_set(lb, eina_slstr_printf("%s", evisum_size_format(inf->usage.total)));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(pd->btn_mount, w, 1);
   r = evas_object_data_get(lb, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   pb = evas_object_data_get(it->obj, "usage");
   elm_progressbar_value_set(pb, (double) (inf->usage.used / (inf->usage.total / 100.0) / 100.0));
   evas_object_show(pb);

   return it->obj;
}

static void
_genlist_ensure_n_items(Evas_Object *genlist, unsigned int items)
{
   Elm_Object_Item *it;
   Elm_Genlist_Item_Class *itc;
   unsigned int i, existing = elm_genlist_items_count(genlist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
           {
              it = elm_genlist_last_item_get(genlist);
              if (it)
                elm_object_item_del(it);
           }
      }

   if (items == existing) return;

   itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.text_get = NULL;
   itc->func.content_get = _content_get;
   itc->func.filter_get = NULL;
   itc->func.del = _item_del;

   for (i = existing; i < items; i++)
     {
        elm_genlist_item_append(genlist, itc, NULL, NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_item_class_free(itc);
}

static void
_item_disk_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *it;
   File_System *fs;
   Ui_Data *pd = data;

   it = event_info;

   elm_genlist_item_selected_set(it, 0);
   fs = elm_object_item_data_get(it);
   if (!fs) return;

   return;
   elm_panes_content_right_size_set(pd->panes, 0.5);
   elm_panes_content_left_size_set(pd->panes, 0.5);
}

static Eina_Bool
_disks_poll_timer_cb(void *data)
{
   Elm_Object_Item *it;
   File_System *fs;
   Eina_List *mounted = NULL;

   eina_lock_take(&_lock);

   mounted = file_system_info_all_get();

   if (_private_data->sort_cb)
     mounted = eina_list_sort(mounted, eina_list_count(mounted), _private_data->sort_cb);
   if (_private_data->sort_reverse) mounted = eina_list_reverse(mounted);

   _genlist_ensure_n_items(_private_data->genlist, eina_list_count(mounted));

   it = elm_genlist_first_item_get(_private_data->genlist);
   EINA_LIST_FREE(mounted, fs)
     {
        File_System *prev = elm_object_item_data_get(it);
        if (prev) _item_del(prev, NULL);
        elm_object_item_data_set(it, fs);
        elm_genlist_item_update(it);
        it = elm_genlist_item_next_get(it);
     }

   eina_lock_release(&_lock);

   return EINA_TRUE;
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evas_object_del(obj);

   if (_private_data)
     {
        ecore_timer_del(_private_data->timer);
        evisum_ui_item_cache_free(_private_data->cache);
        free(_private_data);
     }

   eina_lock_free(&_lock);
   ui->disk.win = NULL;
}

static int
_sort_by_device(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;

   fs1 = p1; fs2 = p2;

   return strcmp(fs1->path, fs2->path);
}

static int
_sort_by_mount(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;

   fs1 = p1; fs2 = p2;

   return strcmp(fs1->mount, fs2->mount);
}

static int
_sort_by_type(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;

   fs1 = p1; fs2 = p2;

   return strcmp(fs1->type_name, fs2->type_name);
}

static int
_sort_by_used(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;

   fs1 = p1; fs2 = p2;

   return fs1->usage.used - fs2->usage.used;
}

static int
_sort_by_total(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;

   fs1 = p1; fs2 = p2;

   return fs1->usage.total - fs2->usage.total;
}

static void
_btn_icon_state_set(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *icon = elm_icon_add(button);

   if (reverse)
     elm_icon_standard_set(icon, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(icon, evisum_icon_path_get("go-up"));

   elm_object_part_content_set(button, "icon", icon);
   evas_object_color_set(icon, 255, 255, 255, 255);
   evas_object_show(icon);
}

static void
_btn_device_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_device)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_device;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_btn_mount_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_mount)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_mount;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_btn_fs_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                   void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_type)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_type;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_btn_used_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_used)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_used;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_btn_total_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_total)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_total;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_btn_usage_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   if (_private_data->sort_cb == _sort_by_total)
     _private_data->sort_reverse = !_private_data->sort_reverse;

   _private_data->sort_cb = _sort_by_total;
   _btn_icon_state_set(obj, _private_data->sort_reverse);
   _disks_poll_timer_cb(NULL);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Ui *ui = data;

   _disks_poll_timer_cb(NULL);
   evisum_ui_config_save(ui);
}

void
ui_win_disk_add(Ui *ui)
{
   Evas_Object *win, *box, *tbl, *scroller;
   Evas_Object *genlist, *btn;
   Evas_Object *panes;
   Evas_Coord x = 0, y = 0;
   int i = 0;

   if (ui->disk.win)
     {
        elm_win_raise(ui->disk.win);
        return;
     }

   eina_lock_new(&_lock);

   ui->disk.win = win = elm_win_util_standard_add("evisum", _("Storage"));
   elm_win_autodel_set(win, EINA_TRUE);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, (evisum_ui_effects_enabled_get() ||
                                   evisum_ui_backgrounds_enabled_get()));

   Ui_Data *pd = _private_data = calloc(1, sizeof(Ui_Data));

   pd->panes = panes = elm_panes_add(win);
   evas_object_size_hint_weight_set(panes, EXPAND, EXPAND);
   elm_panes_horizontal_set(panes, 1);
   evas_object_show(panes);
   elm_object_content_set(win, panes);

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);

   tbl = elm_table_add(win);
   evas_object_size_hint_weight_set(tbl, EXPAND, 0);
   evas_object_size_hint_align_set(tbl, FILL, FILL);
   evas_object_show(tbl);
   elm_box_pack_end(box, tbl);

   pd->btn_device = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Device"));
   evas_object_smart_callback_add(btn, "clicked", _btn_device_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   pd->btn_mount = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Mount"));
   evas_object_smart_callback_add(btn, "clicked", _btn_mount_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   pd->btn_fs = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Type"));
   evas_object_smart_callback_add(btn, "clicked", _btn_fs_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   pd->btn_used = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Used"));
   evas_object_smart_callback_add(btn, "clicked", _btn_used_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   pd->btn_total = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Total"));
   evas_object_smart_callback_add(btn, "clicked", _btn_total_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   pd->btn_usage = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Usage"));
   evas_object_smart_callback_add(btn, "clicked", _btn_usage_clicked_cb, NULL);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                           ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   pd->genlist = genlist = elm_genlist_add(win);
   elm_object_focus_allow_set(genlist, 0);
   evas_object_data_set(genlist, "private", pd);
   elm_genlist_select_mode_set(genlist, ELM_OBJECT_SELECT_MODE_DEFAULT);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   evas_object_smart_callback_add(genlist, "unrealized", _item_unrealized_cb, ui);
   evas_object_smart_callback_add(genlist, "selected", _item_disk_clicked_cb, pd);
   evas_object_show(genlist);
   elm_object_content_set(scroller, genlist);

   pd->cache = evisum_ui_item_cache_new(genlist, _item_create, 10);

   elm_box_pack_end(box, scroller);
   elm_object_part_content_set(panes, "left", box);
   elm_panes_content_left_size_set(panes, 1.0);

   if (ui->disk.width > 0 && ui->disk.height > 0)
     evas_object_resize(win, ui->disk.width, ui->disk.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH * 1.5, UI_CHILD_WIN_HEIGHT * 1.1);

   if (ui->win)
     evas_object_geometry_get(ui->win, &x, &y, NULL, NULL);
   if (x > 0 && y > 0)
     evas_object_move(win, x + 20, y + 20);
   else
     elm_win_center(win, 1, 1);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, ui);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, ui);
   evas_object_show(win);

   pd->timer = ecore_timer_add(3.0, _disks_poll_timer_cb, NULL);

   _disks_poll_timer_cb(NULL);
}

