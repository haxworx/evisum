#ifndef __DISKS_H__
#define __DISKS_H__

#include "filesystems.h"

char *
disk_mount_point_get(const char *path);

Eina_List *
disks_get(void);

#endif
