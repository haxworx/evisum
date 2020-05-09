#ifndef __UI_H__
#define __UI_H__

#include <Elementary.h>
#include "gettext.h"
#include "process.h"
#include "configuration.h"

#define _(STR) gettext(STR)

#define EVISUM_SIZE_WIDTH  640
#define EVISUM_SIZE_HEIGHT 480

typedef enum
{
   PROCESS_INFO_FIELD_PID,
   PROCESS_INFO_FIELD_UID,
   PROCESS_INFO_FIELD_SIZE,
   PROCESS_INFO_FIELD_RSS,
   PROCESS_INFO_FIELD_COMMAND,
   PROCESS_INFO_FIELD_STATE,
   PROCESS_INFO_FIELD_CPU_USAGE,

   // Not displayed in the main UI.
   PROCESS_INFO_FIELD_NICE,
   PROCESS_INFO_FIELD_PRI,
   PROCESS_INFO_FIELD_CPU,
   PROCESS_INFO_FIELD_THREADS,
   // Not used yet in UI.
   PROCESS_INFO_FIELD_CPU_TIME,
} Proc_Info_Field;

#define PROCESS_INFO_FIELDS 7

typedef enum
{
   DATA_UNIT_B  = 'B',
   DATA_UNIT_KB = 'K',
   DATA_UNIT_MB = 'M',
   DATA_UNIT_GB = 'G',
} Data_Unit;

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

#define TAB_BTN_SIZE 75

typedef struct Ui
{
   Evas_Object  *win;
   Evas_Object  *menu;
   Evas_Object  *panel;
   Evas_Object  *scroller;
   Evas_Object  *content;
   Evas_Object  *btn_general;
   Evas_Object  *btn_cpu;
   Evas_Object  *btn_mem;
   Evas_Object  *btn_storage;
   Evas_Object  *btn_misc;

   Evas_Object  *disk_view;
   Evas_Object  *disk_activity;
   Evas_Object  *cpu_view;
   Evas_Object  *cpu_activity;
   Evas_Object  *mem_view;
   Evas_Object  *mem_activity;
   Evas_Object  *misc_view;
   Evas_Object  *misc_activity;
   Evas_Object  *system_activity;

   Elm_Transit  *transit;
   Evas_Object  *current_view;

   Eina_Bool     cpu_visible;
   Eina_Bool     misc_visible;
   Eina_Bool     disk_visible;
   Eina_Bool     mem_visible;

   Evas_Object  *progress_cpu;
   Evas_Object  *progress_mem;

   Evas_Object  *title_mem;
   Evas_Object  *progress_mem_used;
   Evas_Object  *progress_mem_cached;
   Evas_Object  *progress_mem_buffered;
   Evas_Object  *progress_mem_shared;
   Evas_Object  *progress_mem_swap;

   Evas_Object  *btn_pid;
   Evas_Object  *btn_uid;
   Evas_Object  *btn_size;
   Evas_Object  *btn_rss;
   Evas_Object  *btn_cmd;
   Evas_Object  *btn_state;
   Evas_Object  *btn_cpu_usage;

   Eina_List    *item_cache;
   Evas_Object  *genlist_procs;

   Evas_Object  *entry_pid_cmd;
   Evas_Object  *entry_pid_cmd_args;
   Evas_Object  *entry_pid_user;
   Evas_Object  *entry_pid_pid;
   Evas_Object  *entry_pid_uid;
   Evas_Object  *entry_pid_cpu;
   Evas_Object  *entry_pid_threads;
   Evas_Object  *entry_pid_virt;
   Evas_Object  *entry_pid_rss;
   Evas_Object  *entry_pid_size;
   Evas_Object  *entry_pid_nice;
   Evas_Object  *entry_pid_pri;
   Evas_Object  *entry_pid_state;
   Evas_Object  *entry_pid_cpu_usage;
   Evas_Object  *entry_search;

   Ecore_Thread *thread_system;
   Ecore_Thread *thread_process;

   Ecore_Timer  *timer_pid;
   pid_t         selected_pid;
   pid_t         program_pid;

   char         *search_text;
   Evas_Object  *list_pid;

   Eina_Bool     skip_wait;
   Eina_Bool     ready;

   Eina_List    *cpu_times;
   int64_t       pid_cpu_time;

   Eina_List    *cpu_list;
   Evas_Object  *temp_label;

   int           poll_delay;

   Sort_Type     sort_type;
   Eina_Bool     sort_reverse;
   Eina_Bool     panel_visible;
   Eina_Bool     searching;
   Eina_Bool     show_self;

   uint64_t      incoming_max;
   uint64_t      outgoing_max;
} Ui;

Ui *
 ui_add(Evas_Object *win);

void
 ui_shutdown(Ui *ui);

#endif
