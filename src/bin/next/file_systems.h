#ifndef __FILESYSTEMS_H__
#define __FILESYSTEMS_H__

#include <Eina.h>

typedef struct {
   uint64_t total;
   uint64_t used;
} _Usage;

typedef struct _File_System {
   char         path[PATH_MAX];
   char         mount[PATH_MAX];
   char         type_name[64];
   unsigned int type;
   _Usage       usage;

   int          unique_id;
} File_System;

Eina_List *
file_systems_find(void);

void
file_system_info_free(File_System *fs);

Eina_Bool
file_system_in_use(const char *name);

#endif
