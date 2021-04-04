#include "config.h"
#include "evisum_actions.h"
#include "evisum_config.h"
#include "evisum_server.h"

#include "system/filesystems.h"

#include "evisum_ui.h"
#include "ui/ui_cpu.h"
#include "ui/ui_memory.h"
#include "ui/ui_disk.h"
#include "ui/ui_sensors.h"
#include "ui/ui_network.h"
#include "ui/ui_process_view.h"
#include "ui/ui_process_list.h"

int EVISUM_EVENT_CONFIG_CHANGED;

static Evas_Object *_slider_alpha = NULL;

void
evisum_ui_config_save(Evisum_Ui *ui)
{
   Eina_Bool notify = 0;

   if (!config()) return;

   config()->effects = ui->effects;
   config()->backgrounds = 0;

   if (ui->proc.win)
     {
        if ((config()->proc.poll_delay != ui->proc.poll_delay) ||
            (config()->proc.show_kthreads != ui->proc.show_kthreads) ||
            (config()->proc.show_user != ui->proc.show_user) ||
            (config()->proc.show_scroller != ui->proc.show_scroller) ||
            (config()->proc.transparent != ui->proc.transparent) ||
            (config()->proc.alpha != ui->proc.alpha)
           )
          {
             notify = 1;
          }

        config()->proc.width = ui->proc.width;
        config()->proc.height = ui->proc.height;
        config()->proc.x = ui->proc.x;
        config()->proc.y = ui->proc.y;
        config()->proc.restart = ui->proc.restart;
        config()->proc.sort_type = ui->proc.sort_type;
        config()->proc.sort_reverse = ui->proc.sort_reverse;
        config()->proc.poll_delay = ui->proc.poll_delay;
        config()->proc.show_kthreads = ui->proc.show_kthreads;
        config()->proc.show_user = ui->proc.show_user;
        config()->proc.show_scroller = ui->proc.show_scroller;
        config()->proc.transparent = ui->proc.transparent;
        config()->proc.alpha = ui->proc.alpha;
        config()->proc.fields = ui->proc.fields;
        proc_info_kthreads_show_set(ui->proc.show_kthreads);
     }

   if (ui->cpu.win)
     {
        config()->cpu.width = ui->cpu.width;
        config()->cpu.height = ui->cpu.height;
        config()->cpu.x = ui->cpu.x;
        config()->cpu.y = ui->cpu.y;
        config()->cpu.restart = ui->cpu.restart;
     }

   if (ui->mem.win)
     {
        config()->mem.width = ui->mem.width;
        config()->mem.height = ui->mem.height;
        config()->mem.x = ui->mem.x;
        config()->mem.y = ui->mem.y;
        config()->mem.restart = ui->mem.restart;
     }

   if (ui->disk.win)
     {
        config()->disk.width = ui->disk.width;
        config()->disk.height = ui->disk.height;
        config()->disk.x = ui->disk.x;
        config()->disk.y = ui->disk.y;
        config()->disk.restart = ui->disk.restart;
     }

   if (ui->sensors.win)
     {
        config()->sensors.width = ui->sensors.width;
        config()->sensors.height = ui->sensors.height;
        config()->sensors.x = ui->sensors.x;
        config()->sensors.y = ui->sensors.y;
        config()->sensors.restart = ui->sensors.restart;
     }

   if (ui->network.win)
     {
        config()->network.width = ui->network.width;
        config()->network.height = ui->network.height;
        config()->network.x = ui->network.x;
        config()->network.y = ui->network.y;
        config()->network.restart = ui->network.restart;
     }

   config_save(config());

   if (notify)
     ecore_event_add(EVISUM_EVENT_CONFIG_CHANGED, NULL, NULL, NULL);
}

Eina_Bool
evisum_ui_effects_enabled_get(Evisum_Ui *ui)
{
   return ui->effects;
}

void
evisum_ui_effects_enabled_set(Evisum_Ui *ui, Eina_Bool enabled)
{
   ui->effects = enabled;
}

void
evisum_ui_config_load(Evisum_Ui *ui)
{
   config_load();

   ui->effects           = config()->effects;

   ui->proc.sort_type    = config()->proc.sort_type;
   ui->proc.sort_reverse = config()->proc.sort_reverse;
   ui->proc.poll_delay   = config()->proc.poll_delay;
   ui->proc.show_kthreads = config()->proc.show_kthreads;
   ui->proc.fields = config()->proc.fields;
   proc_info_kthreads_show_set(ui->proc.show_kthreads);
   ui->proc.show_user = config()->proc.show_user;
   ui->proc.show_scroller = config()->proc.show_scroller;
   ui->proc.transparent = config()->proc.transparent;
   ui->proc.alpha = config()->proc.alpha;

   ui->proc.width = config()->proc.width;
   ui->proc.height = config()->proc.height;
   ui->proc.x = config()->proc.x;
   ui->proc.y = config()->proc.y;
   ui->proc.restart = config()->proc.restart;

   ui->cpu.width = config()->cpu.width;
   ui->cpu.height = config()->cpu.height;
   ui->cpu.x = config()->cpu.x;
   ui->cpu.y = config()->cpu.y;
   ui->cpu.restart = config()->cpu.restart;

   ui->mem.width = config()->mem.width;
   ui->mem.height = config()->mem.height;
   ui->mem.x = config()->mem.x;
   ui->mem.y = config()->mem.y;
   ui->mem.restart = config()->mem.restart;

   ui->disk.width = config()->disk.width;
   ui->disk.height = config()->disk.height;
   ui->disk.x = config()->disk.x;
   ui->disk.y = config()->disk.y;
   ui->disk.restart = config()->disk.restart;

   ui->sensors.width = config()->sensors.width;
   ui->sensors.height = config()->sensors.height;
   ui->sensors.x = config()->sensors.x;
   ui->sensors.y = config()->sensors.y;
   ui->sensors.restart = config()->sensors.restart;

   ui->network.width = config()->network.width;
   ui->network.height = config()->network.height;
   ui->network.x = config()->network.x;
   ui->network.y = config()->network.y;
   ui->network.restart = config()->network.restart;
}

void
evisum_ui_restart(Evisum_Ui *ui)
{
   if (ui->proc.win) ui->proc.restart = 1;
   if (ui->cpu.win) ui->cpu.restart = 1;
   if (ui->mem.win) ui->mem.restart = 1;
   if (ui->disk.win) ui->disk.restart = 1;
   if (ui->sensors.win) ui->sensors.restart = 1;
   if (ui->network.win) ui->network.restart = 1;

   evisum_ui_config_save(ui);
   evisum_server_shutdown();
   ecore_app_restart();
   ecore_main_loop_quit();
}

static void
_about_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   evisum_about_window_show(ui);
}

static void
_menu_memory_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                 void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_mem_win_add(ui);
}

static void
_menu_network_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                  void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_network_win_add(ui);
}

static void
_menu_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_disk_win_add(ui);
}

static void
_menu_sensors_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_sensors_win_add(ui);
}

static void
_menu_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_cpu_win_add(ui);
}

static void
_menu_process_view_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui_process_list_win_add(ui);
}

static void
_menu_effects_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Eina_Bool state;

   ui = data;

   state = evisum_ui_effects_enabled_get(ui);
   evisum_ui_effects_enabled_set(ui, !state);

   evisum_ui_config_save(ui);
   evisum_ui_restart(ui);
}

static Evas_Object *
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb,
            void *data)
{
   Evas_Object *btn, *ic;

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   evas_object_show(ic);

   elm_object_part_content_set(btn, "icon", ic);
   elm_object_tooltip_text_set(btn, text);
   evas_object_smart_callback_add(btn, "clicked", cb, data);

   return btn;
}

static void
_main_menu_slider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                             void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui->proc.poll_delay = elm_slider_value_get(obj) + 0.5;

   if (ui->proc.poll_delay > 1)
     elm_slider_unit_format_set(obj, _("%1.0f secs"));
   else
     elm_slider_unit_format_set(obj, _("%1.0f sec"));

   evisum_ui_config_save(ui);
}

static void
_main_menu_slider_alpha_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                             void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui->proc.alpha = elm_slider_value_get(obj) + 0.5;

   evisum_ui_config_save(ui);
}

static void
_main_menu_transparent_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                    void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   if (!ui->proc.alpha) return;

   ui->proc.transparent = elm_check_state_get(obj);
   elm_object_disabled_set(_slider_alpha, !ui->proc.transparent);

   evisum_ui_config_save(ui);
}

static void
_main_menu_show_threads_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                   void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui->proc.show_kthreads = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_scroller_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                    void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui->proc.show_scroller = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_user_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui = data;

   ui->proc.show_user = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

typedef struct
{
   Ecore_Timer *timer;
   Evas_Object *it_focus;
} Menu_Inst;

static void
_main_menu_deleted_cb(void *data EINA_UNUSED, Evas_Object *obj, Evas *e,
                      void *event_info EINA_UNUSED)
{
   Menu_Inst *inst = data;

   inst->it_focus = NULL;
   if (inst->timer)
     ecore_timer_del(inst->timer);
   inst->timer = NULL;
   free(inst);
}

static Eina_Bool
_main_menu_focus_timer_cb(void *data)
{
   Menu_Inst *inst = data;
   if (inst->it_focus)
     elm_object_focus_set(inst->it_focus, 1);
   inst->timer = NULL;

   return 0;
}

Evas_Object *
evisum_ui_main_menu_create(Evisum_Ui *ui, Evas_Object *parent, Evas_Object *obj)
{
   Evas_Object *o, *obx, *bx, *tb, *hbx, *sep, *fr, *sli;
   Evas_Object *it_focus, *btn, *chk, *rec;
   Evas_Coord ox, oy, ow, oh;
   int i = 0;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   o = elm_ctxpopup_add(parent);
   evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
   evas_object_size_hint_align_set(o, FILL, FILL);
   elm_object_style_set(o, "noblock");

   obx = elm_box_add(o);
   evas_object_size_hint_weight_set(obx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(obx, FILL, FILL);
   evas_object_show(obx);

   fr = elm_frame_add(o);
   elm_object_text_set(fr, _("Actions"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_show(fr);

   elm_object_content_set(fr, obx);
   elm_object_content_set(o, fr);

   tb = elm_table_add(o);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_show(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);

   it_focus = btn = _btn_create(tb, "proc", _("Processes"),
                     _menu_process_view_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   btn = _btn_create(tb, "cpu", _("CPU"),
                     _menu_cpu_activity_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   btn = _btn_create(tb, "memory", _("Memory"),
                     _menu_memory_activity_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   btn = _btn_create(tb, "storage", _("Storage"),
                     _menu_disk_activity_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   btn = _btn_create(tb, "sensor", _("Sensors"),
                     _menu_sensors_activity_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   btn = _btn_create(tb, "network", _("Network"),
                     _menu_network_activity_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   sep = elm_separator_add(tb);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_table_pack(tb, sep, i++, 0, 1, 1);

   btn = _btn_create(tb, "effects", _("Effects"),
                     _menu_effects_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   sep = elm_separator_add(tb);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_table_pack(tb, sep, i++, 0, 1, 1);

   btn = _btn_create(tb, "evisum", _("About"), _about_clicked_cb, ui);
   rec = evas_object_rectangle_add(evas_object_evas_get(obx));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_HEIGHT), ELM_SCALE_SIZE(BTN_HEIGHT));
   elm_table_pack(tb, rec, i, 0, 1, 1);
   elm_table_pack(tb, btn, i++, 0, 1, 1);

   elm_box_pack_end(obx, tb);

   Menu_Inst *inst = calloc(1, sizeof(Menu_Inst));
   if (!inst) return NULL;
   inst->timer = ecore_timer_add(0.5, _main_menu_focus_timer_cb, inst);
   inst->it_focus = it_focus;
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL,
                                  _main_menu_deleted_cb, inst);

   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_LEFT,
                                       ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy + oh);
   evas_object_show(o);

   if (parent != ui->proc.win) return o;

   fr = elm_frame_add(o);
   elm_object_text_set(fr, _("Options"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_show(fr);

   bx = elm_box_add(o);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   sli = elm_slider_add(o);
   evas_object_size_hint_weight_set(sli, EXPAND, EXPAND);
   elm_slider_min_max_set(sli, 1.0, 10.0);
   elm_slider_span_size_set(sli, 10.0 - 1.0);
   elm_slider_step_set(sli, 1 / 10.0);
   elm_slider_indicator_show_set(sli, 0);
   elm_slider_unit_format_set(sli, _("%1.0f secs"));
   elm_slider_value_set(sli, ui->proc.poll_delay);
   evas_object_size_hint_align_set(sli, FILL, FILL);
   elm_object_tooltip_text_set(sli, _("Poll delay"));
   evas_object_smart_callback_add(sli, "slider,drag,stop",
                                  _main_menu_slider_changed_cb, ui);
   evas_object_smart_callback_add(sli, "changed",
                                  _main_menu_slider_changed_cb, ui);
   evas_object_show(sli);
   _main_menu_slider_changed_cb(ui, sli, NULL);
   elm_box_pack_end(bx, sli);

   sep = elm_separator_add(bx);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 1);
   evas_object_show(sep);
   elm_box_pack_end(bx, sep);

   if (ui->proc.has_kthreads)
     {
        chk = elm_check_add(bx);
        evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
        evas_object_size_hint_align_set(chk, FILL, FILL);
        elm_object_text_set(chk, _("Show kernel threads?"));
        elm_check_state_set(chk, ui->proc.show_kthreads);
        evas_object_show(chk);
        evas_object_smart_callback_add(chk, "changed",
                                       _main_menu_show_threads_changed_cb, ui);
        elm_box_pack_end(bx, chk);
    }

   chk = elm_check_add(bx);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("User only?"));
   elm_check_state_set(chk, ui->proc.show_user);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_user_changed_cb, ui);
   elm_box_pack_end(bx, chk);

   elm_object_content_set(fr, bx);
   elm_box_pack_end(obx, fr);

   fr = elm_frame_add(o);
   elm_object_text_set(fr, _("Display"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_show(fr);

   bx = elm_box_add(o);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   chk = elm_check_add(bx);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("Display scroll bar?"));
   elm_check_state_set(chk, ui->proc.show_scroller);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_scroller_changed_cb, ui);
   elm_box_pack_end(bx, chk);

   hbx = elm_box_add(o);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   elm_box_horizontal_set(hbx, 1);
   evas_object_show(hbx);

   chk = elm_check_add(bx);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("Alpha"));
   elm_check_state_set(chk, ui->proc.transparent);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_transparent_changed_cb, ui);
   elm_box_pack_end(hbx, chk);

   _slider_alpha = sli = elm_slider_add(o);
   evas_object_size_hint_weight_set(sli, EXPAND, EXPAND);
   elm_slider_min_max_set(sli, 25.0, 100.0);
   elm_slider_span_size_set(sli, 100.0);
   elm_slider_step_set(sli, 1 / 100.0);
   elm_slider_unit_format_set(sli, _("%1.0f %%"));
   elm_slider_indicator_visible_mode_set(sli, ELM_SLIDER_INDICATOR_VISIBLE_MODE_NONE);
   elm_slider_value_set(sli, ui->proc.alpha);
   evas_object_size_hint_align_set(sli, FILL, FILL);
   elm_object_disabled_set(sli, !ui->proc.transparent);
   evas_object_smart_callback_add(sli, "slider,drag,stop",
                                  _main_menu_slider_alpha_changed_cb, ui);
   evas_object_smart_callback_add(sli, "changed",
                                  _main_menu_slider_alpha_changed_cb, ui);
   evas_object_show(sli);
   elm_box_pack_end(hbx, sli);
   elm_box_pack_end(bx, hbx);
   elm_object_content_set(fr, bx);

   elm_box_pack_end(obx, fr);

   return o;
}

// Any OS specific feature checks.
static void
_ui_init_system_probe(Evisum_Ui *ui)
{
#if defined(__OpenBSD__)
   ui->proc.has_kthreads = 0;
#else
   ui->proc.has_kthreads = 1;
#endif
#if defined(__FreeBSD__)
   ui->mem.zfs_mounted = file_system_in_use("ZFS");
   ui->kthreads_has_rss = 1;
#endif
#if !defined(__linux__)
   ui->proc.has_wchan = 1;
#endif
}

void
evisum_ui_activate(Evisum_Ui *ui, Evisum_Action action, int pid)
{
   Eina_Bool restart = 0;

   if (ui->proc.restart)
     {
        ui_process_list_win_add(ui);
        restart = 1;
     }

   if (ui->cpu.restart)
     {
        ui_cpu_win_add(ui);
        restart = 1;
     }

   if (ui->mem.restart)
     {
        ui_mem_win_add(ui);
        restart = 1;
     }

   if (ui->disk.restart)
     {
        ui_disk_win_add(ui);
        restart = 1;
     }

   if (ui->sensors.restart)
     {
        ui_sensors_win_add(ui);
        restart = 1;
     }

   if (ui->network.restart)
     {
        ui_network_win_add(ui);
        restart = 1;
     }

   if (restart)
     {
        ui->proc.restart = ui->cpu.restart = 0;
        ui->mem.restart = ui->disk.restart = 0;
        ui->sensors.restart = ui->network.restart = 0;
        evisum_ui_config_save(ui);
        return;
     }

   switch (action)
     {
       case EVISUM_ACTION_DEFAULT:
         ui_process_list_win_add(ui);
         break;
       case EVISUM_ACTION_PROCESS:
         ui_process_view_win_add(pid, PROC_VIEW_DEFAULT);
         break;
       case EVISUM_ACTION_CPU:
         ui_cpu_win_add(ui);
         break;
       case EVISUM_ACTION_MEM:
         ui_mem_win_add(ui);
         break;
       case EVISUM_ACTION_STORAGE:
         ui_disk_win_add(ui);
         break;
       case EVISUM_ACTION_SENSORS:
         ui_sensors_win_add(ui);
         break;
       case EVISUM_ACTION_NETWORK:
         ui_network_win_add(ui);
         break;
     }
}

void
evisum_ui_shutdown(Evisum_Ui *ui)
{
   evisum_icon_cache_shutdown();
   evisum_ui_config_save(ui);

   ecore_thread_cancel(ui->background_poll_thread);
   ecore_thread_wait(ui->background_poll_thread, 0.5);

   free(ui);
}

Evisum_Ui *
evisum_ui_init(void)
{
   Evisum_Ui *ui = calloc(1, sizeof(Evisum_Ui));
   if (!ui) return NULL;

   ui->proc.poll_delay = 3;
   ui->proc.sort_reverse = 0;
   ui->proc.sort_type = PROC_SORT_BY_PID;

   ui->program_pid = getpid();

   EVISUM_EVENT_CONFIG_CHANGED = ecore_event_type_new();

   evisum_ui_config_load(ui);

   evisum_icon_cache_init();

   _ui_init_system_probe(ui);

   return ui;
}

