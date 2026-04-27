#include "Enigmatic.h"
#include "enigmatic_config.h"

#include <Eina.h>
#include <Eet.h>
#include <Ecore_File.h>

static Eet_Data_Descriptor *_enigmatic_conf_desc = NULL;

#define CONFIG_KEY "config"

static char *
enigmatic_config_file_path(void)
{
   char path[PATH_MAX];
   char *home;

   home = getenv("HOME");
   EINA_SAFETY_ON_NULL_RETURN_VAL(home, NULL);

   snprintf(path, sizeof(path), "%s/.config/%s", home, PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkpath(path);

   eina_strlcat(path, eina_slstr_printf("/%s.cfg", PACKAGE), sizeof(path));

   return strdup(path);
}

void
enigmatic_config_init(void)
{
   Eet_Data_Descriptor_Class eddc;

   eet_init();

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Enigmatic_Config);
   _enigmatic_conf_desc = eet_data_descriptor_stream_new(&eddc);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_enigmatic_conf_desc, Enigmatic_Config, "version", version, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_enigmatic_conf_desc, Enigmatic_Config, "log.save_history", log.save_history, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_enigmatic_conf_desc, Enigmatic_Config, "log.rotate_every_hour", log.rotate_every_hour, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_enigmatic_conf_desc, Enigmatic_Config, "log.rotate_every_minute", log.rotate_every_minute, EET_T_UCHAR);
}

void
enigmatic_config_shutdown(void)
{
   eet_shutdown();
}

static Enigmatic_Config *
_config_defaults(void)
{
  Enigmatic_Config *config = calloc(1, sizeof(Enigmatic_Config));
  EINA_SAFETY_ON_NULL_RETURN_VAL(config, NULL);

  config->version = ENIGMATIC_CONFIG_VERSION;
  config->log.save_history = 1;
  config->log.rotate_every_minute = 0;
  config->log.rotate_every_hour = 1;

  return config;
}

Enigmatic_Config *
enigmatic_config_load(void)
{
   Eet_File *f;
   char *path;
   int maj, min;
   Enigmatic_Config *config = NULL;

   path = enigmatic_config_file_path();
   if (path)
     {
        if (!ecore_file_exists(path))
          {
             config = _config_defaults();
             enigmatic_config_save(config);
          }
        else
          {
             f = eet_open(path, EET_FILE_MODE_READ);
             if (!f)
               ERROR("eet_open: (%s)", path);

             config = eet_data_read(f, _enigmatic_conf_desc, CONFIG_KEY);
             if (!config)
               ERROR("eet_data_read: corrupt config?");

             eet_close(f);

             maj = config->version >> 16 & 0xffff;
             min = config->version & 0xffff;
             if ((maj) && (maj != ENIGMATIC_CONFIG_VERSION_MAJOR))
               ERROR("config version mismatch.");

             if (min != ENIGMATIC_CONFIG_VERSION_MINOR)
               {
                  fprintf(stderr, "reinitialising configuration\n");
                  free(config);
                  config = _config_defaults();
               }
          }
        free(path);
     }

   return config;
}

Eina_Bool
enigmatic_config_save(Enigmatic_Config *config)
{
   Eet_File *f;
   char *path;

   path = enigmatic_config_file_path();
   if (path)
     {
        f = eet_open(path, EET_FILE_MODE_WRITE);
        if (!f)
          ERROR("eet_open: (%s)", path);

        int n = eet_data_write(f, _enigmatic_conf_desc, CONFIG_KEY, config, EINA_TRUE);
        if (!n)
          ERROR("eet_data_write()");
        eet_close(f);
        free(path);
        return 1;
     }

   return 0;
}
