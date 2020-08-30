#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include "ui/ui.h"

#define CONFIG_VERSION 0x0003

typedef struct _Evisum_Config
{
   int       version;
   int       sort_type;
   Eina_Bool sort_reverse;
   int       width;
   int       height;
   Eina_Bool effects;
} Evisum_Config;

void config_init(void);
void config_shutdown(void);
Evisum_Config *config_load(void);
Eina_Bool config_save(Evisum_Config *);

#endif
