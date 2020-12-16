#ifndef __UI_H__
#define __UI_H__

#include <Elementary.h>
#include "gettext.h"

#include "evisum_actions.h"

#include "system/machine.h"
#include "system/process.h"
#include "ui/ui_util.h"
#include "ui/ui_cache.h"

#define _(STR) gettext(STR)

#define EVISUM_WIN_WIDTH  600
#define EVISUM_WIN_HEIGHT 600

typedef struct Ui
{
   pid_t            program_pid;

   struct
   {
      Evas_Object     *win;
      int              width;
      int              height;
      Ecore_Animator  *animator;
   } processes;

   Evas_Object     *win;
   Evas_Object     *win_about;
   Evas_Object     *main_menu;

   uint8_t          cpu_usage;

   struct
   {
      Evas_Object  *win;
      Ecore_Thread *thread;
      int           width;
      int           height;
   } cpu;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      Ecore_Thread *thread;
      Eina_Bool     zfs_mounted;
   } mem;

   struct
   {
      Evas_Object *win;
      int          width;
      int          height;
   } disk;

   struct
   {
      Evas_Object *win;
      int          width;
      int          height;
      Evas_Object *box;
      Evas_Object *timer;
   } sensors;

   struct
   {
      Eina_Bool  skip_wait;
      Eina_Bool  ready;
      Eina_Bool  shutdown_now;
   } state;

   struct
   {
      int        poll_delay;
      int        sort_type;
      Eina_Bool  sort_reverse;
      Eina_Bool  show_self;
      Eina_Bool  show_kthreads;
      Eina_Bool  show_user;
      Eina_Bool  show_desktop;
   } settings;
} Ui;

Ui *
evisum_ui_init(void);

void
evisum_ui_shutdown(Ui *ui);

void
evisum_ui_main_menu_create(Ui *ui, Evas_Object *parent);

void
evisum_ui_activate(Ui *ui, Evisum_Action action, int pid);

const char *
evisum_ui_icon_cache_find(Ui *ui, const char *cmd);

void
evisum_ui_config_load(Ui *ui);

void
evisum_ui_config_save(Ui *ui);

#endif
