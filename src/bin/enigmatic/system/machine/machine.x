
#include <stdarg.h>

char *
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

#if defined(__FreeBSD__) || defined(__DragonFly__)
static long int
_sysctlfromname(const char *name, void *mib, int depth, size_t *len)
{
   long int result;

   if (sysctlnametomib(name, mib, len) < 0)
     return -1;

   *len = sizeof(result);
   if (sysctl(mib, depth, &result, len, NULL, 0) < 0)
     return -1;

   return result;
}

#endif

