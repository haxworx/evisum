#include "config.h"
#include "evisum_config.h"

#include "evisum_ui.h"
#include "ui/ui_process_list.h"
#include "ui/ui_process_view.h"

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

#define DIRTY_GENLIST_HACK    1
#define SIZING_ADJUST_PERIOD  3

extern int EVISUM_EVENT_CONFIG_CHANGED;

typedef struct
{
   Proc_Sort type;
   int       (*sort_cb)(const void *p1, const void *p2);
} Sorter;

typedef struct
{
   Ecore_Thread          *thread;
   Evisum_Ui_Cache       *cache;
   Eina_Hash             *icon_cache;
   Ecore_Event_Handler   *handler;
   Eina_Hash             *cpu_times;
   Eina_Bool              skip_wait;
   Eina_Bool              skip_update;
   Eina_Bool              update_every_item;
   Eina_Bool              first_run;
   Sorter                 sorters[PROC_SORT_BY_MAX];
   pid_t                  selected_pid;
   int                    poll_count;

   Ecore_Timer           *resize_timer;
   Evas_Object           *win;
   Evas_Object           *main_menu;
   Ecore_Timer           *main_menu_timer;
   Evas_Object           *menu;
   Eina_Bool              transparent;

   struct
   {
      char               *text;
      size_t              len;
      Ecore_Timer        *timer;
      Evas_Object        *pop;
      Evas_Object        *entry;
      Eina_Bool           visible;
      double              keytime;
   } search;

   Evas_Object           *tb_main;

   Evas_Object           *glist;
   Elm_Genlist_Item_Class itc;

   Evas_Object            *btn_menu;
   Evas_Object            *btn_cmd;
   Evas_Object            *btn_uid;
   Evas_Object            *btn_pid;
   Evas_Object            *btn_threads;
   Evas_Object            *btn_cpu;
   Evas_Object            *btn_pri;
   Evas_Object            *btn_nice;
   Evas_Object            *btn_files;
   Evas_Object            *btn_size;
   Evas_Object            *btn_virt;
   Evas_Object            *btn_rss;
   Evas_Object            *btn_shared;
   Evas_Object            *btn_state;
   Evas_Object            *btn_time;
   Evas_Object            *btn_cpu_usage;

   Evas_Object            *btn_selected;

   Eina_Bool               fields_changed;
   Proc_Field              field_max;
   Evas_Object            *fields_menu;
   Ecore_Timer            *fields_timer;

   struct
   {
      Evas_Object         *fr;
      Evas_Object         *hbx;
      Evas_Object         *pb_cpu;
      Evas_Object         *pb_mem;
      Evas_Object         *lb;
      int                  total;
      int                  running;
      int                  sleeping;
      int                  stopped;
      int                  idle;
      int                  dead;
      int                  zombie;
      int                  dsleep;
   } summary;

   Elm_Layout             *indicator;
   Evisum_Ui              *ui;

} Win_Data;

static Win_Data *_wd = NULL;

typedef struct
{
   Proc_Field   id;
   const char  *desc;
   Eina_Bool    enabled;
   Evas_Object *btn;
} Field;

static Field _fields[PROC_FIELD_MAX];

static void _content_reset(Win_Data *wd);

static const char *
_field_desc(Proc_Field id)
{
  switch (id)
   {
      case PROC_FIELD_CMD:
        return _("Command");
      case PROC_FIELD_UID:
        return _("User");
      case PROC_FIELD_PID:
        return _("Process ID");
      case PROC_FIELD_THREADS:
        return _("Threads");
      case PROC_FIELD_CPU:
        return _("CPU #");
      case PROC_FIELD_PRI:
        return _("Priority");
      case PROC_FIELD_NICE:
        return _("Nice");
      case PROC_FIELD_FILES:
        return _("Open Files");
      case PROC_FIELD_SIZE:
        return _("Memory Size");
      case PROC_FIELD_VIRT:
        return _("Memory Virtual");
      case PROC_FIELD_RSS:
        return _("Memory Reserved");
      case PROC_FIELD_SHARED:
        return _("Memory Shared");
      case PROC_FIELD_STATE:
        return _("State");
      case PROC_FIELD_TIME:
        return _("Time");
      case PROC_FIELD_CPU_USAGE:
        return _("CPU Usage");
      default:
         break;
   }
  return "BUG";
}

static const char *
_field_name(Proc_Field id)
{
  switch (id)
   {
      case PROC_FIELD_CMD:
        return _("command");
      case PROC_FIELD_UID:
        return _("user");
      case PROC_FIELD_PID:
        return _("pid");
      case PROC_FIELD_THREADS:
        return _("thr");
      case PROC_FIELD_CPU:
        return _("cpu");
      case PROC_FIELD_PRI:
        return _("pri");
      case PROC_FIELD_NICE:
        return _("nice");
      case PROC_FIELD_FILES:
        return _("files");
      case PROC_FIELD_SIZE:
        return _("size");
      case PROC_FIELD_VIRT:
        return _("virt");
      case PROC_FIELD_RSS:
        return _("res");
      case PROC_FIELD_SHARED:
        return _("shr");
      case PROC_FIELD_STATE:
        return _("state");
      case PROC_FIELD_TIME:
        return _("time");
      case PROC_FIELD_CPU_USAGE:
        return _("cpu %");
      default:
         break;
   }
  return "BUG";
}

static Eina_Bool
_field_enabled(Proc_Field id)
{
   return _fields[id].enabled;
}

static Eina_Bool
_fields_update_timer_cb(void *data)
{
   Win_Data *wd = data;

   wd->skip_wait = 1;
   wd->skip_update = 0;
   wd->fields_timer = NULL;

   return 0;
}

static void
_cache_reset_done_cb(void *data)
{
   Win_Data *wd = data;

   if (wd->fields_timer)
     ecore_timer_reset(wd->fields_timer);
   else
     wd->fields_timer = ecore_timer_add(1.0, _fields_update_timer_cb, wd);
}

static void
_fields_update(Win_Data *wd)
{
   for (int i = PROC_FIELD_CMD; i < PROC_FIELD_MAX; i++)
     {
        _fields[i].enabled = 1;
        if ((i != PROC_FIELD_CMD) && (!(wd->ui->proc.fields & (1UL << i))))
          _fields[i].enabled = 0;
     }
}

static void
_field_menu_check_changed_cb(void *data, Evas_Object *obj, void *event_info)
{
   Evisum_Ui *ui;
   Win_Data *wd;
   Evas_Object *ic;
   Field *f;

   wd = _wd;
   ui = wd->ui;
   f = data;

   ui->proc.fields ^= (1 << f->id);

   wd->skip_update = 1;
   wd->fields_changed = (ui->proc.fields != config()->proc.fields);

   ic = evas_object_data_get(obj, "icon");
   if (!wd->fields_changed)
     evas_object_hide(ic);
   else
     {
        evisum_ui_config_save(ui);
        evas_object_show(ic);
     }
}

static void
_field_menu_close_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   elm_ctxpopup_dismiss(wd->fields_menu);
   wd->fields_menu = NULL;
}

static void
_field_menu_apply_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   elm_ctxpopup_dismiss(wd->fields_menu);
   wd->fields_menu = NULL;
   if (wd->fields_changed)
     {
        if (evisum_ui_effects_enabled_get(wd->ui))
          {
             elm_object_signal_emit(wd->indicator, "fields,change", "evisum/indicator");
          }
        _content_reset(wd);
     }
   wd->fields_menu = NULL;
}

static void
_icon_mouse_in_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   evas_object_color_set(obj, 128, 128, 128, 255);
}

static void
_icon_mouse_out_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   evas_object_color_set(obj, 255, 255, 255, 255);
}

static Evas_Object *
_field_menu_create(Win_Data *wd, Evas_Object *parent)
{
   Evas_Object *o, *fr, *hbx, *pad, *ic, *ic2, *bx, *ck;

   fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(fr, bx);
   evas_object_show(fr);

   hbx = elm_box_add(parent);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   evas_object_size_hint_align_set(hbx, FILL, FILL);

   pad = elm_box_add(parent);
   evas_object_size_hint_weight_set(pad, EXPAND, 0);
   evas_object_size_hint_align_set(pad, FILL, FILL);
   elm_box_pack_end(hbx, pad);
   evas_object_show(pad);

   ic = elm_icon_add(parent);
   elm_icon_standard_set(ic, evisum_icon_path_get("apply"));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   elm_box_pack_end(hbx, ic);
   evas_object_smart_callback_add(ic, "clicked", _field_menu_apply_clicked_cb, wd);
   evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_IN, _icon_mouse_in_cb, NULL);
   evas_object_event_callback_add(ic, EVAS_CALLBACK_MOUSE_OUT, _icon_mouse_out_cb, NULL);

   ic2 = elm_icon_add(parent);
   elm_icon_standard_set(ic2, evisum_icon_path_get("exit"));
   evas_object_size_hint_min_set(ic2, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_show(ic2);
   elm_box_pack_end(hbx, ic2);
   evas_object_smart_callback_add(ic2, "clicked", _field_menu_close_clicked_cb, wd);
   evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_IN, _icon_mouse_in_cb, NULL);
   evas_object_event_callback_add(ic2, EVAS_CALLBACK_MOUSE_OUT, _icon_mouse_out_cb, NULL);

   elm_box_pack_end(bx, hbx);
   evas_object_show(hbx);

   for (int i = PROC_FIELD_UID; i < PROC_FIELD_MAX; i++)
     {
        ck = elm_check_add(parent);
        evas_object_size_hint_weight_set(ck, EXPAND, EXPAND);
        evas_object_size_hint_align_set(ck, FILL, FILL);
        elm_object_text_set(ck, _fields[i].desc);
        elm_check_state_set(ck, _fields[i].enabled);
        evas_object_data_set(ck, "icon", ic);
        evas_object_smart_callback_add(ck, "changed",
                                       _field_menu_check_changed_cb, &_fields[i]);
        elm_box_pack_end(bx, ck);
        evas_object_show(ck);
     }

   o = elm_ctxpopup_add(parent);
   evas_object_size_hint_weight_set(o, EXPAND, EXPAND);
   evas_object_size_hint_align_set(o, FILL, FILL);
   elm_object_style_set(o, "noblock");

   elm_object_content_set(o, fr);

   return o;
}

static void
_field_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                   Evas_Object *obj, void *event_info)
{
   Evas_Event_Mouse_Up *ev;
   Evas_Object *o;
   Evas_Coord ox, oy, ow, oh;
   Win_Data *wd;

   ev = event_info;
   wd = data;

   if (ev->button != 3) return;
   if (wd->fields_menu) return;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   o = wd->fields_menu = _field_menu_create(wd, wd->win);
   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_LEFT,
                                       ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy + oh);
   evas_object_show(o);
}

static void
_fields_init(Win_Data *wd)
{
   for (int i = PROC_FIELD_CMD; i < PROC_FIELD_MAX; i++)
     {
        Evas_Object *btn;
        const char *name, *desc;
        name = _field_name(i);
        desc = _field_desc(i);
        btn  = _fields[i].btn;

        _fields[i].id = i;
        _fields[i].desc = desc;
        _fields[i].enabled = 1;
        if ((i != PROC_FIELD_CMD) && (!(wd->ui->proc.fields & (1UL << i))))
          _fields[i].enabled = 0;

        elm_object_tooltip_text_set(btn, desc);
        elm_object_text_set(btn, name);
        evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_UP,
                                       _field_mouse_up_cb, wd);
     }
}

// Updating fields is a heavy exercise. We both offset the
// cache clearing and delay the initial update for a better
// experience.
static void
_content_reset(Win_Data *wd)
{
   int j = 0;

   // Update fields from bitmask.
   _fields_update(wd);

   elm_table_clear(wd->tb_main, 0);
   elm_table_pack(wd->tb_main, wd->btn_menu, j++, 0, 1, 1);
   for (int i = j; i < PROC_FIELD_MAX; i++)
     {
        Field *f = &_fields[i];
        if (!f->enabled)
          {
             evas_object_hide(f->btn);
             continue;
          }
        wd->field_max = i;
        elm_table_pack(wd->tb_main, f->btn, j++, 0, 1, 1);
        evas_object_show(f->btn);
     }
   elm_table_pack(wd->tb_main, wd->glist, 0, 1, j, 1);
   elm_table_pack(wd->tb_main, wd->summary.fr, 0, 2, j, 1);
   evas_object_show(wd->summary.fr);
   elm_genlist_clear(wd->glist);
   evisum_ui_item_cache_reset(wd->cache, _cache_reset_done_cb, wd);
   wd->fields_changed = 0;
}

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Evas_Object *o;
   Win_Data *wd;
   Eina_List *contents = NULL;

   wd = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        if (!evisum_ui_item_cache_item_release(wd->cache, o))
          {
             evas_object_del(o);
          }
     }
}

static void
_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Proc_Info *proc = data;
   proc_info_free(proc);
}

static Evas_Object *
_item_column_add(Evas_Object *tb, const char *text, int col)
{
   Evas_Object *hbx, *rec, *lb;

   hbx = elm_box_add(tb);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_size_hint_weight_set(hbx, 1.0, 1.0);

   lb = elm_label_add(tb);
   evas_object_data_set(tb, text, lb);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
   elm_box_pack_end(hbx, lb);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(6), 1);
   elm_box_pack_end(hbx, rec);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_data_set(lb, "rec", rec);
   elm_table_pack(tb, rec, col, 0, 1, 1);
   elm_table_pack(tb, hbx, col, 0, 1, 1);
   evas_object_show(hbx);
   evas_object_show(lb);

   return lb;
}

static Evas_Object *
_item_create(Evas_Object *obj)
{
   Evas_Object *tb, *lb, *ic, *rec;
   Evas_Object *hbx, *pb;
   int i = 0;

   tb = elm_table_add(obj);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_size_hint_weight_set(tb, EXPAND, 0);

   hbx = elm_box_add(tb);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_align_set(hbx, 0.0, FILL);
   evas_object_size_hint_weight_set(hbx, EXPAND, 0);
   ic = elm_icon_add(tb);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   evas_object_size_hint_align_set(ic, FILL, FILL);
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_max_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_data_set(tb, "icon", ic);
   elm_box_pack_end(hbx, ic);
   elm_table_pack(tb, hbx, i, 0, 1, 1);
   evas_object_show(hbx);
   evas_object_show(ic);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(4), 1);
   elm_box_pack_end(hbx, rec);

   rec = evas_object_rectangle_add(evas_object_evas_get(tb));
   evas_object_data_set(ic, "rec", rec);
   elm_table_pack(tb, rec, i++, 0, 1, 1);

   lb = elm_label_add(tb);
   evas_object_size_hint_weight_set(lb, 0, EXPAND);
   evas_object_data_set(tb, "cmd", lb);
   evas_object_data_set(lb, "hbx", hbx);
   elm_box_pack_end(hbx, lb);
   evas_object_show(lb);

   if (_field_enabled(PROC_FIELD_UID))
     {
        lb = _item_column_add(tb, "uid", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_PID))
     {
        lb = _item_column_add(tb, "pid", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_THREADS))
     {
        lb = _item_column_add(tb, "thr", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_CPU))
     {
        lb = _item_column_add(tb, "cpu", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_PRI))
     {
        lb = _item_column_add(tb, "prio", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_NICE))
     {
        lb = _item_column_add(tb, "nice", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_FILES))
     {
        lb = _item_column_add(tb, "files", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_SIZE))
     {
        lb = _item_column_add(tb, "size", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_VIRT))
     {
        lb = _item_column_add(tb, "virt", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_RSS))
     {
        lb = _item_column_add(tb, "rss", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_SHARED))
     {
        lb = _item_column_add(tb, "share", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_STATE))
     {
        lb = _item_column_add(tb, "state", i++);
        evas_object_size_hint_align_set(lb, 1.0, FILL);
     }

   if (_field_enabled(PROC_FIELD_TIME))
     {
        lb = _item_column_add(tb, "time", i++);
        evas_object_size_hint_align_set(lb, 0.5, FILL);
     }

   if (_field_enabled(PROC_FIELD_CPU_USAGE))
     {
        hbx = elm_box_add(tb);
        elm_box_horizontal_set(hbx, 1);
        evas_object_size_hint_weight_set(hbx, 1.0, 1.0);
        evas_object_size_hint_align_set(hbx, FILL, FILL);

        pb = elm_progressbar_add(hbx);
        evas_object_size_hint_weight_set(pb, 0, EXPAND);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        elm_progressbar_unit_format_set(pb, "%1.1f %%");
        elm_box_pack_end(hbx, pb);
        evas_object_show(hbx);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_data_set(pb, "rec", rec);
        elm_table_pack(tb, rec, i, 0, 1, 1);
        elm_table_pack(tb, hbx, i++, 0, 1, 1);
        evas_object_data_set(tb, "cpu_u", pb);
     }

   return tb;
}

static void
_run_time_set(char *buf, size_t n, int64_t secs)
{
   int rem;

   if (secs < 86400)
     snprintf(buf, n, "%02" PRIi64 ":%02"PRIi64, secs / 60, secs % 60);
   else
     {
        rem = secs % 3600;
        snprintf(buf, n, "%02" PRIi64 ":%02d:%02d", secs / 3600, rem / 60, rem % 60);
     }
}

static void
_field_adjust(Win_Data *wd, Proc_Field id, Evas_Object *obj, Evas_Coord w)
{
   Evas_Object *rec;

   rec = evas_object_data_get(obj, "rec");
   if (id != wd->field_max)
     evas_object_size_hint_min_set(rec, w, 1);
   else
     {
        evas_object_size_hint_min_set(rec, 1, 1);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
     }
}

static void
_alignment_fix(Win_Data *wd)
{
   wd->update_every_item = 1;
   wd->skip_wait = 1;
   wd->poll_count = 0;
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Proc_Info *proc;
   struct passwd *pwd_entry;
   Evas_Object *rec, *lb, *o, *pb;
   char buf[128];
   Evas_Coord w, ow, bw;
   Evisum_Ui *ui;
   Win_Data *wd;

   wd = _wd;
   ui = wd->ui;
   proc = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!proc) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(wd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(wd->btn_menu, NULL, NULL, &bw, NULL);
   evas_object_geometry_get(wd->btn_cmd, NULL, NULL, &ow, NULL);
   w = bw + ow;
   lb = evas_object_data_get(it->obj, "cmd");
   snprintf(buf, sizeof(buf), "%s", proc->command);
   if (strcmp(buf, elm_object_text_get(lb)))
     elm_object_text_set(lb, buf);
   evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
   ow += bw;
   if (ow > w)
     {
        evas_object_size_hint_min_set(wd->btn_cmd, ow, 1);
        _alignment_fix(wd);
     }
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);
   evas_object_show(lb);

   const char *new = evisum_icon_path_get(evisum_icon_cache_find(wd->icon_cache, proc));
   const char *old = NULL;
   o = evas_object_data_get(it->obj, "icon");
   elm_image_file_get(o, &old, NULL);
   if ((!old) || (strcmp(old, new)))
     elm_icon_standard_set(o, new);
   rec = evas_object_data_get(o, "rec");
   evas_object_size_hint_min_set(rec, w, 1);
   evas_object_show(o);

   if (_field_enabled(PROC_FIELD_UID))
     {
        evas_object_geometry_get(wd->btn_uid, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "uid");
        pwd_entry = getpwuid(proc->uid);
        if (pwd_entry)
          snprintf(buf, sizeof(buf), "%s", pwd_entry->pw_name);
        else
          snprintf(buf, sizeof(buf), "%i", proc->uid);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);

        evas_object_geometry_get(lb, NULL, NULL, &ow, NULL);
        if (ow > w)
          {
             evas_object_size_hint_min_set(wd->btn_uid, ow, 1);
             _alignment_fix(wd);
          }
        _field_adjust(wd, PROC_FIELD_UID, lb, w);
     }

   if (_field_enabled(PROC_FIELD_PID))
     {
        evas_object_geometry_get(wd->btn_pid, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "pid");
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_PID, lb, w);
     }

   if (_field_enabled(PROC_FIELD_THREADS))
     {
        evas_object_geometry_get(wd->btn_threads, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "thr");
        snprintf(buf, sizeof(buf), "%d", proc->numthreads);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_THREADS, lb, w);
     }

   if (_field_enabled(PROC_FIELD_CPU))
     {
        evas_object_geometry_get(wd->btn_cpu, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "cpu");
        snprintf(buf, sizeof(buf), "%d", proc->cpu_id);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_CPU, lb, w);
     }

   if (_field_enabled(PROC_FIELD_PRI))
     {
        evas_object_geometry_get(wd->btn_pri, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "prio");
        snprintf(buf, sizeof(buf), "%d", proc->priority);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_PRI, lb, w);
     }

   if (_field_enabled(PROC_FIELD_NICE))
     {
        evas_object_geometry_get(wd->btn_nice, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "nice");
        snprintf(buf, sizeof(buf), "%d", proc->nice);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_NICE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_FILES))
     {
        evas_object_geometry_get(wd->btn_files, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "files");
        snprintf(buf, sizeof(buf), "%d", proc->numfiles);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_FILES, lb, w);
     }

   if (_field_enabled(PROC_FIELD_SIZE))
     {
        evas_object_geometry_get(wd->btn_size, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "size");
        if (!proc->is_kernel)
          snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_size, 1));
        else
          {
             buf[0] = '-'; buf[1] = '\0';
          }
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_SIZE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_VIRT))
     {
        evas_object_geometry_get(wd->btn_virt, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "virt");
        if (!proc->is_kernel)
          snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_virt, 1));
        else
          {
             buf[0] = '-'; buf[1] = '\0';
          }
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_VIRT, lb, w);
     }

   if (_field_enabled(PROC_FIELD_RSS))
     {
        evas_object_geometry_get(wd->btn_rss, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "rss");
        if ((!proc->is_kernel) || (ui->kthreads_has_rss))
          snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_rss, 1));
        else
          {
             buf[0] = '-'; buf[1] = '\0';
          }
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_RSS, lb, w);
     }

   if (_field_enabled(PROC_FIELD_SHARED))
     {
        evas_object_geometry_get(wd->btn_shared, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "share");
        if (!proc->is_kernel)
          snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_shared, 1));
        else
          {
             buf[0] = '-'; buf[1] = '\0';
          }
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_SHARED, lb, w);
     }

   if (_field_enabled(PROC_FIELD_STATE))
     {
        evas_object_geometry_get(wd->btn_state, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "state");
        if ((ui->proc.has_wchan) && (proc->state[0] == 's' && proc->state[1] == 'l'))
          snprintf(buf, sizeof(buf), "%s", proc->wchan);
        else
          snprintf(buf, sizeof(buf), "%s", proc->state);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_STATE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_TIME))
     {
        evas_object_geometry_get(wd->btn_time, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "time");
        _run_time_set(buf, sizeof(buf), proc->run_time);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(wd, PROC_FIELD_TIME, lb, w);
     }

   if (_field_enabled(PROC_FIELD_CPU_USAGE))
     {
        pb = evas_object_data_get(it->obj, "cpu_u");
        double value = proc->cpu_usage / 100.0;
        double last = elm_progressbar_value_get(pb);

        evas_object_geometry_get(wd->btn_cpu_usage, NULL, NULL, &w, NULL);
        _field_adjust(wd, PROC_FIELD_CPU_USAGE, pb, w);

        if (!EINA_DBL_EQ(value, last))
          {
             elm_progressbar_value_set(pb, proc->cpu_usage / 100.0);
             snprintf(buf, sizeof(buf), "%1.1f %%", proc->cpu_usage);
             elm_object_part_text_set(pb, "elm.text.status", buf);
          }
        evas_object_show(pb);
     }

   return it->obj;
}

static void
_glist_ensure_n_items(Evas_Object *glist, unsigned int items,
                      Elm_Genlist_Item_Class *itc)
{
   Elm_Object_Item *it;
   unsigned int i, existing = elm_genlist_items_count(glist);

   if (items < existing)
     {
        for (i = existing - items; i > 0; i--)
           {
              it = elm_genlist_last_item_get(glist);
              if (it)
                {
                   elm_object_item_del(it);
                }
           }
      }

   if (items == existing) return;

   for (i = existing; i < items; i++)
     {
        elm_genlist_item_append(glist, itc, NULL, NULL,
                                ELM_GENLIST_ITEM_NONE, NULL, NULL);
     }
}

static Eina_Bool
_bring_in(void *data)
{
   Win_Data *wd;

   wd = data;
   evas_object_show(wd->glist);

   return 0;
}

static void
_summary_reset(Win_Data *wd)
{
   wd->summary.total = wd->summary.running = wd->summary.sleeping = 0;
   wd->summary.stopped = wd->summary.idle  = wd->summary.zombie = 0;
   wd->summary.dsleep = wd->summary.dead = 0;
}

static void
_summary_update(Win_Data *wd)
{
   Evisum_Ui *ui;
   Eina_Strbuf *buf;
   Battery *bat;
   Eina_List *l;

   buf = eina_strbuf_new();

   ui = wd->ui;

   eina_strbuf_append_printf(buf, _("%i processes: "), wd->summary.total);
   if (wd->summary.running)
     eina_strbuf_append_printf(buf, _("%i running, "), wd->summary.running);
   if (wd->summary.sleeping)
     eina_strbuf_append_printf(buf, _("%i sleeping, "), wd->summary.sleeping);
   if (wd->summary.stopped)
     eina_strbuf_append_printf(buf, _("%i stopped, "), wd->summary.stopped);
   if (wd->summary.idle)
     eina_strbuf_append_printf(buf, _("%i idle, "), wd->summary.idle);
   if (wd->summary.dead)
     eina_strbuf_append_printf(buf, _("%i dead, "), wd->summary.dead);
   if (wd->summary.dsleep)
     eina_strbuf_append_printf(buf, _("%i dsleep, "), wd->summary.dsleep);
   if (wd->summary.zombie)
     eina_strbuf_append_printf(buf, _("%i zombie, "), wd->summary.zombie);

   eina_strbuf_replace_last(buf, ",", ".");

   elm_object_text_set(wd->summary.lb, eina_strbuf_string_get(buf));

   elm_progressbar_value_set(wd->summary.pb_cpu, ui->cpu_usage / 100.0);

   eina_strbuf_reset(buf);

   elm_progressbar_value_set(wd->summary.pb_mem, (ui->mem_total / 100) / ui->mem_total);
   eina_strbuf_append_printf(buf, "%s / %s ", evisum_size_format(ui->mem_used, 0), evisum_size_format(ui->mem_total, 0));
   elm_object_part_text_set(wd->summary.pb_mem, "elm.text.status", eina_strbuf_string_get(buf));

   EINA_LIST_FOREACH(ui->batteries, l, bat)
     elm_progressbar_value_set(bat->pb, bat->usage / 100.0);
   eina_strbuf_free(buf);
}

static void
_summary_total(Win_Data *wd, Proc_Info *proc)
{
   wd->summary.total++;
   if (!strcmp(proc->state, _("running")))
     wd->summary.running++;
   else if (!strcmp(proc->state, _("sleeping")))
     wd->summary.sleeping++;
   else if (!strcmp(proc->state, _("stopped")))
     wd->summary.stopped++;
   else if (!strcmp(proc->state, _("idle")))
     wd->summary.idle++;
   else if (!strcmp(proc->state, _("zombie")))
     wd->summary.zombie++;
   else if (!strcmp(proc->state, _("dead")))
     wd->summary.dead++;
   else if (!strcmp(proc->state, _("dsleep")))
     wd->summary.dsleep++;
}

static void
_first_run_tasks(Win_Data *wd)
{
   Evisum_Ui *ui = wd->ui;
   Battery *bat;
   Eina_List *l;
   Evas_Object *hbx, *ic, *pb, *bx;

   hbx = wd->summary.hbx;

   EINA_LIST_FOREACH(ui->batteries, l, bat)
     {
        ic = elm_icon_add(wd->win);
        elm_icon_standard_set(ic, evisum_icon_path_get("sensor"));
        evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
        evas_object_size_hint_weight_set(ic, 0, EXPAND);
        elm_box_pack_end(hbx, ic);
        evas_object_show(ic);

        bat->pb = pb = elm_progressbar_add(wd->win);
        elm_object_tooltip_text_set(pb, eina_slstr_printf("%s (%s)", bat->vendor, bat->model));
        elm_progressbar_span_size_set(pb, 120);
        elm_progressbar_value_set(pb, bat->usage / 100.0);
        elm_box_pack_end(hbx, pb);
        evas_object_show(pb);
     }

   bx = elm_box_add(wd->win);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   elm_box_pack_end(hbx, bx);
   evas_object_show(bx);
   elm_box_pack_end(hbx, wd->summary.lb);

   wd->first_run = 0;

   ecore_timer_add(2.0, _bring_in, wd);
}

static Eina_List *
_process_list_sort(Eina_List *list, Win_Data *wd)
{
   Evisum_Ui *ui;
   Sorter s;
   ui = wd->ui;

   s = wd->sorters[ui->proc.sort_type];

   list = eina_list_sort(list, eina_list_count(list), s.sort_cb);

   if (ui->proc.sort_reverse)
     list = eina_list_reverse(list);

   return list;
}

static Eina_List *
_process_list_uid_trim(Eina_List *list, uid_t uid)
{
   Proc_Info *proc;
   Eina_List *l, *l_next;

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
        if (proc->uid != uid)
          {
             proc_info_free(proc);
             list = eina_list_remove_list(list, l);
          }
     }

   return list;
}

static void
_cpu_times_free_cb(void *data)
{
   int64_t *cpu_time = data;
   free(cpu_time);
}

static Eina_Bool
_process_ignore(Win_Data *wd, Proc_Info *proc)
{
   Evisum_Ui *ui = wd->ui;

   if (proc->pid == ui->program_pid) return 1;

   if (!wd->search.len) return 0;

   if ((strncasecmp(proc->command, wd->search.text, wd->search.len)) &&
       (!strstr(proc->command, wd->search.text)))
     return 1;

   return 0;
}

static Eina_List *
_process_list_search_trim(Eina_List *list, Win_Data *wd)
{
   Eina_List *l, *l_next;
   Proc_Info *proc;

   _summary_reset(wd);

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
        if (_process_ignore(wd, proc))
          {
             proc_info_free(proc);
             list = eina_list_remove_list(list, l);
          }
        else
          {
             int64_t *cpu_time, id = proc->pid;

             if ((cpu_time = eina_hash_find(wd->cpu_times, &id)))
               {
                  if (*cpu_time)
                    proc->cpu_usage = (double) (proc->cpu_time - *cpu_time) /
                                                wd->ui->proc.poll_delay;
                  *cpu_time = proc->cpu_time;
               }
             else
               {
                  cpu_time = malloc(sizeof(int64_t));
                  if (cpu_time)
                    {
                       *cpu_time = proc->cpu_time;
                       eina_hash_add(wd->cpu_times, &id, cpu_time);
                    }
               }
             _summary_total(wd, proc);
          }
     }

    return list;
}

static Eina_List *
_process_list_get(Win_Data *wd)
{
   Eina_List *list;
   Evisum_Ui *ui;

   ui = wd->ui;

   list = proc_info_all_get();

   if (ui->proc.show_user)
     list = _process_list_uid_trim(list, getuid());

   list = _process_list_search_trim(list, wd);
   list = _process_list_sort(list, wd);

   return list;
}

static void
_process_list(void *data, Ecore_Thread *thread)
{
   Win_Data *wd;
   Eina_List *list;
   Evisum_Ui *ui;
   Proc_Info *proc;
   int delay = 1;

   wd = data;
   ui = wd->ui;

   while (!ecore_thread_check(thread))
     {
        for (int i = 0; i < delay * 8; i++)
          {
             if (ecore_thread_check(thread)) return;
             if (wd->skip_wait)
               {
                  wd->skip_wait = 0;
                  break;
               }
             usleep(125000);
          }
        list = _process_list_get(wd);
        if (!wd->skip_update)
          ecore_thread_feedback(thread, list);
        else
          {
             EINA_LIST_FREE(list, proc)
               proc_info_free(proc);
          }
        wd->skip_update = 0;
        if (wd->poll_count > SIZING_ADJUST_PERIOD)
          delay = ui->proc.poll_delay;
        else
          delay = 1;
     }
}

static void
_indicator(Win_Data *wd)
{
   if ((!wd->skip_update) && (!wd->resize_timer) && (wd->poll_count > 5))
     {
        elm_object_signal_emit(wd->indicator, "indicator,show", "evisum/indicator");
     }
}

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED,
                          void *msg EINA_UNUSED)
{
   Win_Data *wd;
   Eina_List *list;
   Proc_Info *proc;
   Elm_Object_Item *it;
   int n;

   wd = data;
   list = msg;

   n = eina_list_count(list);

   _glist_ensure_n_items(wd->glist, n, &wd->itc);

   it = elm_genlist_first_item_get(wd->glist);
   EINA_LIST_FREE(list, proc)
     {
        Proc_Info *prev = elm_object_item_data_get(it);
        if (prev)
          proc_info_free(prev);

        elm_object_item_data_set(it, proc);
        if (wd->update_every_item)
          elm_genlist_item_update(it);

        it = elm_genlist_item_next_get(it);
     }

   if (!wd->update_every_item)
     elm_genlist_realized_items_update(wd->glist);
   wd->update_every_item = 0;

   _summary_update(wd);

#if DIRTY_GENLIST_HACK
   Eina_List *real = elm_genlist_realized_items_get(wd->glist);
   n = eina_list_count(wd->cache->active);
   if (n > eina_list_count(real) * 2)
     {
        evisum_ui_item_cache_steal(wd->cache, real);
        wd->skip_wait = 1;
     }
   eina_list_free(real);
#endif

#if 0
   printf("active %d and inactive %d => %d (realized)\n",
           eina_list_count(wd->cache->active),
           eina_list_count(wd->cache->inactive), n);
#endif
   if (wd->first_run)
     {
        _first_run_tasks(wd);
     }

   wd->poll_count++;

   if (evisum_ui_effects_enabled_get(wd->ui))
     _indicator(wd);
}

static void
_btn_icon_state_update(Evas_Object *btn, Eina_Bool reverse,
                       Eina_Bool selected, Win_Data *wd)
{
   Evas_Object *ic = elm_icon_add(btn);

   if ((wd->btn_selected) && (selected))
     evas_object_color_set(wd->btn_selected, 255, 255, 255, 255);

   if (reverse)
     elm_icon_standard_set(ic, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(ic, evisum_icon_path_get("go-up"));

   if (selected)
     {
        evas_object_color_set(ic, 128, 128, 128, 255);
        wd->btn_selected = ic;
     }

   elm_object_part_content_set(btn, "icon", ic);

   evas_object_show(ic);
}

static Eina_Bool
_btn_clicked_state_save(Win_Data *wd, Evas_Object *btn, Proc_Sort type)
{
   Evisum_Ui *ui = wd->ui;

   if (wd->fields_menu)
     {
        elm_ctxpopup_dismiss(wd->fields_menu);
        wd->fields_menu = NULL;
        return 0;
     }

   if (ui->proc.sort_type == type)
     ui->proc.sort_reverse = !ui->proc.sort_reverse;
   _btn_icon_state_update(btn, ui->proc.sort_reverse, 1, wd);

   elm_scroller_page_bring_in(wd->glist, 0, 0);

   return 1;
}

static void
_btn_clicked_cb(void *data, Evas_Object *obj,
                void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;
   Proc_Sort type;
   int t;

   wd = data;
   ui = wd->ui;

   t = (intptr_t) evas_object_data_get(obj, "type");
   type = (t & 0xff);

   if (!_btn_clicked_state_save(wd, obj, type)) return;

   ui->proc.sort_type = type;
   wd->skip_update = 0;
   wd->skip_wait = 1;
}

static void
_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                        void *ev EINA_UNUSED)
{
   Win_Data *wd = data;

   evas_object_del(obj);

   wd->menu = NULL;
}

static void
_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   kill(wd->selected_pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   kill(wd->selected_pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   if (evisum_ui_effects_enabled_get(wd->ui))
     {
        elm_object_signal_emit(wd->indicator, "process,kill", "evisum/indicator");
     }

   kill(wd->selected_pid, SIGKILL);
}

static void
_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   elm_menu_close(wd->menu);
   wd->menu = NULL;
}

static void
_item_menu_debug_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Proc_Info *proc;
   const char *terminal = "xterm";
   Win_Data *wd = data;

   _item_menu_cancel_cb(wd, NULL, NULL);

   proc = proc_info_by_pid(wd->selected_pid);
   if (!proc) return;

   if (ecore_file_app_installed("terminology"))
     terminal = "terminology";

   ecore_exe_run(eina_slstr_printf("%s -e gdb attach %d", terminal, proc->pid),
                 NULL);

   proc_info_free(proc);
}

static void
_item_menu_actions_add(Evas_Object *menu, Elm_Object_Item *menu_it, Win_Data *wd)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("bug"),
                     _("Debug"), _item_menu_debug_cb, wd);
}

static void
_item_menu_manual_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   _item_menu_cancel_cb(wd, NULL, NULL);

   ui_process_view_win_add(wd->selected_pid, PROC_VIEW_MANUAL);
}

static void
_item_menu_threads_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   _item_menu_cancel_cb(wd, NULL, NULL);

   ui_process_view_win_add(wd->selected_pid, PROC_VIEW_THREADS);
}

static void
_item_menu_children_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   _item_menu_cancel_cb(wd, NULL, NULL);

   ui_process_view_win_add(wd->selected_pid, PROC_VIEW_CHILDREN);
}

static void
_item_menu_general_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   _item_menu_cancel_cb(wd, NULL, NULL);

   ui_process_view_win_add(wd->selected_pid, PROC_VIEW_DEFAULT);
}

static void
_item_menu_info_add(Evas_Object *menu, Elm_Object_Item *menu_it, Win_Data *wd)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("info"),
                     _("General"), _item_menu_general_cb, wd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("proc"),
                     _("Children"), _item_menu_children_cb, wd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("threads"),
                     _("Threads"), _item_menu_threads_cb, wd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("manual"),
                     _("Manual"), _item_menu_manual_cb, wd);
}

static Evas_Object *
_item_menu_create(Win_Data *wd, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;

   if (!proc) return NULL;

   wd->selected_pid = proc->pid;

   wd->menu = menu = elm_menu_add(wd->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed",
                                  _item_menu_dismissed_cb, wd);

   stopped = !(!strcmp(proc->state, "stop"));

   menu_it = elm_menu_item_add(menu, NULL,
                               evisum_icon_path_get(evisum_icon_cache_find(wd->icon_cache, proc)),
                               proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("actions"),
                                _("Actions"), NULL, NULL);
   _item_menu_actions_add(menu, menu_it2, wd);
   elm_menu_item_separator_add(menu, menu_it);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("start"),
                                _("Start"), _item_menu_start_cb, wd);

   elm_object_item_disabled_set(menu_it2, stopped);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("stop"),
                                _("Stop"), _item_menu_stop_cb, wd);

   elm_object_item_disabled_set(menu_it2, !stopped);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("kill"), "Kill",
                     _item_menu_kill_cb, wd);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("info"),
                                _("Info"), NULL, wd);
   _item_menu_info_add(menu, menu_it2, wd);

   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("cancel"),
                     _("Cancel"), _item_menu_cancel_cb, wd);

   return menu;
}

static void
_item_pid_secondary_clicked_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                               Evas_Object *obj, void *event_info)
{
   Evas_Object *menu;
   Evas_Event_Mouse_Up *ev;
   Win_Data *wd;
   Elm_Object_Item *it;
   Proc_Info *proc;

   ev = event_info;
   if (ev->button != 3) return;

   it = elm_genlist_at_xy_item_get(obj, ev->output.x, ev->output.y, NULL);
   proc = elm_object_item_data_get(it);
   if (!proc) return;

   wd = data;

   menu = _item_menu_create(wd, proc);
   if (!menu) return;

   elm_menu_move(menu, ev->canvas.x, ev->canvas.y);
   evas_object_show(menu);
}

static void
_item_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *it;
   Proc_Info *proc;
   Win_Data *wd;

   wd = data;
   it = event_info;

   elm_genlist_item_selected_set(it, 0);
   if (wd->menu) return;

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   wd->selected_pid = proc->pid;
   ui_process_view_win_add(proc->pid, PROC_VIEW_DEFAULT);
}

static void
_glist_scrolled_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Win_Data *wd = data;

   // Update regularly on launch to allow for alignment.
   if (wd->poll_count > SIZING_ADJUST_PERIOD)
     wd->skip_update = 1;
   else
     {
        wd->skip_update = 0;
        wd->skip_wait = 1;
     }
}

static void
_glist_scroll_stopped_cb(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evas_Coord oy;
   static Evas_Coord prev_oy;

   wd = data;

   elm_scroller_region_get(wd->glist, NULL, &oy, NULL, NULL);

   if (oy != prev_oy)
     {
        wd->skip_wait = 1;
        elm_genlist_realized_items_update(wd->glist);
     }
   prev_oy = oy;
}

static Eina_Bool
_main_menu_timer_cb(void *data)
{
   Win_Data *wd = data;

   evas_object_del(wd->main_menu);
   wd->main_menu_timer = NULL;
   wd->main_menu = NULL;

   return 0;
}

static void
_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *ev EINA_UNUSED)
{
   Win_Data *wd = data;

   elm_ctxpopup_dismiss(wd->main_menu);
   if (wd->main_menu_timer)
     _main_menu_timer_cb(wd);
   else
     wd->main_menu_timer = ecore_timer_add(0.2, _main_menu_timer_cb, wd);
}

static Evas_Object *
_btn_create(Evas_Object *parent, const char *icon, const char *text, void *cb,
            void *data)
{
   Evas_Object *btn, *ic;

   btn = elm_button_add(parent);
   evas_object_size_hint_weight_set(btn, 0, EXPAND);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_show(btn);

   ic = elm_icon_add(btn);
   elm_icon_standard_set(ic, evisum_icon_path_get(icon));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   elm_object_part_content_set(btn, "icon", ic);
   evas_object_show(ic);

   elm_object_tooltip_text_set(btn, text);
   evas_object_smart_callback_add(btn, "clicked", cb, data);

   return btn;
}

static void
_btn_menu_clicked_cb(void *data, Evas_Object *obj,
                     void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   if (!wd->main_menu)
     wd->main_menu = evisum_ui_main_menu_create(ui, ui->proc.win, obj);
   else
     _main_menu_dismissed_cb(wd, NULL, NULL);
}

static Evas_Object *
_content_add(Win_Data *wd, Evas_Object *parent)
{
   Evas_Object *tb, *btn, *glist;
   Evas_Object *fr, *hbx, *ic, *pb, *lb;
   Evisum_Ui *ui = wd->ui;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   wd->btn_menu = btn = _btn_create(tb, "menu", _("Menu"),
                                    _btn_menu_clicked_cb, wd);

   wd->btn_cmd = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CMD ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CMD,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, 2.0 * ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CMD);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_CMD].btn = btn;

   wd->btn_uid = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_UID ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_UID,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, 1.4 * ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_UID);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_UID].btn = btn;

   wd->btn_pid = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_PID ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_PID,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_PID);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_PID].btn = btn;

   wd->btn_threads = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_THREADS ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_THREADS,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_THREADS);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_THREADS].btn = btn;

   wd->btn_cpu = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CPU ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CPU,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CPU);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_CPU].btn = btn;

   wd->btn_pri = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_PRI ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_PRI,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_PRI);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_PRI].btn = btn;

   wd->btn_nice = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_NICE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_NICE,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_NICE);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_clicked_cb, wd);
   _fields[PROC_FIELD_NICE].btn = btn;

   wd->btn_files = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_FILES ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_FILES,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_FILES);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_FILES].btn = btn;

   wd->btn_size = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_SIZE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_SIZE,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_SIZE);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_SIZE].btn = btn;

   wd->btn_virt = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_VIRT ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_VIRT,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_VIRT);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_VIRT].btn = btn;

   wd->btn_rss = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_RSS ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_RSS,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_RSS);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_RSS].btn = btn;

   wd->btn_shared = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_SHARED ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_SHARED,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_SHARED);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_SHARED].btn = btn;

   wd->btn_state = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_STATE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_STATE,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_STATE);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_STATE].btn = btn;

   wd->btn_time = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_TIME ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_TIME,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_TIME);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_TIME].btn = btn;

   wd->btn_cpu_usage = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CPU_USAGE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CPU_USAGE,
            wd);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_size_hint_min_set(btn, ELM_SCALE_SIZE(BTN_WIDTH), 1);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CPU_USAGE);
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, wd);
   _fields[PROC_FIELD_CPU_USAGE].btn = btn;

   wd->glist = glist = elm_genlist_add(parent);
   elm_genlist_homogeneous_set(glist, 1);
   elm_scroller_bounce_set(glist, 0, 0);
   elm_object_focus_allow_set(glist, 1);
   elm_scroller_policy_set(glist, ELM_SCROLLER_POLICY_AUTO,
                           (ui->proc.show_scroller ?
                            ELM_SCROLLER_POLICY_AUTO :
                            ELM_SCROLLER_POLICY_OFF));
   elm_genlist_multi_select_set(glist, 0);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);

   wd->itc.item_style = "full";
   wd->itc.func.text_get = NULL;
   wd->itc.func.content_get = _content_get;
   wd->itc.func.filter_get = NULL;
   wd->itc.func.del = _item_del;

   evas_object_smart_callback_add(glist, "selected",
                                  _item_pid_clicked_cb, wd);
   evas_object_event_callback_add(glist, EVAS_CALLBACK_MOUSE_UP,
                                  _item_pid_secondary_clicked_cb, wd);
   evas_object_smart_callback_add(glist, "unrealized",
                                  _item_unrealized_cb, wd);
   evas_object_smart_callback_add(glist, "scroll",
                                  _glist_scrolled_cb, wd);
   evas_object_smart_callback_add(glist, "scroll,anim,stop",
                                  _glist_scroll_stopped_cb, wd);
   evas_object_smart_callback_add(glist, "scroll,drag,stop",
                                  _glist_scroll_stopped_cb, wd);

   wd->summary.fr = fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_small");
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_size_hint_align_set(fr, FILL, FILL);

   wd->summary.hbx = hbx = elm_box_add(parent);
   elm_box_horizontal_set(hbx, 1);
   evas_object_size_hint_weight_set(hbx, 1.0, 0);
   evas_object_size_hint_align_set(hbx, FILL, FILL);
   evas_object_show(hbx);

   ic = elm_icon_add(parent);
   elm_icon_standard_set(ic, evisum_icon_path_get("cpu"));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_weight_set(ic, 0, EXPAND);
   elm_box_pack_end(hbx, ic);
   evas_object_show(ic);

   wd->summary.pb_cpu = pb = elm_progressbar_add(parent);
   elm_progressbar_unit_format_set(pb, "%1.2f %%");
   elm_progressbar_span_size_set(pb, 120);
   elm_box_pack_end(hbx, pb);
   evas_object_show(pb);

   ic = elm_icon_add(parent);
   elm_icon_standard_set(ic, evisum_icon_path_get("memory"));
   evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
   evas_object_size_hint_weight_set(ic, 0, EXPAND);
   elm_box_pack_end(hbx, ic);
   evas_object_show(ic);

   wd->summary.pb_mem = pb= elm_progressbar_add(parent);
   elm_progressbar_span_size_set(pb, 120);
   evas_object_show(pb);
   elm_box_pack_end(hbx, pb);

   wd->summary.lb = lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, EXPAND, 0);
   evas_object_size_hint_align_set(lb, 1.0, FILL);
   evas_object_show(lb);

   elm_object_content_set(fr, hbx);

   _fields_init(wd);

   return tb;
}

static Eina_Bool
_search_empty_cb(void *data)
{
   Win_Data *wd = data;

   if (!wd->search.len)
     {
        evas_object_lower(wd->search.pop);
        evas_object_hide(wd->search.pop);
        elm_object_focus_allow_set(wd->search.entry, 0);
        wd->search.visible = 0;
        wd->search.timer = NULL;
        wd->skip_update = 0;
        wd->skip_wait = 1;
        return 0;
     }

   if (wd->search.keytime &&
       ((ecore_loop_time_get() - wd->search.keytime) > 0.2))
     {
        wd->skip_update = 0;
        wd->skip_wait = 1;
        wd->search.keytime = 0;
     }

   return 1;
}

static void
_search_clear(Win_Data *wd)
{
   if (wd->search.text)
     free(wd->search.text);
   wd->search.text = NULL;
   wd->search.len = 0;
   wd->skip_update = 0;
}

static void
_search_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   const char *text;
   Win_Data *wd;

   wd = data;
   ev = event_info;

   if (ev && !strcmp(ev->keyname, "Escape"))
     elm_object_text_set(wd->search.entry, "");

   text = elm_object_text_get(obj);
   if (text)
     {
        wd->skip_update = 1;
        wd->search.keytime = ecore_loop_time_get();
        _search_clear(wd);
        wd->search.text = strdup(text);
        wd->search.len = strlen(text);
        if (!wd->search.timer)
          wd->search.timer = ecore_timer_add(0.05, _search_empty_cb, wd);
     }
}

static void
_search_add(Win_Data *wd)
{
   Evas_Object *tb, *fr, *rec, *entry;

   wd->search.pop = tb = elm_table_add(wd->win);
   evas_object_lower(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(wd->win));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
   elm_table_pack(tb, rec, 0, 0, 1, 1);

   fr = elm_frame_add(wd->win);
   elm_object_text_set(fr, _("Search"));
   evas_object_size_hint_weight_set(fr, 0, 0);
   evas_object_size_hint_align_set(fr, FILL, 0.5);

   wd->search.entry = entry = elm_entry_add(fr);
   evas_object_size_hint_weight_set(entry, 0, 0);
   evas_object_size_hint_align_set(entry, 0.5, 0.5);
   elm_entry_single_line_set(entry, 1);
   elm_entry_scrollable_set(entry, 1);
   elm_entry_editable_set(entry, 1);
   elm_object_focus_allow_set(entry, 0);
   evas_object_show(entry);
   elm_object_content_set(fr, entry);
   elm_table_pack(tb, fr, 0, 0, 1, 1);
   evas_object_show(fr);

   evas_object_event_callback_add(entry, EVAS_CALLBACK_KEY_DOWN,
                                  _search_key_down_cb, wd);
}

static void
_win_key_down_search(Win_Data *wd, Evas_Event_Key_Down *ev)
{
   Evas_Coord w, h;

   if (!strcmp(ev->keyname, "Escape"))
     {
        elm_object_text_set(wd->search.entry, "");
        _search_clear(wd);
        wd->skip_wait = 0;
        elm_object_focus_allow_set(wd->search.entry, 0);
        evas_object_lower(wd->search.pop);
        evas_object_hide(wd->search.pop);
        wd->search.visible = 0;
     }
   else if (ev->string && strcmp(ev->keyname, "BackSpace"))
     {
        if ((isspace(ev->string[0])) || (iscntrl(ev->string[0]))) return;
        size_t len = strlen(ev->string);
        if (len)
          {
             elm_entry_entry_append(wd->search.entry, ev->string);
             elm_entry_cursor_pos_set(wd->search.entry, len);
             _search_key_down_cb(wd, NULL, wd->search.entry, NULL);
          }
        evas_object_geometry_get(wd->win, NULL, NULL, &w, &h);
        evas_object_move(wd->search.pop, w / 2, h / 2);
        evas_object_raise(wd->search.pop);
        elm_object_focus_allow_set(wd->search.entry, 1);
        elm_object_focus_set(wd->search.entry, 1);
        evas_object_show(wd->search.pop);
        wd->search.visible = 1;
     }
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Win_Data *wd;
   Evas_Coord x, y, w, h;

   wd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!wd) return;

   elm_scroller_region_get(wd->glist, &x, &y, &w, &h);

   if (!strcmp(ev->keyname, "Escape") && !wd->search.visible)
     {
        evas_object_del(wd->win);
        return;
     }
   else if (!strcmp(ev->keyname, "Prior"))
     elm_scroller_region_bring_in(wd->glist, x, y - 512, w, h);
   else if (!strcmp(ev->keyname, "Next"))
     elm_scroller_region_bring_in(wd->glist, x, y + 512, w, h);
   else
     _win_key_down_search(wd, ev);

   wd->skip_wait = 1;
}

static Eina_Bool
_resize_cb(void *data)
{
   Win_Data *wd = data;

   wd->skip_wait = 0;
   wd->resize_timer = NULL;

   return 0;
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   elm_genlist_realized_items_update(wd->glist);

   wd->skip_wait = 1;

   if (wd->resize_timer)
     ecore_timer_reset(wd->resize_timer);
   else wd->resize_timer = ecore_timer_add(0.2, _resize_cb, wd);

   evas_object_lower(wd->search.pop);
   if (wd->main_menu)
     _main_menu_dismissed_cb(wd, NULL, NULL);

   evas_object_geometry_get(obj, NULL, NULL,
                            &ui->proc.width, &ui->proc.height);

   if (!evisum_ui_effects_enabled_get(ui)) return;
   evas_object_move(wd->indicator, ui->proc.width - ELM_SCALE_SIZE(32),
                    ui->proc.height - ELM_SCALE_SIZE(32));
   evas_object_show(wd->indicator);
}

static void
_win_alpha_set(Win_Data *wd)
{
   Evas_Object *bg, *win;
   Evisum_Ui *ui;
   int r, g, b, a;
   double fade;

   win = wd->win;
   ui = wd->ui;

   bg = evas_object_data_get(win, "bg");
   if (!bg) return;

   fade = ui->proc.alpha / 100.0;

   // FIXME: Base window colour from theme.
   if (ui->proc.transparent)
     {
        r = b = g = 128; a = 255;
        evas_object_color_set(bg, r * fade, g * fade, b * fade, fade * a);
        r = b = g = a = 255;
        evas_object_color_set(wd->tb_main, r * fade, g * fade, b * fade, fade * a);
     }
   else
     {
        r = b = g = a = 255;
        evas_object_color_set(wd->tb_main, r, g, b, a);
        r = b = g = 128;  a = 255;
        evas_object_color_set(bg, r, g, b, a);
     }

   if (ui->proc.transparent != wd->transparent)
     {
        elm_win_alpha_set(win, ui->proc.transparent);
     }
   wd->transparent = ui->proc.transparent;
}

static Eina_Bool
_evisum_config_changed_cb(void *data, int type EINA_UNUSED,
                          void *event EINA_UNUSED)
{
   Eina_Iterator *it;
   Evisum_Ui *ui;
   Win_Data *wd;
   void *d = NULL;

   wd = data;
   ui = wd->ui;

   it = eina_hash_iterator_data_new(wd->cpu_times);
   while (eina_iterator_next(it, &d))
     {
       int64_t *t = d;
       *t = 0;
     }
   eina_iterator_free(it);

   elm_scroller_policy_set(wd->glist, ELM_SCROLLER_POLICY_OFF,
                           (ui->proc.show_scroller ?
                            ELM_SCROLLER_POLICY_AUTO :
                            ELM_SCROLLER_POLICY_OFF));
   wd->skip_wait = 1;

   _win_alpha_set(wd);

   return 1;
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
             void *event_info EINA_UNUSED)
{
   Win_Data *wd;
   Evisum_Ui *ui;

   wd = data;
   ui = wd->ui;

   evas_object_geometry_get(obj, &ui->proc.x, &ui->proc.y, NULL, NULL);
}

static void
_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
            Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Win_Data *wd;

   wd = data;
   ui = wd->ui;

   evisum_ui_config_save(ui);

   if (wd->search.timer)
     ecore_timer_del(wd->search.timer);

   if (wd->thread)
     ecore_thread_cancel(wd->thread);

   if (wd->thread)
     ecore_thread_wait(wd->thread, 0.5);

   ecore_event_handler_del(wd->handler);
   evisum_icon_cache_del(wd->icon_cache);

   wd->thread = NULL;
   ui->proc.win = NULL;

   if (wd->search.text)
     free(wd->search.text);

   if (wd->cache)
     evisum_ui_item_cache_free(wd->cache);

   eina_hash_free(wd->cpu_times);

   free(wd);
   wd = NULL;
}

static void
_effects_add(Win_Data *wd, Evas_Object *win)
{
   Elm_Layout *lay;
   Evas_Object *pb;

   if (evisum_ui_effects_enabled_get(wd->ui))
     {
        pb = elm_progressbar_add(win);
        elm_object_style_set(pb, "wheel");
        elm_progressbar_pulse_set(pb, 1);
        elm_progressbar_pulse(pb, 1);
        evas_object_show(pb);

        wd->indicator = lay = elm_layout_add(win);
        elm_layout_file_set(lay, PACKAGE_DATA_DIR"/themes/evisum.edj", "proc");
        elm_layout_content_set(lay, "evisum/indicator", pb);
        evas_object_show(lay);
     }

   _win_alpha_set(wd);
   evas_object_show(win);
}

static void
_init(Win_Data *wd)
{
   wd->sorters[PROC_SORT_BY_NONE].sort_cb = proc_sort_by_pid;
   wd->sorters[PROC_SORT_BY_UID].sort_cb = proc_sort_by_uid;
   wd->sorters[PROC_SORT_BY_PID].sort_cb = proc_sort_by_pid;
   wd->sorters[PROC_SORT_BY_THREADS].sort_cb = proc_sort_by_threads;
   wd->sorters[PROC_SORT_BY_CPU].sort_cb = proc_sort_by_cpu;
   wd->sorters[PROC_SORT_BY_PRI].sort_cb = proc_sort_by_pri;
   wd->sorters[PROC_SORT_BY_NICE].sort_cb = proc_sort_by_nice;
   wd->sorters[PROC_SORT_BY_FILES].sort_cb = proc_sort_by_files;
   wd->sorters[PROC_SORT_BY_SIZE].sort_cb = proc_sort_by_size;
   wd->sorters[PROC_SORT_BY_VIRT].sort_cb = proc_sort_by_virt;
   wd->sorters[PROC_SORT_BY_RSS].sort_cb = proc_sort_by_rss;
   wd->sorters[PROC_SORT_BY_SHARED].sort_cb = proc_sort_by_shared;
   wd->sorters[PROC_SORT_BY_CMD].sort_cb = proc_sort_by_cmd;
   wd->sorters[PROC_SORT_BY_STATE].sort_cb = proc_sort_by_state;
   wd->sorters[PROC_SORT_BY_TIME].sort_cb = proc_sort_by_time;
   wd->sorters[PROC_SORT_BY_CPU_USAGE].sort_cb = proc_sort_by_cpu_usage;
}

void
ui_process_list_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *icon;
   Evas_Object *tb;

   if (ui->proc.win)
     {
        elm_win_raise(ui->proc.win);
        return;
     }

   Win_Data *wd = _wd = calloc(1, sizeof(Win_Data));
   if (!wd) return;

   wd->selected_pid = -1;
   wd->first_run = 1;
   wd->ui = ui;
   wd->handler = ecore_event_handler_add(EVISUM_EVENT_CONFIG_CHANGED,
                                         _evisum_config_changed_cb, wd);
   _init(wd);

   ui->proc.win = wd->win = win = elm_win_add(NULL, "evisum", ELM_WIN_BASIC);
   elm_win_autodel_set(win, 1);
   elm_win_title_set(win, _("Process Explorer"));
   icon = elm_icon_add(win);
   elm_icon_standard_set(icon, "evisum");
   elm_win_icon_object_set(win, icon);
   evisum_ui_background_add(win);

   if ((ui->proc.width > 1) && (ui->proc.height > 1))
     evas_object_resize(win, ui->proc.width, ui->proc.height);
   else
     evas_object_resize(win, EVISUM_WIN_WIDTH * elm_config_scale_get(),
                        EVISUM_WIN_HEIGHT * elm_config_scale_get());

   if ((ui->proc.x > 0) && (ui->proc.y > 0))
     evas_object_move(win, ui->proc.x, ui->proc.y);
   else
     elm_win_center(win, 1, 1);

   wd->tb_main = tb = _content_add(wd, win);
   elm_win_resize_object_add(win, tb);
   elm_object_content_set(win, tb);

   wd->cache = evisum_ui_item_cache_new(wd->glist, _item_create, 30);
   wd->icon_cache = evisum_icon_cache_new();
   wd->cpu_times = eina_hash_int64_new(_cpu_times_free_cb);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL,
                                  _win_del_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, wd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE,
                                  _win_move_cb, wd);
   evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN,
                                  _win_key_down_cb, wd);

   _search_add(wd);
   _effects_add(wd, win);
   _content_reset(wd);

   _win_resize_cb(wd, NULL, win, NULL);
   wd->thread = ecore_thread_feedback_run(_process_list,
                                          _process_list_feedback_cb,
                                          NULL,
                                          NULL,
                                          wd, 0);
}

