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
} _Magic;

static _Magic _magique[] = {
   { .magic = 0xdf5,      .name = "ADFS" },
   { .magic = 0xadff,     .name = "AFFS" },
   { .magic = 0x5346414F, .name = "AFS" },
   { .magic = 0x0187,     .name = "AUTOFS" },
   { .magic = 0x73757245, .name = "CODA" },
   { .magic = 0x28cd3d45, .name = "CRAMFS" },
   { .magic = 0x453dcd28, .name = "CRAMFS" },
   { .magic = 0x64626720, .name = "DEBUGFS" },
   { .magic = 0x73636673, .name = "SECURITYFS" },
   { .magic = 0xf97cff8c, .name = "SELINUX" },
   { .magic = 0x43415d53, .name = "SMAC" },
   { .magic = 0x858458f6, .name = "RAMFS" },
   { .magic = 0x01021994, .name = "TMPFS" },
   { .magic = 0x958458f6, .name = "HUGETLBFS" },
   { .magic = 0x73717368, .name = "SQUASHFS" },
   { .magic = 0xf15f,     .name = "ECRYPTFS" },
   { .magic = 0x414A53,   .name = "EFS" },
   { .magic = 0xE0F5E1E2, .name = "EROFS" },
   { .magic = 0xEF53,     .name = "EXT2" },
   { .magic = 0xEF53,     .name = "EXT3" },
   { .magic = 0xEF53,     .name = "EXT4" },
   { .magic = 0xabba1974, .name = "XENFS" },
   { .magic = 0x9123683E, .name = "BTRFS" },
   { .magic = 0x3434,     .name = "NILFS" },
   { .magic = 0xF2F52010, .name = "F2FS" },
   { .magic = 0xf995e849, .name = "HPFS" },
   { .magic = 0x9660,     .name = "ISOFS" },
   { .magic = 0x72b6,     .name = "JFFS2" },
   { .magic = 0x58465342, .name = "XFS" },
   { .magic = 0x6165676C, .name = "PSTOREFS" },
   { .magic = 0xde5e81e4, .name = "EFIVARFS" },
   { .magic = 0x00c0ffee, .name = "HOSTFS" },
   { .magic = 0x794c7630, .name = "OVERLAYFS" },
   { .magic = 0x137F,     .name = "MINIX" },
   { .magic = 0x138F,     .name = "MINIX" },
   { .magic = 0x2468,     .name = "MINIX2" },
   { .magic = 0x2478,     .name = "MINIX2" },
   { .magic = 0x4d5a,     .name = "MINIX3" },
   { .magic = 0x4d44,     .name = "MSDOS" },
   { .magic = 0x564c,     .name = "NCP" },
   { .magic = 0x6969,     .name = "NFS" },
   { .magic = 0x7461636f, .name = "OCFS2" },
   { .magic = 0x9fa1,     .name = "OPENPROM" },
   { .magic = 0x002f,     .name = "QNX4" },
   { .magic = 0x68191122, .name = "QNX6" },
   { .magic = 0x6B414653, .name = "AFS FS" },
   { .magic = 0x52654973, .name = "REISERFS" },
   { .magic = 0x517B,     .name = "SMB" },
   { .magic = 0x27e0eb,   .name = "CGROUP" },
   { .magic = 0x63677270, .name = "CGROUP2" },
   { .magic = 0x7655821,  .name = "RDTGROUP" },
   { .magic = 0x74726163, .name = "TRACEFS" },
   { .magic = 0x01021997, .name = "V9FS" },
   { .magic = 0x62646576, .name = "BDEVFS" },
   { .magic = 0x64646178, .name = "DAXFS" },
   { .magic = 0x42494e4d, .name = "BINFMTFS" },
   { .magic = 0x1cd1,     .name = "DEVPTS" },
   { .magic = 0x6c6f6f70, .name = "BINDERFS" },
   { .magic = 0x50495045, .name = "PIPEFS" },
   { .magic = 0x62656572, .name = "SYSFS" },
   { .magic = 0x6e736673, .name = "NSFS" },
   { .magic = 0xcafe4a11, .name = "BPF FS" },
   { .magic = 0x5a3c69f0, .name = "AAFS" },
   { .magic = 0x5a4f4653, .name = "ZONEFS" },
   { .magic = 0x15013346, .name = "UDF" },
   { .magic = 0x2Fc12Fc1, .name = "ZFS" },
   { .magic = 0x482B,     .name = "HFS+" },
};

unsigned int
filesystem_id_by_name(const char *name)
{
   for (int i = 0; i < sizeof(_magique); i++)
     {
        if (!strcasecmp(name, _magique[i].name))
          return _magique[i].magic;
     }

   return 0;
}

const char *
filesystem_name_by_id(unsigned int id)
{
   for (int i = 0; i < sizeof(_magique); i++)
     {
        if (_magique[i].magic == id)
          return _magique[i].name;
     }
   return NULL;
}

Filesystem_Info *
filesystem_info_get(const char *path)
{
   Filesystem_Info *fs;
   const char *mountpoint;
   struct statfs stats;

   mountpoint = disk_mount_point_get(path);
   if (!mountpoint) return NULL;

   if (statfs(mountpoint, &stats) < 0)
     return NULL;

   fs = calloc(1, sizeof(Filesystem_Info));
   fs->mount = strdup(mountpoint);
   fs->path  = strdup(path);
   fs->type  = stats.f_type;
   fs->usage.total = stats.f_bsize * stats.f_blocks;
   fs->usage.used  = fs->usage.total - (stats.f_bsize * stats.f_bfree);

   return fs;
}

void
filesystem_info_free(Filesystem_Info *fs)
{
   free(fs->mount);
   free(fs->path);
   free(fs);
}

Eina_Bool
filesystem_in_use(const char *name)
{
   Eina_List *disks;
   char *path;
   Eina_Bool fs_mounted = EINA_FALSE;
   if (!name) return EINA_FALSE;

   disks = disks_get();
   EINA_LIST_FREE(disks, path)
     {
        Filesystem_Info *fs = filesystem_info_get(path);
        if (fs)
          {
             if (fs->type == filesystem_id_by_name(name))
               fs_mounted = EINA_TRUE;

             filesystem_info_free(fs);
          }
        free(path);
     }

   return fs_mounted;
}

