/* Interactive examples by default; --test runs checks on an off-screen canvas. */
#include <Elementary.h>
#include <Ecore_Evas.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ui/evisum_ui_widget_exel.h"
#include "ui/evisum_ui_cache.h"

/* Icon appearance is outside this test; avoid loading the process/icon cache. */
void
evisum_ui_icon_set(Evas_Object *obj, const char *name)
{
   elm_icon_standard_set(obj, name);
}

const char *
evisum_ui_icon_name_get(const char *name)
{
   return name;
}

static void
_count_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   ++*(int *) data;
}

static void
_count_call(void *data)
{
   ++*(int *) data;
}

static void
_count_signal(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   ++*(int *) data;
}

static void
_settle(Evas *e)
{
   for (int i = 0; i < 10; i++)
     {
        ecore_main_loop_iterate_may_block(EINA_FALSE);
        evas_smart_objects_calculate(e);
     }
}

static Evas_Object *
_root_get(Evas_Object *obj)
{
   Eina_List *members = evas_object_smart_members_get(obj);
   assert(eina_list_count(members) == 1);
   Evas_Object *root = eina_list_data_get(members);
   eina_list_free(members);
   return root;
}

static Evas_Object *
_check_find(Evas_Object *obj)
{
   Evas_Object *child, *result = NULL;
   Eina_List *l, *members;
   const char *type = elm_object_widget_type_get(obj);
   if (type && !strcmp(type, "Elm_Check")) return obj;
   members = evas_object_smart_members_get(obj);
   EINA_LIST_FOREACH (members, l, child)
     {
        result = _check_find(child);
        if (result) break;
     }
   eina_list_free(members);
   return result;
}

static Evas_Object *
_fields_menu_get(Evas_Object *check)
{
   Evas_Object *menu = check;
   while (menu)
     {
        const char *type = elm_object_widget_type_get(menu);
        if (type && !strcmp(type, "Elm_Ctxpopup")) return menu;
        menu = elm_object_parent_widget_get(menu);
     }
   assert(menu);
   return menu;
}

static Evas_Object *
_field_add(Evas_Object *obj, int id)
{
   Evisum_Ui_Widget_Exel_Field_Params params = {
      .id = id,
      .title = id ? "Value" : "Command",
      .desc = id ? "Value" : "Command",
      .key = id ? "value" : "cmd",
      .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
      .align_x = 0.0,
      .weight_x = EVAS_HINT_EXPAND,
      .boxed = EINA_TRUE,
      .default_width = 120,
      .min_width = 60,
      .always_visible = !id,
   };
   return evisum_ui_widget_exel_field_add(obj, &params);
}

static void
_test_object(Evas_Object *win)
{
   Evas_Object *obj = evisum_ui_widget_exel_add(win);
   Evas_Object *root = _root_get(obj);
   Evas_Object *glist = evisum_ui_widget_exel_genlist_obj_get(obj);
   Evas *e = evas_object_evas_get(obj);
   Evas_Coord x, y, w, h;
   int r, g, b, a;
   int deletes = 0, selected = 0, pending = 0, reset = 0;

   assert(evas_object_smart_type_check(obj, "evisum_ui_widget_exel"));
   assert(!evas_object_visible_get(obj));
   assert(evas_object_data_get(glist, "widget_exel") == obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _count_del, &deletes);

   evas_object_move(obj, 25, 30);
   evas_object_resize(obj, 500, 300);
   evas_object_geometry_get(root, &x, &y, &w, &h);
   assert(x == 25 && y == 30 && w == 500 && h == 300);
   evas_object_show(obj);
   assert(evas_object_visible_get(root));
   evas_object_hide(obj);
   assert(!evas_object_visible_get(root));
   evas_object_color_set(obj, 100, 110, 120, 130);
   evas_object_color_get(root, &r, &g, &b, &a);
   assert(r == 100 && g == 110 && b == 120 && a == 130);
   Evas_Object *clip = evas_object_rectangle_add(e);
   evas_object_clip_set(obj, clip);
   assert(evas_object_clip_get(root) == clip);
   evas_object_clip_unset(obj);
   assert(!evas_object_clip_get(root));
   evas_object_del(clip);

   evisum_ui_widget_exel_field_bounds_set(obj, 0, 2);
   assert(_field_add(obj, 0));
   Evas_Object *header = _field_add(obj, 1);
   assert(header);
   evisum_ui_widget_exel_fields_apply(obj);
   Evas_Object *control = elm_button_add(win);
   Evas_Object *footer = elm_label_add(win);
   int parts_deleted = 0;
   evas_object_event_callback_add(control, EVAS_CALLBACK_DEL, _count_del, &parts_deleted);
   evas_object_event_callback_add(footer, EVAS_CALLBACK_DEL, _count_del, &parts_deleted);
   evisum_ui_widget_exel_header_content_set(obj, control);
   evisum_ui_widget_exel_footer_set(obj, footer);
   evas_object_show(obj);
   _settle(e);
   evas_object_geometry_get(header, NULL, NULL, &w, &h);
   assert(w > 0 && h > 0);
   evas_object_size_hint_min_get(obj, &w, &h);
   assert(w >= 240 && h > 0);

   evas_object_smart_callback_add(obj, "selected", _count_signal, &selected);
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   Elm_Object_Item *it = evisum_ui_widget_exel_genlist_item_append(obj, itc, NULL, NULL,
                                                                  ELM_GENLIST_ITEM_NONE, NULL, NULL);
   assert(it);
   /* The same event payload is forwarded by the widget. */
   evas_object_smart_callback_call(glist, "selected", it);
   assert(selected == 1);
   elm_genlist_item_class_free(itc);

   int changes = 0, applies = 0;
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_CHANGED, _count_signal, &changes);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_APPLIED, _count_signal, &applies);
   Evas_Event_Mouse_Up up = { .button = 3 };
   unsigned int mask = evisum_ui_widget_exel_fields_mask_get(obj);
   evisum_ui_widget_exel_fields_menu_show(obj, header, &up);
   assert(changes == 1);
   assert(evisum_ui_widget_exel_fields_menu_visible_get(obj));
   Evas_Object *check = NULL;
   for (Evas_Object *top = evas_object_top_get(e); top && !check; top = evas_object_below_get(top))
      check = _check_find(top);
   assert(check);
   int menus_deleted = 0;
   Evas_Object *menu = _fields_menu_get(check);
   evas_object_event_callback_add(menu, EVAS_CALLBACK_DEL, _count_del, &menus_deleted);
   Evas_Object *apply = evas_object_data_get(check, "apply_icon");
   assert(apply);
   evas_object_smart_callback_call(check, "changed", NULL);
   assert(evisum_ui_widget_exel_fields_mask_get(obj) != mask);
   evas_object_smart_callback_call(apply, "clicked", NULL);
   assert(applies == 1);
   assert(menus_deleted == 1);
   assert(!evisum_ui_widget_exel_field_enabled_get(obj, 1));
   assert(!evisum_ui_widget_exel_fields_menu_visible_get(obj));
   evisum_ui_widget_exel_fields_menu_show(obj, header, &up);
   evisum_ui_widget_exel_fields_menu_dismiss(obj);

   /* Theme/outside-click dismissal cancels edits and releases the popup too. */
   evisum_ui_widget_exel_fields_menu_show(obj, header, &up);
   check = NULL;
   for (Evas_Object *top = evas_object_top_get(e); top && !check; top = evas_object_below_get(top))
      check = _check_find(top);
   assert(check);
   menu = _fields_menu_get(check);
   evas_object_event_callback_add(menu, EVAS_CALLBACK_DEL, _count_del, &menus_deleted);
   mask = evisum_ui_widget_exel_fields_mask_get(obj);
   evas_object_smart_callback_call(check, "changed", NULL);
   assert(evisum_ui_widget_exel_fields_mask_get(obj) != mask);
   evas_object_smart_callback_call(menu, "dismissed", NULL);
   assert(!evisum_ui_widget_exel_fields_menu_visible_get(obj));
   assert(evisum_ui_widget_exel_fields_mask_get(obj) == mask);
   assert(menus_deleted == 2);

   /* Keep a chooser open while destroying its owning table. */
   evisum_ui_widget_exel_fields_menu_show(obj, header, &up);
   check = NULL;
   for (Evas_Object *top = evas_object_top_get(e); top && !check; top = evas_object_below_get(top))
      check = _check_find(top);
   assert(check);
   menu = _fields_menu_get(check);
   evas_object_event_callback_add(menu, EVAS_CALLBACK_DEL, _count_del, &menus_deleted);

   /* A reset with no cache still completes. Pending work dies with the object. */
   evisum_ui_widget_exel_item_cache_reset(obj, _count_call, &reset);
   assert(reset == 1);
   Evas_Object *row = evisum_ui_widget_exel_item_cache_object_get(obj);
   assert(row);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 0, "cmd", "test");
   /* Changing presentation rebuilds cached cells and preserves status text. */
   Evisum_Ui_Widget_Exel_Item_Cell_Def def = {
      .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
      .key = "cmd",
      .unit_format = "",
   };
   evisum_ui_widget_exel_field_row_def_set(obj, 0, &def);
   evisum_ui_widget_exel_item_cache_reset(obj, NULL, NULL);
   row = evisum_ui_widget_exel_item_cache_object_get(obj);
   assert(row);
   evisum_ui_widget_exel_item_field_progress_set(obj, row, 0, "cmd", 0.25, "128 MiB");
   Evas_Object *cell = evisum_ui_widget_exel_item_object_get(row, "cmd");
   assert(EINA_DBL_EQ(elm_progressbar_value_get(cell), 0.25));
   assert(!strcmp(elm_object_part_text_get(cell, "elm.text.status"), "128 MiB"));
   def.type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT;
   evisum_ui_widget_exel_field_row_def_set(obj, 0, &def);
   evisum_ui_widget_exel_item_cache_reset(obj, NULL, NULL);
   row = evisum_ui_widget_exel_item_cache_object_get(obj);
   assert(row);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 0, "cmd", "128 MiB");
   cell = evisum_ui_widget_exel_item_object_get(row, "cmd");
   assert(!strcmp(elm_object_text_get(cell), "128 MiB"));
   evisum_ui_widget_exel_item_cache_reset(obj, _count_call, &pending);
   evisum_ui_widget_exel_deferred_call_schedule(obj, 0.001, _count_call, &pending);
   evas_object_del(obj);
   _settle(e);
   assert(deletes == 1 && parts_deleted == 2 && pending == 0);
   assert(menus_deleted == 3);
}

static Evas_Object *
_row_content_get(void *data EINA_UNUSED, Evas_Object *glist, const char *part)
{
   if (strcmp(part, "elm.swallow.content")) return NULL;
   Evas_Object *obj = evas_object_data_get(glist, "widget_exel");
   Evas_Object *row = evisum_ui_widget_exel_item_cache_object_get(obj);
   assert(row);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 0, "cmd", "Row content");
   return row;
}

static void
_test_rows_and_input(Evas_Object *win)
{
   Evas *e = evas_object_evas_get(win);
   Evas_Object *box = elm_box_add(win);
   elm_win_resize_object_add(win, box);
   evas_object_show(box);
   Evas_Object *obj = evisum_ui_widget_exel_add(box);
   elm_box_pack_end(box, obj);
   evas_object_show(obj);
   evisum_ui_widget_exel_field_bounds_set(obj, 0, 3);
   int order[3] = { 0, 1, 2 };
   evisum_ui_widget_exel_field_order_bind(obj, order);
   evisum_ui_widget_exel_field_reorder_enabled_set(obj, EINA_TRUE);
   assert(_field_add(obj, 0));
   Evas_Object *header = _field_add(obj, 1);
   Evas_Object *last = _field_add(obj, 2);
   assert(header && last);
   evisum_ui_widget_exel_fields_apply(obj);
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.content_get = _row_content_get;
   evisum_ui_widget_exel_genlist_items_ensure(obj, 30, itc);
   elm_genlist_item_class_free(itc);
   _settle(e);
   assert(evisum_ui_widget_exel_item_cache_active_count_get(obj) > 0);

   int resize_live = 0, resize_done = 0, reordered = 0;
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZE, _count_signal, &resize_live);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZED, _count_signal, &resize_done);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_REORDERED, _count_signal, &reordered);
   Evas_Coord x, y, w, h;
   evas_object_geometry_get(header, &x, &y, &w, &h);
   assert(w > 0 && h > 0);
   evas_event_feed_mouse_in(e, 0, NULL);
   evas_event_feed_mouse_move(e, x + w - 2, y + h / 2, 0, NULL);
   evas_event_feed_mouse_down(e, 1, EVAS_BUTTON_NONE, 0, NULL);
   evas_event_feed_mouse_move(e, x + w - 32, y + h / 2, 0, NULL);
   _settle(e);
   evas_event_feed_mouse_up(e, 1, EVAS_BUTTON_NONE, 0, NULL);
   _settle(e);
   assert(resize_live > 0 && resize_done == 1);
   assert(evisum_ui_widget_exel_sort_ignore_consume(obj));
   assert(!evisum_ui_widget_exel_sort_ignore_consume(obj));

   evas_object_geometry_get(header, &x, &y, &w, &h);
   Evas_Coord lx, lw;
   evas_object_geometry_get(last, &lx, NULL, &lw, NULL);
   evas_event_feed_mouse_move(e, x + w / 2, y + h / 2, 0, NULL);
   evas_event_feed_mouse_down(e, 1, EVAS_BUTTON_NONE, 0, NULL);
   evas_event_feed_mouse_move(e, lx + lw - 2, y + h / 2, 0, NULL);
   evas_event_feed_mouse_up(e, 1, EVAS_BUTTON_NONE, 0, NULL);
   _settle(e);
   assert(reordered == 1 && order[0] == 0 && order[1] == 2 && order[2] == 1);

   /* Tear down with both realized rows and a pending cache reset. */
   evisum_ui_widget_exel_genlist_clear(obj);
   int pending = 0;
   evisum_ui_widget_exel_item_cache_reset(obj, _count_call, &pending);
   evas_object_del(obj);
   _settle(e);
   assert(pending == 0);
   evas_object_del(box);
}

static void
_test_parent_lifetime(Evas_Object *win)
{
   int deletes = 0, pending = 0;
   Evas_Object *parent = elm_box_add(win);
   Evas_Object *obj = evisum_ui_widget_exel_add(parent);
   elm_box_pack_end(parent, obj);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _count_del, &deletes);
   evisum_ui_widget_exel_deferred_call_schedule(obj, 0.001, _count_call, &pending);
   evas_object_del(parent);
   _settle(evas_object_evas_get(win));
   assert(deletes == 1 && pending == 0);
}

static Evas_Object *
_cache_row_create(Evas_Object *parent)
{
   return elm_box_add(parent);
}

static void
_test_cache_external_deletion(Evas_Object *win)
{
   int done = 0;
   Evisum_Ui_Cache *cache = evisum_ui_item_cache_new(win, _cache_row_create, 2);
   assert(cache);
   Evas_Object *row = evisum_ui_item_cache_item_get(cache)->obj;
   evas_object_del(row);
   assert(!cache->active);
   Item_Cache *inactive = eina_list_data_get(cache->inactive);
   assert(inactive);
   evas_object_del(inactive->obj);
   assert(!cache->inactive);

   row = evisum_ui_item_cache_item_get(cache)->obj;
   Evas_Object *remaining = evisum_ui_item_cache_item_get(cache)->obj;
   int deleted = 0;
   evas_object_event_callback_add(remaining, EVAS_CALLBACK_DEL, _count_del, &deleted);
   evisum_ui_item_cache_reset(cache, _count_call, &done);
   assert(eina_list_count(cache->pending) == 2);
   evas_object_del(row);
   assert(eina_list_count(cache->pending) == 1);
   /* Deferred cleanup skips the dead row and deletes the remaining live row. */
   ecore_timer_interval_set(cache->pending_timer, 0.0);
   ecore_timer_reset(cache->pending_timer);
   _settle(evas_object_evas_get(win));
   assert(done == 1 && deleted == 1 && !cache->pending_timer);

   /* A row removed by steal must no longer refer back to the cache. */
   row = evisum_ui_item_cache_item_get(cache)->obj;
   evisum_ui_item_cache_steal(cache, NULL);
   assert(!cache->active);
   evisum_ui_item_cache_free(cache);
   evas_object_del(row);
}

static Evas_Object *
_demo_content_get(void *data, Evas_Object *glist, const char *part)
{
   if (strcmp(part, "elm.swallow.content")) return NULL;
   Evas_Object *obj = evas_object_data_get(glist, "widget_exel");
   Evas_Object *row = evisum_ui_widget_exel_item_cache_object_get(obj);
   if (!row) return NULL;
   unsigned int index = (uintptr_t) data;
   char text[64];
   snprintf(text, sizeof(text), "Example process %02u", index + 1);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 0, "cmd", text);
   snprintf(text, sizeof(text), "%u", 1000 + index);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 1, "pid", text);
   double usage = (index * 17 % 101) / 100.0;
   snprintf(text, sizeof(text), "%.0f %%", usage * 100.0);
   evisum_ui_widget_exel_item_field_progress_set(obj, row, 2, "usage", usage, text);
   double rate = (index * 29 % 101) / 100.0;
   snprintf(text, sizeof(text), "%.2f MB/s", rate * 10.0);
   evisum_ui_widget_exel_item_field_progress_set(obj, row, 3, "rate", rate, text);
   return row;
}

static void
_demo_rows_add(void *data)
{
   Evas_Object *obj = data;
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
   itc->item_style = "full";
   itc->func.content_get = _demo_content_get;
   for (uintptr_t i = 0; i < 40; i++)
      evisum_ui_widget_exel_genlist_item_append(obj, itc, (void *) i, NULL,
                                               ELM_GENLIST_ITEM_NONE, NULL, NULL);
   elm_genlist_item_class_free(itc);
}

static void
_demo_fields_applied(void *data EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   if (!*(Eina_Bool *) event_info) return;
   evisum_ui_widget_exel_genlist_clear(obj);
   evisum_ui_widget_exel_item_cache_reset(obj, _demo_rows_add, obj);
}

static void
_demo_columns_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evisum_ui_widget_exel_genlist_realized_items_update(obj);
}

static void
_demo_close(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   elm_exit();
}

static void
_demo_setup(Evas_Object *win)
{
   Evas_Object *obj = evisum_ui_widget_exel_add(win);
   elm_win_resize_object_add(win, obj);
   evisum_ui_widget_exel_field_bounds_set(obj, 0, 4);
   evisum_ui_widget_exel_field_reorder_enabled_set(obj, EINA_TRUE);
   evisum_ui_widget_exel_resize_hit_width_set(obj, 8);
   const char *titles[] = { "Command", "PID (text)", "CPU (progress)", "RX (progress)" };
   const char *keys[] = { "cmd", "pid", "usage", "rate" };
   for (int i = 0; i < 4; i++)
     {
        Evisum_Ui_Widget_Exel_Field_Params params = {
           .id = i, .title = titles[i], .desc = titles[i], .key = keys[i],
           .type = i < 2 ? EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT : EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
           .unit_format = "", .align_x = i ? 1.0 : 0.0,
           .weight_x = EVAS_HINT_EXPAND, .boxed = EINA_TRUE,
           .default_width = i ? 170 : 240, .min_width = 60, .always_visible = !i,
        };
        evisum_ui_widget_exel_field_add(obj, &params);
     }
   evisum_ui_widget_exel_fields_apply(obj);
   Evas_Object *footer = elm_label_add(win);
   elm_object_text_set(footer, "Right-click a header to choose columns. Drag edges to resize; drag headers to reorder.");
   evisum_ui_widget_exel_footer_set(obj, footer);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_APPLIED, _demo_fields_applied, NULL);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZE, _demo_columns_changed, NULL);
   evas_object_smart_callback_add(obj, EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_REORDERED, _demo_columns_changed, NULL);
   _demo_rows_add(obj);
   evas_object_show(obj);
}

int
main(int argc, char **argv)
{
   Eina_Bool test = EINA_FALSE, smoke = EINA_FALSE;
   for (int i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "--test")) test = EINA_TRUE;
        else if (!strcmp(argv[i], "--demo-smoke")) smoke = EINA_TRUE;
        else
          {
             fprintf(stderr, "Usage: %s [--test | --demo-smoke]\n", argv[0]);
             return 1;
          }
     }
   assert(elm_init(argc, argv));
   if (!test && !smoke)
     {
        Evas_Object *demo = elm_win_util_standard_add("exel-demo", "Exel widget examples");
        if (!demo) { elm_shutdown(); return 1; }
        evas_object_smart_callback_add(demo, "delete,request", _demo_close, NULL);
        _demo_setup(demo);
        evas_object_resize(demo, 900, 600);
        evas_object_show(demo);
        elm_run();
        evas_object_del(demo);
        elm_shutdown();
        return 0;
     }
   Ecore_Evas *ee = ecore_evas_buffer_new(640, 480);
   assert(ee);
   Evas_Object *win = elm_win_fake_add(ee);
   assert(win);
   evas_object_resize(win, 640, 480);
   evas_object_show(win);
   if (smoke)
     {
        _demo_setup(win);
        _settle(evas_object_evas_get(win));
        evas_object_del(win);
        ecore_evas_free(ee);
        elm_shutdown();
        puts("exel demo smoke check passed");
        return 0;
     }
   _test_object(win);
   _test_rows_and_input(win);
   _test_parent_lifetime(win);
   _test_cache_external_deletion(win);

   int deletes = 0;
   Evas_Object *obj = evisum_ui_widget_exel_add(win);
   evas_object_show(obj);
   assert(evas_object_visible_get(obj));
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _count_del, &deletes);
   evas_object_del(win);
   assert(deletes == 1);
   ecore_evas_free(ee);
   elm_shutdown();
   puts("exel smart object tests passed");
   return 0;
}
