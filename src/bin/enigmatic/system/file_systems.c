#include "file_systems.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
 #include <sys/vfs.h>
 #include <sys/stat.h>
 #include <sys/sysmacros.h>
#endif

#if defined(__APPLE__) && defined(__MACH__)
# define __MacOS__
#endif

#if defined (__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/param.h>
# include <sys/ucred.h>
# include <sys/mount.h>
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <sys/param.h>
# include <sys/sysctl.h>
# include <sys/mount.h>
#endif

#if defined(__linux__)
typedef struct
{
   uint64_t read;
   uint64_t write;
} Linux_Disk_Usage;

static void
_linux_proc_unescape(char *s)
{
   char *src, *dst;

   src = dst = s;
   while (*src)
     {
        if ((src[0] == '\\') &&
            (src[1] >= '0') && (src[1] <= '7') &&
            (src[2] >= '0') && (src[2] <= '7') &&
            (src[3] >= '0') && (src[3] <= '7'))
          {
             *dst++ = (char) (((src[1] - '0') << 6) |
                              ((src[2] - '0') << 3) |
                              (src[3] - '0'));
             src += 4;
          }
        else
          {
             *dst++ = *src++;
          }
     }
   *dst = '\0';
}

static Eina_Hash *
_linux_disk_usage_hash_get(void)
{
   Eina_Hash *hash;
   FILE *f;
   char buf[4096];

   f = fopen("/proc/diskstats", "r");
   if (!f) return NULL;

   hash = eina_hash_string_superfast_new(free);
   if (!hash)
     {
        fclose(f);
        return NULL;
     }

   while (fgets(buf, sizeof(buf), f))
     {
        unsigned int major_id, minor_id;
        unsigned long long reads, reads_merged, sectors_read, ms_read;
        unsigned long long writes, writes_merged, sectors_written;
        char name[128];

        if (sscanf(buf, " %u %u %127s %llu %llu %llu %llu %llu %llu %llu",
                   &major_id, &minor_id, name, &reads, &reads_merged,
                   &sectors_read, &ms_read, &writes, &writes_merged,
                   &sectors_written) == 10)
          {
             Linux_Disk_Usage *usage;
             char key[64];

             usage = calloc(1, sizeof(Linux_Disk_Usage));
             if (!usage) continue;

             usage->read = sectors_read * 512ULL;
             usage->write = sectors_written * 512ULL;
             snprintf(key, sizeof(key), "%u:%u", major_id, minor_id);
             eina_hash_set(hash, key, usage);
          }
     }

   fclose(f);

   return hash;
}

static Eina_Bool
_linux_file_system_disk_usage_key_set(File_System *fs, Eina_Hash *disk_usage, unsigned int major_id,
                                      unsigned int minor_id)
{
   Linux_Disk_Usage *usage;
   char key[64];

   if (!disk_usage) return EINA_FALSE;

   snprintf(key, sizeof(key), "%u:%u", major_id, minor_id);
   usage = eina_hash_find(disk_usage, key);
   if (!usage) return EINA_FALSE;

   fs->usage.read = usage->read;
   fs->usage.write = usage->write;
   return EINA_TRUE;
}

static void
_linux_file_system_disk_usage_set(File_System *fs, Eina_Hash *disk_usage, unsigned int major_id, unsigned int minor_id)
{
   struct stat st;

   if (_linux_file_system_disk_usage_key_set(fs, disk_usage, major_id, minor_id))
     return;

   if ((stat(fs->path, &st) < 0) || (!S_ISBLK(st.st_mode)))
     return;

   _linux_file_system_disk_usage_key_set(fs, disk_usage, major(st.st_rdev), minor(st.st_rdev));
}
#endif

Eina_List *
file_systems_find(void)
{
   Eina_List *list = NULL;
# if defined(__linux__)
   FILE *f;
   char *separator, *post_separator;
   struct statfs stats;
   char buf[4096];
   Eina_Hash *disk_usage;

   f = fopen("/proc/self/mountinfo", "r");
   if (!f) return NULL;

   disk_usage = _linux_disk_usage_hash_get();

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        unsigned int major_id, minor_id;
        char mount[PATH_MAX], path[PATH_MAX], type_name[64], options[4096];

        separator = strstr(buf, " - ");
        if (!separator) continue;

        *separator = '\0';
        post_separator = separator + 3;

        if (sscanf(buf, "%*u %*u %u:%u %*4095s %4095s %4095s",
                   &major_id, &minor_id, mount, options) != 4)
          continue;

        if (sscanf(post_separator, "%63s %4095s", type_name, path) != 2)
          continue;

        _linux_proc_unescape(mount);
        _linux_proc_unescape(path);
        _linux_proc_unescape(type_name);

        Eina_Bool ok = EINA_TRUE;
        if ((strcmp(type_name, "fuseblk")) && (strcmp(type_name, "exfat"))) ok = EINA_FALSE;

        if (((!ok) && strstr(options, "nodev")) ||
            (statfs(mount, &stats) < 0) ||
            (stats.f_blocks == 0 && stats.f_bfree == 0))
          continue;

        File_System *fs = calloc(1, sizeof(File_System));
        if (fs)
          {
             snprintf(fs->mount, sizeof(fs->mount), "%s", mount);
             snprintf(fs->path, sizeof(fs->path), "%s", path);
             snprintf(fs->type_name, sizeof(fs->type_name), "%s", type_name);
             fs->usage.total = stats.f_bsize * stats.f_blocks;
             fs->usage.used  = fs->usage.total - (stats.f_bsize * stats.f_bfree);
             _linux_file_system_disk_usage_set(fs, disk_usage, major_id, minor_id);
             list = eina_list_append(list, fs);
          }
     }
   fclose(f);
   if (disk_usage)
     eina_hash_free(disk_usage);
# else
   struct statfs *mounts;
   int i, count;

   count = getmntinfo(&mounts, MNT_WAIT);
   for (i = 0; i < count; i++)
     {
        File_System *fs = calloc(1, sizeof(File_System));
        if (fs)
          {
             snprintf(fs->mount, sizeof(fs->mount), "%s", mounts[i].f_mntonname);
             snprintf(fs->path, sizeof(fs->path), "%s", mounts[i].f_mntfromname);
#if defined(__OpenBSD__)
#else
             fs->type  = mounts[i].f_type;
#endif
             snprintf(fs->type_name, sizeof(fs->type_name), "%s", mounts[i].f_fstypename);

             fs->usage.total = mounts[i].f_bsize * mounts[i].f_blocks;
             fs->usage.used  = fs->usage.total - (mounts[i].f_bsize * mounts[i].f_bfree);

             list = eina_list_append(list, fs);
          }
     }
# endif
   return list;
}

void
file_system_info_free(File_System *fs)
{
   free(fs);
}

Eina_Bool
file_system_in_use(const char *name)
{
   Eina_List *list;
   File_System *fs;
   Eina_Bool mounted = 0;

   if (!name) return 0;

   list = file_systems_find();
   EINA_LIST_FREE(list, fs)
     {
        if ((fs->type_name[0]) && (!strcasecmp(fs->type_name, name)))
          mounted = 1;

        file_system_info_free(fs);
     }

   return mounted;
}
