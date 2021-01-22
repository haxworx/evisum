#include "config.h"
#include "evisum_actions.h"
#include "evisum_config.h"
#include "evisum_server.h"

#include "system/filesystems.h"

#include "ui.h"
#include "ui/ui_cpu.h"
#include "ui/ui_memory.h"
#include "ui/ui_disk.h"
#include "ui/ui_sensors.h"
#include "ui/ui_process_view.h"
#include "ui/ui_process_list.h"

Evisum_Config *_evisum_config;
int EVISUM_EVENT_CONFIG_CHANGED;

void
evisum_ui_config_save(Ui *ui)
{
   Evas_Coord x, y, w, h;
   Eina_Bool notify = EINA_FALSE;

   if (!_evisum_config) return;

   _evisum_config->effects = 0;
   _evisum_config->backgrounds = evisum_ui_backgrounds_enabled_get();

   if (ui->proc.win)
     {
        if ((_evisum_config->proc.poll_delay != ui->proc.poll_delay) ||
            (_evisum_config->proc.show_kthreads != ui->proc.show_kthreads) ||
            (_evisum_config->proc.show_user != ui->proc.show_user) ||
            (_evisum_config->proc.show_scroller != ui->proc.show_scroller)
           )
          {
             notify = EINA_TRUE;
          }

        _evisum_config->proc.width = ui->proc.width;
        _evisum_config->proc.height = ui->proc.height;
        _evisum_config->proc.x = ui->proc.x;
        _evisum_config->proc.y = ui->proc.y;
        _evisum_config->proc.restart = ui->proc.restart;
        _evisum_config->proc.sort_type = ui->proc.sort_type;
        _evisum_config->proc.sort_reverse = ui->proc.sort_reverse;
        _evisum_config->proc.poll_delay = ui->proc.poll_delay;
        _evisum_config->proc.show_kthreads = ui->proc.show_kthreads;
        _evisum_config->proc.show_user = ui->proc.show_user;
        _evisum_config->proc.show_scroller = ui->proc.show_scroller;
        proc_info_kthreads_show_set(ui->proc.show_kthreads);
     }

   if (ui->cpu.win)
     {
        evas_object_geometry_get(ui->cpu.win, &x, &y, &w, &h);
        _evisum_config->cpu.width = ui->cpu.width = w;
        _evisum_config->cpu.height = ui->cpu.height = h;
        _evisum_config->cpu.x = x;
        _evisum_config->cpu.y = y;
        _evisum_config->cpu.restart = ui->cpu.restart;
     }

   if (ui->mem.win)
     {
        evas_object_geometry_get(ui->mem.win, &x, &y, &w, &h);
        _evisum_config->mem.width = ui->mem.width = w;
        _evisum_config->mem.height = ui->mem.height = h;
        _evisum_config->mem.x = x;
        _evisum_config->mem.y = y;
        _evisum_config->mem.restart = ui->mem.restart;
     }

   if (ui->disk.win)
     {
        evas_object_geometry_get(ui->disk.win, &x, &y, &w, &h);
        _evisum_config->disk.width = ui->disk.width = w;
        _evisum_config->disk.height = ui->disk.height = h;
        _evisum_config->disk.x = x;
        _evisum_config->disk.y = y;
        _evisum_config->disk.restart = ui->disk.restart;
     }

   if (ui->sensors.win)
     {
        evas_object_geometry_get(ui->sensors.win, &x, &y, &w, &h);
        _evisum_config->sensors.width = ui->sensors.width = w;
        _evisum_config->sensors.height = ui->sensors.height = h;
        _evisum_config->sensors.x = x;
        _evisum_config->sensors.y = y;
        _evisum_config->sensors.restart = ui->sensors.restart;
     }

   config_save(_evisum_config);

   if (notify)
     ecore_event_add(EVISUM_EVENT_CONFIG_CHANGED, NULL, NULL, NULL);
}

void
evisum_ui_config_load(Ui *ui)
{
   _evisum_config = NULL;

   _evisum_config = config_load();

   evisum_ui_backgrounds_enabled_set(_evisum_config->backgrounds);

   ui->proc.sort_type    = _evisum_config->proc.sort_type;
   ui->proc.sort_reverse = _evisum_config->proc.sort_reverse;
   ui->proc.poll_delay   = _evisum_config->proc.poll_delay;
   ui->proc.show_kthreads = _evisum_config->proc.show_kthreads;
   proc_info_kthreads_show_set(ui->proc.show_kthreads);
   ui->proc.show_user = _evisum_config->proc.show_user;
   ui->proc.show_scroller = _evisum_config->proc.show_scroller;

   ui->proc.width = _evisum_config->proc.width;
   ui->proc.height = _evisum_config->proc.height;
   ui->proc.x = _evisum_config->proc.x;
   ui->proc.y = _evisum_config->proc.y;
   ui->proc.restart = _evisum_config->proc.restart;

   ui->cpu.width = _evisum_config->cpu.width;
   ui->cpu.height = _evisum_config->cpu.height;
   ui->cpu.x = _evisum_config->cpu.x;
   ui->cpu.y = _evisum_config->cpu.y;
   ui->cpu.restart = _evisum_config->cpu.restart;

   ui->mem.width = _evisum_config->mem.width;
   ui->mem.height = _evisum_config->mem.height;
   ui->mem.x = _evisum_config->mem.x;
   ui->mem.y = _evisum_config->mem.y;
   ui->mem.restart = _evisum_config->mem.restart;

   ui->disk.width = _evisum_config->disk.width;
   ui->disk.height = _evisum_config->disk.height;
   ui->disk.x = _evisum_config->disk.x;
   ui->disk.y = _evisum_config->disk.y;
   ui->disk.restart = _evisum_config->disk.restart;

   ui->sensors.width = _evisum_config->sensors.width;
   ui->sensors.height = _evisum_config->sensors.height;
   ui->sensors.x = _evisum_config->sensors.x;
   ui->sensors.y = _evisum_config->sensors.y;
   ui->sensors.restart = _evisum_config->sensors.restart;
}

void
evisum_ui_restart(Ui *ui)
{
   if (ui->proc.win) ui->proc.restart = 1;
   if (ui->cpu.win) ui->cpu.restart = 1;
   if (ui->mem.win) ui->mem.restart = 1;
   if (ui->disk.win) ui->disk.restart = 1;
   if (ui->sensors.win) ui->sensors.restart = 1;

   evisum_ui_config_save(ui);
   evisum_server_shutdown();
   ecore_app_restart();
   ecore_main_loop_quit();
}

static void
_about_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                  void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   evisum_about_window_show(ui);
}

static void
_menu_memory_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                 void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_mem_win_add(ui);
}

static void
_menu_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_disk_win_add(ui);
}

static void
_menu_sensors_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_sensors_win_add(ui);
}

static void
_menu_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_cpu_win_add(ui);
}

static void
_menu_process_view_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_process_list_win_add(ui);
}

static void
_menu_effects_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Ui *ui;
   Eina_Bool state;

   ui = data;

   state = !evisum_ui_backgrounds_enabled_get();

   evisum_ui_backgrounds_enabled_set(state);

   evisum_ui_config_save(ui);

   evisum_ui_restart(ui);
}

static Evas_Object *
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb,
            void *data)
{
   Evas_Object *ot, *or, *btn, *ic;

   ot = elm_table_add(parent);
   evas_object_show(ot);

   or = evas_object_rectangle_add(evas_object_evas_get(parent));
   elm_table_pack(ot, or, 0, 0, 1, 1);

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, 0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_size_hint_min_set(or,
                                 24 * elm_config_scale_get(),
                                 24 * elm_config_scale_get());
   evas_object_show(ic);

   elm_object_tooltip_text_set(btn, text);
   evas_object_smart_callback_add(btn, "clicked", cb, data);

   elm_table_pack(ot, btn, 0, 0, 1, 1);

   return ot;
}

static void
_main_menu_slider_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                             void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->proc.poll_delay = elm_slider_value_get(obj) + 0.5;

   if (ui->proc.poll_delay > 1)
     elm_slider_unit_format_set(obj, _("%1.0f secs"));
   else
     elm_slider_unit_format_set(obj, _("%1.0f sec"));

   evisum_ui_config_save(ui);
}

static void
_main_menu_show_threads_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                   void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->proc.show_kthreads = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_scroller_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                    void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->proc.show_scroller = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_user_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                void *event_info EINA_UNUSED)
{
   Ui *ui = data;

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

   return EINA_FALSE;
}

Evas_Object *
evisum_ui_main_menu_create(Ui *ui, Evas_Object *parent, Evas_Object *obj)
{
   Evas_Object *o, *bx, *bx2, *hbox, *sep, *fr, *sli;
   Evas_Object *it_focus, *btn, *chk;
   Evas_Coord ox, oy, ow, oh;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   o = elm_ctxpopup_add(parent);
   evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
   evas_object_size_hint_align_set(o, FILL, FILL);
   elm_object_style_set(o, "noblock");

   bx = elm_box_add(o);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);

   fr = elm_frame_add(o);
   elm_object_text_set(fr, _("Actions"));
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   evas_object_show(fr);

   evas_object_size_hint_min_set(fr, 100, 100);
   elm_object_content_set(fr, bx);
   elm_object_content_set(o, fr);

   hbox = elm_box_add(o);
   elm_box_horizontal_set(hbox, 1);
   evas_object_size_hint_align_set(hbox, FILL, FILL);
   evas_object_size_hint_weight_set(hbox, EXPAND, EXPAND);
   evas_object_show(hbox);

   it_focus = btn = _btn_create(hbox, "proc", _("Processes"),
                     _menu_process_view_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "cpu", _("CPU"),
                     _menu_cpu_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "memory", _("Memory"),
                     _menu_memory_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "storage", _("Storage"),
                     _menu_disk_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "sensor", _("Sensors"),
                     _menu_sensors_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_box_pack_end(hbox, sep);

   btn = _btn_create(hbox, "effects", _("Effects"),
                     _menu_effects_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   sep = elm_separator_add(hbox);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 0);
   evas_object_show(sep);
   elm_box_pack_end(hbox, sep);

   btn = _btn_create(hbox, "evisum", _("About"), _about_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);
   elm_box_pack_end(bx, hbox);

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

   bx2 = elm_box_add(o);
   evas_object_size_hint_weight_set(bx2, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx2, FILL, FILL);
   evas_object_show(bx2);

   sli = elm_slider_add(o);
   evas_object_size_hint_weight_set(sli, EXPAND, EXPAND);
   elm_slider_min_max_set(sli, 1.0, 10.0);
   elm_slider_span_size_set(sli, 10.0);
   elm_slider_step_set(sli, 1 / 10.0);
   elm_slider_indicator_format_set(sli, "%1.0f");
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
   elm_box_pack_end(bx2, sli);

   sep = elm_separator_add(bx2);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 1);
   evas_object_show(sep);
   elm_box_pack_end(bx2, sep);

   if (ui->proc.has_kthreads)
     {
        chk = elm_check_add(bx2);
        evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
        evas_object_size_hint_align_set(chk, FILL, FILL);
        elm_object_text_set(chk, _("Show kernel threads?"));
        elm_check_state_set(chk, ui->proc.show_kthreads);
        evas_object_show(chk);
        evas_object_smart_callback_add(chk, "changed",
                                       _main_menu_show_threads_changed_cb, ui);
        elm_box_pack_end(bx2, chk);
    }

   chk = elm_check_add(bx2);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("User only?"));
   elm_check_state_set(chk, ui->proc.show_user);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_user_changed_cb, ui);
   elm_box_pack_end(bx2, chk);

   sep = elm_separator_add(bx2);
   evas_object_size_hint_align_set(sep, FILL, FILL);
   evas_object_size_hint_weight_set(sep, EXPAND, EXPAND);
   elm_separator_horizontal_set(sep, 1);
   evas_object_show(sep);
   elm_box_pack_end(bx2, sep);

   chk = elm_check_add(bx2);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("Display scroll bar?"));
   elm_check_state_set(chk, ui->proc.show_scroller);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_scroller_changed_cb, ui);
   elm_box_pack_end(bx2, chk);


   elm_object_content_set(fr, bx2);
   elm_box_pack_end(bx, fr);

   return o;
}

// Any OS specific feature checks.
static void
_ui_init_system_probe(Ui *ui)
{
#if defined(__OpenBSD__)
   ui->proc.has_kthreads = 0;
#else
   ui->proc.has_kthreads = 1;
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
   ui->mem.zfs_mounted = file_system_in_use("ZFS");
#endif
}

void
evisum_ui_activate(Ui *ui, Evisum_Action action, int pid)
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

   if (restart)
     {
        ui->proc.restart = ui->cpu.restart = 0;
        ui->mem.restart = ui->disk.restart = 0;
        ui->sensors.restart = 0;
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
     }
}

void
evisum_ui_shutdown(Ui *ui)
{
   evisum_icon_cache_shutdown();

   free(ui);
}

Ui *
evisum_ui_init(void)
{
   Ui *ui = calloc(1, sizeof(Ui));
   if (!ui) return NULL;

   ui->proc.poll_delay = 3;
   ui->proc.sort_reverse = EINA_FALSE;
   ui->proc.sort_type = SORT_BY_PID;

   ui->program_pid = getpid();

   EVISUM_EVENT_CONFIG_CHANGED = ecore_event_type_new();

   evisum_ui_backgrounds_enabled_set(0);
   evisum_ui_config_load(ui);

   evisum_icon_cache_init();

   _ui_init_system_probe(ui);

   return ui;
}

