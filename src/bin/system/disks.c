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
        *end = '\0';

        if (!strcmp(path, start))
          {
             start = end + 1;
             if (!start) continue;
             end = strchr(start, ' ');
             if (!end) continue;
             *end = '\0';
             fclose(f);
             return strdup(start);
          }
     }

   fclose(f);
#endif
   return NULL;
}

static int
_cmp_cb(const void *p1, const void *p2)
{
   const char *s1, *s2;

   s1 = p1; s2 = p2;

   return strcmp(s1, s2);
}

Eina_Bool
disk_zfs_mounted_get(void)
{
   Eina_List *disks;
   char *path;
   Eina_Bool zfs_mounted = EINA_FALSE;

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        Filesystem_Info *fs = filesystem_info_get(path);
        if (fs)
          {
             if (fs->type == filesystem_id_by_name("ZFS"))
               zfs_mounted = EINA_TRUE;

             filesystem_info_free(fs);
          }
        free(path);
     }
   if (disks)
     eina_list_free(disks);

   return zfs_mounted;
}

Eina_List *
disks_get(void)
{
#if defined(__FreeBSD__) || defined(__DragonFly__)
   struct statfs *mounts;
   char *drives, *dev, *end;
   Eina_List *list = NULL;
   int count;
   size_t len;

   if ((sysctlbyname("kern.disks", NULL, &len, NULL, 0)) == -1)
     return NULL;

   drives = malloc(len + 1);

   if ((sysctlbyname("kern.disks", drives, &len, NULL, 0)) == -1)
     {
        free(drives);
        return NULL;
     }

   dev = drives;
   while (dev)
     {
        end = strchr(dev, ' ');
        if (!end)
          break;
        *end = '\0';

        list = eina_list_append(list, strdup(eina_slstr_printf("/dev/%s", dev)));

        dev = end + 1;
     }

   free(drives);

   count = getmntinfo(&mounts, MNT_WAIT);
   for (int i = 0; i < count; i++)
     {
        list = eina_list_append(list, strdup(mounts[i].f_mntfromname));
     }
#elif defined(__OpenBSD__) || defined(__NetBSD__)
   static const int mib[] = { CTL_HW, HW_DISKNAMES };
   static const unsigned int miblen = 2;
   struct statfs *mounts;
   char *drives, *dev, *end;
   Eina_List *list = NULL;
   size_t len;
   int count;

   if ((sysctl(mib, miblen, NULL, &len, NULL, 0)) == -1)
     return NULL;

   drives = malloc(len + 1);

   if ((sysctl(mib, miblen, drives, &len, NULL, 0)) == -1)
     {
        free(drives);
        return NULL;
     }

   dev = drives;
   while (dev)
     {
        end = strchr(dev, ':');
        if (!end) break;

        *end = '\0';

        if (dev[0] == ',')
          dev++;

        list = eina_list_append(list, strdup(eina_slstr_printf("/dev/%s", dev)));

        end++;
        dev = strchr(end, ',');
        if (!dev) break;
     }

   free(drives);

   count = getmntinfo(&mounts, MNT_WAIT);
   for (int i = 0; i < count; i++)
     {
        list = eina_list_append(list, strdup(mounts[i].f_mntfromname));
     }
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
             list = eina_list_append(list, strdup(eina_slstr_printf("/dev/%s", name)));
          }
        free(name);
     }

   if (devs)
     eina_list_free(devs);
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
#endif
   list = eina_list_sort(list, eina_list_count(list), _cmp_cb);

   return list;
}

