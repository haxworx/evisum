#include "evisum_ui_util.h"

#include <Elementary.h>
#include "config.h"

Evas_Object *
evisum_ui_tab_add(Evas_Object *parent, Evas_Object **alias, const char *text, Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tb, *rect, *btn, *lb;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);

   rect = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_weight_set(rect, EXPAND, EXPAND);
   evas_object_size_hint_align_set(rect, FILL, FILL);
   evas_object_size_hint_min_set(rect, TAB_BTN_WIDTH * elm_config_scale_get(), TAB_BTN_HEIGHT * elm_config_scale_get());

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

   lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   evas_object_show(lb);
   elm_object_text_set(lb, eina_slstr_printf("%s", text));
   elm_layout_content_set(btn, "elm.swallow.content", lb);

   elm_table_pack(tb, rect, 0, 0, 1, 1);
   elm_table_pack(tb, btn, 0, 0, 1, 1);

   if (alias) *alias = btn;

   return tb;
}

Evas_Object *
evisum_ui_button_add(Evas_Object *parent, Evas_Object **alias, const char *text, const char *icon,
                     Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *tb, *rect, *btn, *lb, *hbx, *ic;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);

   rect = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rect, ELM_SCALE_SIZE(BTN_WIDTH), ELM_SCALE_SIZE(BTN_HEIGHT));

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

   hbx = elm_box_add(parent);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_weight_set(hbx, 0.0, EXPAND);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_show(hbx);

   ic = elm_icon_add(parent);
   evisum_ui_icon_set(ic, icon);
   evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
   evas_object_size_hint_align_set(ic, FILL, FILL);
   evas_object_show(ic);

   elm_box_pack_end(hbx, ic);

   lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, 1.0, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   evas_object_show(lb);
   elm_object_text_set(lb, eina_slstr_printf("%s", text));

   elm_box_pack_end(hbx, lb);
   elm_layout_content_set(btn, "elm.swallow.content", hbx);

   elm_table_pack(tb, rect, 0, 0, 1, 1);
   elm_table_pack(tb, btn, 0, 0, 1, 1);

   if (alias) *alias = btn;

   return tb;
}

Evas_Object *
evisum_ui_background_add(Evas_Object *win)
{
   Evas_Object *bg;

   bg = elm_bg_add(win);
   evas_object_size_hint_weight_set(bg, EXPAND, EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_data_set(win, "bg", bg);
   evas_object_show(bg);

   return bg;
}

void
evisum_ui_icon_size_set(Evas_Object *ic, int size)
{
   evas_object_size_hint_min_set(ic, size, size);
   evas_object_size_hint_max_set(ic, size, size);
}

Evas_Object *
evisum_ui_menu_button_add(Evas_Object *parent, Evas_Smart_Cb clicked_cb, void *data)
{
   Evas_Object *btn, *ic, *lay;

   btn = elm_button_add(parent);
   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, "menu");
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);
   elm_object_focus_allow_set(btn, 0);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   evas_object_smart_callback_add(btn, "clicked", clicked_cb, data);

   lay = elm_layout_add(parent);
   evas_object_size_hint_weight_set(lay, 1.0, 1.0);
   evas_object_size_hint_align_set(lay, 0.99, 0.01);
   elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "cpu");
   elm_layout_content_set(lay, "evisum/menu", btn);

   return lay;
}

void
evisum_ui_menu_hover_update(Evas_Object *layout, Evas_Object *region, Evas_Object *menu,
                            Eina_Bool *visible, const Evas_Event_Mouse_Move *ev)
{
   Evas_Coord x, y, w, h, size;
   Eina_Bool show;

   if (!layout || !region || !visible || !ev) return;

   evas_object_geometry_get(region, &x, &y, &w, &h);
   size = ELM_SCALE_SIZE(UI_MENU_HOVER_SIZE);
   show = (w > 0) && (h > 0) && (ev->cur.canvas.x >= x) && (ev->cur.canvas.x >= x + w - size)
          && (ev->cur.canvas.x <= x + w) && (ev->cur.canvas.y >= y)
          && (ev->cur.canvas.y <= y + h) && (ev->cur.canvas.y <= y + size);
   if (menu) show = EINA_TRUE;
   if (show == *visible) return;

   elm_object_signal_emit(layout, show ? "menu,show" : "menu,hide", "evisum/menu");
   *visible = show;
}
