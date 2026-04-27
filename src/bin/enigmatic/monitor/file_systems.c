#include "system/file_systems.h"
#include "file_systems.h"
#include "uid.h"
#include "enigmatic_log.h"

void
file_system_key(char *buf, size_t len, File_System *fs)
{
   snprintf(buf, len, "%s:%s", fs->path, fs->mount);
}

static void
cb_file_system_free(void *data)
{
   char key[PATH_MAX * 2 + 1 + 1];
   File_System *fs = data;
   file_system_key(key, sizeof(key), fs);

   DEBUG("del %s", key);

   free(fs);
}

static int
cb_file_system_cmp(const void *a, const void *b)
{
   File_System *fs1, *fs2;

   fs1 = (File_System *) a;
   fs2 = (File_System *) b;

   return strcmp(fs1->path, fs2->path);
}

static void
file_systems_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL;
   void *d = NULL;
   File_System *fs;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
       fs = d;
       ordered = eina_list_append(ordered, fs);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_file_system_cmp);

   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = FILE_SYSTEM;
   msg.number = n;
   enigmatic_log_list_write(enigmatic, EVENT_MESSAGE, msg, ordered, sizeof(File_System));
   eina_list_free(ordered);
}

Eina_Bool
enigmatic_monitor_file_systems(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l, *file_systems;
   File_System *fs, *fs2;
   char key[PATH_MAX * 2  + 1 + 1];
   Eina_Bool changed = 0;

   file_systems = file_systems_find();
   if (!*cache_hash)
     {
        *cache_hash = eina_hash_string_superfast_new(cb_file_system_free);
        EINA_LIST_FOREACH(file_systems, l, fs)
          {
             fs2 = malloc(sizeof(File_System));
             if (fs2)
               {
                  memcpy(fs2, fs, sizeof(File_System));
                  file_system_key(key, sizeof(key), fs);
                  DEBUG("fs add: %s", key);

                  fs2->unique_id = unique_id_find(&enigmatic->unique_ids);
                  eina_hash_add(*cache_hash, key, fs2);
              }
          }
     }

   if (enigmatic->broadcast)
     {
        file_systems_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        File_System *fs2 = d;
        Eina_Bool found = 0;
        char key2[4096 * 2 + 1 + 1];
        EINA_LIST_FOREACH(file_systems, l, fs)
          {
             file_system_key(key, sizeof(key), fs);
             file_system_key(key2, sizeof(key2), fs2);

             if (!strcmp(key2, key))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, fs2);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, fs)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = FILE_SYSTEM;
        msg.number = fs->unique_id;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        file_system_key(key, sizeof(key), fs);
        unique_id_release(&enigmatic->unique_ids, fs->unique_id);
        eina_hash_del(*cache_hash, key, NULL);
     }

   EINA_LIST_FREE(file_systems, fs)
     {
        file_system_key(key, sizeof(key), fs);
        fs2 = eina_hash_find(*cache_hash, key);
        if (!fs2)
          {
             fs->unique_id = unique_id_find(&enigmatic->unique_ids);

             Message msg;
             msg.type = MESG_ADD;
             msg.object_type = FILE_SYSTEM;
             msg.number = 1;
             enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, fs, sizeof(File_System));

             DEBUG("fs add: %s", key);

             eina_hash_add(*cache_hash, key, fs);
             continue;
          }

        Message msg;
        msg.type = MESG_MOD;

        if (fs2->usage.total != fs->usage.total)
          {
             msg.object_type = FILE_SYSTEM_TOTAL;
             msg.number = fs2->unique_id;

             enigmatic_log_diff(enigmatic, msg, (fs->usage.total - fs2->usage.total));

             DEBUG("%s :%i", key,  (int) ((fs->usage.total - fs2->usage.total)));
             changed = 1;
          }

        if (fs2->usage.used != fs->usage.used)
          {
             msg.object_type = FILE_SYSTEM_USED;
             msg.number = fs2->unique_id;
             enigmatic_log_diff(enigmatic, msg, (fs->usage.used - fs2->usage.used));

             DEBUG("%s :%i", key, (int) ((fs->usage.used - fs2->usage.used)));
             changed = 1;
          }
        fs2->usage.total = fs->usage.total;
        fs2->usage.used = fs->usage.used;
        free(fs);
     }

   return changed;
}

