#include "config.h"
#include "evisum_config.h"
#include <Eina.h>
#include <Ecore_File.h>
#include <Efreet.h>

static const char *
_config_file_path(void)
{
    const char *path = eina_slstr_printf("%s/evisum", efreet_config_home_get());

    if (!ecore_file_exists(path))
      ecore_file_mkpath(path);

    path = eina_slstr_printf("%s/evisum/evisum.cfg", efreet_config_home_get());

    return path;
}

void
config_init(void)
{
   efreet_init();
}

void
config_shutdown(void)
{
   efreet_shutdown();
}

Evisum_Config *
config_load(void)
{
   Eet_File *f;
   int size;
   Evisum_Config *cfg = NULL;

   const char *path = _config_file_path();
   if (!ecore_file_exists(path))
     {
        cfg = calloc(1, sizeof(Evisum_Config));
        cfg->version = CONFIG_VERSION;
        f = eet_open(path, EET_FILE_MODE_WRITE);
        eet_write(f, "Config", cfg, sizeof(Evisum_Config), 0);
        eet_close(f);
     }
   else
     {
        f = eet_open(path, EET_FILE_MODE_READ);
        if (!f) exit(127);
        cfg = eet_read(f, "Config", &size);
        eet_close(f);
     }

   return cfg;
}

Eina_Bool
config_save(Evisum_Config *cfg)
{
   Eet_File *f;
   const char *path = _config_file_path();
   f = eet_open(path, EET_FILE_MODE_WRITE);
   if (!f) exit(127);
   eet_write(f, "Config", cfg, sizeof(Evisum_Config), 0);
   eet_close(f);

   return EINA_TRUE;
}
