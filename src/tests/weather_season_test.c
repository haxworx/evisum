/*
 * Author: Codex (OpenAI), controlled and directed by Alastair Poole.
 */

#include "../bin/ui/evisum_ui_weather_season.h"

#include <stdio.h>
#include <stdlib.h>

static void
_check(const char *zone, time_t when, int expected)
{
   setenv("TZ", zone, 1);
   if (evisum_ui_weather_winter_get(when) != expected)
     {
        fprintf(stderr, "Wrong winter season for %s at %lld\n", zone, (long long) when);
        exit(1);
     }
}

static void
_months_check(const char *zone, int south)
{
   int month;
   setenv("TZ", zone, 1);
   tzset();
   for (month = 0; month < 12; month++)
     {
        struct tm local = { .tm_year = 126, .tm_mon = month, .tm_mday = 15,
                            .tm_hour = 12, .tm_isdst = -1 };
        int winter = south ? month >= 5 && month <= 7 : month == 11 || month <= 1;
        _check(zone, mktime(&local), winter);
     }
}

static time_t
_utc(int month, int day, int hour)
{
   struct tm date = { .tm_year = 126, .tm_mon = month - 1, .tm_mday = day,
                     .tm_hour = hour, .tm_isdst = -1 };
   setenv("TZ", "UTC", 1);
   tzset();
   return mktime(&date);
}

int
main(void)
{
   static const char *north[] = {
      "Europe/London", "Europe/Paris", "Europe/Berlin", "America/New_York",
      "America/Chicago", "America/Los_Angeles", "America/Anchorage", "US/Eastern", "UTC"
   };
   static const char *south[] = {
      "Australia/Sydney", "Australia/Melbourne", "Australia/Perth", "Australia/Adelaide",
      "Australia/Brisbane", "Australia/Darwin", "Australia/Hobart", "Australia/NSW",
      "Pacific/Auckland", "Pacific/Chatham", "NZ", "NZ-CHAT", ":Australia/Sydney",
      "AEST-10", "NZST-12NZDT,M9.5.0,M4.1.0", "Africa/Johannesburg", "America/Santiago"
   };
   unsigned int i;
   time_t boundary;

   for (i = 0; i < sizeof(north) / sizeof(north[0]); i++) _months_check(north[i], 0);
   for (i = 0; i < sizeof(south) / sizeof(south[0]); i++) _months_check(south[i], 1);
   boundary = _utc(12, 1, 0);
   _check("Europe/London", boundary, 1);
   _check("America/Los_Angeles", boundary, 0); /* Still November locally. */
   boundary = _utc(5, 31, 12);
   _check("Pacific/Auckland", boundary, 1); /* Already June locally. */
   _check("Australia/Sydney", boundary, 0);
   boundary = _utc(8, 31, 12);
   _check("Pacific/Auckland", boundary, 0); /* Already September locally. */
   _check("Australia/Sydney", boundary, 1);
   puts("Season checks passed for 26 timezones and local month boundaries.");
   return 0;
}
