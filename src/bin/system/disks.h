#ifndef __DISKS_H__
#define __DISKS_H__

#include "filesystems.h"
#include <Eina.h>
#include <Ecore.h>

Eina_Bool
disk_zfs_mounted_get(void);

char *
disk_mount_point_get(const char *path);

Eina_List *
disks_get(void);

#endif
