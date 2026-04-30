#ifndef ENIGMATIC_UTIL_H
#define ENIGMATIC_UTIL_H

#include <Eina.h>
#include <stdint.h>
#include "enigmatic_visibility.h"

typedef struct _Enigmatic Enigmatic;

ENIGMATIC_API Eina_Bool
enigmatic_pidfile_create(Enigmatic *enigmatic);

ENIGMATIC_API char *
enigmatic_pidfile_path(void);

ENIGMATIC_API const char *
enigmatic_cache_dir_get(void);

ENIGMATIC_API void
enigmatic_pidfile_delete(Enigmatic *enigmatic);

ENIGMATIC_API uint32_t
enigmatic_pidfile_pid_get(const char *path);

ENIGMATIC_API Eina_Bool
enigmatic_terminate(void);

ENIGMATIC_API Eina_Bool
enigmatic_launch(void);

ENIGMATIC_API Eina_Bool
enigmatic_running(void);

ENIGMATIC_API char *
enigmatic_log_path(void);

ENIGMATIC_API char *
enigmatic_log_directory(void);

ENIGMATIC_API void
enigmatic_about(void);

#endif
