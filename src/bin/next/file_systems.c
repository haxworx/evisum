#include "file_systems.h"

#include <stdio.h>
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

Eina_List *
file_systems_find(void)
{
   Eina_List *list = NULL;
# if defined(__linux__)
   FILE *f;
   char *dev, *mount, *type_name, *cp, *end;
   struct statfs stats;
   char buf[4096];

   f = fopen("/proc/mounts", "r");
   if (!f) return NULL;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        mount = strchr(buf, ' ') + 1;
        if (!mount) continue;
        dev = strndup(buf, mount - buf - 1);
        cp = strchr(mount, ' ');
        if (!cp) continue;
        end = cp;
        *end = '\0';
        cp++;
        end = strchr(cp, ' ');
        if (!end) continue;
        type_name = strndup(cp, end - cp);

        if ((statfs(mount, &stats) < 0) ||
            (stats.f_blocks == 0 && stats.f_bfree == 0))
          {
             free(dev);
             free(type_name);
             continue;
          }

        File_System *fs = calloc(1, sizeof(File_System));
        if (fs)
          {
             snprintf(fs->mount, sizeof(fs->mount), "%s", mount);
             snprintf(fs->path, sizeof(fs->path), "%s", dev);
             snprintf(fs->type_name, sizeof(fs->type_name), "%s", type_name);
             fs->usage.total = stats.f_bsize * stats.f_blocks;
             fs->usage.used  = fs->usage.total - (stats.f_bsize * stats.f_bfree);
             free(dev); free(type_name);
             list = eina_list_append(list, fs);
          }
     }
   fclose(f);
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

        free(fs);
     }

   return mounted;
}

