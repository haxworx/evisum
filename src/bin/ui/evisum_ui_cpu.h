#ifndef __EVISUM_UI_CPU_H__
#define __EVISUM_UI_CPU_H__

#include "evisum_ui.h"
#include "evisum_ui_colors.h"
#include "../system/machine.h"

typedef struct {
   short id;
   short percent;
   unsigned int freq;
   unsigned int temp;
} Core;

typedef struct {
   Evisum_Ui      *ui;
   Ecore_Thread   *thread;

   Evas_Object    *menu;
   Evas_Object    *win;
   Elm_Layout     *btn_menu;
   Eina_Bool       btn_visible;

   // Callback to free user data.
   void (*ext_free_cb)(void *);
   void               *ext;

} Ui_Cpu_Data;

typedef struct {
   const char *name;
   Ui_Cpu_Data *(*func)(Evas_Object *parent);
} Cpu_Visual;

void
evisum_ui_cpu_win_add(Evisum_Ui *ui);

void
evisum_ui_cpu_win_restart(Evisum_Ui *ui);

Eina_List *
evisum_ui_cpu_visuals_get(void);

Cpu_Visual *
evisum_ui_cpu_visual_by_name(const char *name);

#endif
