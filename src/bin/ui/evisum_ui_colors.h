#ifndef __EVISUM_UI_COLORS_H__
#define __EVISUM_UI_COLORS_H__

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

#define COLOR_CPU_NUM 5
static const Color_Point cpu_colormap_in[] = {
   {   0, 0xff202020 },
   {  25, 0xff2030a0 },
   {  50, 0xffa040a0 },
   {  75, 0xffff9040 },
   { 100, 0xffffffff },
   { 256, 0xffffffff }
};
#define COLOR_FREQ_NUM 4
static const Color_Point freq_colormap_in[] = {
   {   0, 0xff202020 },
   {  33, 0xff285020 },
   {  67, 0xff30a060 },
   { 100, 0xffa0ff80 },
   { 256, 0xffa0ff80 }
};

#define COLOR_TEMP_NUM 5
static const Color_Point temp_colormap_in[] = {
   {  0,  0xff57bb8a },
   {  25, 0xffa4c073 },
   {  50, 0xfff5ce62 },
   {  75, 0xffe9a268 },
   { 100, 0xffdd776e },
   { 256, 0xffdd776e }
};

void evisum_ui_colors_init();

#endif
