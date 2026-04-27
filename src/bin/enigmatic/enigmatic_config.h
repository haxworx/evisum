#ifndef ENIGMATIC_CONFIG_H
#define ENIGMATIC_CONFIG_H

#define ENIGMATIC_CONFIG_VERSION_MAJOR 0x0001
#define ENIGMATIC_CONFIG_VERSION_MINOR 0x0003

#define ENIGMATIC_CONFIG_VERSION ((ENIGMATIC_CONFIG_VERSION_MAJOR << 16) | ENIGMATIC_CONFIG_VERSION_MINOR)

typedef struct _Enigmatic_Config
{
   int version;
   struct
   {
      Eina_Bool rotate_every_minute;
      Eina_Bool rotate_every_hour;
      Eina_Bool save_history;
   } log;
} Enigmatic_Config;

void
enigmatic_config_init(void);

void
enigmatic_config_shutdown(void);

Enigmatic_Config *
enigmatic_config_load(void);

Eina_Bool
enigmatic_config_save(Enigmatic_Config *config);

#endif
