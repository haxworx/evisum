# Exel widget

`evisum_ui_widget_exel_add(parent)` returns an **Evas smart object** containing
its header, genlist and optional footer. Use `elm_box_pack_end()`,
`elm_object_content_set()`, `evas_object_show()` and `evas_object_del()` directly.
The parent also destroys the widget. Deletion cancels deferred calls and cache
cleanup, removes input handlers and releases rows and popup menus.

```c
Evas_Object *exel = evisum_ui_widget_exel_add(box);
elm_box_pack_end(box, exel);
evisum_ui_widget_exel_field_bounds_set(exel, 0, FIELD_COUNT);

Evisum_Ui_Widget_Exel_Field_Params command = {
   .id = 0,
   .title = "Command",
   .desc = "Command",
   .key = "command",
   .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,
   .align_x = 0.0,
   .weight_x = EVAS_HINT_EXPAND,
   .boxed = EINA_TRUE,
   .default_width = 180,
   .min_width = 90,
   .always_visible = EINA_TRUE,
};
Evas_Object *header = evisum_ui_widget_exel_field_add(exel, &command);
/* Customize the returned, widget-owned button if needed. */
evisum_ui_widget_exel_fields_apply(exel);
evas_object_show(exel);
```

Register each field before calling `fields_apply()`. A field definition covers
its header and cached row cell. Resizing, right-click field selection and
optional drag reordering are handled inside the widget, including mouse release
outside a header. The first field stays pinned.

Use `header_content_set(exel, button)` for a leading action such as the main
menu and `footer_set(exel, summary)` for a summary panel. Both parts are owned;
replacing or clearing one deletes its old content. Do not pack headers, the
internal list or parts into another container.

## Data and notifications

Keep your model and `Elm_Genlist_Item_Class` callbacks in the view. Populate the
list through `genlist_item_append()` or `genlist_items_ensure()`. In a content
callback, the genlist's `"widget_exel"` data is the public widget object. Fetch a
cached row with `item_cache_object_get()` and update cells using
`item_field_text_set()` or `item_field_progress_set()`.

Use `reference_mask_callback_set()` when the field menu should compare changes
against a saved baseline. Application context can live on the public object with
`evas_object_data_set(exel, "view", view)`; no callback bundle is required.

Use `evas_object_smart_callback_add()` to observe:

| Signal | `event_info` |
| --- | --- |
| `fields,changed` | Borrowed `Eina_Bool *`: unapplied visibility changes |
| `fields,applied` | Borrowed `Eina_Bool *`: whether visibility changed |
| `fields,reordered` | `NULL` |
| `columns,resize` | `NULL` (live update) |
| `columns,resized` | `NULL` (committed width) |
| `selected`, `unselected`, `activated`, `expanded`, `contracted` | `Elm_Object_Item *` |
| `scroll`, `scroll,anim,stop`, `scroll,drag,stop` | Same payload as the genlist |

Signal names for fields and resizing have `EVISUM_UI_WIDGET_EXEL_EVENT_*`
constants in the header. Signals report the exel as the callback object.
`genlist_obj_get()` remains available for operations specific to Elementary.

For saved column preferences, bind the visibility mask, width array and order
array using `state_bind()` and `field_order_bind()`. Bound arrays and callback
data must outlive the widget. Their indices are field IDs, not display positions.
Delete the widget before releasing a view's model and bound state.

## Compatibility

The former `Evisum_Ui_Widget_Exel` type aliases `Evas_Object`. `create()` creates
and shows the smart object, `object_get()` returns that same object, and `free()`
calls `evas_object_del()`. `genlist_add()` returns the already-created list and
ignores its parent argument; remove old code that packs that list separately.
Legacy `callbacks_set()` callbacks remain supported alongside smart callbacks.

`src/tests/widget_test.c` and `widget_test2.c` show full data-driven examples.
Run `meson test -C build 'exel smart object' --print-errorlogs` for the headless
lifecycle and interaction regression test.
