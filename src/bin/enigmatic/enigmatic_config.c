#include "Enigmatic.h"
#include "enigmatic_config.h"

#include <Eina.h>
#include <Eet.h>
#include <Ecore_File.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

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
   EET_DATA_DESCRIPTOR_ADD_BASIC(_enigmatic_conf_desc, Enigmatic_Config, "interval", interval, EET_T_INT);
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
  config->interval = INTERVAL_NORMAL;

  return config;
}

static Eina_Bool
_path_join(char *buf, size_t size, const char *dir, const char *name)
{
   int n;

   n = snprintf(buf, size, "%s/%s", dir, name);
   return ((n >= 0) && ((size_t) n < size));
}

static Eina_Bool
_cache_entry_remove(const char *path)
{
   struct stat st;

   if (lstat(path, &st) == -1)
     return (errno == ENOENT);

   if (S_ISDIR(st.st_mode))
     {
        DIR *dir;
        struct dirent *entry;

        dir = opendir(path);
        if (!dir)
          return EINA_FALSE;

        while ((entry = readdir(dir)))
          {
             char child[PATH_MAX];

             if ((!strcmp(entry->d_name, ".")) || (!strcmp(entry->d_name, "..")))
               continue;

             if (!_path_join(child, sizeof(child), path, entry->d_name))
               {
                  closedir(dir);
                  return EINA_FALSE;
               }
             if (!_cache_entry_remove(child))
               {
                  closedir(dir);
                  return EINA_FALSE;
               }
          }

        closedir(dir);
        return !rmdir(path);
     }

   return !unlink(path);
}

static void
_cache_dir_clear(void)
{
   char path[PATH_MAX];
   DIR *dir;
   struct dirent *entry;
   Eina_Bool ok = EINA_TRUE;

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);

   dir = opendir(path);
   if (!dir)
     {
        if (errno == ENOENT)
          ok = EINA_TRUE;
        else
          ok = EINA_FALSE;
        goto done;
     }

   while ((entry = readdir(dir)))
     {
        char child[PATH_MAX];

        if ((!strcmp(entry->d_name, ".")) || (!strcmp(entry->d_name, "..")))
          continue;

        if (!_path_join(child, sizeof(child), path, entry->d_name))
          {
             ok = EINA_FALSE;
             continue;
          }
        if (!_cache_entry_remove(child))
          ok = EINA_FALSE;
     }

   closedir(dir);

done:
   if (ok)
     fprintf(stdout, "Enigmatic cache directory cleared after config version change.\n");
   else
     fprintf(stdout, "Enigmatic cache directory cleanup after config version change was incomplete.\n");
}

Enigmatic_Config *
enigmatic_config_load(void)
{
   Eet_File *f;
   char *path;
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

             if (config->version != ENIGMATIC_CONFIG_VERSION)
               {
                  _cache_dir_clear();
                  free(config);
                  config = _config_defaults();
                  enigmatic_config_save(config);
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
