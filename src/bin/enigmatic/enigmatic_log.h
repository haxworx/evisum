#ifndef LOG_FILE_H
#define LOG_FILE_H

#include "Enigmatic.h"

#if defined(__linux__)
# include <linux/limits.h>
#endif

#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

#define ENIGMATIC_LOG_HEADER(s, e) enigmatic_log_header(s, e, (Message) { 0 });

Log *
enigmatic_log_open(Enigmatic *enigmatic);

void
enigmatic_log_close(Enigmatic *enigmatic);

void
enigmatic_log_header(Enigmatic *enigmatic, Event event, Message mesg);

void
enigmatic_log_write(Enigmatic *enigmatic, const char *buf, size_t len);

void
enigmatic_log_list_write(Enigmatic *enigmatic, Event event, Message mesg, Eina_List *list, size_t size);

void
enigmatic_log_obj_write(Enigmatic *enigmatic, Event event, Message mesg, void *obj, size_t size);

void
enigmatic_log_diff(Enigmatic *enigmatic, Message msg, int64_t change);

void
enigmatic_log_flush(Enigmatic *enigmatic);

void
enigmatic_log_crush(Enigmatic *enigmatic);

Eina_Bool
enigmatic_log_rotate(Enigmatic *enigmatic);

int
enigmatic_log_lock(void);

void
enigmatic_log_unlock(int lock_fd);

Eina_Bool
enigmatic_log_compress(const char *path, Eina_Bool staggered);

char *
enigmatic_log_decompress(const char *path, uint32_t *length);

#endif
