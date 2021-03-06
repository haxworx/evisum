/*
 * Copyright (c) 2018 Alastair Roy Poole <netstar@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

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

   buf[len] = 0;

   return buf;
}

void
strimmer(char *s)
{
   char *cp = s;

   while (*cp)
     {
        if ((*cp == '\r') || (*cp == '\n'))
          {
             *cp = '\0';
             return;
          }
        cp++;
     }
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

