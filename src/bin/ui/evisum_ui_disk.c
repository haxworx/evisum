#include "evisum_ui_disk.h"
#include "../system/filesystems.h"

typedef struct
{
   Evas_Object     *btn_device;
   Evas_Object     *btn_mount;
   Evas_Object     *btn_fs;
   Evas_Object     *btn_usage;
   Evas_Object     *btn_total;
   Evas_Object     *btn_used;
   Evas_Object     *btn_free;
   Evas_Object     *glist;
   Evisum_Ui_Cache *cache;
   Ecore_Thread    *thread;
   int             (*sort_cb)(const void *, const void *);
   Eina_Bool        sort_reverse;
   Eina_Bool        skip_wait;

   Evisum_Ui       *ui;
} Win_Data;

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evas_Object *o;
   Eina_List *contents = NULL;

   wd = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        evisum_ui_item_cache_item_release(wd->cache, o);
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   File_System *inf = data;

   file_system_info_free(inf);
}

static Evas_Object *
_item_column_add(Evas_Object *tb, const char *text, int col)
{
   Evas_Object *rec, *lb;

   lb = elm_label_add(tb);
   evas_object_data_set(tb, text, lb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_data_set(lb, "rec", rec);

   elm_table_pack(tb, lb, col, 0, 1, 1);
   elm_table_pack(tb, rec, col, 0, 1, 1);
   evas_object_show(lb);

   return lb;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *tb, *lb, *pb;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   lb = _item_column_add(tb, "device", 0);
   evas_object_size_hint_weight_set(lb, EXPAND, 0);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(tb, "mount", 1);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(tb, "fs", 2);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(tb, "total", 3);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(tb, "used", 4);
   evas_object_size_hint_align_set(lb, 0, FILL);
   lb = _item_column_add(tb, "free", 5);
   evas_object_size_hint_align_set(lb, 0, FILL);

   pb = elm_progressbar_add(tb);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_data_set(tb, "usage", pb);
   elm_table_pack(tb, pb, 6, 0, 1, 1);

   return tb;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Evas_Object *lb, *r, *pb;
   Evas_Coord w, ow;
   Win_Data *wd;
   File_System *inf = data;

   if (!inf) return NULL;
   if (strcmp(source, "elm.swallow.content")) return NULL;
   wd = evas_object_data_get(obj, "private");
   if (!wd) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(wd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(wd->btn_device, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "device");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->path));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_device, w, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_mount, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "mount");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->mount));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_mount, w, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_fs, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "fs");
   elm_object_text_set(lb, eina_slstr_printf("%s", inf->type_name));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_fs, w, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_total, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "total");
   elm_object_text_set(lb, eina_slstr_printf("%s", evisum_size_format(inf->usage.total, 0)));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_total, w, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_used, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "used");
   elm_object_text_set(lb, eina_slstr_printf("%s", evisum_size_format(inf->usage.used, 0)));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_used, ow, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_free, NULL, NULL, &w, NULL);
   lb = evas_object_data_get(it->obj, "free");
   int64_t avail = inf->usage.total - inf->usage.used;
   elm_object_text_set(lb, eina_slstr_printf("%s", evisum_size_format(avail, 0)));
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wd->btn_free, ow, 1);
   r = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(lb);

   evas_object_geometry_get(wd->btn_usage, NULL, NULL, &w, NULL);
   pb = evas_object_data_get(it->obj, "usage");
   elm_progressbar_value_set(pb, (double) (inf->usage.used / (inf->usage.total / 100.0) / 100.0));
   evas_object_show(pb);

   return it->obj;
}

static void
_glist_ensure_n_items(Evas_Object *glist, unsigned int items)
{
   Elm_Object_Item *it;
   Elm_Genlist_Item_Class *itc;
   unsigned int i, existing = elm_genlist_items_count(glist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
           {
              it = elm_genlist_last_item_get(glist);
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
        elm_genlist_item_append(glist, itc, NULL, NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }

   elm_genlist_item_class_free(itc);
}

static void
_disks_poll(void *data, Ecore_Thread *thread)
{
   Win_Data *wd = data;

   while (!ecore_thread_check(thread))
     {
        for (int i = 0; i < 8; i++)
          {
             if (wd->skip_wait)
               {
                  wd->skip_wait = 0;
                  break;
               }
             if (ecore_thread_check(thread))
               return;
             usleep(125000);
          }
        ecore_thread_feedback(thread, file_system_info_all_get());
     }
}

static void
_disks_poll_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata)
{
   Elm_Object_Item *it;
   File_System *fs;
   Win_Data *wd;
   Eina_List *mounted;

   wd = data;
   mounted = msgdata;

   if (wd->sort_cb)
     mounted = eina_list_sort(mounted, eina_list_count(mounted), wd->sort_cb);
   if (wd->sort_reverse) mounted = eina_list_reverse(mounted);

   _glist_ensure_n_items(wd->glist, eina_list_count(mounted));

   it = elm_genlist_first_item_get(wd->glist);
   EINA_LIST_FREE(mounted, fs)
     {
        File_System *prev = elm_object_item_data_get(it);
        if (prev) _item_del(prev, NULL);
        elm_object_item_data_set(it, fs);
        elm_genlist_item_update(it);
        it = elm_genlist_item_next_get(it);
     }
   elm_genlist_realized_items_update(wd->glist);
}

static void
_disks_poll_update(Win_Data *wd)
{
   wd->skip_wait = 1;
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Win_Data *wd;

   wd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(wd->ui->disk.win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   evas_object_geometry_get(obj, &ui->disk.x, &ui->disk.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(wd->thread);
   ecore_thread_wait(wd->thread, 0.5);

   evisum_ui_item_cache_free(wd->cache);
   free(wd);

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
   int64_t used1, used2;

   fs1 = p1; fs2 = p2;

   used1 = fs1->usage.used;
   used2 = fs2->usage.used;

   if (used1 > used2) return 1;
   if (used1 < used2) return -1;

   return 0;
}

static int
_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;
   double used1 = 0, used2 = 0;

   fs1 = p1; fs2 = p2;

   if (fs1->usage.used && fs1->usage.total)
     used1 = (double) fs1->usage.used / (fs1->usage.total / 100);
   if (fs2->usage.used && fs2->usage.total)
     used2 = (double) fs2->usage.used / (fs2->usage.total / 100);

   if (used1 > used2) return 1;
   if (used1 < used2) return -1;

   return 0;
}

static int
_sort_by_free(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;
   int64_t free1, free2;

   fs1 = p1; fs2 = p2;

   free1 = fs1->usage.total - fs1->usage.used;
   free2 = fs2->usage.total - fs2->usage.used;

   if (free1 > free2) return 1;
   if (free1 < free2) return -1;

   return 0;
}

static int
_sort_by_total(const void *p1, const void *p2)
{
   const File_System *fs1, *fs2;
   int64_t total1, total2;

   fs1 = p1; fs2 = p2;

   total1 = fs1->usage.total;
   total2 = fs2->usage.total;

   if (total1 > total2) return 1;
   if (total1 < total2) return -1;

   return 0;
}

static void
_btn_icon_state_set(Evas_Object *button, Eina_Bool reverse)
{
   Evas_Object *ic = elm_icon_add(button);

   if (reverse)
     elm_icon_standard_set(ic, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(ic, evisum_icon_path_get("go-up"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_show(ic);
}

static void
_btn_device_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                       void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_device)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_device;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_mount_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_mount)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_mount;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_fs_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                   void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_type)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_type;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_used_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_used)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_used;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_free_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_free)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_free;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_total_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_total)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_total;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_btn_usage_clicked_cb(void *data EINA_UNUSED, Evas_Object *obj,
                      void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (wd->sort_cb == _sort_by_cpu_usage)
     wd->sort_reverse = !wd->sort_reverse;

   wd->sort_cb = _sort_by_cpu_usage;
   _btn_icon_state_set(obj, wd->sort_reverse);
   _disks_poll_update(wd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Win_Data *wd = data;
   Evisum_Ui *ui = wd->ui;

   _disks_poll_update(wd);
   evas_object_geometry_get(obj, NULL, NULL, &ui->disk.width, &ui->disk.height);
}

static Evas_Object *
_btn_min_size(Evas_Object *parent)
{
   Evas_Object *rec = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_WIDTH), 1);

   return rec;
}

void
evisum_ui_disk_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *tb, *scr;
   Evas_Object *glist, *rec, *btn;
   int i = 0;

   if (ui->disk.win)
     {
        elm_win_raise(ui->disk.win);
        return;
     }

   ui->disk.win = win = elm_win_util_standard_add("evisum", _("Storage"));
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   Win_Data *wd = calloc(1, sizeof(Win_Data));
   wd->ui = ui;
   wd->skip_wait = 1;

   tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
   evas_object_size_hint_max_set(rec, -1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
   elm_table_pack(tb, rec, i++, 0, 1, 1);

   wd->btn_device = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("device"));
   evas_object_smart_callback_add(btn, "clicked", _btn_device_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_mount = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("mount"));
   evas_object_smart_callback_add(btn, "clicked", _btn_mount_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_fs = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("type"));
   evas_object_smart_callback_add(btn, "clicked", _btn_fs_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_total = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("total"));
   evas_object_smart_callback_add(btn, "clicked", _btn_total_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   rec = _btn_min_size(btn);
   elm_table_pack(tb, rec, i, i, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_used = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("used"));
   evas_object_smart_callback_add(btn, "clicked", _btn_used_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   rec = _btn_min_size(btn);
   elm_table_pack(tb, rec, i, i, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_free = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("free"));
   evas_object_smart_callback_add(btn, "clicked", _btn_free_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   rec = _btn_min_size(btn);
   elm_table_pack(tb, rec, i, i, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   wd->btn_usage = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   elm_object_text_set(btn, _("usage"));
   evas_object_smart_callback_add(btn, "clicked", _btn_usage_clicked_cb, wd);
   _btn_icon_state_set(btn, 0);
   elm_table_pack(tb, btn, i++, 0, 1, 1);
   evas_object_show(btn);

   scr = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scr, FILL, FILL);
   elm_scroller_policy_set(scr,
                           ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   wd->glist = glist = elm_genlist_add(win);
   elm_object_focus_allow_set(glist, 0);
   evas_object_data_set(glist, "private", wd);
   elm_genlist_select_mode_set(glist, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);
   evas_object_smart_callback_add(glist, "unrealized", _item_unrealized_cb, wd);
   elm_genlist_homogeneous_set(glist, 1);
   evas_object_show(glist);
   elm_object_content_set(scr, glist);
   evas_object_show(scr);

   wd->cache = evisum_ui_item_cache_new(glist, _item_create, 10);

   elm_table_pack(tb, scr, 0, 2, 8, 2);
   elm_object_content_set(win, tb);

   if ((ui->disk.width > 0) && (ui->disk.height > 0))
     evas_object_resize(win, ui->disk.width, ui->disk.height);
   else
     evas_object_resize(win, ELM_SCALE_SIZE(UI_CHILD_WIN_WIDTH * 1.5), ELM_SCALE_SIZE(UI_CHILD_WIN_HEIGHT * 0.5));

   if ((ui->disk.x > 0) && (ui->disk.y > 0))
     evas_object_move(win, ui->disk.x, ui->disk.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, wd);
   evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, wd);
   evas_object_show(win);

   wd->thread = ecore_thread_feedback_run(_disks_poll,
                                          _disks_poll_feedback_cb,
                                          NULL,
                                          NULL,
                                          wd, 1);
}

