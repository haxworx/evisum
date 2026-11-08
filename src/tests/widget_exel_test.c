/* Exercise the real widget on an Evas buffer canvas; no display server needed. */
#include <Elementary.h>
#include <Ecore_Evas.h>
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ui/evisum_ui_widget_exel.h"

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
   Evas_Object *apply = evas_object_data_get(check, "apply_icon");
   assert(apply);
   evas_object_smart_callback_call(check, "changed", NULL);
   assert(evisum_ui_widget_exel_fields_mask_get(obj) != mask);
   evas_object_smart_callback_call(apply, "clicked", NULL);
   assert(applies == 1);
   assert(!evisum_ui_widget_exel_field_enabled_get(obj, 1));
   assert(!evisum_ui_widget_exel_fields_menu_visible_get(obj));
   evisum_ui_widget_exel_fields_menu_show(obj, header, &up);
   evisum_ui_widget_exel_fields_menu_dismiss(obj);

   /* A reset with no cache still completes. Pending work dies with the object. */
   evisum_ui_widget_exel_item_cache_reset(obj, _count_call, &reset);
   assert(reset == 1);
   Evas_Object *row = evisum_ui_widget_exel_item_cache_object_get(obj);
   assert(row);
   evisum_ui_widget_exel_item_field_text_set(obj, row, 0, "cmd", "test");
   evisum_ui_widget_exel_item_cache_reset(obj, _count_call, &pending);
   evisum_ui_widget_exel_deferred_call_schedule(obj, 0.001, _count_call, &pending);
   evas_object_del(obj);
   _settle(e);
   assert(deletes == 1 && parts_deleted == 2 && pending == 0);
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

int
main(int argc, char **argv)
{
   assert(elm_init(argc, argv));
   Ecore_Evas *ee = ecore_evas_buffer_new(640, 480);
   assert(ee);
   Evas_Object *win = elm_win_fake_add(ee);
   assert(win);
   evas_object_resize(win, 640, 480);
   evas_object_show(win);
   _test_object(win);
   _test_rows_and_input(win);
   _test_parent_lifetime(win);

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
