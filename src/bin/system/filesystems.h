#ifndef __FILESYSTEMS_H__
#define __FILESYSTEMS_H__

#include <Eina.h>

typedef struct {
   unsigned long long total;
   unsigned long long used;
} _Usage;

typedef struct _Filesystem_Info {
   char         *path;
   char         *mount;
   unsigned int  type;
   _Usage        usage;
} Filesystem_Info;

const char *
filesystem_name_by_id(unsigned int id);

unsigned int
filesystem_id_by_name(const char *name);

Filesystem_Info *
filesystem_info_get(const char *path);

void
filesystem_info_free(Filesystem_Info *fs);

Eina_Bool
filesystem_in_use(const char *name);

#endif
