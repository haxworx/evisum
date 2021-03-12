#include "ui_memory.h"
#include "system/filesystems.h"

#include <Elementary.h>

typedef struct  {
   Ecore_Thread *thread;
   Evas_Object  *win;

   Evas_Object  *used;
   Evas_Object  *cached;
   Evas_Object  *buffered;
   Evas_Object  *shared;
   Evas_Object  *swap;
   Evas_Object  *video[MEM_VIDEO_CARD_MAX];

   Evisum_Ui    *ui;
} Data;

static Evas_Object *
_label_mem(Evas_Object *parent, const char *text)
{
   Evas_Object *lb = elm_label_add(parent);
   evas_object_size_hint_weight_set(lb, 0, EXPAND);
   evas_object_size_hint_align_set(lb, FILL, FILL);
   elm_object_text_set(lb, eina_slstr_printf("%s",text));

   return lb;
}

static Evas_Object *
_progress_add(Evas_Object *parent)
{
   Evas_Object *pb = elm_progressbar_add(parent);
   evas_object_size_hint_align_set(pb, FILL, FILL);
   evas_object_size_hint_weight_set(pb, EXPAND, EXPAND);
   elm_progressbar_span_size_set(pb, 1.0);

   return pb;
}

static void
_mem_usage_main_cb(void *data EINA_UNUSED, Ecore_Thread *thread)
{
   static meminfo_t memory;

   while (!ecore_thread_check(thread))
     {
        memset(&memory, 0, sizeof(memory));
        system_memory_usage_get(&memory);
        if (file_system_in_use("ZFS"))
          memory.used += memory.zfs_arc_used;

        ecore_thread_feedback(thread, &memory);
        for (int i = 0; i < 8; i++)
           {
              if (ecore_thread_check(thread))
                break;
              usleep(125000);
           }
     }
}

static void
_update_widgets(Data *pd, meminfo_t *memory)
{
   Evas_Object *pb;
   double ratio, value;

   ratio = memory->total / 100.0;

   pb = pd->used;
   value = memory->used / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->used),
                   evisum_size_format(memory->total)));

   pb = pd->cached;
   value = memory->cached / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->cached),
                   evisum_size_format(memory->total)));

   pb = pd->buffered;
   value = memory->buffered / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->buffered),
                   evisum_size_format(memory->total)));
   pb = pd->shared;
   value = memory->shared / ratio;
   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->shared),
                   evisum_size_format(memory->total)));
   pb = pd->swap;
   if (memory->swap_total)
     {
        elm_object_disabled_set(pb, 0);
        ratio = memory->swap_total / 100.0;
        value = memory->swap_used / ratio;
     }
   else
     {
        elm_object_disabled_set(pb, 1);
        value = 0.0;
     }

   elm_progressbar_value_set(pb, value / 100);
   elm_progressbar_unit_format_set(pb,
                   eina_slstr_printf("%s / %s",
                   evisum_size_format(memory->swap_used),
                   evisum_size_format(memory->swap_total)));

   for (int i = 0; i < memory->video_count; i++)
     {
        pb = pd->video[i];
        if (!pb) break;
        if (memory->video[i].total) elm_object_disabled_set(pb, 0);
        else elm_object_disabled_set(pb, 1);
        ratio = memory->video[i].total / 100.0;
        value = memory->video[i].used / ratio;
        elm_progressbar_value_set(pb, value / 100);
        elm_progressbar_unit_format_set(pb,
                        eina_slstr_printf("%s / %s",
                        evisum_size_format(memory->video[i].used),
                        evisum_size_format(memory->video[i].total)));
     }
}

static void
_mem_usage_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msgdata)
{
   Data *pd;
   meminfo_t *memory;

   pd = data;
   memory = msgdata;

   if (ecore_thread_check(thread)) return;

   _update_widgets(pd, memory);
}

static void
_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Evas_Event_Key_Down *ev;
   Data *pd;

   pd = data;
   ev = event_info;

   if (!ev || !ev->keyname)
     return;

   if (!strcmp(ev->keyname, "Escape"))
     evas_object_del(pd->win);
}

static void
_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Data *pd;
   Evisum_Ui *ui;

   pd = data;
   ui = pd->ui;

   evas_object_geometry_get(obj, &ui->mem.x, &ui->mem.y, NULL, NULL);
}

static void
_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
            void *event_info EINA_UNUSED)
{
   Evisum_Ui *ui;
   Data *pd = data;

   ui = pd->ui;

   evisum_ui_config_save(ui);
   ecore_thread_cancel(pd->thread);
   ecore_thread_wait(pd->thread, 0.5);

   ui->mem.win = NULL;
   free(pd);
}

static void
_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
   Data *pd = data;
   Evisum_Ui *ui = pd->ui;

   evas_object_geometry_get(obj, NULL, NULL, &ui->mem.width, &ui->mem.height);
}

void
ui_mem_win_add(Evisum_Ui *ui)
{
   Evas_Object *win, *lb, *bx, *tb, *pb;
   Evas_Object *fr;
   int i;
   meminfo_t memory;

   if (ui->mem.win)
     {
        elm_win_raise(ui->mem.win);
        return;
     }

   Data *pd = calloc(1, sizeof(Data));
   if (!pd) return;
   pd->ui = ui;

   memset(&memory, 0, sizeof(memory));
   system_memory_usage_get(&memory);

   ui->mem.win = win = elm_win_util_standard_add("evisum", _("Memory Usage"));
   pd->win = win;
   elm_win_autodel_set(win, 1);
   evas_object_size_hint_weight_set(win, EXPAND, EXPAND);
   evas_object_size_hint_align_set(win, FILL, FILL);

   bx = elm_box_add(win);
   evas_object_size_hint_weight_set(bx, EXPAND, 0);
   evas_object_size_hint_align_set(bx, FILL, FILL);
   evas_object_show(bx);
   elm_object_content_set(win, bx);

   fr = elm_frame_add(win);
   evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
   evas_object_size_hint_align_set(fr, FILL, FILL);
   elm_object_style_set(fr, "pad_medium");
   evas_object_show(fr);

   tb = elm_table_add(win);
   evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
   evas_object_size_hint_align_set(tb, FILL, FILL);
   elm_table_padding_set(tb, 8, 2);
   evas_object_show(tb);

   elm_object_content_set(fr, tb);
   elm_box_pack_end(bx, fr);

   lb = _label_mem(tb, _("Used"));
   pd->used = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 1, 1, 1);
   elm_table_pack(tb, pb, 2, 1, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Cached"));
   pd->cached = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 2, 1, 1);
   elm_table_pack(tb, pb, 2, 2, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Buffered"));
   pd->buffered = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 3, 1, 1);
   elm_table_pack(tb, pb, 2, 3, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Shared"));
   pd->shared = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 4, 1, 1);
   elm_table_pack(tb, pb, 2, 4, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   lb = _label_mem(tb, _("Swapped"));
   pd->swap = pb = _progress_add(tb);
   elm_table_pack(tb, lb, 1, 5, 1, 1);
   elm_table_pack(tb, pb, 2, 5, 1, 1);
   evas_object_show(lb);
   evas_object_show(pb);

   for (i = 0; i < memory.video_count; i++)
     {
        lb = _label_mem(tb, _("Video"));
        pd->video[i] = pb = _progress_add(tb);
        elm_table_pack(tb, lb, 1, 6 + i, 1, 1);
        elm_table_pack(tb, pb, 2, 6 + i, 1, 1);
        evas_object_show(lb);
        evas_object_show(pb);
     }

   if ((ui->mem.width > 0) && (ui->mem.height > 0))
     evas_object_resize(win, ui->mem.width, ui->mem.height);
   else
     evas_object_resize(win, UI_CHILD_WIN_WIDTH , UI_CHILD_WIN_HEIGHT);

   if ((ui->mem.x > 0) && (ui->mem.y > 0))
     evas_object_move(win, ui->mem.x, ui->mem.y);
   else
     elm_win_center(win, 1, 1);

   evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _win_resize_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _win_del_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _win_move_cb, pd);
   evas_object_event_callback_add(win, EVAS_CALLBACK_KEY_DOWN, _win_key_down_cb, pd);
   evas_object_show(win);

   pd->thread = ecore_thread_feedback_run(_mem_usage_main_cb,
                                          _mem_usage_feedback_cb,
                                          NULL,
                                          NULL,
                                          pd, 1);
}
