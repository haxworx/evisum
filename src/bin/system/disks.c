#define _DEFAULT_SOURCE

#include "disks.h"
#include <Ecore.h>
#include <Ecore_File.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
 #include <sys/vfs.h>
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

char *
disk_mount_point_get(const char *path)
{
#if defined(__MacOS__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
   struct statfs *mounts;
   int i, count;

   count = getmntinfo(&mounts, MNT_WAIT);
   for (i = 0; i < count; i++)
     {
        if (!strcmp(path, mounts[i].f_mntfromname))
          {
             return strdup(mounts[i].f_mntonname);
          }
     }
#elif defined(__linux__)
   char buf[4096];
   char *start, *end;
   FILE *f = fopen("/proc/mounts", "r");

   if (!f)
     return NULL;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        start = &buf[0];
        end = strchr(start, ' ');
        if (!end) continue;
        *end = 0x0;

        if (!strcmp(path, start))
          {
             start = end + 1;
             if (!start) continue;
             end = strchr(start, ' ');
             if (!end) continue;
             *end = 0x0;
             fclose(f);
             return strdup(start);
          }
     }

   fclose(f);

   return NULL;
#else
#endif
   return NULL;
}

File_System *
disk_mount_file_system_get(const char *path)
{
   File_System *fs;
   const char *mountpoint;
   struct statfs stats;

   mountpoint = disk_mount_point_get(path);
   if (!mountpoint) return NULL;

   if (statfs(mountpoint, &stats) < 0)
     return NULL;

   fs = calloc(1, sizeof(File_System));
   fs->mount = strdup(mountpoint);
   fs->path  = strdup(path);
   fs->type  = stats.f_type;
   fs->usage.total = stats.f_bsize * stats.f_blocks;
   fs->usage.used  = fs->usage.total - (stats.f_bsize * stats.f_bfree);

   return fs;
}

void
disk_mount_file_system_free(File_System *fs)
{
   free(fs->mount);
   free(fs->path);
   free(fs);
}

static int
_cmp_cb(const void *p1, const void *p2)
{
   const char *s1, *s2;

   s1 = p1; s2 = p2;

   return strcmp(s1, s2);
}

Eina_List *
disks_get(void)
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
   struct statfs *mounts;
   int i, count;
   char *drives, *dev, *end;
   char buf[4096];
   size_t len;

   if ((sysctlbyname("kern.disks", NULL, &len, NULL, 0)) == -1)
     return NULL;

   drives = malloc(len + 1);

   if ((sysctlbyname("kern.disks", drives, &len, NULL, 0)) == -1)
     {
        free(drives);
        return NULL;
     }

   Eina_List *list = NULL;
   dev = drives;
   while (dev)
     {
        end = strchr(dev, ' ');
        if (!end)
          break;

        *end = '\0';

        snprintf(buf, sizeof(buf), "/dev/%s", dev);

        list = eina_list_append(list, strdup(buf));

        dev = end + 1;
     }

   free(drives);

   count = getmntinfo(&mounts, MNT_WAIT);
   for (i = 0; i < count; i++)
     {
        list = eina_list_append(list, strdup(mounts[i].f_mntfromname));
     }

   list = eina_list_sort(list, eina_list_count(list), _cmp_cb);

   return list;
#elif defined(__OpenBSD__) || defined(__NetBSD__)
   static const int mib[] = { CTL_HW, HW_DISKNAMES };
   static const unsigned int miblen = 2;
   struct statfs *mounts;
   char *drives, *dev, *end;
   char buf[4096];
   size_t len;
   int i, count;

   if ((sysctl(mib, miblen, NULL, &len, NULL, 0)) == -1)
     return NULL;

   drives = malloc(len + 1);

   if ((sysctl(mib, miblen, drives, &len, NULL, 0)) == -1)
     {
        free(drives);
        return NULL;
     }

   Eina_List *list = NULL;

   dev = drives;
   while (dev)
     {
        end = strchr(dev, ':');
        if (!end) break;

        *end = '\0';

        if (dev[0] == ',')
          dev++;

        snprintf(buf, sizeof(buf), "/dev/%s", dev);

        list = eina_list_append(list, strdup(buf));

        end++;
        dev = strchr(end, ',');
        if (!dev) break;
     }

   free(drives);

   count = getmntinfo(&mounts, MNT_WAIT);
   for (i = 0; i < count; i++)
     {
        list = eina_list_append(list, strdup(mounts[i].f_mntfromname));
     }

   list = eina_list_sort(list, eina_list_count(list), _cmp_cb);

   return list;
#elif defined(__MacOS__)
   char *name;
   char buf[4096];
   Eina_List *devs, *list;

   list = NULL;

   devs = ecore_file_ls("/dev");

   EINA_LIST_FREE(devs, name)
     {
        if (!strncmp(name, "disk", 4))
          {
             snprintf(buf, sizeof(buf), "/dev/%s", name);
             list = eina_list_append(list, strdup(buf));
          }
        free(name);
     }

   if (devs)
     eina_list_free(devs);

   list = eina_list_sort(list, eina_list_count(list), _cmp_cb);

   return list;
#elif defined(__linux__)
   char *name;
   Eina_List *devs, *list;
   const char *disk_search = "/dev/disk/by-uuid";

   devs = ecore_file_ls(disk_search);
   if (!devs)
     {
        disk_search = "/dev/disk/by-path";
        devs = ecore_file_ls(disk_search);
     }

   list = NULL;

   EINA_LIST_FREE(devs, name)
     {
        char *real = realpath(eina_slstr_printf("%s/%s", disk_search, name), NULL);
        if (real)
          {
             list = eina_list_append(list, real);
          }
        free(name);
     }

   if (devs)
     eina_list_free(devs);

   devs = ecore_file_ls("/dev/mapper");
   EINA_LIST_FREE(devs, name)
     {
        list = eina_list_append(list, strdup(eina_slstr_printf("/dev/mapper/%s", name)));
        free(name);
     }

   if (devs)
     eina_list_free(devs);

   list = eina_list_sort(list, eina_list_count(list), _cmp_cb);

   return list;
#else

   return NULL;
#endif
}

