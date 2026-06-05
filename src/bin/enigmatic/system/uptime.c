#include "uptime.h"

#include <stdio.h>

#if defined(__FreeBSD__) || defined(__DragonFly__) || (defined(__APPLE__) && defined(__MACH__))
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/time.h>
#endif

time_t
enigmatic_system_uptime_get(void)
{
#if defined(__linux__)
   FILE *f;
   double uptime = 0.0;

   f = fopen("/proc/uptime", "r");
   if (!f) return (time_t) -1;

   if (fscanf(f, "%lf", &uptime) != 1)
     {
        fclose(f);
        return (time_t) -1;
     }
   fclose(f);

   if (uptime < 0.0) return (time_t) -1;

   return (time_t) uptime;
#elif defined(__FreeBSD__) || defined(__DragonFly__) || (defined(__APPLE__) && defined(__MACH__))
   struct timeval boot_time;
   size_t len = sizeof(boot_time);
   time_t now;
   int mib[2] = { CTL_KERN, KERN_BOOTTIME };

   if (sysctl(mib, 2, &boot_time, &len, NULL, 0) == -1)
     return (time_t) -1;

   now = time(NULL);
   if ((now == (time_t) -1) || (boot_time.tv_sec > now))
     return (time_t) -1;

   return now - boot_time.tv_sec;
#else
   return (time_t) -1;
#endif
}
