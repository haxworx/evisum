#ifndef __UI_CPU_H__
#define __UI_CPU_H__

#include "evisum_ui.h"
#include "../system/machine.h"

typedef struct _Color_Point {
   unsigned int val;
   unsigned int color;
} Color_Point;

extern unsigned int cpu_colormap[256];
extern unsigned int freq_colormap[256];
extern unsigned int temp_colormap[256];

#define AVAL(x) (((x) >> 24) & 0xff)
#define RVAL(x) (((x) >> 16) & 0xff)
#define GVAL(x) (((x) >>  8) & 0xff)
#define BVAL(x) (((x)      ) & 0xff)
#define ARGB(a, r, g, b) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))

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
ui_cpu_win_add(Evisum_Ui *ui);

void
ui_cpu_win_restart(Evisum_Ui *ui);

Eina_List *
ui_cpu_visuals_get(void);

Cpu_Visual *
ui_cpu_visual_by_name(const char *name);

#endif
