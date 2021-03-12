#ifndef __EVISUM_UI_H__
#define __EVISUM_UI_H__

#include <Elementary.h>
#include "gettext.h"

#include "evisum_actions.h"

#include "system/machine.h"
#include "system/process.h"
#include "ui/ui_util.h"
#include "ui/ui_cache.h"

#define _(STR) gettext(STR)

#define EVISUM_WIN_WIDTH  800
#define EVISUM_WIN_HEIGHT 600

typedef struct _Evisum_Ui
{
   pid_t                program_pid;
   Ecore_Event_Handler *handler_sig;

   Eina_Bool            effects;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;

      Eina_Bool     has_kthreads;

      int           poll_delay;
      int           sort_type;
      unsigned int  fields;
      Eina_Bool     sort_reverse;
      Eina_Bool     show_self;
      Eina_Bool     show_kthreads;
      Eina_Bool     show_user;

      unsigned char alpha;
      Eina_Bool     transparent;
      Eina_Bool     show_scroller;
   } proc;

   Evas_Object     *win_about;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;
   } cpu;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;
      Eina_Bool     zfs_mounted;
   } mem;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;
   } disk;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;
   } sensors;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;
   } network;
} Evisum_Ui;

Evisum_Ui *
evisum_ui_init(void);

void
evisum_ui_shutdown(Evisum_Ui *ui);

Evas_Object *
evisum_ui_main_menu_create(Evisum_Ui *ui, Evas_Object *parent, Evas_Object *obj);

void
evisum_ui_activate(Evisum_Ui *ui, Evisum_Action action, int pid);

const char *
evisum_ui_icon_cache_find(Evisum_Ui *ui, const char *cmd);

void
evisum_ui_config_load(Evisum_Ui *ui);

void
evisum_ui_config_save(Evisum_Ui *ui);

void
evisum_ui_restart(Evisum_Ui *ui);

Eina_Bool
evisum_ui_effects_enabled_get(Evisum_Ui *ui);

void
evisum_ui_effects_enabled_set(Evisum_Ui *ui, Eina_Bool enabled);

#endif
