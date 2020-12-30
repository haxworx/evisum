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
   pid_t                program_pid;
   Ecore_Event_Handler *handler_sig;

   struct
   {
      Evas_Object     *win;
      int              width;
      int              height;

      int        poll_delay;
      int        sort_type;
      Eina_Bool  sort_reverse;
      Eina_Bool  show_self;
      Eina_Bool  show_kthreads;
      Eina_Bool  show_user;
   } proc;

   Evas_Object     *win_about;
   Evas_Object     *menu;
   Evas_Object     *menu_parent;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
   } cpu;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
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
      Evas_Object  *win;
      int           width;
      int           height;
   } sensors;
} Ui;

Ui *
evisum_ui_init(void);

void
evisum_ui_shutdown(Ui *ui);

void
evisum_ui_main_menu_create(Ui *ui, Evas_Object *parent, Evas_Object *obj);

void
evisum_ui_activate(Ui *ui, Evisum_Action action, int pid);

const char *
evisum_ui_icon_cache_find(Ui *ui, const char *cmd);

void
evisum_ui_config_load(Ui *ui);

void
evisum_ui_config_save(Ui *ui);

#endif
