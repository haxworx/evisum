#ifndef __EVISUM_UI_WIDGET_EXEL_H__
#define __EVISUM_UI_WIDGET_EXEL_H__

/* Copyright 2026. Alastair Poole (Codex-generated). */

#include <Elementary.h>
#include "evisum_ui_cache.h"

typedef struct _Evisum_Ui_Widget_Exel Evisum_Ui_Widget_Exel;

typedef struct {
    Eina_Bool boxed;
    Evas_Coord boxed_spacer;
    double align_x;
    double align_y;
    double weight_x;
    double weight_y;
} Evisum_Ui_Widget_Exel_Column_Params;

typedef enum {
    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_CMD,
} Evisum_Ui_Widget_Exel_Item_Cell_Type;

typedef struct {
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
typedef void (*Evisum_Ui_Widget_Exel_Fields_Changed_Cb)(void *data, Eina_Bool changed);
typedef void (*Evisum_Ui_Widget_Exel_Fields_Applied_Cb)(void *data, Eina_Bool changed);
typedef void (*Evisum_Ui_Widget_Exel_Resize_Live_Cb)(void *data);
typedef void (*Evisum_Ui_Widget_Exel_Resize_Done_Cb)(void *data);

typedef struct {
    Evas_Object *win;
    Evas_Object *parent;
    int field_first;
    int field_max;
    int resize_hit_width;
    unsigned int *fields_mask;
    int *field_widths;

    Evisum_Ui_Widget_Exel_Reference_Mask_Get_Cb reference_mask_get_cb;
    Evisum_Ui_Widget_Exel_Fields_Changed_Cb fields_changed_cb;
    Evisum_Ui_Widget_Exel_Fields_Applied_Cb fields_applied_cb;
    Evisum_Ui_Widget_Exel_Resize_Live_Cb resize_live_cb;
    Evisum_Ui_Widget_Exel_Resize_Done_Cb resize_done_cb;
    void *data;
} Evisum_Ui_Widget_Exel_Params;

typedef struct {
    Eina_Bool homogeneous;
    Eina_Bool focus_allow;
    Eina_Bool multi_select;
    Eina_Bool bounce_h;
    Eina_Bool bounce_v;
    Elm_Object_Select_Mode select_mode;
    Elm_Scroller_Policy policy_h;
    Elm_Scroller_Policy policy_v;
    double weight_x;
    double weight_y;
    double align_x;
    double align_y;
    const char *data_key;
    const void *data;
} Evisum_Ui_Widget_Exel_Genlist_Params;

/* Create and initialize the exel widget controller with field masks, widths, and callbacks.
 * Use this once per view before registering fields so resize/menu behavior is managed centrally. */
Evisum_Ui_Widget_Exel *evisum_ui_widget_exel_add(const Evisum_Ui_Widget_Exel_Params *params);

/* Release the exel widget controller and any menu state it owns.
 * Call this during view teardown to avoid leaked UI objects and stale pointers. */
void evisum_ui_widget_exel_free(Evisum_Ui_Widget_Exel *wx);

/* Register a field header/button and its sizing constraints with the controller.
 * The controller uses this metadata for show/hide state, proportional sizing, and resizing rules. */
void evisum_ui_widget_exel_field_register(Evisum_Ui_Widget_Exel *wx, int id, Evas_Object *btn, const char *desc,
                                          int default_width, int min_width, Eina_Bool always_visible);

/* Query whether a field is currently enabled and should be visible in the active layout.
 * Use this when building row content or packing headers so hidden fields stay consistent. */
Eina_Bool evisum_ui_widget_exel_field_enabled_get(const Evisum_Ui_Widget_Exel *wx, int id);

/* Return true when `id` is the current trailing visible field in the active layout.
 * Use this to apply stretch behavior without keeping a mirrored `fields_max` in consumers. */
Eina_Bool evisum_ui_widget_exel_field_is_last_visible(const Evisum_Ui_Widget_Exel *wx, int id);

/* Pack all currently enabled field headers into a table starting at `col_start`.
 * Disabled field headers are hidden automatically and the returned value is the next free column index. */
int evisum_ui_widget_exel_fields_pack_visible(Evisum_Ui_Widget_Exel *wx, Evas_Object *tb, int col_start);

/* Return the current geometry width for a registered field header.
 * Use this to align row cell widths without accessing header button objects directly. */
Evas_Coord evisum_ui_widget_exel_field_width_get(const Evisum_Ui_Widget_Exel *wx, int id);

/* Set a minimum width on a registered field header button.
 * Use this when content outgrows header size so columns can expand consistently. */
void evisum_ui_widget_exel_field_min_width_set(Evisum_Ui_Widget_Exel *wx, int id, Evas_Coord width);

/* Apply the stored width for one field header while respecting its minimum width.
 * Call this after registration or config load to sync button geometry with persisted widths. */
void evisum_ui_widget_exel_field_width_apply(Evisum_Ui_Widget_Exel *wx, int id);

/* Recompute header button weights from active field widths.
 * Call this after toggling fields or resizing so all columns keep correct proportions. */
void evisum_ui_widget_exel_field_proportions_apply(Evisum_Ui_Widget_Exel *wx);

/* Expand the command/title column width when content grows beyond the current width.
 * This updates internal width state and reapplies proportions without shrinking existing widths. */
void evisum_ui_widget_exel_cmd_width_sync(Evisum_Ui_Widget_Exel *wx, int field_cmd_id, Evas_Coord width);

/* Attach resize hit-zone mouse handling to a header button for a specific field id.
 * After attaching, the shared controller handles edge drag detection for left/right resize starts. */
void evisum_ui_widget_exel_field_resize_attach(Evisum_Ui_Widget_Exel *wx, Evas_Object *btn, int field_id);

/* Process global mouse-move events while a header resize drag is active.
 * Wire this from the window mouse-move callback so live column resizing updates instantly. */
void evisum_ui_widget_exel_field_resize_mouse_move(Evisum_Ui_Widget_Exel *wx, Evas_Event_Mouse_Move *ev);

/* Process global mouse-up events to finish an active header resize drag.
 * This commits final width state and triggers configured resize-done callbacks. */
void evisum_ui_widget_exel_field_resize_mouse_up(Evisum_Ui_Widget_Exel *wx, Evas_Event_Mouse_Up *ev);

/* Consume and clear the internal "ignore next sort click" flag set after resizing.
 * Use this at sort-click entry points to prevent accidental sort toggles on drag release. */
Eina_Bool evisum_ui_widget_exel_sort_ignore_consume(Evisum_Ui_Widget_Exel *wx);

/* Show the shared show/hide-fields popup menu anchored to a header button.
 * Pass the original mouse-up event so right-click behavior stays consistent across views. */
void evisum_ui_widget_exel_fields_menu_show(Evisum_Ui_Widget_Exel *wx, Evas_Object *anchor_btn,
                                            Evas_Event_Mouse_Up *ev);

/* Dismiss the show/hide-fields popup if it is visible.
 * Call this during view resize, focus changes, or tab switches to keep UI state clean. */
void evisum_ui_widget_exel_fields_menu_dismiss(Evisum_Ui_Widget_Exel *wx);

/* Check whether the fields popup menu is currently visible.
 * Use this to short-circuit conflicting interactions like header sort clicks. */
Eina_Bool evisum_ui_widget_exel_fields_menu_visible_get(const Evisum_Ui_Widget_Exel *wx);

/* Build a table column content widget and register it under a key for later updates.
 * This centralizes label/rectangle packing so row-cell construction stays consistent. */
Evas_Object *evisum_ui_widget_exel_item_column_add(Evas_Object *tb, const char *key, int col,
                                                   const Evisum_Ui_Widget_Exel_Column_Params *params);

/* Build a full item row table from an ordered list of cell definitions.
 * Use this to keep row layout declarative so callers only describe cells instead of constructing widgets manually. */
Evas_Object *evisum_ui_widget_exel_item_row_add(Evas_Object *parent, const Evisum_Ui_Widget_Exel_Item_Cell_Def *defs,
                                                unsigned int count);

/* Create a command-style column that packs an icon and label into one table cell and stores both by key.
 * Use this for leading columns that need app icon + text without repeating row layout code in each view. */
Evas_Object *evisum_ui_widget_exel_item_cmd_column_add(Evas_Object *tb, const char *icon_key, const char *label_key,
                                                       int col, int icon_size, int spacer);

/* Create a progressbar column packed in a table cell and register it under a key for updates.
 * Use this for percentage/usage columns so the row template stays consistent across consumers. */
Evas_Object *evisum_ui_widget_exel_item_progress_column_add(Evas_Object *tb, const char *key, int col,
                                                            const char *unit_format);

/* Set a text object value only when it changed and return whether an update happened.
 * Use this when callers already have the target label/proxy object and should not do key lookups. */
Eina_Bool evisum_ui_widget_exel_item_text_object_if_changed_set(Evas_Object *obj, const char *text);

/* Set a progressbar value/status only when needed and return whether it updated.
 * Use this to avoid repeated value churn and duplicated status formatting checks in views. */
Eina_Bool evisum_ui_widget_exel_item_progress_if_changed_set(Evas_Object *pb, double value, const char *status);

/* Return a row child object stored under `key` on the row container.
 * Use this instead of direct `evas_object_data_get` so row object access stays explicit and centralized. */
Evas_Object *evisum_ui_widget_exel_item_object_get(Evas_Object *row, const char *key);

/* Return a row cell for a field and apply header width/stretch rules in one step.
 * Use this from content callbacks to avoid repeating field width and trailing-column logic. */
Evas_Object *evisum_ui_widget_exel_item_field_cell_get(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id,
                                                       const char *key);

/* Set text into a field cell by key, applying width rules and changed-only updates.
 * This is the compact default path for updating text fields in exel-backed rows. */
Eina_Bool evisum_ui_widget_exel_item_field_text_set(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id,
                                                    const char *key, const char *text);

/* Set progress value/status into a field cell by key with width handling and changed-only updates.
 * Use this for progressbar fields so content code stays minimal and consistent with text fields. */
Eina_Bool evisum_ui_widget_exel_item_field_progress_set(Evisum_Ui_Widget_Exel *wx, Evas_Object *row, int field_id,
                                                        const char *key, double value, const char *status);

/* Apply width constraints to a row column object using its bound background rectangle.
 * Pass `is_last` for trailing columns that should stretch and all others will be fixed to header width. */
void evisum_ui_widget_exel_item_column_width_apply(Evas_Object *obj, Evas_Coord width, Eina_Bool is_last);

/* Ensure a genlist has exactly the requested item count using a shared item class.
 * Use this before setting item data so add/remove behavior stays uniform across views. */
void evisum_ui_widget_exel_glist_ensure_n_items(Evas_Object *glist, unsigned int items, Elm_Genlist_Item_Class *itc);

/* Add a menu item with optional themed icon and fallback icon behavior.
 * This keeps process menus consistent and avoids repeated icon-part compatibility code. */
Elm_Object_Item *evisum_ui_widget_exel_menu_item_add(Evas_Object *menu, Elm_Object_Item *parent, const char *icon,
                                                     const char *label, Evas_Smart_Cb func, const void *data);

/* Apply sort direction icon state to an existing header button.
 * Use this whenever sort direction changes so the title button reflects current order. */
void evisum_ui_widget_exel_sort_header_button_state_set(Evas_Object *btn, Eina_Bool reverse);

/* Create a sortable header/title button with text, icon state, and click callback.
 * This is the default constructor for column-title sort buttons in exel-based views. */
Evas_Object *evisum_ui_widget_exel_sort_header_button_add(Evas_Object *parent, const char *text, Eina_Bool reverse,
                                                          double weight_x, double align_x, Evas_Smart_Cb clicked_cb,
                                                          const void *data);

/* Create a compact icon button with optional tooltip and click callback.
 * Use this for optional action buttons such as the process-list menu trigger. */
Evas_Object *evisum_ui_widget_exel_icon_button_add(Evas_Object *parent, const char *icon, const char *tooltip,
                                                   double weight_x, double align_x, Evas_Smart_Cb clicked_cb,
                                                   const void *data);

/* Create and attach an item cache to this widget controller.
 * Use this when row content objects are pooled and should be owned by the widget lifecycle. */
void evisum_ui_widget_exel_item_cache_add(Evisum_Ui_Widget_Exel *wx, Evas_Object *parent,
                                          Evas_Object *(*create_cb)(Evas_Object *), int size);

/* Create and attach an item cache to the widget-owned genlist.
 * Use this when the exel controller already owns the target genlist and callers should not pass list objects around. */
void evisum_ui_widget_exel_item_cache_attach(Evisum_Ui_Widget_Exel *wx, Evas_Object *(*create_cb)(Evas_Object *),
                                             int size);

/* Return one cached row object from the widget-owned item cache.
 * Use this in content callbacks to avoid exposing `Item_Cache` internals in consumers. */
Evas_Object *evisum_ui_widget_exel_item_cache_object_get(Evisum_Ui_Widget_Exel *wx);

/* Release a realized row object back to the widget-owned cache.
 * Returns true when the object was found and handled by the cache. */
Eina_Bool evisum_ui_widget_exel_item_cache_item_release(Evisum_Ui_Widget_Exel *wx, Evas_Object *obj);

/* Reset and rebuild the widget-owned cache and invoke completion callback when cleanup completes.
 * Use this after column/field layout changes so row object structure is recreated safely. */
void evisum_ui_widget_exel_item_cache_reset(Evisum_Ui_Widget_Exel *wx, void (*done_cb)(void *data), void *data);

/* Steal non-realized cache objects using the provided realized object list.
 * Use this to cap cache growth under churn without leaking stale pooled objects. */
void evisum_ui_widget_exel_item_cache_steal(Evisum_Ui_Widget_Exel *wx, Eina_List *objs);

/* Return the number of active objects currently tracked by the widget-owned cache.
 * This is useful for deciding when to compact cache state after feedback updates. */
unsigned int evisum_ui_widget_exel_item_cache_active_count_get(const Evisum_Ui_Widget_Exel *wx);

/* Create and own a genlist configured from one parameter block and return the created object.
 * Use this so exel consumers do not manually construct genlists and keep list setup centralized. */
Evas_Object *evisum_ui_widget_exel_genlist_add(Evisum_Ui_Widget_Exel *wx, Evas_Object *parent,
                                               const Evisum_Ui_Widget_Exel_Genlist_Params *params);

/* Return the widget-owned genlist object, or NULL if none was created yet.
 * Use this only when another container API requires the object for packing. */
Evas_Object *evisum_ui_widget_exel_genlist_obj_get(const Evisum_Ui_Widget_Exel *wx);

/* Add a smart callback to the widget-owned genlist.
 * This keeps callback registration scoped to the exel controller instead of direct genlist usage. */
void evisum_ui_widget_exel_genlist_smart_callback_add(Evisum_Ui_Widget_Exel *wx, const char *name, Evas_Smart_Cb cb,
                                                      const void *data);

/* Add an evas event callback to the widget-owned genlist.
 * Use this for low-level mouse/key handling without exposing genlist ownership. */
void evisum_ui_widget_exel_genlist_event_callback_add(Evisum_Ui_Widget_Exel *wx, Evas_Callback_Type type,
                                                      Evas_Object_Event_Cb cb, const void *data);

/* Clear all items in the widget-owned genlist.
 * Call this when field layout or data model resets to rebuild list content from scratch. */
void evisum_ui_widget_exel_genlist_clear(Evisum_Ui_Widget_Exel *wx);

/* Ensure the widget-owned genlist has exactly `items` entries for a given item class.
 * Use this to normalize list size before assigning per-item data. */
void evisum_ui_widget_exel_genlist_items_ensure(Evisum_Ui_Widget_Exel *wx, unsigned int items,
                                                Elm_Genlist_Item_Class *itc);

/* Return the first item in the widget-owned genlist.
 * Use this as the starting point when applying updated row data in order. */
Elm_Object_Item *evisum_ui_widget_exel_genlist_first_item_get(const Evisum_Ui_Widget_Exel *wx);

/* Update all realized items in the widget-owned genlist.
 * Call this after data changes that should refresh currently visible rows. */
void evisum_ui_widget_exel_genlist_realized_items_update(Evisum_Ui_Widget_Exel *wx);

/* Return the currently realized items list for the widget-owned genlist.
 * The returned list should be freed by the caller after use. */
Eina_List *evisum_ui_widget_exel_genlist_realized_items_get(const Evisum_Ui_Widget_Exel *wx);

/* Scroll the widget-owned genlist to a page location.
 * Use this to reset list position after sort changes. */
void evisum_ui_widget_exel_genlist_page_bring_in(Evisum_Ui_Widget_Exel *wx, int h_pagenumber, int v_pagenumber);

/* Get current scroller region geometry from the widget-owned genlist.
 * Use this when implementing keyboard page-up/page-down behavior. */
void evisum_ui_widget_exel_genlist_region_get(const Evisum_Ui_Widget_Exel *wx, Evas_Coord *x, Evas_Coord *y,
                                              Evas_Coord *w, Evas_Coord *h);

/* Bring a region into view in the widget-owned genlist.
 * Use this to navigate by explicit scroll offsets without touching raw genlist APIs. */
void evisum_ui_widget_exel_genlist_region_bring_in(Evisum_Ui_Widget_Exel *wx, Evas_Coord x, Evas_Coord y, Evas_Coord w,
                                                   Evas_Coord h);

/* Set scroller policies for the widget-owned genlist.
 * This keeps runtime policy changes encapsulated in the exel API. */
void evisum_ui_widget_exel_genlist_policy_set(Evisum_Ui_Widget_Exel *wx, Elm_Scroller_Policy policy_h,
                                              Elm_Scroller_Policy policy_v);

/* Return the item at the given output coordinates in the widget-owned genlist.
 * Use this for context-menu hit-testing without touching raw genlist APIs. */
Elm_Object_Item *evisum_ui_widget_exel_genlist_at_xy_item_get(Evisum_Ui_Widget_Exel *wx, int x, int y, int *posret);

/* Return the next item after `it` in the widget-owned genlist.
 * Use this while iterating sequentially through list items during data updates. */
Elm_Object_Item *evisum_ui_widget_exel_genlist_item_next_get(Elm_Object_Item *it);

/* Set selection state for a genlist item.
 * Use this to clear selection after handling clicks in a unified helper path. */
void evisum_ui_widget_exel_genlist_item_selected_set(Elm_Object_Item *it, Eina_Bool selected);

/* Trigger visual/data refresh for a genlist item.
 * Use this after swapping row data on an existing item. */
void evisum_ui_widget_exel_genlist_item_update(Elm_Object_Item *it);

/* Unset all realized content objects from a genlist item and return them in `contents`.
 * Use this in unrealize callbacks before returning objects to the shared item cache. */
void evisum_ui_widget_exel_genlist_item_all_contents_unset(Elm_Object_Item *it, Eina_List **contents);

/* Append a new item to the widget-owned genlist using Elm-style append parameters.
 * Pass `func`/`func_data` when per-item selection callbacks are needed, or NULL to match existing behavior. */
Elm_Object_Item *evisum_ui_widget_exel_genlist_item_append(Evisum_Ui_Widget_Exel *wx, Elm_Genlist_Item_Class *itc,
                                                           const void *data, Elm_Object_Item *parent,
                                                           Elm_Genlist_Item_Type type, Evas_Smart_Cb func,
                                                           const void *func_data);

/* Get item data pointer for a genlist item.
 * Use this helper to keep item payload access consistent in exel consumers. */
void *evisum_ui_widget_exel_object_item_data_get(Elm_Object_Item *it);

/* Set item data pointer for a genlist item.
 * Use this when replacing payloads during periodic list updates in the same way as `elm_object_item_data_set`. */
void evisum_ui_widget_exel_object_item_data_set(Elm_Object_Item *it, void *data);

/* Schedule a one-shot deferred callback owned by the widget and replace any pending one.
 * Use this for field-layout settling timers so consumer files no longer manage timer objects directly. */
void evisum_ui_widget_exel_deferred_call_schedule(Evisum_Ui_Widget_Exel *wx, double delay_seconds,
                                                  void (*cb)(void *data), void *data);

/* Cancel any pending widget-owned deferred callback.
 * Call this when abandoning pending UI work, though widget free also cancels it automatically. */
void evisum_ui_widget_exel_deferred_call_cancel(Evisum_Ui_Widget_Exel *wx);

#endif
