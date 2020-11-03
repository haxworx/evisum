#include "ui_disk.h"
#include "../system/disks.h"

typedef struct
{
   Evas_Object     *btn_device;
   Evas_Object     *btn_mount;
   Evas_Object     *btn_fs;
   Evas_Object     *btn_usage;
   Evas_Object     *btn_used;
   Evas_Object     *btn_total;
   Evas_Object     *genlist;
   Evisum_Ui_Cache *cache;
   Ecore_Timer     *timer;
} Widgets;

static Eina_Lock _lock;

static Widgets *_widgets = NULL;

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   File_System *inf = data;

   file_system_info_free(inf);
}

static Evas_Object *
_item_column_add(Evas_Object *table, const char *text, int col)
{
   Evas_Object *rect, *label;

   label = elm_label_add(table);
   evas_object_data_set(table, text, label);
   evas_object_show(label);

   rect = evas_object_rectangle_add(table);
   evas_object_data_set(label, "rect", rect);

   elm_table_pack(table, label, col, 0, 1, 1);
   elm_table_pack(table, rect, col, 0, 1, 1);

   return label;
}

static Evas_Object *
_item_create(Evas_Object *parent)
{
   Evas_Object *table, *label, *pb;

   table = elm_table_add(parent);
   evas_object_size_hint_align_set(table, EXPAND, EXPAND);
   evas_object_size_hint_weight_set(table, FILL, FILL);
   evas_object_show(table);

   label = _item_column_add(table, "device", 0);
   evas_object_size_hint_align_set(label, 0.5, FILL);
   label = _item_column_add(table, "mount", 1);
   evas_object_size_hint_align_set(label, 0.5, FILL);
   label = _item_column_add(table, "fs", 2);
   evas_object_size_hint_align_set(label, 0.5, FILL);
   label = _item_column_add(table, "used", 3);
   evas_object_size_hint_align_set(label, 0.5, FILL);
   label = _item_column_add(table, "total", 4);
   evas_object_size_hint_align_set(label, 0.5, FILL);

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
   Evas_Object *l, *r, *pb;
   Evas_Coord w, ow;
   Widgets *wig;
   File_System *inf =  data;

   if (!inf) return NULL;
   if (strcmp(source, "elm.swallow.content")) return NULL;
   wig = evas_object_data_get(obj, "widgets");
   if (!wig) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(wig->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(wig->btn_device, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "device");
   elm_object_text_set(l, eina_slstr_printf("%s", inf->path));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wig->btn_device, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(wig->btn_mount, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "mount");
   elm_object_text_set(l, eina_slstr_printf("%s", inf->mount));
   evas_object_geometry_get(l, NULL, NULL, &ow, NULL);
   if (ow > w) evas_object_size_hint_min_set(wig->btn_mount, w, 1);
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(wig->btn_fs, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "fs");
   elm_object_text_set(l, eina_slstr_printf("%s", inf->type_name));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(wig->btn_used, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "used");
   elm_object_text_set(l, eina_slstr_printf("%s", evisum_size_format(inf->usage.used)));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   evas_object_geometry_get(wig->btn_total, NULL, NULL, &w, NULL);
   l = evas_object_data_get(it->obj, "total");
   elm_object_text_set(l, eina_slstr_printf("%s", evisum_size_format(inf->usage.total)));
   r = evas_object_data_get(l, "rect");
   evas_object_size_hint_min_set(r, w, 1);
   evas_object_show(l);

   pb = evas_object_data_get(it->obj, "usage");
   if (inf->usage.used != inf->usage.total)
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

static Eina_Bool
_disks_poll_timer_cb(void *data)
{
   Eina_List *disks;
   char *path;
   Elm_Object_Item *it;
   File_System *fs;
   Eina_List *mounted = NULL;

   eina_lock_take(&_lock);

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        fs = file_system_info_get(path);
        if (fs)
          mounted = eina_list_append(mounted, fs);

        free(path);
     }

   _genlist_ensure_n_items(_widgets->genlist, eina_list_count(mounted));

   it = elm_genlist_first_item_get(_widgets->genlist);
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
_win_del_cb(void *data, Evas_Object *obj EINA_UNUSED,
            void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if (_widgets)
     {
        ecore_timer_del(_widgets->timer);
        evisum_ui_item_cache_free(_widgets->cache);
        free(_widgets);
     }

   eina_lock_free(&_lock);
   evas_object_del(obj);
   ui->disk.win = NULL;

   if (evisum_ui_can_exit(ui))
     ecore_main_loop_quit();
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   _disks_poll_timer_cb(NULL);
}

void
ui_win_disk_add(Ui *ui)
{
   Evas_Object *win, *box, *tbl, *scroller;
   Evas_Object *genlist, *btn;
   int i = 0;

   if (ui->disk.win)
     {
        elm_win_raise(ui->disk.win);
        return;
     }

   eina_lock_new(&_lock);
   ui->disk.win = win = elm_win_util_standard_add("evisum",
                   _("Storage"));
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);
   evisum_ui_background_random_add(win, (evisum_ui_effects_enabled_get() ||
                                   evisum_ui_backgrounds_enabled_get()));

   box = elm_box_add(win);
   evas_object_size_hint_weight_set(box, EXPAND, EXPAND);
   evas_object_size_hint_align_set(box, FILL, FILL);
   evas_object_show(box);

   tbl = elm_table_add(win);
   evas_object_size_hint_weight_set(tbl, EXPAND, 0);
   evas_object_size_hint_align_set(tbl, FILL, 0);
   evas_object_show(tbl);
   elm_box_pack_end(box, tbl);

   Widgets *w = _widgets = calloc(1, sizeof(Widgets));

   w->btn_device = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Device"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   w->btn_mount = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Mount"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   w->btn_fs = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("FS"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   w->btn_used = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Used"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   w->btn_total = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Total"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);

   w->btn_usage = btn = elm_button_add(win);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   elm_object_text_set(btn, _("Usage"));
   elm_table_pack(tbl, btn, i++, 0, 1, 1);


   scroller = elm_scroller_add(win);
   evas_object_size_hint_weight_set(scroller, EXPAND, EXPAND);
   evas_object_size_hint_align_set(scroller, FILL, FILL);
   elm_scroller_policy_set(scroller,
                   ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
   evas_object_show(scroller);

   w->genlist = genlist = elm_genlist_add(win);
   elm_object_focus_allow_set(genlist, 0);
   evas_object_data_set(genlist, "widgets", w);
   elm_genlist_select_mode_set(genlist, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(genlist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(genlist, FILL, FILL);
   evas_object_show(genlist);

   elm_object_content_set(scroller, genlist);
   elm_box_pack_end(box, scroller);
   elm_object_content_set(win, box);

   evas_object_smart_callback_add(win, "delete,request", _win_del_cb, ui);
   evisum_child_window_show(ui->win, win);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, NULL);


   w->cache = evisum_ui_item_cache_new(genlist, _item_create, 10);

   _disks_poll_timer_cb(NULL);

   w->timer = ecore_timer_add(3.0, _disks_poll_timer_cb, NULL);
}

