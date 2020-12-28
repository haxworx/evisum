#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "ui/ui.h"

#define CONFIG_VERSION 0x000b

typedef struct _Evisum_Config
{
   int       version;

   Eina_Bool effects;
   Eina_Bool backgrounds;

   struct
   {
      Evas_Object *win;
      int          width;
      int          height;

      Eina_Bool    show_kthreads;
      Eina_Bool    show_user;
      int          sort_type;
      Eina_Bool    sort_reverse;
      int          poll_delay;
   } proc;

   struct
   {
      int width;
      int height;
   } cpu;

   struct
   {
      int width;
      int height;
   } mem;

   struct
   {
      int width;
      int height;
   } disk;

   struct
   {
      int width;
      int height;
   } sensors;

} Evisum_Config;

void config_init(void);
void config_shutdown(void);
Evisum_Config *config_load(void);
Eina_Bool config_save(Evisum_Config *);

#endif
