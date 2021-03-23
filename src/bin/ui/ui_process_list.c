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

#define DIRTY_GENLIST_HACK     1

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
   Ecore_Event_Handler   *handler;
   Eina_Hash             *cpu_times;
   Eina_Bool              skip_wait;
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

   Proc_Field              field_max;
   Evas_Object            *fields_menu;
   Ecore_Timer            *fields_timer;

   struct
   {
      Evas_Object         *fr;
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

} Data;

static Data *_pd = NULL;

typedef struct
{
   Proc_Field   id;
   const char  *name;
   Eina_Bool    enabled;
   Evas_Object *btn;
} Field;

static Field _fields[PROC_FIELD_MAX];

static const char *
_field_name(Proc_Field id)
{
  switch (id)
   {
      case PROC_FIELD_CMD:
        return _("Command");
      case PROC_FIELD_UID:
        return _("User");
      case PROC_FIELD_PID:
        return _("PID");
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

static Eina_Bool
_field_enabled(Proc_Field id)
{
   return _fields[id].enabled;
}

static Eina_Bool
_fields_update_timer_cb(void *data)
{
   Data *pd = data;

   pd->skip_wait = 1;
   pd->fields_timer = NULL;

   return 0;
}

static void
_cache_reset_done_cb(void *data)
{
   Data *pd = data;

   if (pd->fields_timer)
     ecore_timer_reset(pd->fields_timer);
   else
     pd->fields_timer = ecore_timer_add(1.0, _fields_update_timer_cb, pd);
}

// Updating fields is a heavy exercise. We both offset the
// cache clearing and delay the initial update for a better
// experience.
static void
_content_reset(Data *pd)
{
   int j = 0;
   elm_table_clear(pd->tb_main, 0);
   elm_table_pack(pd->tb_main, pd->btn_menu, j++, 0, 1, 1);
   for (int i = j; i < PROC_FIELD_MAX; i++)
     {
        Field *f = &_fields[i];
        if (!f->enabled)
          {
             evas_object_hide(f->btn);
             continue;
          }
        pd->field_max = i;
        elm_table_pack(pd->tb_main, f->btn, j++, 0, 1, 1);
        evas_object_show(f->btn);
     }
   elm_table_pack(pd->tb_main, pd->glist, 0, 1, j, 1);
   elm_table_pack(pd->tb_main, pd->summary.fr, 0, 2, j, 1);
   evas_object_show(pd->summary.fr);
   elm_genlist_clear(pd->glist);
   evisum_ui_item_cache_reset(pd->cache, _cache_reset_done_cb, pd);
}

static void
_field_menu_check_changed_cb(void *data, Evas_Object *obj, void *event_info)
{
   Evisum_Ui *ui;
   Data *pd;
   Field *f;

   pd = _pd;
   ui = pd->ui;

   f = data;
   f->enabled = !f->enabled;
   _content_reset(pd);
   ui->proc.fields ^= (1 << f->id);
}

static Evas_Object *
_field_menu_create(Data *pd, Evas_Object *parent)
{
   Evas_Object *o, *fr, *bx, *ck;

   fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_small");

   bx = elm_box_add(parent);
   evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(fr, bx);
   evas_object_show(fr);

   for (int i = PROC_FIELD_UID; i < PROC_FIELD_MAX; i++)
     {
        ck = elm_check_add(parent);
        evas_object_size_hint_weight_set(ck, EXPAND, EXPAND);
        evas_object_size_hint_align_set(ck, FILL, FILL);
        elm_object_text_set(ck, _fields[i].name);
        elm_check_state_set(ck, _fields[i].enabled);
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
   Data *pd;

   ev = event_info;
   pd = data;

   if (ev->button != 3) return;
   if (pd->fields_menu) return;

   evas_object_geometry_get(obj, &ox, &oy, &ow, &oh);
   o = pd->fields_menu = _field_menu_create(pd, pd->win);
   elm_ctxpopup_direction_priority_set(o, ELM_CTXPOPUP_DIRECTION_DOWN,
                                       ELM_CTXPOPUP_DIRECTION_UP,
                                       ELM_CTXPOPUP_DIRECTION_LEFT,
                                       ELM_CTXPOPUP_DIRECTION_RIGHT);
   evas_object_move(o, ox + (ow / 2), oy + oh);
   evas_object_show(o);
}

static void
_fields_init(Data *pd)
{
   for (int i = PROC_FIELD_CMD; i < PROC_FIELD_MAX; i++)
     {
        Evas_Object *btn;
        const char *name = _field_name(i);
        btn = _fields[i].btn;

        _fields[i].id = i;
        _fields[i].name = name;
        _fields[i].enabled = 1;
        if ((i != PROC_FIELD_CMD) && (!(pd->ui->proc.fields & (1UL << i))))
          _fields[i].enabled = 0;

        elm_object_tooltip_text_set(btn, name);
        evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_UP,
                                       _field_mouse_up_cb, pd);
     }
}

static void
_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Evas_Object *o;
   Data *pd;
   Eina_List *contents = NULL;

   pd = data;

   elm_genlist_item_all_contents_unset(event_info, &contents);

   EINA_LIST_FREE(contents, o)
     {
        if (!evisum_ui_item_cache_item_release(pd->cache, o))
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
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(2), 1);
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

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(2), 1);
        elm_box_pack_end(hbx, rec);

        pb = elm_progressbar_add(hbx);
        evas_object_size_hint_weight_set(pb, 0, EXPAND);
        evas_object_size_hint_align_set(pb, FILL, FILL);
        elm_progressbar_unit_format_set(pb, "%1.1f %%");
        elm_box_pack_end(hbx, pb);
        evas_object_show(hbx);

        rec = evas_object_rectangle_add(evas_object_evas_get(tb));
        evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(2), 1);
        elm_box_pack_end(hbx, rec);

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
_field_adjust(Data *pd, Proc_Field id, Evas_Object *obj, Evas_Coord w)
{
   Evas_Object *rec;

   rec = evas_object_data_get(obj, "rec");
   if (id != pd->field_max)
     evas_object_size_hint_min_set(rec, w, 1);
   else
     {
        evas_object_size_hint_min_set(rec, 1, 1);
        evas_object_size_hint_weight_set(rec, EXPAND, EXPAND);
        evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
     }
   evas_object_show(obj);
}

static Evas_Object *
_content_get(void *data, Evas_Object *obj, const char *source)
{
   Proc_Info *proc;
   struct passwd *pwd_entry;
   Evas_Object *rec, *lb, *o, *hbx, *pb;
   char buf[128];
   Evas_Coord w, ow;
   Data *pd = _pd;

   proc = (void *) data;

   if (strcmp(source, "elm.swallow.content")) return NULL;
   if (!proc) return NULL;

   Item_Cache *it = evisum_ui_item_cache_item_get(pd->cache);
   if (!it)
     {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
     }

   evas_object_geometry_get(pd->btn_menu, NULL, NULL, &ow, NULL);
   evas_object_geometry_get(pd->btn_cmd, NULL, NULL, &w, NULL);
   w += (ow - 8);
   lb = evas_object_data_get(it->obj, "cmd");
   snprintf(buf, sizeof(buf), "%s", proc->command);
   if (strcmp(buf, elm_object_text_get(lb)))
     elm_object_text_set(lb, buf);
   hbx = evas_object_data_get(lb, "hbx");
   evas_object_geometry_get(hbx, NULL, NULL, &ow, NULL);
   if (ow > w)
     {
        evas_object_size_hint_min_set(pd->btn_cmd, ow , 1);
        pd->skip_wait = 1;
     }
   rec = evas_object_data_get(lb, "rec");
   evas_object_size_hint_min_set(rec, w, 1);
   evas_object_show(lb);
   elm_box_recalculate(hbx);

   const char *new = evisum_icon_path_get(evisum_icon_cache_find(proc));
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
        evas_object_geometry_get(pd->btn_uid, NULL, NULL, &w, NULL);
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
             evas_object_size_hint_min_set(pd->btn_uid, ow, 1);
             pd->skip_wait = 1;
          }
        _field_adjust(pd, PROC_FIELD_UID, lb, w);
     }

   if (_field_enabled(PROC_FIELD_PID))
     {
        evas_object_geometry_get(pd->btn_pid, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "pid");
        snprintf(buf, sizeof(buf), "%d", proc->pid);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_PID, lb, w);
     }

   if (_field_enabled(PROC_FIELD_THREADS))
     {
        evas_object_geometry_get(pd->btn_threads, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "thr");
        snprintf(buf, sizeof(buf), "%d", proc->numthreads);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_THREADS, lb, w);
     }

   if (_field_enabled(PROC_FIELD_CPU))
     {
        evas_object_geometry_get(pd->btn_cpu, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "cpu");
        snprintf(buf, sizeof(buf), "%d", proc->cpu_id);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_CPU, lb, w);
     }

   if (_field_enabled(PROC_FIELD_PRI))
     {
        evas_object_geometry_get(pd->btn_pri, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "prio");
        snprintf(buf, sizeof(buf), "%d", proc->priority);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_PRI, lb, w);
     }

   if (_field_enabled(PROC_FIELD_NICE))
     {
        evas_object_geometry_get(pd->btn_nice, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "nice");
        snprintf(buf, sizeof(buf), "%d", proc->nice);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_NICE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_FILES))
     {
        evas_object_geometry_get(pd->btn_files, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "files");
        snprintf(buf, sizeof(buf), "%d", proc->numfiles);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_FILES, lb, w);
     }

   if (_field_enabled(PROC_FIELD_SIZE))
     {
        evas_object_geometry_get(pd->btn_size, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "size");
        snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_size));
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_SIZE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_VIRT))
     {
        evas_object_geometry_get(pd->btn_virt, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "virt");
        snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_virt));
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_VIRT, lb, w);
     }

   if (_field_enabled(PROC_FIELD_RSS))
     {
        evas_object_geometry_get(pd->btn_rss, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "rss");
        snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_rss));
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_RSS, lb, w);
     }

   if (_field_enabled(PROC_FIELD_SHARED))
     {
        evas_object_geometry_get(pd->btn_shared, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "share");
        snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_shared));
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_SHARED, lb, w);
     }

   if (_field_enabled(PROC_FIELD_STATE))
     {
        Evisum_Ui *ui = pd->ui;

        evas_object_geometry_get(pd->btn_state, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "state");
        if ((ui->proc.has_wchan) && (proc->state[0] == 's' && proc->state[1] == 'l'))
          snprintf(buf, sizeof(buf), "%s", proc->wchan);
        else
          snprintf(buf, sizeof(buf), "%s", proc->state);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_STATE, lb, w);
     }

   if (_field_enabled(PROC_FIELD_TIME))
     {
        evas_object_geometry_get(pd->btn_time, NULL, NULL, &w, NULL);
        lb = evas_object_data_get(it->obj, "time");
        _run_time_set(buf, sizeof(buf), proc->run_time);
        if (strcmp(buf, elm_object_text_get(lb)))
          elm_object_text_set(lb, buf);
        _field_adjust(pd, PROC_FIELD_TIME, lb, w);
     }

   if (_field_enabled(PROC_FIELD_CPU_USAGE))
     {
        pb = evas_object_data_get(it->obj, "cpu_u");
        double value = proc->cpu_usage / 100.0;
        double last = elm_progressbar_value_get(pb);

        evas_object_geometry_get(pd->btn_cpu_usage, NULL, NULL, &w, NULL);
        _field_adjust(pd, PROC_FIELD_CPU_USAGE, pb, w);

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
   Data *pd;
   int h_page, v_page;

   pd = data;
   elm_scroller_gravity_set(pd->glist, 0.0, 0.0);
   elm_scroller_last_page_get(pd->glist, &h_page, &v_page);
   elm_scroller_page_bring_in(pd->glist, h_page, v_page);
   elm_genlist_realized_items_update(pd->glist);
   evas_object_show(pd->glist);

   return 0;
}

static void
_summary_reset(Data *pd)
{
   pd->summary.total = pd->summary.running = pd->summary.sleeping = 0;
   pd->summary.stopped = pd->summary.idle  = pd->summary.zombie = 0;
   pd->summary.dsleep = pd->summary.dead = 0;
}

static void
_summary_update(Data *pd)
{
   Eina_Strbuf *buf = eina_strbuf_new();

   eina_strbuf_append_printf(buf, "%i processes: ", pd->summary.total);
   if (pd->summary.running)
     eina_strbuf_append_printf(buf, "%i running, ", pd->summary.running);
   if (pd->summary.sleeping)
     eina_strbuf_append_printf(buf, "%i sleeping, ", pd->summary.sleeping);
   if (pd->summary.stopped)
     eina_strbuf_append_printf(buf, "%i stopped, ", pd->summary.stopped);
   if (pd->summary.idle)
     eina_strbuf_append_printf(buf, "%i idle, ", pd->summary.idle);
   if (pd->summary.dead)
     eina_strbuf_append_printf(buf, "%i dead, ", pd->summary.dead);
   if (pd->summary.dsleep)
     eina_strbuf_append_printf(buf, "%i dsleep, ", pd->summary.dsleep);
   if (pd->summary.zombie)
     eina_strbuf_append_printf(buf, "%i zombie, ", pd->summary.zombie);

   eina_strbuf_replace_last(buf, ",", ".");

   elm_object_text_set(pd->summary.lb, eina_strbuf_string_get(buf));

   eina_strbuf_free(buf);
}

static void
_summary_total(Data *pd, Proc_Info *proc)
{
   pd->summary.total++;
   if (!strcmp(proc->state, "running"))
     pd->summary.running++;
   else if (!strcmp(proc->state, "sleeping"))
     pd->summary.sleeping++;
   else if (!strcmp(proc->state, "stopped"))
     pd->summary.stopped++;
   else if (!strcmp(proc->state, "idle"))
     pd->summary.idle++;
   else if (!strcmp(proc->state, "zombie"))
     pd->summary.zombie++;
   else if (!strcmp(proc->state, "dead"))
     pd->summary.dead++;
   else if (!strcmp(proc->state, "dsleep"))
     pd->summary.dsleep++;
}

static Eina_List *
_process_list_sort(Eina_List *list, Data *pd)
{
   Evisum_Ui *ui;
   Sorter s;
   ui = pd->ui;

   s = pd->sorters[ui->proc.sort_type];

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
_process_ignore(Data *pd, Proc_Info *proc)
{
   Evisum_Ui *ui = pd->ui;

   if (proc->pid == ui->program_pid) return 1;

   if (!pd->search.len) return 0;

   if ((strncasecmp(proc->command, pd->search.text, pd->search.len)) &&
       (!strstr(proc->command, pd->search.text)))
     return 1;

   return 0;
}

static Eina_List *
_process_list_search_trim(Eina_List *list, Data *pd)
{
   Eina_List *l, *l_next;
   Proc_Info *proc;

   _summary_reset(pd);

   EINA_LIST_FOREACH_SAFE(list, l, l_next, proc)
     {
       if (_process_ignore(pd, proc))
         {
            proc_info_free(proc);
            list = eina_list_remove_list(list, l);
         }
        else
         {
            int64_t *cpu_time, id = proc->pid;

            if ((cpu_time = eina_hash_find(pd->cpu_times, &id)))
              {
                 if (*cpu_time)
                   proc->cpu_usage = (double) (proc->cpu_time - *cpu_time) /
                                               pd->ui->proc.poll_delay;
                 *cpu_time = proc->cpu_time;
              }
            else
              {
                 cpu_time = malloc(sizeof(int64_t));
                 if (cpu_time)
                   {
                      *cpu_time = proc->cpu_time;
                      eina_hash_add(pd->cpu_times, &id, cpu_time);
                   }
              }
            _summary_total(pd, proc);
         }
     }

    return list;
}

static Eina_List *
_process_list_get(Data *pd)
{
   Eina_List *list;
   Evisum_Ui *ui;

   ui = pd->ui;

   list = proc_info_all_get();

   if (ui->proc.show_user)
     list = _process_list_uid_trim(list, getuid());

   list = _process_list_search_trim(list, pd);
   list = _process_list_sort(list, pd);

   return list;
}

static void
_process_list(void *data, Ecore_Thread *thread)
{
   Data *pd;
   Eina_List *list;
   Evisum_Ui *ui;
   Proc_Info *proc;
   int delay = 1;

   pd = data;
   ui = pd->ui;

   while (!ecore_thread_check(thread))
     {
        for (int i = 0; i < delay * 8; i++)
          {
             if (ecore_thread_check(thread)) return;

             if (pd->skip_wait)
               {
                  pd->skip_wait = 0;
                  break;
               }
             usleep(125000);
          }
        list = _process_list_get(pd);
        if (!pd->skip_wait)
          {
             ecore_thread_feedback(thread, list);
          }
        else
          {
             EINA_LIST_FREE(list, proc)
               proc_info_free(proc);
          }
        delay = ui->proc.poll_delay;
     }
}

static void
_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED,
                          void *msg EINA_UNUSED)
{
   Data *pd;
   Eina_List *list;
   Proc_Info *proc;
   Elm_Object_Item *it;
   int n;

   pd = data;
   list = msg;

   n = eina_list_count(list);

   _glist_ensure_n_items(pd->glist, n, &pd->itc);

   it = elm_genlist_first_item_get(pd->glist);
   EINA_LIST_FREE(list, proc)
     {
        Proc_Info *prev = elm_object_item_data_get(it);
        if (prev)
          proc_info_free(prev);

        elm_object_item_data_set(it, proc);

        it = elm_genlist_item_next_get(it);
     }

   elm_genlist_realized_items_update(pd->glist);

   _summary_update(pd);

#if DIRTY_GENLIST_HACK
   Eina_List *real = elm_genlist_realized_items_get(pd->glist);
   n = eina_list_count(pd->cache->active);
   if (n > eina_list_count(real) * 2)
     {
        evisum_ui_item_cache_steal(pd->cache, real);
        pd->skip_wait = 1;
     }
   eina_list_free(real);
#endif

#if 0
   printf("active %d and inactive %d => %d (realized)\n",
           eina_list_count(pd->cache->active),
           eina_list_count(pd->cache->inactive), n);
#endif
   if (!pd->poll_count)
     ecore_timer_add(1.0, _bring_in, pd);
   pd->poll_count++;

   if (evisum_ui_effects_enabled_get(pd->ui))
     elm_object_signal_emit(pd->indicator, "indicator,show", "evisum/indicator");
}

static void
_process_list_update(Data *pd)
{
   pd->skip_wait = 1;
}

static void
_btn_icon_state_update(Evas_Object *btn, Eina_Bool reverse,
                       Eina_Bool selected EINA_UNUSED)
{
   Evas_Object *ic = elm_icon_add(btn);

   if (reverse)
     elm_icon_standard_set(ic, evisum_icon_path_get("go-down"));
   else
     elm_icon_standard_set(ic, evisum_icon_path_get("go-up"));

   elm_object_part_content_set(btn, "icon", ic);

   evas_object_show(ic);
}

static void
_btn_clicked_state_save(Data *pd, Evas_Object *btn)
{
   Evisum_Ui *ui = pd->ui;

   if (pd->fields_menu)
     {
        evas_object_del(pd->fields_menu);
        pd->fields_menu = NULL;
        return;
     }
   _btn_icon_state_update(btn, ui->proc.sort_reverse, 0);

   _process_list_update(pd);

   elm_scroller_page_bring_in(pd->glist, 0, 0);
}

static void
_btn_clicked_cb(void *data, Evas_Object *obj,
                void *event_info EINA_UNUSED)
{
   Data *pd;
   Evisum_Ui *ui;
   Proc_Sort type;
   int t;

   pd = data;
   ui = pd->ui;

   t = (intptr_t) evas_object_data_get(obj, "type");
   type = (t & 0xff);

   if (ui->proc.sort_type == type)
     ui->proc.sort_reverse = !ui->proc.sort_reverse;
   ui->proc.sort_type = type;
   _btn_clicked_state_save(pd, obj);
}

static void
_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj,
                        void *ev EINA_UNUSED)
{
   Data *pd = data;

   evas_object_del(obj);

   pd->menu = NULL;
}

static void
_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Data *pd = data;

   kill(pd->selected_pid, SIGCONT);
}

static void
_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Data *pd = data;

   kill(pd->selected_pid, SIGSTOP);
}

static void
_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Data *pd = data;

   kill(pd->selected_pid, SIGKILL);
}

static void
_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   elm_menu_close(pd->menu);
   pd->menu = NULL;
}

static void
_item_menu_debug_cb(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Proc_Info *proc;
   const char *terminal = "xterm";
   Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   proc = proc_info_by_pid(pd->selected_pid);
   if (!proc) return;

   if (ecore_file_app_installed("terminology"))
     terminal = "terminology";

   ecore_exe_run(eina_slstr_printf("%s -e gdb attach %d", terminal, proc->pid),
                 NULL);

   proc_info_free(proc);
}

static void
_item_menu_actions_add(Evas_Object *menu, Elm_Object_Item *menu_it, Data *pd)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("bug"),
                     _("Debug"), _item_menu_debug_cb, pd);
}

static void
_item_menu_manual_cb(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   ui_process_view_win_add(pd->selected_pid, PROC_VIEW_MANUAL);
}

static void
_item_menu_threads_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   ui_process_view_win_add(pd->selected_pid, PROC_VIEW_THREADS);
}

static void
_item_menu_children_cb(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   ui_process_view_win_add(pd->selected_pid, PROC_VIEW_CHILDREN);
}

static void
_item_menu_general_cb(void *data, Evas_Object *obj EINA_UNUSED,
                      void *event_info EINA_UNUSED)
{
   Data *pd = data;

   _item_menu_cancel_cb(pd, NULL, NULL);

   ui_process_view_win_add(pd->selected_pid, PROC_VIEW_DEFAULT);
}

static void
_item_menu_info_add(Evas_Object *menu, Elm_Object_Item *menu_it, Data *pd)
{
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("info"),
                     _("General"), _item_menu_general_cb, pd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("proc"),
                     _("Children"), _item_menu_children_cb, pd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("threads"),
                     _("Threads"), _item_menu_threads_cb, pd);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("manual"),
                     _("Manual"), _item_menu_manual_cb, pd);
}

static Evas_Object *
_item_menu_create(Data *pd, Proc_Info *proc)
{
   Elm_Object_Item *menu_it, *menu_it2;
   Evas_Object *menu;
   Eina_Bool stopped;

   if (!proc) return NULL;

   pd->selected_pid = proc->pid;

   pd->menu = menu = elm_menu_add(pd->win);
   if (!menu) return NULL;

   evas_object_smart_callback_add(menu, "dismissed",
                                  _item_menu_dismissed_cb, pd);

   stopped = !(!strcmp(proc->state, "stop"));

   menu_it = elm_menu_item_add(menu, NULL,
                               evisum_icon_path_get(evisum_icon_cache_find(proc)),
                               proc->command, NULL, NULL);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("actions"),
                                _("Actions"), NULL, NULL);
   _item_menu_actions_add(menu, menu_it2, pd);
   elm_menu_item_separator_add(menu, menu_it);

   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("start"),
                                _("Start"), _item_menu_start_cb, pd);

   elm_object_item_disabled_set(menu_it2, stopped);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("stop"),
                                _("Stop"), _item_menu_stop_cb, pd);

   elm_object_item_disabled_set(menu_it2, !stopped);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("kill"), "Kill",
                     _item_menu_kill_cb, pd);

   elm_menu_item_separator_add(menu, menu_it);
   menu_it2 = elm_menu_item_add(menu, menu_it, evisum_icon_path_get("info"),
                                _("Info"), NULL, pd);
   _item_menu_info_add(menu, menu_it2, pd);

   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, evisum_icon_path_get("cancel"),
                     _("Cancel"), _item_menu_cancel_cb, pd);

   return menu;
}

static void
_item_pid_secondary_clicked_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
                               Evas_Object *obj, void *event_info)
{
   Evas_Object *menu;
   Evas_Event_Mouse_Up *ev;
   Data *pd;
   Elm_Object_Item *it;
   Proc_Info *proc;

   ev = event_info;
   if (ev->button != 3) return;

   it = elm_genlist_at_xy_item_get(obj, ev->output.x, ev->output.y, NULL);
   proc = elm_object_item_data_get(it);
   if (!proc) return;

   pd = data;

   menu = _item_menu_create(pd, proc);
   if (!menu) return;

   elm_menu_move(menu, ev->canvas.x, ev->canvas.y);
   evas_object_show(menu);
}

static void
_item_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Elm_Object_Item *it;
   Proc_Info *proc;
   Data *pd;

   pd = data;
   it = event_info;

   elm_genlist_item_selected_set(it, 0);
   if (pd->menu) return;

   proc = elm_object_item_data_get(it);
   if (!proc) return;

   pd->selected_pid = proc->pid;
   ui_process_view_win_add(proc->pid, PROC_VIEW_DEFAULT);
}

static Eina_Bool
_main_menu_timer_cb(void *data)
{
   Data *pd = data;

   evas_object_del(pd->main_menu);
   pd->main_menu_timer = NULL;
   pd->main_menu = NULL;

   return 0;
}

static void
_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
                        void *ev EINA_UNUSED)
{
   Data *pd = data;

   elm_ctxpopup_dismiss(pd->main_menu);
   if (pd->main_menu_timer)
     _main_menu_timer_cb(pd);
   else
     pd->main_menu_timer = ecore_timer_add(0.2, _main_menu_timer_cb, pd);
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
   Data *pd;
   Evisum_Ui *ui;

   pd = data;
   ui = pd->ui;

   if (!pd->main_menu)
     pd->main_menu = evisum_ui_main_menu_create(ui, ui->proc.win, obj);
   else
     _main_menu_dismissed_cb(pd, NULL, NULL);
}

static Evas_Object *
_content_add(Data *pd, Evas_Object *parent)
{
   Evas_Object *tb, *btn, *glist;
   Evas_Object *fr, *lb;
   Evisum_Ui *ui = pd->ui;

   tb = elm_table_add(parent);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   evas_object_show(tb);

   pd->btn_menu = btn = _btn_create(tb, "menu", _("Menu"),
                                    _btn_menu_clicked_cb, pd);

   pd->btn_cmd = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CMD ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CMD);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CMD);
   elm_object_text_set(btn, _("command"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_CMD].btn = btn;

   pd->btn_uid = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_UID ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_UID);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_UID);
   elm_object_text_set(btn, _("user"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_UID].btn = btn;

   pd->btn_pid = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_PID ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_PID);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_PID);
   elm_object_text_set(btn, _("pid"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_PID].btn = btn;

   pd->btn_threads = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_THREADS ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_THREADS);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_THREADS);
   elm_object_text_set(btn, _("thr"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_THREADS].btn = btn;

   pd->btn_cpu = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CPU ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CPU);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CPU);
   elm_object_text_set(btn, _("cpu"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_CPU].btn = btn;

   pd->btn_pri = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_PRI ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_PRI);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_PRI);
   elm_object_text_set(btn, _("prio"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_PRI].btn = btn;

   pd->btn_nice = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_NICE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_NICE);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_NICE);
   elm_object_text_set(btn, _("nice"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked",
                                  _btn_clicked_cb, pd);
   _fields[PROC_FIELD_NICE].btn = btn;

   pd->btn_files = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_FILES ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_FILES);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_FILES);
   elm_object_text_set(btn, _("files"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_FILES].btn = btn;

   pd->btn_size = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_SIZE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_SIZE);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_SIZE);
   elm_object_text_set(btn, _("size"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_SIZE].btn = btn;

   pd->btn_virt = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_VIRT ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_VIRT);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_VIRT);
   elm_object_text_set(btn, _("virt"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_VIRT].btn = btn;

   pd->btn_rss = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_RSS ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_RSS);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_RSS);
   elm_object_text_set(btn, _("res"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_RSS].btn = btn;

   pd->btn_shared = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_SHARED ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_SHARED);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_SHARED);
   elm_object_text_set(btn, _("shr"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_SHARED].btn = btn;

   pd->btn_state = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_STATE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_STATE);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_STATE);
   elm_object_text_set(btn, _("state"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_STATE].btn = btn;

   pd->btn_time = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_TIME ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_TIME);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_TIME);
   elm_object_text_set(btn, _("time"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_TIME].btn = btn;

   pd->btn_cpu_usage = btn = elm_button_add(parent);
   _btn_icon_state_update(btn,
            (ui->proc.sort_type == PROC_SORT_BY_CPU_USAGE ?
            ui->proc.sort_reverse : 0),
            ui->proc.sort_type == PROC_SORT_BY_CPU_USAGE);
   evas_object_size_hint_weight_set(btn, 1.0, 0);
   evas_object_size_hint_align_set(btn, FILL, FILL);
   evas_object_data_set(btn, "type", (void *) (int) PROC_SORT_BY_CPU_USAGE);
   elm_object_text_set(btn, _("cpu %"));
   evas_object_show(btn);
   evas_object_smart_callback_add(btn, "clicked", _btn_clicked_cb, pd);
   _fields[PROC_FIELD_CPU_USAGE].btn = btn;

   pd->glist = glist = elm_genlist_add(parent);
   elm_genlist_homogeneous_set(glist, 1);
   elm_scroller_gravity_set(glist, 0.0, 1.0);
   elm_object_focus_allow_set(glist, 1);
   elm_scroller_policy_set(glist, ELM_SCROLLER_POLICY_AUTO,
                           (ui->proc.show_scroller ?
                            ELM_SCROLLER_POLICY_AUTO :
                            ELM_SCROLLER_POLICY_OFF));
   elm_genlist_multi_select_set(glist, 0);
   evas_object_size_hint_weight_set(glist, EXPAND, EXPAND);
   evas_object_size_hint_align_set(glist, FILL, FILL);

   pd->itc.item_style = "full";
   pd->itc.func.text_get = NULL;
   pd->itc.func.content_get = _content_get;
   pd->itc.func.filter_get = NULL;
   pd->itc.func.del = _item_del;

   evas_object_smart_callback_add(glist, "selected",
                                  _item_pid_clicked_cb, pd);
   evas_object_event_callback_add(glist, EVAS_CALLBACK_MOUSE_UP,
                                  _item_pid_secondary_clicked_cb, pd);
   evas_object_smart_callback_add(glist, "unrealized",
                                  _item_unrealized_cb, pd);

   pd->summary.fr = fr = elm_frame_add(parent);
   elm_object_style_set(fr, "pad_small");
   evas_object_size_hint_weight_set(fr, EXPAND, 0);
   evas_object_size_hint_align_set(fr, 0, FILL);

   pd->summary.lb = lb = elm_label_add(fr);
   evas_object_size_hint_weight_set(lb, EXPAND, 0);
   evas_object_size_hint_align_set(lb, 0.0, FILL);
   evas_object_show(lb);
   elm_object_content_set(fr, lb);

   _fields_init(pd);

   return tb;
}

static Eina_Bool
_search_empty_cb(void *data)
{
   Data *pd = data;

   if (!pd->search.len)
     {
        evas_object_lower(pd->search.pop);
        evas_object_hide(pd->search.pop);
        elm_object_focus_allow_set(pd->search.entry, 0);
        pd->search.visible = 0;
        pd->search.timer = NULL;
        pd->skip_wait = 1;
        return 0;
     }

   if (pd->search.keytime &&
       ((ecore_loop_time_get() - pd->search.keytime) > 0.1))
     {
        pd->skip_wait = 1;
        pd->search.keytime = 0;
     }

   return 1;
}

static void
_search_clear(Data *pd)
{
   if (pd->search.text)
     free(pd->search.text);
   pd->search.text = NULL;
   pd->search.len = 0;
}

static void
_search_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   const char *text;
   Data *pd;

   pd = data;
   ev = event_info;

   if (ev && !strcmp(ev->keyname, "Escape"))
     elm_object_text_set(pd->search.entry, "");

   text = elm_object_text_get(obj);
   if (text)
     {
        pd->search.keytime = ecore_loop_time_get();
        _search_clear(pd);
        pd->search.text = strdup(text);
        pd->search.len = strlen(text);
        if (!pd->search.timer)
          pd->search.timer = ecore_timer_add(0.05, _search_empty_cb, pd);
     }
}

static void
_search_add(Data *pd)
{
   Evas_Object *tb, *fr, *rec, *entry;

   pd->search.pop = tb = elm_table_add(pd->win);
   evas_object_lower(tb);

   rec = evas_object_rectangle_add(evas_object_evas_get(pd->win));
   evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
   evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
   elm_table_pack(tb, rec, 0, 0, 1, 1);

   fr = elm_frame_add(pd->win);
   elm_object_text_set(fr, _("Search"));
   evas_object_size_hint_weight_set(fr, 0, 0);
   evas_object_size_hint_align_set(fr, FILL, 0.5);

   pd->search.entry = entry = elm_entry_add(fr);
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
                                  _search_key_down_cb, pd);
}

static void
_win_key_down_search(Data *pd, Evas_Event_Key_Down *ev)
{
   Evas_Coord w, h;

   if (!strcmp(ev->keyname, "Escape"))
     {
        elm_object_text_set(pd->search.entry, "");
        _search_clear(pd);
        pd->skip_wait = 0;
        elm_object_focus_allow_set(pd->search.entry, 0);
        evas_object_lower(pd->search.pop);
        evas_object_hide(pd->search.pop);
        pd->search.visible = 0;
     }
   else if (ev->string && strcmp(ev->keyname, "BackSpace"))
     {
        if ((isspace(ev->string[0])) || (iscntrl(ev->string[0]))) return;
        size_t len = strlen(ev->string);
        if (len)
          {
             elm_entry_entry_append(pd->search.entry, ev->string);
             elm_entry_cursor_pos_set(pd->search.entry, len);
             _search_key_down_cb(pd, NULL, pd->search.entry, NULL);
          }
        evas_object_geometry_get(pd->win, NULL, NULL, &w, &h);
        evas_object_move(pd->search.pop, w / 2, h / 2);
        evas_object_raise(pd->search.pop);
        elm_object_focus_allow_set(pd->search.entry, 1);
        elm_object_focus_set(pd->search.entry, 1);
        evas_object_show(pd->search.pop);
        pd->search.visible = 1;
     }
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Data *pd;
   Evas_Coord x, y, w, h;

   pd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!pd) return;

   elm_scroller_region_get(pd->glist, &x, &y, &w, &h);

   if (!strcmp(ev->keyname, "Escape") && !pd->search.visible)
     {
        evas_object_del(pd->win);
        return;
     }
   else if (!strcmp(ev->keyname, "Prior"))
     elm_scroller_region_bring_in(pd->glist, x, y - h, w, h);
   else if (!strcmp(ev->keyname, "Next"))
     elm_scroller_region_bring_in(pd->glist, x, y + h, w, h);
   else
     _win_key_down_search(pd, ev);

   pd->skip_wait = 1;
}

static Eina_Bool
_resize_cb(void *data)
{
   Data *pd = data;

   pd->skip_wait = 0;
   pd->resize_timer = NULL;

   return 0;
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Data *pd;
   Evisum_Ui *ui;

   pd = data;
   ui = pd->ui;

   elm_genlist_realized_items_update(pd->glist);

   pd->skip_wait = 1;

   if (pd->resize_timer)
     ecore_timer_reset(pd->resize_timer);
   else pd->resize_timer = ecore_timer_add(0.2, _resize_cb, pd);

   evas_object_lower(pd->search.pop);
   if (pd->main_menu)
     _main_menu_dismissed_cb(pd, NULL, NULL);

   evas_object_geometry_get(obj, NULL, NULL,
                            &ui->proc.width, &ui->proc.height);

   if (!evisum_ui_effects_enabled_get(ui)) return;
   evas_object_move(pd->indicator, ui->proc.width - ELM_SCALE_SIZE(32),
                    ui->proc.height - ELM_SCALE_SIZE(32));
   evas_object_show(pd->indicator);
}

static void
_win_alpha_set(Data *pd)
{
   Evas_Object *bg, *win;
   Evisum_Ui *ui;
   int r, g, b, a;
   double fade;

   win = pd->win;
   ui = pd->ui;

   bg = evas_object_data_get(win, "bg");
   if (!bg) return;

   fade = ui->proc.alpha / 100.0;

   // FIXME: Base window colour from theme.
   if (ui->proc.transparent)
     {
        r = b = g = 128; a = 255;
        evas_object_color_set(bg, r * fade, g * fade, b * fade, fade * a);
        r = b = g = a = 255;
        evas_object_color_set(pd->tb_main, r * fade, g * fade, b * fade, fade * a);
     }
   else
     {
        r = b = g = a = 255;
        evas_object_color_set(pd->tb_main, r, g, b, a);
        r = b = g = 128;  a = 255;
        evas_object_color_set(bg, r, g, b, a);
     }

   if (ui->proc.transparent != pd->transparent)
     {
        elm_win_alpha_set(win, ui->proc.transparent);
     }
   pd->transparent = ui->proc.transparent;
}

static Eina_Bool
_evisum_config_changed_cb(void *data, int type EINA_UNUSED,
                          void *event EINA_UNUSED)
{
   Eina_Iterator *it;
   Evisum_Ui *ui;
   Data *pd;
   void *d = NULL;

   pd = data;
   ui = pd->ui;

   it = eina_hash_iterator_data_new(pd->cpu_times);
   while (eina_iterator_next(it, &d))
     {
       int64_t *t = d;
       *t = 0;
     }
   eina_iterator_free(it);

   elm_scroller_policy_set(pd->glist, ELM_SCROLLER_POLICY_OFF,
                           (ui->proc.show_scroller ?
                            ELM_SCROLLER_POLICY_AUTO :
                            ELM_SCROLLER_POLICY_OFF));
   pd->skip_wait = 1;

   _win_alpha_set(pd);

   return 1;
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
             void *event_info EINA_UNUSED)
{
   Data *pd;
   Evisum_Ui *ui;

   pd = data;
   ui = pd->ui;

   evas_object_geometry_get(obj, &ui->proc.x, &ui->proc.y, NULL, NULL);
}

static void
_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
            Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Data *pd;

   pd = data;
   ui = pd->ui;

   evisum_ui_config_save(ui);

   if (pd->search.timer)
     ecore_timer_del(pd->search.timer);

   if (pd->thread)
     ecore_thread_cancel(pd->thread);

   if (pd->thread)
     ecore_thread_wait(pd->thread, 0.5);

   ecore_event_handler_del(pd->handler);

   pd->thread = NULL;
   ui->proc.win = NULL;

   if (pd->search.text)
     free(pd->search.text);

   if (pd->cache)
     evisum_ui_item_cache_free(pd->cache);

   eina_hash_free(pd->cpu_times);

   free(pd);
   pd = NULL;
}

static void
_effects_add(Data *pd, Evas_Object *win)
{
   Elm_Layout *lay;
   Evas_Object *pb;

   if (evisum_ui_effects_enabled_get(pd->ui))
     {
        pb = elm_progressbar_add(win);
        elm_object_style_set(pb, "wheel");
        elm_progressbar_pulse_set(pb, 1);
        elm_progressbar_pulse(pb, 1);
        evas_object_show(pb);

        pd->indicator = lay = elm_layout_add(win);
        elm_layout_file_set(lay, PACKAGE_DATA_DIR"/themes/evisum.edj", "proc");
        elm_layout_content_set(lay, "evisum/indicator", pb);
        evas_object_show(lay);
     }

   _win_alpha_set(pd);
   evas_object_show(win);
}

static void
_init(Data *pd)
{
   pd->sorters[PROC_SORT_BY_NONE].sort_cb = proc_sort_by_pid;
   pd->sorters[PROC_SORT_BY_UID].sort_cb = proc_sort_by_uid;
   pd->sorters[PROC_SORT_BY_PID].sort_cb = proc_sort_by_pid;
   pd->sorters[PROC_SORT_BY_THREADS].sort_cb = proc_sort_by_threads;
   pd->sorters[PROC_SORT_BY_CPU].sort_cb = proc_sort_by_cpu;
   pd->sorters[PROC_SORT_BY_PRI].sort_cb = proc_sort_by_pri;
   pd->sorters[PROC_SORT_BY_NICE].sort_cb = proc_sort_by_nice;
   pd->sorters[PROC_SORT_BY_FILES].sort_cb = proc_sort_by_files;
   pd->sorters[PROC_SORT_BY_SIZE].sort_cb = proc_sort_by_size;
   pd->sorters[PROC_SORT_BY_VIRT].sort_cb = proc_sort_by_virt;
   pd->sorters[PROC_SORT_BY_RSS].sort_cb = proc_sort_by_rss;
   pd->sorters[PROC_SORT_BY_SHARED].sort_cb = proc_sort_by_shared;
   pd->sorters[PROC_SORT_BY_CMD].sort_cb = proc_sort_by_cmd;
   pd->sorters[PROC_SORT_BY_STATE].sort_cb = proc_sort_by_state;
   pd->sorters[PROC_SORT_BY_TIME].sort_cb = proc_sort_by_time;
   pd->sorters[PROC_SORT_BY_CPU_USAGE].sort_cb = proc_sort_by_cpu_usage;
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

   Data *pd = _pd = calloc(1, sizeof(Data));
   if (!pd) return;

   pd->selected_pid = -1;
   pd->ui = ui;
   pd->handler = ecore_event_handler_add(EVISUM_EVENT_CONFIG_CHANGED,
                                         _evisum_config_changed_cb, pd);
   _init(pd);

   ui->proc.win = pd->win = win = elm_win_add(NULL, "evisum", ELM_WIN_BASIC);
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

   pd->tb_main = tb = _content_add(pd, win);
   elm_win_resize_object_add(win, tb);
   elm_object_content_set(win, tb);

   pd->cache = evisum_ui_item_cache_new(pd->glist, _item_create, 40);
   pd->cpu_times = eina_hash_int64_new(_cpu_times_free_cb);

   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL,
                                  _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE,
                                  _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE,
                                  _win_move_cb, pd);
   evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN,
                                  _win_key_down_cb, pd);

   _search_add(pd);
   _effects_add(pd, win);
   _content_reset(pd);

   _win_resize_cb(pd, NULL, win, NULL);
   pd->thread = ecore_thread_feedback_run(_process_list,
                                          _process_list_feedback_cb,
                                          NULL,
                                          NULL,
                                          pd, 0);
}

