/*
 * Author: Codex (OpenAI), controlled and directed by Alastair Poole.
 */

#include "evisum_ui_weather_season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
_timezone_name_get(char *name, size_t size)
{
   const char *env = getenv("TZ");
   FILE *file;
   ssize_t length;

   if (env)
     {
        snprintf(name, size, "%s", env[0] == ':' ? env + 1 : env);
        return;
     }
   length = readlink("/etc/localtime", name, size - 1);
   if (length >= 0)
     {
        name[length] = '\0';
        return;
     }
   /* Also support systems that copy the zone file rather than symlinking it. */
   file = fopen("/etc/timezone", "r");
   if (!file) file = fopen("/var/db/zoneinfo", "r");
   name[0] = '\0';
   if (file)
     {
        if (fgets(name, size, file)) name[strcspn(name, "\r\n")] = '\0';
        fclose(file);
     }
}

static bool
_southern_get(const char *name)
{
   const char *zone = strstr(name, "zoneinfo/");
   FILE *file;
   char line[1024];
   bool south = false;

   if (zone) name = zone + strlen("zoneinfo/");
   if (!strncmp(name, "posix/", 6)) name += 6;
   if (!strncmp(name, "right/", 6)) name += 6;
   /* Include common aliases and POSIX TZ forms used in Australia and NZ. */
   if (!strncmp(name, "Australia/", 10) ||
       !strcmp(name, "Pacific/Auckland") || !strcmp(name, "Pacific/Chatham") ||
       !strcmp(name, "NZ") || !strcmp(name, "NZ-CHAT") ||
       !strncmp(name, "NZST", 4) || !strncmp(name, "NZDT", 4) ||
       !strncmp(name, "AEST", 4) || !strncmp(name, "ACST", 4) ||
       !strncmp(name, "AWST", 4) || !strncmp(name, "AEDT", 4)) return true;

   /* IANA coordinates handle other southern regions without guessing by offset. */
   file = fopen("/usr/share/zoneinfo/zone.tab", "r");
   if (!file) file = fopen("/usr/share/zoneinfo/zone1970.tab", "r");
   if (!file) return false;
   while (fgets(line, sizeof(line), file))
     {
        char *coordinates, *id, *end;
        if (line[0] == '#') continue;
        coordinates = strchr(line, '\t');
        if (!coordinates) continue;
        coordinates++;
        id = strchr(coordinates, '\t');
        if (!id) continue;
        id++;
        end = id + strcspn(id, "\t\r\n");
        *end = '\0';
        if (strcmp(id, name)) continue;
        south = coordinates[0] == '-';
        break;
     }
   fclose(file);
   /* Unlocated zones (UTC or numeric offsets) use northern seasons. */
   return south;
}

bool
evisum_ui_weather_winter_get(time_t when)
{
   struct tm local;
   char zone[512];

   tzset();
   if (!localtime_r(&when, &local)) return false;
   _timezone_name_get(zone, sizeof(zone));
   if (_southern_get(zone)) return local.tm_mon >= 5 && local.tm_mon <= 7;
   return local.tm_mon == 11 || local.tm_mon <= 1;
}
