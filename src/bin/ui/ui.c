#include "config.h"
#include "evisum_actions.h"
#include "ui.h"
#include "ui/ui_cpu.h"
#include "ui/ui_memory.h"
#include "ui/ui_disk.h"
#include "ui/ui_sensors.h"
#include "ui/ui_process_view.h"
#include "ui/ui_process_list.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

// These should be static. Please do not change.
// OpenBSD has issues which are undetermined yet.

Ui *_ui;
Evisum_Config *_evisum_config;

void
evisum_ui_config_save(Ui *ui)
{
   Evas_Coord w, h;

   if (!_evisum_config) return;

   evas_object_geometry_get(ui->win, NULL, NULL, &w, &h);

   _evisum_config->sort_type    = ui->settings.sort_type;
   _evisum_config->sort_reverse = ui->settings.sort_reverse;
   _evisum_config->width = w;
   _evisum_config->height = h;
   _evisum_config->effects = evisum_ui_effects_enabled_get();
   _evisum_config->backgrounds = evisum_ui_backgrounds_enabled_get();
   _evisum_config->poll_delay = ui->settings.poll_delay;
   _evisum_config->show_kthreads = ui->settings.show_kthreads;
   _evisum_config->show_user = ui->settings.show_user;
   _evisum_config->show_desktop = ui->settings.show_desktop;

   proc_info_kthreads_show_set(ui->settings.show_kthreads);

   config_save(_evisum_config);
}

void
evisum_ui_config_load(Ui *ui)
{
   _evisum_config   = config_load();

   ui->settings.sort_type    = _evisum_config->sort_type;
   ui->settings.sort_reverse = _evisum_config->sort_reverse;
   ui->settings.poll_delay   = _evisum_config->poll_delay;

   if ((_evisum_config->width > 0) && (_evisum_config->height > 0))
     evas_object_resize(ui->win, _evisum_config->width, _evisum_config->height);

   evisum_ui_effects_enabled_set(_evisum_config->effects);
   evisum_ui_backgrounds_enabled_set(_evisum_config->backgrounds);

   ui->settings.show_kthreads = _evisum_config->show_kthreads;
   proc_info_kthreads_show_set(ui->settings.show_kthreads);
   ui->settings.show_user = _evisum_config->show_user;
   ui->settings.show_desktop = _evisum_config->show_desktop;
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

   ui_win_memory_add(ui);
}

static void
_menu_disk_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_disk_add(ui);
}

static void
_menu_sensors_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                               void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_sensors_add(ui);
}

static void
_menu_cpu_activity_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                              void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui_win_cpu_add(ui);
}

static void
_menu_effects_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   if ((!evisum_ui_effects_enabled_get()) && (!evisum_ui_backgrounds_enabled_get()))
     evisum_ui_backgrounds_enabled_set(1);
   else if (evisum_ui_backgrounds_enabled_get() && (!evisum_ui_effects_enabled_get()))
     evisum_ui_effects_enabled_set(1);
   else
     {
        evisum_ui_effects_enabled_set(0);
        evisum_ui_backgrounds_enabled_set(0);
     }

   evisum_ui_config_save(ui);

   evisum_restart();
}

static Evas_Object *
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb,
            void *data)
{
   Evas_Object *ot, *or, *btn, *ic;

   ot = elm_table_add(parent);
   evas_object_show(ot);

   or = evas_object_rectangle_add(evas_object_evas_get(parent));
   evas_object_size_hint_min_set(or,
                                 24 * elm_config_scale_get(),
                                 24 * elm_config_scale_get());
   elm_table_pack(ot, or, 0, 0, 1, 1);

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, EXPAND, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   elm_object_part_content_set(btn, "icon", ic);
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

   ui->settings.poll_delay = elm_slider_value_get(obj) + 0.5;

   if (ui->settings.poll_delay > 1)
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

   ui->settings.show_kthreads = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_desktop_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                   void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->settings.show_desktop = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

static void
_main_menu_show_user_changed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                                void *event_info EINA_UNUSED)
{
   Ui *ui = data;

   ui->settings.show_user = elm_check_state_get(obj);
   evisum_ui_config_save(ui);
}

void
evisum_ui_main_menu_create(Ui *ui, Evas_Object *parent)
{
   Evas_Object *o, *bx, *bx2, *hbox, *sep, *fr, *sli;
   Evas_Object *btn, *chk;
   Evas_Coord ox, oy, ow, oh;

   evas_object_geometry_get(parent, &ox, &oy, &ow, &oh);
   o = elm_ctxpopup_add(ui->win);
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

   btn = _btn_create(hbox, "cpu", _("CPU"),
                     _menu_cpu_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "memory", _("Memory"),
                     _menu_memory_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "storage", _("Storage"),
                     _menu_disk_activity_clicked_cb, ui);
   elm_box_pack_end(hbox, btn);

   btn = _btn_create(hbox, "misc", _("Sensors"),
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
   elm_slider_value_set(sli, ui->settings.poll_delay);
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

   chk = elm_check_add(bx2);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("Show kernel threads?"));
   elm_check_state_set(chk, _evisum_config->show_kthreads);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_threads_changed_cb, ui);
   elm_box_pack_end(bx2, chk);

   chk = elm_check_add(bx2);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("User only?"));
   elm_check_state_set(chk, _evisum_config->show_user);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_user_changed_cb, ui);
   elm_box_pack_end(bx2, chk);

   chk = elm_check_add(bx2);
   evas_object_size_hint_weight_set(chk, EXPAND, EXPAND);
   evas_object_size_hint_align_set(chk, FILL, FILL);
   elm_object_text_set(chk, _("Current desktop session only?"));
   elm_check_state_set(chk, _evisum_config->show_desktop);
   evas_object_show(chk);
   evas_object_smart_callback_add(chk, "changed",
                                  _main_menu_show_desktop_changed_cb, ui);
   elm_box_pack_end(bx2, chk);

   elm_object_content_set(fr, bx2);
   elm_box_pack_end(bx, fr);

   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_LEFT,
                                       ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy);
   evas_object_show(o);
   ui->main_menu = o;
}

Eina_Bool
evisum_ui_can_exit(Ui *ui)
{
   if (!ui->win && !ui->cpu.win && !ui->mem.win && !ui->sensors.win)
     return 1;
   return 0;
}

static void
_ui_init_system_probe(Ui *ui)
{
   ui->mem.zfs_mounted = file_system_in_use("ZFS");
}

void
evisum_restart(void)
{
   evisum_server_shutdown();
   ecore_app_restart();
   ecore_main_loop_quit();
}

static void
_process_win_add(Evas_Object *parent, int pid, int delay)
{
   Proc_Info *proc;

   proc = proc_info_by_pid(pid);
   if (!proc) return;

   ui_process_win_add(parent, proc->pid, proc->command, delay);

   proc_info_free(proc);
}

void
evisum_ui_activate(Ui *ui, Evisum_Action action, int pid)
{
   switch (action)
     {
       case EVISUM_ACTION_DEFAULT:
         ui_process_list_win_add(ui);
         break;
       case EVISUM_ACTION_PROCESS:
         _process_win_add(NULL, pid, 3);
         break;
       case EVISUM_ACTION_CPU:
         ui_win_cpu_add(ui);
         break;
       case EVISUM_ACTION_MEM:
         ui_win_memory_add(ui);
         break;
       case EVISUM_ACTION_STORAGE:
         ui_win_disk_add(ui);
         break;
       case EVISUM_ACTION_SENSORS:
         ui_win_sensors_add(ui);
         break;
     }
}

void
evisum_ui_del(Ui *ui)
{
   evisum_icon_cache_shutdown();

   free(ui);
}

static Ui *
_ui_init(void)
{
   Ui *ui = calloc(1, sizeof(Ui));
   if (!ui) return NULL;

   ui->settings.poll_delay = 3;
   ui->settings.sort_reverse = EINA_FALSE;
   ui->settings.sort_type = SORT_BY_PID;
   ui->selected_pid = -1;
   ui->program_pid = getpid();
   ui->cpu_times = NULL;
   ui->cpu_list = NULL;

   evisum_icon_cache_init();

   _ui_init_system_probe(ui);

   _ui = NULL;
   _evisum_config = NULL;

   evisum_ui_config_load(ui);

   return ui;
}

Ui *
evisum_ui_init(void)
{
   Ui *ui = _ui = _ui_init();
   if (!ui) return NULL;

   return ui;
}

