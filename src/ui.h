#ifndef __UI_H__
#define __UI_H__

#include <Elementary.h>

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
} Proc_Stats_Field;

#define PROCESS_INFO_FIELDS 7

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
   Evas_Object *win;
   Evas_Object *panel;
   Evas_Object *scroller;

   Evas_Object *progress_cpu;
   Evas_Object *progress_mem;

   Evas_Object *entry_pid;
   Evas_Object *entry_uid;
   Evas_Object *entry_size;
   Evas_Object *entry_rss;
   Evas_Object *entry_cmd;
   Evas_Object *entry_state;
   Evas_Object *entry_cpu_usage;

   Evas_Object *btn_pid;
   Evas_Object *btn_uid;
   Evas_Object *btn_size;
   Evas_Object *btn_rss;
   Evas_Object *btn_cmd;
   Evas_Object *btn_state;
   Evas_Object *btn_cpu_usage;

   Evas_Object *entry_pid_cmd;
   Evas_Object *entry_pid_user;
   Evas_Object *entry_pid_pid;
   Evas_Object *entry_pid_uid;
   Evas_Object *entry_pid_cpu;
   Evas_Object *entry_pid_threads;
   Evas_Object *entry_pid_size;
   Evas_Object *entry_pid_rss;
   Evas_Object *entry_pid_nice;
   Evas_Object *entry_pid_pri;
   Evas_Object *entry_pid_state;
   Evas_Object *entry_pid_cpu_usage;

   Ecore_Timer *timer_pid;
   pid_t        selected_pid;
   pid_t        program_pid;

#define TEXT_FIELD_MAX 65535
   char *fields[PROCESS_INFO_FIELDS];

   Evas_Object *list_pid;

   Eina_Bool    first_run;

   int64_t      cpu_times[PID_MAX];
   int64_t      pid_cpu_time;

   int          poll_delay;

   Sort_Type    sort_type;
   Eina_Bool    sort_reverse;
   Eina_Bool    panel_visible;

} Ui;

typedef struct Sys_Stats
{
   int    cpu_count;
   double cpu_usage;
   long   mem_total;
   long   mem_used;
} Sys_Stats;

void
ui_add(Evas_Object *win);

#endif
