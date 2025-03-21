#include "config.h"
#include "evisum_config.h"
#include <Eina.h>
#include <Ecore_File.h>
#include <Efreet.h>

Evisum_Config *_evisum_config;

static Eet_Data_Descriptor *_evisum_conf_descriptor = NULL;

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
   Eet_Data_Descriptor_Class eddc;
   efreet_init();

   EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Evisum_Config);
   _evisum_conf_descriptor = eet_data_descriptor_stream_new(&eddc);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "version", version, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "effects", effects, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "backgrounds", backgrounds, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.width", proc.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.height", proc.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.x", proc.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.y", proc.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.restart", proc.restart, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_kthreads", proc.show_kthreads, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_user", proc.show_user, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.sort_type", proc.sort_type, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.sort_reverse", proc.sort_reverse, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.poll_delay", proc.poll_delay, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.fields", proc.fields, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_statusbar", proc.show_statusbar, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.transparent", proc.transparent, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.alpha", proc.alpha, EET_T_UCHAR);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.width", cpu.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.height", cpu.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.x", cpu.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.y", cpu.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.restart", cpu.restart, EET_T_UCHAR);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.visual", cpu.visual, EET_T_STRING);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.width", mem.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.height", mem.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.x", mem.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.y", mem.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.restart", mem.restart, EET_T_UCHAR);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.width", disk.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.height", disk.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.x", disk.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.y", disk.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.restart", disk.restart, EET_T_UCHAR);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.width", sensors.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.height", sensors.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.x", sensors.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.y", sensors.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.restart", sensors.restart, EET_T_UCHAR);

   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.width", network.width, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.height", network.height, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.x", network.x, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.y", network.y, EET_T_INT);
   EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.restart", network.restart, EET_T_UCHAR);
}

void
config_shutdown(void)
{
   efreet_shutdown();
}

Evisum_Config *
config(void)
{
   return _evisum_config;
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
   cfg->proc.poll_delay = 3;
   cfg->proc.show_kthreads = 0;
   cfg->proc.show_statusbar = 0;
   cfg->proc.transparent = 0;
   cfg->proc.fields = 0xffffe24f;
   cfg->proc.alpha = 100;

   cfg->cpu.visual = strdup("default");

   return cfg;
}

Evisum_Config *
config_load(void)
{
   Eet_File *f;
   Evisum_Config *cfg = NULL;

   const char *path = _config_file_path();
   if (!ecore_file_exists(path))
     {
        cfg = _config_init();

        f = eet_open(path, EET_FILE_MODE_WRITE);
        if (!f) _config_fail("create");
        eet_data_write(f, _evisum_conf_descriptor, "Config", cfg, EINA_TRUE);
        eet_close(f);
     }
   else
     {
        f = eet_open(path, EET_FILE_MODE_READ);
        if (!f) _config_fail("read");
        cfg = eet_data_read(f, _evisum_conf_descriptor, "Config");
        if (!cfg)
          {
             fprintf(stderr, "FATAL: Corrupt config?\n");
             exit(1);
          }

        if (cfg->version < CONFIG_VERSION)
          {
             free(cfg);
             fprintf(stderr, "INFO: Reinitialising configuration\n");

             cfg = _config_init();
          }
        _config_check(cfg);

        eet_close(f);
     }
   _evisum_config = cfg;

   return cfg;
}

Eina_Bool
config_save(Evisum_Config *cfg)
{
   Eet_File *f;
   const char *path = _config_file_path();
   f = eet_open(path, EET_FILE_MODE_WRITE);
   if (!f) exit(127);
   eet_data_write(f, _evisum_conf_descriptor, "Config", cfg, EINA_TRUE);
   eet_close(f);

   return 1;
}
