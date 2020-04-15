/*
   Copyright (c) 2017, Al Poole <netstar@gmail.com>
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#if defined(__linux__)
# define BORING_USER 1
#else
# include <sys/sysctl.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <pthread.h>

#if defined(__APPLE__) && defined(__MACH__)
#define __MacOS__
# include <mach/mach.h>
# include <mach/vm_statistics.h>
# include <mach/mach_types.h>
# include <mach/mach_init.h>
# include <mach/mach_host.h>
# include <net/if_mib.h>
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
# include <sys/sched.h>
# include <sys/swap.h>
# include <sys/mount.h>
# include <sys/sensors.h>
# include <net/if_types.h>
# include <ifaddrs.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <net/if_mib.h>
# include <vm/vm_param.h>
# include <dev/acpica/acpiio.h>
#endif

#include "system.h"

#if defined(__OpenBSD__)
# define CPU_STATES      6
#else
# define CPU_STATES      5
#endif

/* Filter requests and results */
#define RESULTS_CPU       0x01
#define RESULTS_MEM       0x02
#define RESULTS_PWR       0x04
#define RESULTS_TMP       0x08
#define RESULTS_AUD       0x10
#define RESULTS_NET       0x20
#define RESULTS_DEFAULT   0x3f
#define RESULTS_MEM_MB    0x40
#define RESULTS_MEM_GB    0x80
#define RESULTS_CPU_CORES 0x100

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
static void
_memsize_bytes_to_kb(unsigned long *bytes)
{
   *bytes = (unsigned int)*bytes >> 10;
}
#endif

#if defined(__linux__)
static char *
file_contents(const char *path)
{
   char *buf;
   char byte[1];
   size_t count, bytes = 0;
   FILE *f;

   f = fopen(path, "r");
   if (!f) return NULL;

   int n = 1024;

   buf = malloc(n * sizeof(byte) + 1);
   if (!buf) return NULL;

   while ((count = (fread(byte, sizeof(byte), 1, f))) > 0)
     {
        bytes += sizeof(byte);
        if (bytes == (n * sizeof(byte)))
          {
             n *= 2;
             char *tmp = realloc(buf, n * sizeof(byte) + 1);
             if (!tmp) return NULL;
             buf = tmp;
          }
        memcpy(&buf[bytes - sizeof(byte)], byte, sizeof(byte));
     }

   if (!feof(f))
     {
        free(buf);
        fclose(f);
        return NULL;
     }
   fclose(f);

   buf[bytes] = 0;

   return buf;
}

#endif

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

static int
cpu_count(void)
{
   static int cores = 0;

   if (cores != 0)
     return cores;

#if defined(__linux__)
   char buf[4096];
   FILE *f;
   int line = 0;

   f = fopen("/proc/stat", "r");
   if (!f) return 0;

   while (fgets(buf, sizeof(buf), f))
     {
        if (line)
          {
             if (!strncmp(buf, "cpu", 3))
               cores++;
             else
               break;
          }
        line++;
     }

   fclose(f);
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__) || defined(__NetBSD__)
   size_t len;
   int mib[2] = { CTL_HW, HW_NCPU };

   len = sizeof(cores);
   if (sysctl(mib, 2, &cores, &len, NULL, 0) < 0)
     return 0;
#endif
   return cores;
}

int
system_cpu_online_count_get(void)
{
#if defined(__OpenBSD__)
   static int cores = 0;

   if (cores != 0) return cores;

   size_t len;
   int mib[2] = { CTL_HW, HW_NCPUONLINE };

   len = sizeof(cores);
   if (sysctl(mib, 2, &cores, &len, NULL, 0) < 0)
     return cpu_count();

   return cores;
#else
   return cpu_count();
#endif
}

static void
_cpu_state_get(cpu_core_t **cores, int ncpu)
{
   int diff_total, diff_idle;
   double ratio, percent;
   unsigned long total, idle, used;
   cpu_core_t *core;
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__) || defined(__NetBSD__)
   size_t size;
   int i, j;
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
   if (!ncpu)
     return;
   size = sizeof(unsigned long) * (CPU_STATES * ncpu);
   unsigned long cpu_times[ncpu][CPU_STATES];

   if (sysctlbyname("kern.cp_times", cpu_times, &size, NULL, 0) < 0)
     return;

   for (i = 0; i < ncpu; i++) {
        core = cores[i];
        unsigned long *cpu = cpu_times[i];

        total = 0;
        for (j = 0; j < CPU_STATES; j++)
          total += cpu[j];

        idle = cpu[4];

        diff_total = total - core->total;
        diff_idle = idle - core->idle;
        if (diff_total == 0) diff_total = 1;

        ratio = diff_total / 100.0;
        used = diff_total - diff_idle;
        percent = used / ratio;

        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;

        core->percent = percent;
        core->total = total;
        core->idle = idle;
     }
#elif defined(__OpenBSD__)
   static struct cpustats cpu_times[CPU_STATES];
   static int cpu_time_mib[] = { CTL_KERN, KERN_CPUSTATS, 0 };

   memset(&cpu_times, 0, CPU_STATES * sizeof(struct cpustats));
   if (!ncpu)
     return;

   for (i = 0; i < ncpu; i++)
     {
        core = cores[i];
        size = sizeof(struct cpustats);
        cpu_time_mib[2] = i;
        if (sysctl(cpu_time_mib, 3, &cpu_times[i], &size, NULL, 0) < 0)
          return;

        total = 0;
        for (j = 0; j < CPU_STATES; j++)
          total += cpu_times[i].cs_time[j];

        idle = cpu_times[i].cs_time[CP_IDLE];

        diff_total = total - core->total;
        if (diff_total == 0) diff_total = 1;

        diff_idle = idle - core->idle;
        ratio = diff_total / 100.0;
        used = diff_total - diff_idle;
        percent = used / ratio;

        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;

        core->percent = percent;
        core->total = total;
        core->idle = idle;
     }
#elif defined(__linux__)
   char *buf, name[128];
   int i;

   buf = file_contents("/proc/stat");
   if (!buf) return;

   for (i = 0; i < ncpu; i++) {
        core = cores[i];
        snprintf(name, sizeof(name), "cpu%d", i);
        char *line = strstr(buf, name);
        if (line)
          {
             line = strchr(line, ' ') + 1;
             unsigned long cpu_times[4] = { 0 };

             if (4 != sscanf(line, "%lu %lu %lu %lu", &cpu_times[0], &cpu_times[1], &cpu_times[2], &cpu_times[3]))
               return;

             total = cpu_times[0] + cpu_times[1] + cpu_times[2] + cpu_times[3];
             idle = cpu_times[3];
             diff_total = total - core->total;
             if (diff_total == 0) diff_total = 1;

             diff_idle = idle - core->idle;
             ratio = diff_total / 100.0;
             used = diff_total - diff_idle;
             percent = used / ratio;

             if (percent > 100) percent = 100;
             else if (percent < 0)
               percent = 0;

             core->percent = percent;
             core->total = total;
             core->idle = idle;
          }
     }
   free(buf);
#elif defined(__MacOS__)
   mach_msg_type_number_t count;
   processor_cpu_load_info_t load;
   mach_port_t mach_port;
   unsigned int cpu_count;
   int i;

   cpu_count = ncpu;

   count = HOST_CPU_LOAD_INFO_COUNT;
   mach_port = mach_host_self();
   if (host_processor_info(mach_port, PROCESSOR_CPU_LOAD_INFO, &cpu_count, (processor_info_array_t *)&load, &count) != KERN_SUCCESS)
     exit(-1);

   for (i = 0; i < ncpu; i++) {
        core = cores[i];

        total = load[i].cpu_ticks[CPU_STATE_USER] + load[i].cpu_ticks[CPU_STATE_SYSTEM] + load[i].cpu_ticks[CPU_STATE_IDLE] + load[i].cpu_ticks[CPU_STATE_NICE];
        idle = load[i].cpu_ticks[CPU_STATE_IDLE];

        diff_total = total - core->total;
        if (diff_total == 0) diff_total = 1;
        diff_idle = idle - core->idle;
        ratio = diff_total / 100.0;
        used = diff_total - diff_idle;
        percent = used / ratio;

        if (percent > 100) percent = 100;
        else if (percent < 0)
          percent = 0;

        core->percent = percent;
        core->total = total;
        core->idle = idle;
     }
#endif
}

static cpu_core_t **
_cpu_cores_state_get(int *ncpu)
{
   cpu_core_t **cores;
   int i;

   *ncpu = cpu_count();

   cores = malloc((*ncpu) * sizeof(cpu_core_t *));

   for (i = 0; i < *ncpu; i++)
     cores[i] = calloc(1, sizeof(cpu_core_t));

   _cpu_state_get(cores, *ncpu);
   usleep(1000000);
   _cpu_state_get(cores, *ncpu);

   return cores;
}

#if defined(__linux__)
static unsigned long
_meminfo_parse_line(const char *line)
{
   char *p, *tok;

   p = strchr(line, ':') + 1;
   while (isspace(*p))
     p++;
   tok = strtok(p, " ");

   return atol(tok);
}

#endif

static void
_memory_usage_get(meminfo_t *memory)
{
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
   size_t len = 0, miblen;
   int i = 0;
#endif
   memset(memory, 0, sizeof(meminfo_t));
#if defined(__linux__)
   FILE *f;
   unsigned long swap_free = 0, tmp_free = 0, tmp_slab = 0;
   char line[256];
   int fields = 0;

   f = fopen("/proc/meminfo", "r");
   if (!f) return;

   while (fgets(line, sizeof(line), f) != NULL)
     {
        if (!strncmp("MemTotal:", line, 9))
          {
             memory->total = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("MemFree:", line, 8))
          {
             tmp_free = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Cached:", line, 7))
          {
             memory->cached = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Slab:", line, 5))
          {
             tmp_slab = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Buffers:", line, 8))
          {
             memory->buffered = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Shmem:", line, 6))
          {
             memory->shared = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("SwapTotal:", line, 10))
          {
             memory->swap_total = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("SwapFree:", line, 9))
          {
             swap_free = _meminfo_parse_line(line);
             fields++;
          }

        if (fields >= 8)
          break;
     }

   memory->cached += tmp_slab;
   memory->used = memory->total - tmp_free - memory->cached - memory->buffered;
   memory->swap_used = memory->swap_total - swap_free;

   fclose(f);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int total_pages = 0, free_pages = 0, inactive_pages = 0;
   long int result = 0;
   int page_size = getpagesize();
   int mib[4] = { CTL_HW, HW_PHYSMEM, 0, 0 };

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;
   memory->total /= 1024;

   total_pages =
     _sysctlfromname("vm.stats.vm.v_page_count", mib, 4, &len);
   if (total_pages < 0)
     return;

   free_pages = _sysctlfromname("vm.stats.vm.v_free_count", mib, 4, &len);
   if (free_pages < 0)
     return;

   inactive_pages =
     _sysctlfromname("vm.stats.vm.v_inactive_count", mib, 4, &len);
   if (inactive_pages < 0)
     return;

   memory->used = (total_pages - free_pages - inactive_pages) * page_size;
   _memsize_bytes_to_kb(&memory->used);

   result = _sysctlfromname("vfs.bufspace", mib, 2, &len);
   if (result < 0)
     return;
   memory->buffered = (result);
   _memsize_bytes_to_kb(&memory->buffered);

   result = _sysctlfromname("vm.stats.vm.v_active_count", mib, 4, &len);
   if (result < 0)
     return;
   memory->cached = (result * page_size);
   _memsize_bytes_to_kb(&memory->cached);

   result = _sysctlfromname("vm.stats.vm.v_cache_count", mib, 4, &len);
   if (result < 0)
     return;
   memory->shared = (result * page_size);
   _memsize_bytes_to_kb(&memory->shared);

   result = _sysctlfromname("vm.swap_total", mib, 2, &len);
   if (result < 0)
     return;
   memory->swap_total = (result / 1024);

   miblen = 3;
   if (sysctlnametomib("vm.swap_info", mib, &miblen) == -1) return;

   struct xswdev xsw;
   /* previous mib is important for this one... */

   for (i = 0; ; i++)
     {
        mib[miblen] = i;
        len = sizeof(xsw);
        if (sysctl(mib, miblen + 1, &xsw, &len, NULL, 0) == -1)
          break;

        memory->swap_used += (unsigned long) xsw.xsw_used * page_size;
     }

   memory->swap_used >>= 10;

#elif defined(__OpenBSD__)
   static int mib[] = { CTL_HW, HW_PHYSMEM64 };
   static int bcstats_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   static int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   int nswap, rnswap;
   struct swapent *swdev = NULL;

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;

   len = sizeof(uvmexp);
   if (sysctl(uvmexp_mib, 2, &uvmexp, &len, NULL, 0) == -1)
     return;

   len = sizeof(bcstats);
   if (sysctl(bcstats_mib, 3, &bcstats, &len, NULL, 0) == -1)
     return;

   /* Don't fail if there's not swap! */
   nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap == 0)
     goto swap_out;

   swdev = calloc(nswap, sizeof(*swdev));
   if (swdev == NULL)
     goto swap_out;

   rnswap = swapctl(SWAP_STATS, swdev, nswap);
   if (rnswap == -1)
     goto swap_out;

   for (i = 0; i < nswap; i++) {
        if (swdev[i].se_flags & SWF_ENABLE)
          {
             memory->swap_used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
             memory->swap_total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
          }
     }
swap_out:
   if (swdev)
     free(swdev);

   memory->total /= 1024;

   memory->cached = (uvmexp.pagesize * bcstats.numbufpages);
   _memsize_bytes_to_kb(&memory->cached);

   memory->used = (uvmexp.active * uvmexp.pagesize);
   _memsize_bytes_to_kb(&memory->used);

   memory->buffered = (uvmexp.pagesize * (uvmexp.npages - uvmexp.free));
   _memsize_bytes_to_kb(&memory->buffered);

   memory->shared = (uvmexp.pagesize * uvmexp.wired);
   _memsize_bytes_to_kb(&memory->shared);
#elif defined(__MacOS__)
   int mib[2] = { CTL_HW, HW_MEMSIZE };
   size_t total;
   vm_size_t page_size;
   mach_port_t mach_port;
   mach_msg_type_number_t count;
   vm_statistics64_data_t vm_stats;
   struct xsw_usage xsu;

   size_t len = sizeof(size_t);
   if (sysctl(mib, 2, &total, &len, NULL, 0) == -1)
     return;
   mach_port = mach_host_self();
   count = sizeof(vm_stats) / sizeof(natural_t);

   total >>= 10;
   memory->total = total;

   if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
       host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS)
     {
        memory->used = vm_stats.active_count + vm_stats.inactive_count + vm_stats.wire_count * page_size;
        memory->used >>= 10;
        memory->cached = vm_stats.active_count * page_size;
        memory->cached >>= 10;
        memory->shared = vm_stats.wire_count * page_size;
        memory->shared >>= 10;
        memory->buffered = vm_stats.inactive_count * page_size;
        memory->buffered >>= 10;
     }

   total = sizeof(xsu);
   if (sysctlbyname("vm.swapusage", &xsu, &total, NULL, 0) != -1)
     {
        memory->swap_total = xsu.xsu_total;
        memory->swap_used = xsu.xsu_used;
     }
#endif
}

static void
_temperature_cpu_get(int *temperature)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
   int mibs[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int devn, numt;
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);

   for (devn = 0;; devn++) {
        mibs[2] = devn;

        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENOENT)
               break;
             else
               continue;
          }
        if (!strcmp("cpu0", snsrdev.xname))
          break;
        else if (!strcmp("km0", snsrdev.xname))
          break;
     }

   for (numt = 0; numt < snsrdev.maxnumt[SENSOR_TEMP]; numt++) {
        mibs[4] = numt;

        if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
          continue;

        if (slen > 0 && (snsr.flags & SENSOR_FINVALID) == 0)
          break;
     }

   if (sysctl(mibs, 5, &snsr, &slen, NULL, 0)
       != -1)
     {
        *temperature = (snsr.value - 273150000) / 1000000.0;
     }
   else
     *temperature = INVALID_TEMP;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int value;
   size_t len = sizeof(value);
   if ((sysctlbyname
          ("hw.acpi.thermal.tz0.temperature", &value, &len, NULL,
          0)) != -1)
     {
        *temperature = (value -  2732) / 10;
     }
   else
     *temperature = INVALID_TEMP;
#elif defined(__linux__)
   struct dirent *dh;
   DIR *dir;
   char path[PATH_MAX];

   *temperature = INVALID_TEMP;

   dir = opendir("/sys/class/thermal");
   if (!dir) return;

   while ((dh = readdir(dir)) != NULL)
     {
        if (!strncmp(dh->d_name, "thermal_zone", 12))
          {
             snprintf(path, sizeof(path), "/sys/class/thermal/%s/type", dh->d_name);
             char *type = file_contents(path);
             if (type)
               {
                  /* This should ensure we get the highest available core temperature */
                  if (strstr(type, "_pkg_temp"))
                    {
                       snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp", dh->d_name);
                       char *value = file_contents(path);
                       if (value)
                         {
                            *temperature = atoi(value) / 1000;
                            free(value);
                            free(type);
                            break;
                         }
                    }
                  free(type);
               }
          }
     }

   closedir(dir);
#elif defined(__MacOS__)
   *temperature = INVALID_TEMP;
#endif
}

static int
_power_battery_count_get(power_t *power)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   int mib[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int i, devn;
   for (devn = 0;; devn++) {
        mib[2] = devn;
        if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO)
               continue;
             if (errno == ENOENT)
               break;
          }

        for (i = 0; i < MAX_BATTERIES; i++) {
             char buf[64];
             snprintf(buf, sizeof(buf), "acpibat%d", i);
             if (!strcmp(buf, snsrdev.xname))
               {
                  power->bat_mibs[power->battery_count] =
                    malloc(sizeof(int) * 5);
                  power->battery_names[power->battery_count] = strdup(buf);
                  int *tmp = power->bat_mibs[power->battery_count++];
                  tmp[0] = mib[0];
                  tmp[1] = mib[1];
                  tmp[2] = mib[2];
               }
          }

        if (!strcmp("acpiac0", snsrdev.xname))
          {
             power->ac_mibs[0] = mib[0];
             power->ac_mibs[1] = mib[1];
             power->ac_mibs[2] = mib[2];
          }
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   size_t len;

   if ((sysctlbyname("hw.acpi.battery.units", &power->battery_count, &len, NULL, 0))  < 0)
     {
        power->battery_count = 0;
     }

   if ((sysctlbyname("hw.acpi.acline", NULL, &len, NULL, 0)) != -1)
     {
        sysctlnametomib("hw.acpi.acline", power->ac_mibs, &len);
     }
#elif defined(__linux__)
   char path[PATH_MAX];
   struct dirent **names;
   int i, n;

   n = scandir("/sys/class/power_supply", &names, 0, alphasort);
   if (n < 0) return power->battery_count;

   for (i = 0; i < n; i++){
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", names[i]->d_name);
        char *type = file_contents(path);
        if (type)
          {
             if (!strncmp(type, "Battery", 7))
               power->battery_names[power->battery_count++] = strdup(names[i]->d_name);
             free(type);
          }

        free(names[i]);
     }

   free(names);
#endif

   power->batteries = malloc(power->battery_count * sizeof(bat_t **));
   for (int i = 0; i < power->battery_count; i++)
     {
        power->batteries[i] = calloc(1, sizeof(bat_t));
     }

   return power->battery_count;
}

static void
_battery_state_get(power_t *power)
{
#if defined(__OpenBSD__) || defined(__NetBSD__)
   int *mib;
   double charge_full, charge_current;
   size_t slen = sizeof(struct sensor);
   struct sensor snsr;

   for (int i = 0; i < power->battery_count; i++)
     {
        charge_full = charge_current = 0;

        mib = power->bat_mibs[i];
        mib[3] = 7;
        mib[4] = 0;

        if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
          charge_full = (double)snsr.value;

        mib[3] = 7;
        mib[4] = 3;

        if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
          charge_current = (double)snsr.value;

        /* ACPI bug workaround... */
        if (charge_current == 0 || charge_full == 0)
          {
             mib[3] = 8;
             mib[4] = 0;

             if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
               charge_full = (double)snsr.value;

             mib[3] = 8;
             mib[4] = 3;

             if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
               charge_current = (double)snsr.value;
          }

        power->batteries[i]->charge_full = charge_full;
        power->batteries[i]->charge_current = charge_current;
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int fd, i;
   union acpi_battery_ioctl_arg battio;
   char name[256];

   if ((fd = open("/dev/acpi", O_RDONLY)) == -1) return;

   for (i = 0; i < power->battery_count; i++)
     {
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BIF, &battio) != -1)
          {
             power->batteries[i]->charge_full = battio.bif.lfcap;
          }

        snprintf(name, sizeof(name), "%s %s", battio.bif.oeminfo, battio.bif.model);
        power->battery_names[i] = strdup(name);
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BST, &battio) != -1)
          {
             power->batteries[i]->charge_current = battio.bst.cap;
          }
     }
   close(fd);

#elif defined(__linux__)
   char path[PATH_MAX];
   struct dirent *dh;
   struct stat st;
   DIR *dir;
   char *buf, *naming = NULL;
   int i = 0;
   unsigned long charge_full = 0;
   unsigned long charge_current = 0;

   for (i = 0; i < power->battery_count; i++)
     {
        naming = NULL;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s", power->battery_names[i]);

        if (stat(path, &st) < 0) continue;
        if (S_ISLNK(st.st_mode)) continue;
        if (!S_ISDIR(st.st_mode)) continue;

        dir = opendir(path);
        if (!dir) return;
        while ((dh = readdir(dir)) != NULL)
          {
             char *e;
             if (dh->d_name[0] == '.') continue;

             if ((e = strstr(dh->d_name, "_full\0")))
              {
                 naming = strndup(dh->d_name, e - dh->d_name);
                 break;
              }
          }
        closedir(dir);

        if (!naming)
          {
             continue;
          }

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s_full", power->battery_names[i], naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_full = atol(buf);
             free(buf);
          }
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s_now", power->battery_names[i], naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_current = atol(buf);
             free(buf);
          }

        char *model, *vendor;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/manufacturer", power->battery_names[i]);
        vendor = file_contents(path);

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/model_name", power->battery_names[i]);
        model = file_contents(path);

        if (vendor && vendor[0] && model && model[0])
          {
             char name[256];
             int len;

             len = strlen(vendor);
             if (vendor[len - 1] == '\n' || vendor[len - 1] == '\r')
               {
                  vendor[len - 1] = '\0';
               }

             len = strlen(model);
             if (model[len - 1] == '\n' || model[len - 1] == '\r')
               {
                  model[len - 1] = '\0';
               }

             free(power->battery_names[i]);
             snprintf(name, sizeof(name), "%s %s", vendor, model);
             power->battery_names[i] = strdup(name);
          }

        power->batteries[i]->charge_full = charge_full;
        power->batteries[i]->charge_current = charge_current;

        if (model)
          free(model);
        if (vendor)
          free(vendor);

        free(naming);

        if (i == MAX_BATTERIES)
          {
             fprintf(stderr, "Error: Should not be here!\n");
             exit(-1);
          }
     }
#endif
}

static void
_power_state_get(power_t *power)
{
   int i;
#if defined(__OpenBSD__) || defined(__NetBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int value;
   size_t len;
#elif defined(__linux__)
   char *buf;
#endif

#if defined(__OpenBSD__) || defined(__NetBSD__)
   power->ac_mibs[3] = 9;
   power->ac_mibs[4] = 0;

   if (sysctl(power->ac_mibs, 5, &snsr, &slen, NULL, 0) != -1)
     power->have_ac = (int)snsr.value;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   len = sizeof(value);
   if ((sysctl(power->ac_mibs, 3, &value, &len, NULL, 0)) == -1)
     {
        return;
     }
   power->have_ac = value;
#elif defined(__linux__)
   buf = file_contents("/sys/class/power_supply/AC/online");
   if (buf)
     {
        power->have_ac = atoi(buf);
        free(buf);
     }
#endif

  _battery_state_get(power);

   for (i = 0; i < power->battery_count; i++)
     {
        double percent =
           100 * (power->batteries[i]->charge_current / power->batteries[i]->charge_full);
        power->batteries[i]->percent = percent;
        if (power->bat_mibs[i])
          free(power->bat_mibs[i]);
     }
}

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
static void
_freebsd_generic_network_status(unsigned long int *in,
                                unsigned long int *out)
{
   struct ifmibdata *ifmd;
   size_t len;
   int i, count;
   len = sizeof(count);

   if (sysctlbyname
         ("net.link.generic.system.ifcount", &count, &len, NULL, 0) < 0)
     return;

   ifmd = malloc(sizeof(struct ifmibdata));
   if (!ifmd)
     return;

   for (i = 1; i <= count; i++) {
        int mib[] = { CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL };
        len = sizeof(*ifmd);
        if (sysctl(mib, 6, ifmd, &len, NULL, 0) < 0) continue;
        if (!strcmp(ifmd->ifmd_name, "lo0"))
          continue;
        *in += ifmd->ifmd_data.ifi_ibytes;
        *out += ifmd->ifmd_data.ifi_obytes;
     }
   free(ifmd);
}

#endif

#if defined(__OpenBSD__)
static void
_openbsd_generic_network_status(unsigned long int *in,
                                unsigned long int *out)
{
   struct ifaddrs *interfaces, *ifa;

   if (getifaddrs(&interfaces) < 0)
     return;

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
     return;

   for (ifa = interfaces; ifa; ifa = ifa->ifa_next) {
        struct ifreq ifreq;
        struct if_data if_data;

        ifreq.ifr_data = (void *)&if_data;
        strncpy(ifreq.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFDATA, &ifreq) < 0)
          return;

        struct if_data *const ifi = &if_data;
        if (ifi->ifi_type == IFT_ETHER ||
            ifi->ifi_type == IFT_FASTETHER ||
            ifi->ifi_type == IFT_GIGABITETHERNET ||
            ifi->ifi_type == IFT_IEEE80211)
          {
             if (ifi->ifi_ibytes)
               *in += ifi->ifi_ibytes;

             if (ifi->ifi_obytes)
               *out += ifi->ifi_obytes;
          }
     }
   close(sock);
}

#endif

#if defined(__linux__)
static void
_linux_generic_network_status(unsigned long int *in,
                              unsigned long int *out)
{
   FILE *f;
   char buf[4096], dummy_s[256];
   unsigned long int tmp_in, tmp_out, dummy;

   f = fopen("/proc/net/dev", "r");
   if (!f) return;

   while (fgets(buf, sizeof(buf), f))
     {
        if (17 == sscanf(buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                              "%lu %lu %lu %lu\n", dummy_s, &tmp_in, &dummy, &dummy,
                         &dummy, &dummy, &dummy, &dummy, &dummy, &tmp_out, &dummy,
                         &dummy, &dummy, &dummy, &dummy, &dummy, &dummy))
          {
             *in += tmp_in;
             *out += tmp_out;
          }
     }

   fclose(f);
}

#endif

static void
_network_transfer_get(results_t *results)
{
   unsigned long first_in = 0, first_out = 0;
   unsigned long last_in = 0, last_out = 0;
#if defined(__linux__)
   _linux_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _linux_generic_network_status(&last_in, &last_out);
#elif defined(__OpenBSD__)
   _openbsd_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _openbsd_generic_network_status(&last_in, &last_out);
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
   _freebsd_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _freebsd_generic_network_status(&last_in, &last_out);
#endif
   results->incoming = last_in - first_in;
   results->outgoing = last_out - first_out;
}

static double
_results_cpu(cpu_core_t **cores, int cpu_count)
{
   double total = 0;
   for (int i = 0; i < cpu_count; i++)
     total += cores[i]->percent;

   total = total / cpu_count;

   return total;
}

int
system_temperature_cpu_get(void)
{
   int temp;

   _temperature_cpu_get(&temp);

   return temp;
}

void
system_power_state_get(power_t *power)
{
   if (_power_battery_count_get(power))
     _power_state_get(power);
}

bool
system_network_transfer_get(unsigned long *incoming, unsigned long *outgoing)
{
   unsigned long first_in = 0, first_out = 0;
   unsigned long last_in = 0, last_out = 0;
#if defined(__linux__)
   _linux_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _linux_generic_network_status(&last_in, &last_out);
#elif defined(__OpenBSD__)
   _openbsd_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _openbsd_generic_network_status(&last_in, &last_out);
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
   _freebsd_generic_network_status(&first_in, &first_out);
   usleep(1000000);
   _freebsd_generic_network_status(&last_in, &last_out);
#else
   return false;
#endif
   *incoming = last_in - first_in;
   *outgoing = last_out - first_out;

   return true;
}

int
system_cpu_memory_get(double *percent_cpu, long *memory_total, long *memory_used)
{
   results_t results;

   memset(&results, 0, sizeof(results));

   results.cores = _cpu_cores_state_get(&results.cpu_count);

   _memory_usage_get(&results.memory);

   *percent_cpu = _results_cpu(results.cores, results.cpu_count);
   *memory_total = results.memory.total;
   *memory_used = results.memory.used;

   for (int i = 0; i < results.cpu_count; i++)
     {
        free(results.cores[i]);
     }

   free(results.cores);

   return results.cpu_count;
}

static void *
_network_transfer_get_thread_cb(void *arg)
{
   results_t *results = arg;

   _network_transfer_get(results);

   return (void *)0;
}

results_t *
system_stats_get(void)
{
   results_t *results;
   void *ret;
   pthread_t tid;
   int error;

   results = calloc(1, sizeof(results_t));
   if (!results) return NULL;

   results->cores = _cpu_cores_state_get(&results->cpu_count);

   _memory_usage_get(&results->memory);

   error = pthread_create(&tid, NULL, _network_transfer_get_thread_cb, results);
   if (error)
     _network_transfer_get(results);

   if (_power_battery_count_get(&results->power))
     _power_state_get(&results->power);

   _temperature_cpu_get(&results->temperature);

   if (!error)
     {
        ret = NULL;
        pthread_join(tid, ret);
     }

   return results;
}
