#ifndef __DISKS_H__
#define __DISKS_H__

#include <Eina.h>
#include <Ecore.h>

Eina_Bool
 disk_usage_get(const char *mountpoint, unsigned long *total, unsigned long *used);

char *
 disk_mount_point_get(const char *path);

Eina_List *
 disks_get(void);

#endif
