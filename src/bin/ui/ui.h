#ifndef __UI_H__
#define __UI_H__

#include <Elementary.h>
#include "gettext.h"
#include "system/machine.h"
#include "system/process.h"
#include "../evisum_config.h"
#include "ui/ui_util.h"
#include "ui/ui_cache.h"

#define _(STR) gettext(STR)

#define EVISUM_WIN_WIDTH  600
#define EVISUM_WIN_HEIGHT 600

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
   Evas_Object     *bg;
   Evas_Object     *main_menu;
   Evas_Object     *menu;
   Evas_Object     *scroller;
   Evas_Object     *content;

   Evas_Object     *win_about;

   Evas_Object     *progress_cpu;
   Evas_Object     *progress_mem;

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

   Ecore_Thread    *thread_system;
   Ecore_Thread    *thread_process;

   struct
   {
      Evas_Object  *win;
      Ecore_Thread *thread;
   } cpu;

   struct
   {
      Evas_Object *win;
      Ecore_Timer *timer;
      Eina_Bool     zfs_mounted;
   } mem;

   struct
   {
      Evas_Object *win;
      Evas_Object *box;
      Ecore_Timer *timer;
   } disk;

   struct
   {
      Evas_Object *win;
      Evas_Object *box;
      Evas_Object *timer;
      uint64_t     incoming_max;
      uint64_t     outgoing_max;
   } misc;

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
   Eina_Bool       show_kthreads;
   Eina_Bool       shutdown_now;

   Ecore_Animator *animator;

   uint8_t         cpu_usage;
} Ui;

Ui *
evisum_ui_add(Evas_Object *win);

void
evisum_ui_del(Ui *ui);

void
evisum_ui_shutdown(Ui *ui);

#endif
