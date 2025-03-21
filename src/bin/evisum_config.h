#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "ui/evisum_ui.h"

#define CONFIG_VERSION 0x0014

typedef struct _Evisum_Config
{
   int       version;

   Eina_Bool effects;
   Eina_Bool backgrounds;

   struct
   {
      Evas_Object  *win;
      int           width;
      int           height;
      int           x, y;
      Eina_Bool     restart;

      Eina_Bool     show_kthreads;
      Eina_Bool     show_user;
      unsigned char sort_type;
      Eina_Bool     sort_reverse;
      unsigned char poll_delay;
      unsigned int  fields;

      Eina_Bool     show_statusbar;
      Eina_Bool     transparent;
      unsigned char alpha;
   } proc;

   struct
   {
      int width;
      int height;
      int x, y;
      Eina_Bool    restart;
      char        *visual;
   } cpu;

   struct
   {
      int width;
      int height;
      int x, y;
      Eina_Bool    restart;
   } mem;

   struct
   {
      int width;
      int height;
      int x, y;
      Eina_Bool    restart;
   } disk;

   struct
   {
      int width;
      int height;
      int x, y;
      Eina_Bool    restart;
   } sensors;

   struct
   {
      int width;
      int height;
      int x, y;
      Eina_Bool    restart;
   } network;

} Evisum_Config;

void config_init(void);
void config_shutdown(void);
Evisum_Config *config_load(void);
Eina_Bool config_save(Evisum_Config *);
Evisum_Config *config(void);

#endif
