#ifndef __EVISUM_UI_WIDGET_EXEL_H__
#define __EVISUM_UI_WIDGET_EXEL_H__

/* Copyright 2026. Alastair Poole (Codex-generated). */

#include <Elementary.h>

/* An exel is an Evas smart object. Pack, show, hide and delete it normally. */

/* Smart callbacks: fields,changed / fields,applied (event_info: Eina_Bool *),
 * fields,reordered / columns,resize / columns,resized (event_info: NULL).
 * Standard genlist signals (selected, unselected, activated, expanded, contracted,
 * scroll, scroll,anim,stop, scroll,drag,stop) are forwarded with their event_info.
 * Event data is borrowed for the duration of the callback. */
#define EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_CHANGED   "fields,changed"
#define EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_APPLIED   "fields,applied"
#define EVISUM_UI_WIDGET_EXEL_EVENT_FIELDS_REORDERED "fields,reordered"
#define EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZE   "columns,resize"
#define EVISUM_UI_WIDGET_EXEL_EVENT_COLUMNS_RESIZED  "columns,resized"

/* Create a widget containing its header and genlist. Hidden until shown.
 * The parent owns its lifetime; deleting the widget cancels its pending work. */
Evas_Object *
evisum_ui_widget_exel_add(Evas_Object *parent);

/* Optional leading header control and footer, owned by the widget.
 * Replacing a part deletes its previous content. NULL clears the part. */
void
evisum_ui_widget_exel_header_content_set(Evas_Object *obj, Evas_Object *content);

void
evisum_ui_widget_exel_footer_set(Evas_Object *obj, Evas_Object *content);

typedef enum
{
   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_CMD,
} Evisum_Ui_Widget_Exel_Item_Cell_Type;

typedef struct
{
   Evisum_Ui_Widget_Exel_Item_Cell_Type type;
   const char *key;
   const char *aux_key;
   const char *unit_format;
   double align_x;
   double weight_x;
   Eina_Bool boxed;
   int spacer;
   int icon_size;
} Evisum_Ui_Widget_Exel_Item_Cell_Def;

typedef unsigned int (*Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb)(void *data);

typedef struct
{
   int id;
   const char *title;
   const char *desc;
   const char *key;
   const char *aux_key;
   const char *unit_format;
   Evisum_Ui_Widget_Exel_Item_Cell_Type type;
   double align_x;
   double weight_x;
   Eina_Bool boxed;
   int spacer;
   int icon_size;
   int default_width;
   int min_width;
   Eina_Bool always_visible;
   Eina_Bool reverse;
   Evas_Smart_Cb clicked_cb;
   const void *clicked_data;
} Evisum_Ui_Widget_Exel_Field_Params;

/* Add a field and its row definition; return its header button for optional customization.
 * The widget owns the button and attaches resize, reorder and context-menu behavior. */
Evas_Object *
evisum_ui_widget_exel_field_add(Evas_Object *obj, const Evisum_Ui_Widget_Exel_Field_Params *params);

/* Apply width/proportion/pack operations for all registered headers in one call.
 * Call once after field registration and whenever field visibility changes are applied. */
void
evisum_ui_widget_exel_fields_apply(Evas_Object *obj);

/* Return the current field visibility mask tracked by the widget.
 * Use this when persisting user field selection without direct access to internal params. */
unsigned int
evisum_ui_widget_exel_fields_mask_get(const Evas_Object *obj);

/* Bind an external fields mask/widths state to the widget.
 * Pass NULL pointers to keep/create widget-owned state. */
void
evisum_ui_widget_exel_state_bind(Evas_Object *obj, unsigned int *fields_mask, int *field_widths);

/* Configure the active field id range used by the widget.
 * Must be called before registering fields when using non-default ranges. */
void
evisum_ui_widget_exel_field_bounds_set(Evas_Object *obj, int field_first, int field_max);

/* Bind an external ordered list of field ids keyed by display position.
 * Pass NULL to keep/create widget-owned order state. */
void
evisum_ui_widget_exel_field_order_bind(Evas_Object *obj, int *field_order);

/* Enable/disable drag-reordering of field headers.
 * Reordering is opt-in so existing views keep current interaction behavior. */
void
evisum_ui_widget_exel_field_reorder_enabled_set(Evas_Object *obj, Eina_Bool enabled);

/* Set header-resize hit width in pixels for edge drag detection.
 * Keep zero to disable resize hit-testing. */
void
evisum_ui_widget_exel_resize_hit_width_set(Evas_Object *obj, int hit_width);

/* Optional baseline for determining unapplied visibility changes. */
void
evisum_ui_widget_exel_reference_mask_callback_set(Evas_Object *obj,
                                                 Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb cb, void *data);

/* Query whether a field is currently enabled and should be visible in the active layout.
 * Use this when building row content or packing headers so hidden fields stay consistent. */
Eina_Bool
evisum_ui_widget_exel_field_enabled_get(const Evas_Object *obj, int id);

/* Return true when `id` is the current trailing visible field in the active layout.
 * Use this to apply stretch behavior without keeping a mirrored `fields_max` in consumers. */
Eina_Bool
evisum_ui_widget_exel_field_is_last_visible(const Evas_Object *obj, int id);

/* Return the current geometry width for a registered field header.
 * Use this to align row cell widths without accessing header button objects directly. */
Evas_Coord
evisum_ui_widget_exel_field_width_get(const Evas_Object *obj, int id);

/* Set a minimum width on a registered field header button.
 * Use this when content outgrows header size so columns can expand consistently. */
void
evisum_ui_widget_exel_field_min_width_set(Evas_Object *obj, int id, Evas_Coord width);

/* Expand the command/title column width when content grows beyond the current width.
 * This updates internal width state and reapplies proportions without shrinking existing widths. */
void
evisum_ui_widget_exel_cmd_width_sync(Evas_Object *obj, int field_cmd_id, Evas_Coord width);

/* Consume and clear the internal "ignore next sort click" flag set after resizing.
 * Use this at sort-click entry points to prevent accidental sort toggles on drag release. */
Eina_Bool
evisum_ui_widget_exel_sort_ignore_consume(Evas_Object *obj);

/* Show the shared show/hide-fields popup menu anchored to a header button.
 * Pass the original mouse-up event so right-click behavior stays consistent across views. */
void
evisum_ui_widget_exel_fields_menu_show(Evas_Object *obj, Evas_Object *anchor_btn, Evas_Event_Mouse_Up *ev);

/* Dismiss the show/hide-fields popup if it is visible.
 * Call this during view resize, focus changes, or tab switches to keep UI state clean. */
void
evisum_ui_widget_exel_fields_menu_dismiss(Evas_Object *obj);

/* Check whether the fields popup menu is currently visible.
 * Use this to short-circuit conflicting interactions like header sort clicks. */
Eina_Bool
evisum_ui_widget_exel_fields_menu_visible_get(const Evas_Object *obj);

/* Set a text object value only when it changed and return whether an update happened.
 * Use this when callers already have the target label/proxy object and should not do key lookups. */
Eina_Bool
evisum_ui_widget_exel_item_text_object_if_changed_set(Evas_Object *obj, const char *text);

/* Return a row child object stored under `key` on the row container.
 * Use this instead of direct `evas_object_data_get` so row object access stays explicit and centralized. */
Evas_Object *
evisum_ui_widget_exel_item_object_get(Evas_Object *row, const char *key);

/* Return a row cell for a field and apply header width/stretch rules in one step.
 * Use this from content callbacks to avoid repeating field width and trailing-column logic. */
Evas_Object *
evisum_ui_widget_exel_item_field_cell_get(Evas_Object *obj, Evas_Object *row, int field_id, const char *key);

/* Set text into a field cell by key, applying width rules and changed-only updates.
 * This is the compact default path for updating text fields in exel-backed rows. */
Eina_Bool
evisum_ui_widget_exel_item_field_text_set(Evas_Object *obj, Evas_Object *row, int field_id, const char *key,
                                          const char *text);

/* Set progress value/status into a field cell by key with width handling and changed-only updates.
 * Use this for progressbar fields so content code stays minimal and consistent with text fields. */
Eina_Bool
evisum_ui_widget_exel_item_field_progress_set(Evas_Object *obj, Evas_Object *row, int field_id,
                                              const char *key, double value, const char *status);

/* Apply width constraints to a row column object using its bound background rectangle.
 * Pass `is_last` for trailing columns that should stretch and all others will be fixed to header width. */
void
evisum_ui_widget_exel_item_column_width_apply(Evas_Object *obj, Evas_Coord width, Eina_Bool is_last);

/* Add a menu item with optional themed icon and fallback icon behavior.
 * This keeps process menus consistent and avoids repeated icon-part compatibility code. */
Elm_Object_Item *
evisum_ui_widget_exel_menu_item_add(Evas_Object *menu, Elm_Object_Item *parent, const char *icon, const char *label,
                                    Evas_Smart_Cb func, const void *data);

/* Apply sort direction icon state to an existing header button.
 * Use this whenever sort direction changes so the title button reflects current order. */
void
evisum_ui_widget_exel_sort_header_button_state_set(Evas_Object *btn, Eina_Bool reverse);

/* Create a compact icon button with optional tooltip and click callback.
 * Use this for optional action buttons such as the process-list menu trigger. */
Evas_Object *
evisum_ui_widget_exel_icon_button_add(Evas_Object *parent, const char *icon, const char *tooltip, double weight_x,
                                      double align_x, Evas_Smart_Cb clicked_cb, const void *data);

/* Return one cached row object from the widget-owned item cache.
 * Use this in content callbacks to avoid exposing `Item_Cache` internals in consumers. */
Evas_Object *
evisum_ui_widget_exel_item_cache_object_get(Evas_Object *obj);

/* Reset and rebuild the widget-owned cache and invoke completion callback when cleanup completes.
 * Use this after column/field layout changes so row object structure is recreated safely. */
void
evisum_ui_widget_exel_item_cache_reset(Evas_Object *obj, void (*done_cb)(void *data), void *data);

/* Steal non-realized cache objects using the provided realized genlist item list.
 * Use this to cap cache growth under churn without leaking stale pooled objects. */
void
evisum_ui_widget_exel_item_cache_steal(Evas_Object *obj, Eina_List *items);

/* Return the number of active objects currently tracked by the widget-owned cache.
 * This is useful for deciding when to compact cache state after feedback updates. */
unsigned int
evisum_ui_widget_exel_item_cache_active_count_get(const Evas_Object *obj);

/* Return the widget-owned genlist object, or NULL if none was created yet.
 * Use this only when another container API requires the object for packing. */
Evas_Object *
evisum_ui_widget_exel_genlist_obj_get(const Evas_Object *obj);

/* Add an evas event callback to the widget-owned genlist.
 * Use this for low-level mouse/key handling without exposing genlist ownership. */
void
evisum_ui_widget_exel_genlist_event_callback_add(Evas_Object *obj, Evas_Callback_Type type,
                                                 Evas_Object_Event_Cb cb, const void *data);

/* Clear all items in the widget-owned genlist.
 * Call this when field layout or data model resets to rebuild list content from scratch. */
void
evisum_ui_widget_exel_genlist_clear(Evas_Object *obj);

/* Ensure the widget-owned genlist has exactly `items` entries for a given item class.
 * Use this to normalize list size before assigning per-item data. */
void
evisum_ui_widget_exel_genlist_items_ensure(Evas_Object *obj, unsigned int items, Elm_Genlist_Item_Class *itc);

/* Return the first item in the widget-owned genlist.
 * Use this as the starting point when applying updated row data in order. */
Elm_Object_Item *
evisum_ui_widget_exel_genlist_first_item_get(const Evas_Object *obj);

/* Update all realized items in the widget-owned genlist.
 * Call this after data changes that should refresh currently visible rows. */
void
evisum_ui_widget_exel_genlist_realized_items_update(Evas_Object *obj);

/* Return the currently realized items list for the widget-owned genlist.
 * The returned list should be freed by the caller after use. */
Eina_List *
evisum_ui_widget_exel_genlist_realized_items_get(const Evas_Object *obj);

/* Scroll the widget-owned genlist to a page location.
 * Use this to reset list position after sort changes. */
void
evisum_ui_widget_exel_genlist_page_bring_in(Evas_Object *obj, int h_pagenumber, int v_pagenumber);

/* Get current scroller region geometry from the widget-owned genlist.
 * Use this when implementing keyboard page-up/page-down behavior. */
void
evisum_ui_widget_exel_genlist_region_get(const Evas_Object *obj, Evas_Coord *x, Evas_Coord *y, Evas_Coord *w,
                                         Evas_Coord *h);

/* Bring a region into view in the widget-owned genlist.
 * Use this to navigate by explicit scroll offsets without touching raw genlist APIs. */
void
evisum_ui_widget_exel_genlist_region_bring_in(Evas_Object *obj, Evas_Coord x, Evas_Coord y, Evas_Coord w,
                                              Evas_Coord h);

/* Set scroller policies for the widget-owned genlist.
 * This keeps runtime policy changes encapsulated in the exel API. */
void
evisum_ui_widget_exel_genlist_policy_set(Evas_Object *obj, Elm_Scroller_Policy policy_h,
                                         Elm_Scroller_Policy policy_v);

/* Return the item at the given output coordinates in the widget-owned genlist.
 * Use this for context-menu hit-testing without touching raw genlist APIs. */
Elm_Object_Item *
evisum_ui_widget_exel_genlist_at_xy_item_get(Evas_Object *obj, int x, int y, int *posret);

/* Return the next item after `it` in the widget-owned genlist.
 * Use this while iterating sequentially through list items during data updates. */
Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_next_get(Elm_Object_Item *it);

/* Set selection state for a genlist item.
 * Use this to clear selection after handling clicks in a unified helper path. */
void
evisum_ui_widget_exel_genlist_item_selected_set(Elm_Object_Item *it, Eina_Bool selected);

/* Trigger visual/data refresh for a genlist item.
 * Use this after swapping row data on an existing item. */
void
evisum_ui_widget_exel_genlist_item_update(Elm_Object_Item *it);

/* Append a new item to the widget-owned genlist using Elm-style append parameters.
 * Pass `func`/`func_data` when per-item selection callbacks are needed, or NULL to match existing behavior. */
Elm_Object_Item *
evisum_ui_widget_exel_genlist_item_append(Evas_Object *obj, Elm_Genlist_Item_Class *itc, const void *data,
                                          Elm_Object_Item *parent, Elm_Genlist_Item_Type type, Evas_Smart_Cb func,
                                          const void *func_data);

/* Get item data pointer for a genlist item.
 * Use this helper to keep item payload access consistent in exel consumers. */
void *
evisum_ui_widget_exel_object_item_data_get(Elm_Object_Item *it);

/* Set item data pointer for a genlist item.
 * Use this when replacing payloads during periodic list updates in the same way as `elm_object_item_data_set`. */
void
evisum_ui_widget_exel_object_item_data_set(Elm_Object_Item *it, void *data);

/* Schedule a one-shot deferred callback owned by the widget and replace any pending one.
 * Use this for field-layout settling timers so consumer files no longer manage timer objects directly. */
void
evisum_ui_widget_exel_deferred_call_schedule(Evas_Object *obj, double delay_seconds, void (*cb)(void *data),
                                             void *data);

/* Cancel any pending widget-owned deferred callback.
 * Call this when abandoning pending UI work, though widget free also cancels it automatically. */
void
evisum_ui_widget_exel_deferred_call_cancel(Evas_Object *obj);

#endif
