#include "filesystems.h"
#include "disks.h"

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

typedef struct {
   unsigned int  magic;
   const char   *name;
} _magic;

/* These values are reliable though some variants may be ambiguous. We use
 * these superblock constants to detect unusual file systems such as ZFS.
 */
static _magic _magique[] = {
   { .magic = 0xdf5,      .name = "adfs" },
   { .magic = 0xadff,     .name = "affs" },
   { .magic = 0x5346414f, .name = "afs" },
   { .magic = 0x0187,     .name = "autofs" },
   { .magic = 0x73757245, .name = "coda" },
   { .magic = 0x28cd3d45, .name = "cramfs" },
   { .magic = 0x453dcd28, .name = "cramfs" },
   { .magic = 0x64626720, .name = "debugfs" },
   { .magic = 0x73636673, .name = "securityfs" },
   { .magic = 0xf97cff8c, .name = "selinux" },
   { .magic = 0x43415d53, .name = "smac" },
   { .magic = 0x858458f6, .name = "ramfs" },
   { .magic = 0x01021994, .name = "tmpfs" },
   { .magic = 0x958458f6, .name = "hugetlbfs" },
   { .magic = 0x73717368, .name = "squashfs" },
   { .magic = 0xf15f,     .name = "ecryptfs" },
   { .magic = 0x414a53,   .name = "efs" },
   { .magic = 0xe0f5e1e2, .name = "erofs" },
   { .magic = 0xef53,     .name = "ext2/3/4" },
   { .magic = 0xef53,     .name = "ext2/3/4" },
   { .magic = 0xef53,     .name = "ext2/3/4" },
   { .magic = 0xabba1974, .name = "xenfs" },
   { .magic = 0x9123683e, .name = "btrfs" },
   { .magic = 0x3434,     .name = "nilfs" },
   { .magic = 0xf2f52010, .name = "f2fs" },
   { .magic = 0xf995e849, .name = "hpfs" },
   { .magic = 0x9660,     .name = "isofs" },
   { .magic = 0x72b6,     .name = "jffs2" },
   { .magic = 0x58465342, .name = "xfs" },
   { .magic = 0x6165676c, .name = "pstorefs" },
   { .magic = 0xde5e81e4, .name = "efivarfs" },
   { .magic = 0x00c0ffee, .name = "hostfs" },
   { .magic = 0x794c7630, .name = "overlayfs" },
   { .magic = 0x137f,     .name = "minix" },
   { .magic = 0x138f,     .name = "minix" },
   { .magic = 0x2468,     .name = "minix2" },
   { .magic = 0x2478,     .name = "minix2" },
   { .magic = 0x4d5a,     .name = "minix3" },
   { .magic = 0x4d44,     .name = "msdos" },
   { .magic = 0x564c,     .name = "ncp" },
   { .magic = 0x6969,     .name = "nfs" },
   { .magic = 0x7461636f, .name = "ocfs2" },
   { .magic = 0x9fa1,     .name = "openprom" },
   { .magic = 0x002f,     .name = "qnx4" },
   { .magic = 0x68191122, .name = "qnx6" },
   { .magic = 0x6b414653, .name = "afs fs" },
   { .magic = 0x52654973, .name = "reiserfs" },
   { .magic = 0x517b,     .name = "smb" },
   { .magic = 0x27e0eb,   .name = "cgroup" },
   { .magic = 0x63677270, .name = "cgroup2" },
   { .magic = 0x7655821,  .name = "rdtgroup" },
   { .magic = 0x74726163, .name = "tracefs" },
   { .magic = 0x01021997, .name = "v9fs" },
   { .magic = 0x62646576, .name = "bdevfs" },
   { .magic = 0x64646178, .name = "daxfs" },
   { .magic = 0x42494e4d, .name = "binfmtfs" },
   { .magic = 0x1cd1,     .name = "devpts" },
   { .magic = 0x6c6f6f70, .name = "binderfs" },
   { .magic = 0x50495045, .name = "pipefs" },
   { .magic = 0x62656572, .name = "sysfs" },
   { .magic = 0x6e736673, .name = "nsfs" },
   { .magic = 0xcafe4a11, .name = "bpf fs" },
   { .magic = 0x5a3c69f0, .name = "aafs" },
   { .magic = 0x5a4f4653, .name = "zonefs" },
   { .magic = 0x15013346, .name = "udf" },
   { .magic = 0x2fc12fc1, .name = "zfs" },
   { .magic = 0x482b,     .name = "hfsplus" },
};

unsigned int
file_system_id_by_name(const char *name)
{
   size_t n = sizeof(_magique) / sizeof(_magic);
   for (int i = 0; i < n; i++)
     {
        if (!strcasecmp(name, _magique[i].name))
          return _magique[i].magic;
     }
   return 0;
}

const char *
file_system_name_by_id(unsigned int id)
{
   size_t n = sizeof(_magique) / sizeof(_magic);
   for (int i = 0; i < n; i++)
     {
        if (_magique[i].magic == id)
          return _magique[i].name;
     }
   return NULL;
}

#if defined(__linux__)
static char *
file_system_type_name(const char *mountpoint)
{
   FILE *f;
   char *mount, *res, *type, *end;
   char buf[4096];

   res = NULL;

   f = fopen("/proc/mounts", "r");
   if (!f) return NULL;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        mount = strchr(buf, ' ') + 1;
        if (!mount) continue;
        type = strchr(mount, ' ');
        if (!type) continue;
        end = type;
        *end = '\0';
        if (strcmp(mount, mountpoint))
          continue;
        type++;
        end = strchr(type, ' ');
        if (!end) continue;
        res = strndup(type, end - type);
        break;
     }
   fclose(f);
   return res;
}

#endif

Eina_List *
file_system_info_all_get(void)
{
# if defined(__linux__)
   return NULL;
# else
   struct statfs *mounts;
   int i, count;
   Eina_List *list = NULL;

   count = getmntinfo(&mounts, MNT_WAIT);
   for (i = 0; i < count; i++)
     {
        File_System *fs = calloc(1, sizeof(File_System));
        fs->mount = strdup(mounts[i].f_mntonname);
        fs->path  = strdup(mounts[i].f_mntfromname);
#if defined(__OpenBSD__)
#else
        fs->type  = mounts[i].f_type;
#endif
        fs->type_name = strdup(mounts[i].f_fstypename);
        fs->usage.total = mounts[i].f_bsize * mounts[i].f_blocks;
        fs->usage.used  = fs->usage.total - (mounts[i].f_bsize * mounts[i].f_bfree);

        list = eina_list_append(list, fs);
     }
# endif
   return list;
}

File_System *
file_system_info_get(const char *path)
{
   File_System *fs;
   char *mountpoint;
   struct statfs stats;

   mountpoint = disk_mount_point_get(path);
   if (!mountpoint) return NULL;

   if (statfs(mountpoint, &stats) < 0)
     return NULL;

   fs = calloc(1, sizeof(File_System));
   fs->mount = mountpoint;
   fs->path  = strdup(path);

#if defined(__OpenBSD__)
#else
   fs->type  = stats.f_type;
#endif

#if defined(__linux__)
   fs->type_name = file_system_type_name(fs->mount);
#else
   fs->type_name = strdup(stats.f_fstypename);
#endif

   fs->usage.total = stats.f_bsize * stats.f_blocks;
   fs->usage.used  = fs->usage.total - (stats.f_bsize * stats.f_bfree);

   return fs;
}

void
file_system_info_free(File_System *fs)
{
   free(fs->mount);
   free(fs->path);
   if (fs->type_name)
     free(fs->type_name);
   free(fs);
}

Eina_Bool
file_system_in_use(const char *name)
{
   Eina_List *disks;
   char *path;
   Eina_Bool mounted = EINA_FALSE;
   if (!name) return EINA_FALSE;

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        File_System *fs = file_system_info_get(path);
        if (fs)
          {
             if (fs->type == file_system_id_by_name(name))
               mounted = EINA_TRUE;

             file_system_info_free(fs);
          }
        free(path);
     }

   return mounted;
}

