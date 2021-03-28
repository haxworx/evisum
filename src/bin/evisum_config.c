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

static void
_config_fail(const char *msg)
{
   fprintf(stderr, "ERR: config %s.\n", msg);
   exit(1);
}

static void
_config_check(Evisum_Config *cfg)
{
   if (cfg->version > CONFIG_VERSION)
     _config_fail("version");

   if (cfg->proc.poll_delay <= 0)
     _config_fail("poll");
}

static Evisum_Config *
_config_init()
{
   Evisum_Config *cfg = calloc(1, sizeof(Evisum_Config));
   cfg->version = CONFIG_VERSION;
   cfg->proc.poll_delay = 1;
   cfg->proc.show_kthreads = 0;
   cfg->proc.transparent = 0;
   cfg->proc.fields = 0xffffffff;
   cfg->proc.alpha = 100;

   return cfg;
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
        cfg = _config_init();

        f = eet_open(path, EET_FILE_MODE_WRITE);
        if (!f) _config_fail("create");
        eet_write(f, "Config", cfg, sizeof(Evisum_Config), 0);
        eet_close(f);
     }
   else
     {
        f = eet_open(path, EET_FILE_MODE_READ);
        if (!f) _config_fail("read");
        cfg = eet_read(f, "Config", &size);

        if (cfg->version < CONFIG_VERSION)
          {
             free(cfg);
             fprintf(stderr, "INFO: Reinitialising configuration\n");

             cfg = _config_init();
          }
        _config_check(cfg);

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

   return 1;
}
