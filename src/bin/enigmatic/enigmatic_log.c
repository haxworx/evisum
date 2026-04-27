#include "config.h"
#include "Enigmatic.h"
#include "Events.h"
#include "enigmatic_log.h"
#include "enigmatic_util.h"
#include "lz4.h"
#include "lz4frame.h"

#include <Ecore_File.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/file.h>
#include <sys/mman.h>

void
enigmatic_log_header(Enigmatic *enigmatic, Event event, Message mesg)
{
   Header hdr;
   Interval interval;
   uint32_t time = enigmatic->poll_time;

   interval = enigmatic->interval;
   hdr.event = event;
   hdr.time = time;
   enigmatic_log_write(enigmatic, (char *) &hdr, sizeof(Header));

   if (event == EVENT_BROADCAST)
     {
        enigmatic_log_write(enigmatic, (char *) &interval, sizeof(Interval));
        static uint32_t specialfriend[4] = { HEADER_MAGIC, HEADER_MAGIC, HEADER_MAGIC, HEADER_MAGIC };
        enigmatic_log_write(enigmatic, (char *) &specialfriend, sizeof(specialfriend));
     }
   switch (mesg.type)
     {
        case MESG_ERROR:
        case MESG_REFRESH:
        case MESG_ADD:
        case MESG_MOD:
        case MESG_DEL:
          enigmatic_log_write(enigmatic, (char *) &mesg, sizeof(Message));
          break;
        default:
          break;
     }
}

static char *
lockfile_path(void)
{
   char path[PATH_MAX];

   snprintf(path, sizeof(path), "%s/%s", enigmatic_cache_dir_get(), PACKAGE);
   if (!ecore_file_exists(path))
     ecore_file_mkdir(path);

   snprintf(path, sizeof(path), "%s/%s/%s", enigmatic_cache_dir_get(), PACKAGE, LCK_FILE_NAME);

   return strdup(path);
}

int
enigmatic_log_lock(void)
{
   int lock_fd;
   char *path = lockfile_path();

   lock_fd = open(path, O_CREAT | O_RDONLY, 0666);
   if (lock_fd == -1)
     ERROR("open %s (%s)\n", path, strerror(errno));

   if (flock(lock_fd, LOCK_EX | LOCK_NB) == -1)
     {
        if (errno != EWOULDBLOCK)
          {
             ERROR("flock  %s\n", strerror(errno));
          }
        else
          {
             fprintf(stdout, "Program already running!\n");
             exit(0);
          }
     }
   free(path);

   return lock_fd;
}

void
enigmatic_log_unlock(int lock_fd)
{
   char *path = lockfile_path();

   flock(lock_fd, LOCK_UN);
   unlink(path);
   close(lock_fd);
   free(path);
}

void
enigmatic_log_obj_write(Enigmatic *enigmatic, Event event, Message mesg, void *obj, size_t size)
{
   char *buf;
   Header *hdr;
   ssize_t len;

   len = sizeof(Header) + sizeof(mesg) + size;

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);

   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = event;

   memcpy((char *) hdr + sizeof(Header), &mesg, sizeof(Message));
   memcpy((char *) hdr + sizeof(Header) + sizeof(Message), obj, size);

   enigmatic_log_write(enigmatic, buf, len);

   free(buf);
}

void
enigmatic_log_list_write(Enigmatic *enigmatic, Event event, Message mesg, Eina_List *list, size_t size)
{
   Eina_List *l;
   char *buf;
   int n;
   ssize_t len;
   Header *hdr;

   n = eina_list_count(list);
   if (!n) return;

   len = sizeof(Header) + sizeof(Message) + (n * size);

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);

   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = event;

   void *o;
   char *addr = (char *) hdr + sizeof(Header);
   memcpy(addr, &mesg, sizeof(Message));
   addr += sizeof(Message);

   EINA_LIST_FOREACH(list, l, o)
     {
        memcpy(addr, o, size);
        addr += size;
     }

   enigmatic_log_write(enigmatic, buf, len);

   free(buf);
}

void
enigmatic_log_write(Enigmatic *enigmatic, const char *buf, size_t len)
{
   Buffer *buffer;
   Log *file = enigmatic->log.file;

   if (!file->buf.data)
     {
        buffer = &file->buf;
        buffer->data = malloc(len);
        EINA_SAFETY_ON_NULL_RETURN(buffer->data);
        buffer->length = len;
        memcpy(buffer->data, buf, len);
     }
   else
     {
        buffer = &file->buf;
        void *tmp = realloc(buffer->data, buffer->length + len);
        EINA_SAFETY_ON_NULL_RETURN(tmp);
        buffer->data = tmp;
        memcpy(&buffer->data[buffer->length], buf, len);
        buffer->length += len;
     }
}

void
enigmatic_log_crush(Enigmatic *enigmatic)
{
   Buffer *buffer;
   size_t sz, nw;
   Log *file;
   LZ4F_preferences_t prefs;

   file = enigmatic->log.file;
   buffer = &file->buf;

   memset(&prefs, 0, sizeof(LZ4F_preferences_t));
   prefs.frameInfo.blockSizeID = LZ4F_max256KB;
   prefs.frameInfo.blockMode = LZ4F_blockLinked;
   prefs.frameInfo.contentChecksumFlag = LZ4F_noContentChecksum;
   prefs.frameInfo.frameType =  LZ4F_frame;
   prefs.frameInfo.contentSize = 0;
   prefs.frameInfo.dictID = 0;
   prefs.frameInfo.blockChecksumFlag = LZ4F_noBlockChecksum;
   prefs.compressionLevel = 0;
   prefs.autoFlush = 0;
   prefs.favorDecSpeed = 0;

   size_t outlen = LZ4F_compressFrameBound(buffer->length, &prefs);
   char *out = malloc(outlen);
   EINA_SAFETY_ON_NULL_RETURN(out);

   sz = LZ4F_compressFrame(out, outlen, buffer->data, buffer->length, &prefs);
   if ((nw = write(file->fd, out, sz)) == 0 || nw == -1 || nw != sz)
     ERROR("write () %s", strerror(errno));
   free(out);

   free(buffer->data);
   buffer->data = NULL;
   buffer->length = 0;
}

void
enigmatic_log_flush(Enigmatic *enigmatic)
{
   size_t nw;
   Buffer *buffer;
   Log *file = enigmatic->log.file;

   buffer = &file->buf;
   if (!buffer->length) return;

   if ((nw = write(file->fd, buffer->data, buffer->length)) == 0 || nw == -1 || nw != buffer->length)
     ERROR("write() %s", strerror(errno));

   free(buffer->data);
   buffer->data = NULL;
   buffer->length = 0;
}

Log *
enigmatic_log_open(Enigmatic *enigmatic)
{
   Log *file;
   int fd, flags;
   struct tm *tm_now;
   time_t t = time(NULL);

   tm_now = localtime(&t);

   enigmatic->log.path = enigmatic_log_path();

   enigmatic->log.file = file = calloc(1, sizeof(Log));
   if (!file) return NULL;

   flags = O_CREAT | O_WRONLY | O_TRUNC;
   fd = open(enigmatic->log.path, flags, 0600);
   if (fd == -1)
     {
        free(file);
        return NULL;
     }

   file->fd = fd;
   file->flags = flags;

   enigmatic->log.file = file;
   enigmatic->log.hour = tm_now->tm_hour;
   enigmatic->log.min = tm_now->tm_min;

   return file;
}

void
enigmatic_log_close(Enigmatic *enigmatic)
{
   Log *file = enigmatic->log.file;

   ENIGMATIC_LOG_HEADER(enigmatic, EVENT_LAST_RECORD);
   enigmatic_log_crush(enigmatic);

   if (file->fd != -1)
     {
        close(file->fd);
        file->fd = -1;
     }
   if (file->buf.data)
     free(file->buf.data);

   free(enigmatic->log.path);
   free(file);
   file = NULL;
}

#define BLOCK_SIZE 16384

Eina_Bool
enigmatic_log_compress(const char *path, Eina_Bool staggered)
{
   FILE *f;
   int fd;
   char *out;
   int length;
   struct stat st;
   uint32_t size;
   uint8_t *map;
   char path2[PATH_MAX];
   Eina_Bool ret = 0;

   fd = open(path, O_RDONLY);
   if (fd == -1) return 0;

   if (fstat(fd, &st) == -1) ERROR("fstat() %s\n", strerror(errno));

   size = st.st_size;

   map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
   if (map == MAP_FAILED)
     ERROR("mmap()");

   length = LZ4_compressBound(size);
   out = malloc(length);
   EINA_SAFETY_ON_NULL_RETURN_VAL(out, 0);

   snprintf(path2, sizeof(path2), "%s.lz4.size", path);
   f = fopen(path2, "w");
   EINA_SAFETY_ON_NULL_RETURN_VAL(f, 0);

   int newlength = 0;

   for (int off = 0; off < st.st_size; off += BLOCK_SIZE)
     {
        if ((off + BLOCK_SIZE) > st.st_size)
          size = st.st_size - off;
        else
          size = BLOCK_SIZE;

        int sz = LZ4_compress_default((char *) map + off, out + newlength, size, length);
        fprintf(f, "%i-%i,", size, sz);
        newlength += sz;
        if (staggered) usleep(50000);
     }
   fclose(f);

   snprintf(path2, sizeof(path2), "%s.lz4", path);
   f = fopen(path2, "wb");
   if (f)
     {
        int n = fwrite(out, 1, newlength, f);
        if (n == newlength)
          ret = 1;
        fclose(f);
     }

   free(out);
   munmap(map, st.st_size);
   close(fd);

   return ret;
}

static char *
file_contents(const char *path)
{
   FILE *f;
   char *buf, *tmp;
   size_t n = 1, len = 0;
   const size_t block = 4096;

   f = fopen(path, "r");
   if (!f) return NULL;

   buf = NULL;

   while ((!feof(f)) && (!ferror(f)))
     {
        tmp = realloc(buf, ++n * (sizeof(char) * block) + 1);
        if (!tmp) return NULL;
        buf = tmp;
        len += fread(buf + len, sizeof(char), block, f);
     }

   if (ferror(f))
     {
        free(buf);
        fclose(f);
        return NULL;
     }
   fclose(f);

   if ((buf[len - 1] == '\n') || (buf[len -1] == '\r'))
     buf[len - 1] = 0;
   else
     buf[len] = 0;

   return buf;
}

char *
enigmatic_log_decompress(const char *path, uint32_t *length)
{
   int fd;
   struct stat st;
   char *map;
   char *buf;
   char *out = NULL;
   int len, total = 0, off = 0, newlength = 0;

   *length = 0;

   fd = open(path, O_RDONLY);
   if (fd == -1) return NULL;

   if (fstat(fd, &st) == -1) ERROR("fstat() %s", strerror(errno));

   map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
   if (map == MAP_FAILED)
     ERROR("mmap()");

   char path2[PATH_MAX];
   snprintf(path2, sizeof(path2), "%s.size", path);

   buf = file_contents(path2);
   char *tok = strtok(buf, ",");
   while (tok)
     {
        int sz, csz;
        if ((sscanf(tok, "%i-%i", &sz, &csz)) != 2)
          ERROR("%s (token parsing)", path2);

        total += sz + 1;
        void *t = realloc(out, total);
        EINA_SAFETY_ON_NULL_RETURN_VAL(t, NULL);
        out = t;

        len = LZ4_decompress_safe((const char *) map + off, out + newlength, csz, total);
        if (len < 0)
          ERROR("LZ4_decompress_safe error (%i)", len);
        if (len != sz)
          ERROR("LZ4_decompress_safe mismatched sizes");

        off += csz;
        newlength += sz;
        tok = strtok(NULL, ",");
     }

   free(buf);

   munmap(map, st.st_size);
   close(fd);
   *length = newlength;

   return out;
}

static void *
log_background_compress(void *data, Eina_Thread tid EINA_UNUSED)
{
   char *path = data;

   if (enigmatic_log_compress(path, EINA_TRUE))
     unlink(path);
   free(path);

   return NULL;
}

Eina_Bool
enigmatic_log_rotate(Enigmatic *enigmatic)
{
   Enigmatic_Config *config;
   struct tm *tm_now;
   char *path;
   char saved[PATH_MAX];
   Eina_Bool ok;
   time_t t = time(NULL);

   config = enigmatic->config;

   if (enigmatic->log.hour == -1)
     return 0;

   tm_now = localtime(&t);

   if ((config->log.rotate_every_hour) && (enigmatic->log.hour == tm_now->tm_hour))
     return 0;

   if ((config->log.rotate_every_minute) && (enigmatic->log.hour == tm_now->tm_hour) && (enigmatic->log.min == tm_now->tm_min))
     return 0;

   ENIGMATIC_LOG_HEADER(enigmatic, EVENT_EOF);
   enigmatic_log_close(enigmatic);

   if (!config->log.save_history)
     {
        enigmatic_log_open(enigmatic);
        return 1;
     }

   if (config->log.rotate_every_hour)
     snprintf(saved, sizeof(saved), "%s/%s/%02i", enigmatic_cache_dir_get(), PACKAGE, enigmatic->log.hour);
   else if (config->log.rotate_every_minute)
     snprintf(saved, sizeof(saved), "%s/%s/%02i-%02i", enigmatic_cache_dir_get(), PACKAGE, enigmatic->log.hour, enigmatic->log.min);

   path = enigmatic_log_path();
   ecore_file_cp(path, saved);
   free(path);

   // Join our previous background thread (if existing).
   if (enigmatic->log.rotate_thread)
     {
        eina_thread_join(*enigmatic->log.rotate_thread);
        free(enigmatic->log.rotate_thread);
     }
   enigmatic->log.rotate_thread = calloc(1, sizeof(Eina_Thread));
   EINA_SAFETY_ON_NULL_RETURN_VAL(enigmatic->log.rotate_thread, 0);

   enigmatic_log_open(enigmatic);

   ok = eina_thread_create(enigmatic->log.rotate_thread, EINA_THREAD_BACKGROUND, -1, log_background_compress, strdup(saved));
   if (!ok)
     ERROR("eina_thread_create: log_background_compress");

   return 1;
}

void
enigmatic_log_diff(Enigmatic *enigmatic, Message msg, int64_t value)
{
   Change change;

   if ((value >= -128) && (value <= 127))
     {
        change = CHANGE_I8;
        int8_t diff = (int8_t) value & 0xff;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &diff, sizeof(int8_t));
     }
   else if ((value >= -32768) && (value <= 32767))
     {
        change = CHANGE_I16;
        int16_t diff = (int16_t) value & 0xffff;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &diff, sizeof(int16_t));
     }
   else if ((value >= -2147483648) && (value <= 2147483647))
     {
        change = CHANGE_I32;
        int32_t diff = (int32_t) value & 0xffffffff;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &diff, sizeof(int32_t));
     }
   else
     {
        change = CHANGE_I64;
        int64_t diff = value;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
        enigmatic_log_write(enigmatic, (char *) &change, sizeof(Change));
        enigmatic_log_write(enigmatic, (char *) &diff, sizeof(int64_t));
     }
}
