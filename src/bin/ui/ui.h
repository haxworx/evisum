#ifndef __UI_H__
#define __UI_H__

#include <Elementary.h>
#include "gettext.h"
#include "system/process.h"
#include "../evisum_config.h"
#include "ui/ui_util.h"
#include "ui/ui_cache.h"

#define _(STR) gettext(STR)

#define EVISUM_WIN_WIDTH  600
#define EVISUM_WIN_HEIGHT 520

typedef enum
{
   SORT_BY_NONE,
   SORT_BY_PID,
   SORT_BY_UID,
   SORT_BY_NICE,
   SORT_BY_PRI,
   SORT_BY_CPU,
   SORT_BY_THREADS,
   SORT_BY_SIZE,
   SORT_BY_RSS,
   SORT_BY_CMD,
   SORT_BY_STATE,
   SORT_BY_CPU_USAGE,
} Sort_Type;

typedef struct Ui
{
   Evas_Object     *win;
   Evas_Object     *menu;
   Evas_Object     *scroller;
   Evas_Object     *content;
   Evas_Object     *btn_general;
   Evas_Object     *btn_cpu;
   Evas_Object     *btn_mem;
   Evas_Object     *btn_storage;
   Evas_Object     *btn_misc;

   Evas_Object     *disk_view;
   Evas_Object     *disk_activity;
   Evas_Object     *cpu_view;
   Evas_Object     *cpu_activity;
   Evas_Object     *mem_view;
   Evas_Object     *mem_activity;
   Evas_Object     *misc_view;
   Evas_Object     *misc_activity;
   Evas_Object     *system_activity;

   Elm_Transit     *transit;
   Evas_Object     *current_view;

   Eina_Bool       cpu_visible;
   Eina_Bool       misc_visible;
   Eina_Bool       disk_visible;
   Eina_Bool       mem_visible;

   Evas_Object     *progress_cpu;
   Evas_Object     *progress_mem;

   Evas_Object     *progress_mem_used;
   Evas_Object     *progress_mem_cached;
   Evas_Object     *progress_mem_buffered;
   Evas_Object     *progress_mem_shared;
   Evas_Object     *progress_mem_swap;

   Evas_Object     *btn_pid;
   Evas_Object     *btn_uid;
   Evas_Object     *btn_size;
   Evas_Object     *btn_rss;
   Evas_Object     *btn_cmd;
   Evas_Object     *btn_state;
   Evas_Object     *btn_cpu_usage;

   Evisum_Ui_Cache *cache;
   Evas_Object     *genlist_procs;
   Evas_Object     *entry_search;
   Evas_Object     *entry_search_border;

   Ecore_Thread    *thread_system;
   Ecore_Thread    *thread_process;
   Ecore_Thread    *thread_cpu;

   Ecore_Timer     *timer_pid;
   pid_t           selected_pid;
   pid_t           program_pid;

   char            *search_text;

   Eina_Bool       skip_wait;
   Eina_Bool       ready;

   Eina_List       *cpu_times;
   Eina_List       *cpu_list;

   int             poll_delay;

   Sort_Type       sort_type;
   Eina_Bool       sort_reverse;
   Eina_Bool       show_self;

   Eina_Bool       zfs_mounted;

   uint64_t        incoming_max;
   uint64_t        outgoing_max;
} Ui;

Ui *
evisum_ui_add(Evas_Object *win);

void
evisum_ui_shutdown(Ui *ui);

#endif
