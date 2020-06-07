#ifndef __DISKS_H__
#define __DISKS_H__

#include <Eina.h>
#include <Ecore.h>

typedef struct _Disk_Usage {
   unsigned long total;
   unsigned long used;
} Disk_Usage;

typedef struct _File_System {
   char        *path;
   char        *mount;
   unsigned int type;
   Disk_Usage   usage;
} File_System;

Eina_Bool
disk_zfs_mounted_get(void);

File_System *
disk_mount_file_system_get(const char *path);

void
disk_mount_file_system_free(File_System *fs);

char *
disk_mount_point_get(const char *path);

Eina_List *
disks_get(void);

#endif
