#ifndef __UI_PROCESS_H__
#define __UI_PROCESS_H__

#include "ui.h"

typedef struct _Ui_Process {
   Evas_Object  *win;
   Evas_Object  *content;

   Evas_Object  *main_view;
   Evas_Object  *info_view;
   Evas_Object  *thread_view;

   Evas_Object  *btn_main;
   Evas_Object  *btn_info;
   Evas_Object  *btn_thread;

   Evas_Object  *entry_info;

   Evas_Object  *entry_pid_cmd;
   Evas_Object  *entry_pid_cmd_args;
   Evas_Object  *entry_pid_user;
   Evas_Object  *entry_pid_pid;
   Evas_Object  *entry_pid_uid;
   Evas_Object  *entry_pid_cpu;
   Evas_Object  *entry_pid_threads;
   Evas_Object  *entry_pid_virt;
   Evas_Object  *entry_pid_rss;
   Evas_Object  *entry_pid_shared;
   Evas_Object  *entry_pid_size;
   Evas_Object  *entry_pid_nice;
   Evas_Object  *entry_pid_pri;
   Evas_Object  *entry_pid_state;
   Evas_Object  *entry_pid_cpu_usage;

   int          poll_delay;
   char        *selected_cmd;
   int          selected_pid;
   int64_t      pid_cpu_time;
   Eina_Bool    info_init;

   Ecore_Timer *timer_pid;
} Ui_Process;

void
ui_process_win_add(int pid, const char *cmd);

#endif
