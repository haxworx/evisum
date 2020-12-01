#ifndef __FILESYSTEMS_H__
#define __FILESYSTEMS_H__

#include <Eina.h>
#include "disks.h"

typedef struct {
   unsigned long long total;
   unsigned long long used;
} _Usage;

typedef struct _File_System {
   char         *path;
   char         *mount;
   char         *type_name;
   unsigned int  type;
   _Usage        usage;
} File_System;

const char *
file_system_name_by_id(unsigned int id);

unsigned int
file_system_id_by_name(const char *name);

Eina_List *
file_system_info_all_get(void);

File_System *
file_system_info_get(const char *path);

void
file_system_info_free(File_System *fs);

Eina_Bool
file_system_in_use(const char *name);

#endif
