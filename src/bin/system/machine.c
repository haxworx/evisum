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
#if !defined(__linux__)
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

#if defined(__OpenBSD__)
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

#include "machine.h"

#if defined(__OpenBSD__)
# define CPU_STATES      6
#else
# define CPU_STATES      5
#endif

#if defined(__linux__)
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

   buf[len] = 0;

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
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
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
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
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

             if (4 != sscanf(line, "%lu %lu %lu %lu", &cpu_times[0],
                   &cpu_times[1], &cpu_times[2], &cpu_times[3]))
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
   if (host_processor_info(mach_port, PROCESSOR_CPU_LOAD_INFO, &cpu_count,
                (processor_info_array_t *)&load, &count) != KERN_SUCCESS)
     exit(-1);

   for (i = 0; i < ncpu; i++) {
        core = cores[i];

        total = load[i].cpu_ticks[CPU_STATE_USER] +
           load[i].cpu_ticks[CPU_STATE_SYSTEM] +
           load[i].cpu_ticks[CPU_STATE_IDLE] +
           load[i].cpu_ticks[CPU_STATE_NICE];
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

cpu_core_t **
system_cpu_usage_get(int *ncpu)
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

void
system_memory_usage_get(meminfo_t *memory)
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

   memory->total *= 1024;
   memory->used *= 1024;
   memory->buffered *= 1024;
   memory->cached *= 1024;
   memory->shared *= 1024;
   memory->swap_total *= 1024;
   memory->swap_used *= 1024;

   fclose(f);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int free = 0, active = 0, inactive = 0, wired = 0;
   unsigned int cached = 0, buffered = 0, zfs_arc = 0;
   long int result = 0;
   int page_size = getpagesize();
   int mib[5] = { CTL_HW, HW_PHYSMEM, 0, 0, 0 };

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;
   if ((active =
        _sysctlfromname("vm.stats.vm.v_active_count", mib, 4, &len)) < 0)
     return;
   if ((inactive =
        _sysctlfromname("vm.stats.vm.v_inactive_count", mib, 4, &len)) < 0)
     return;
   if ((wired =
        _sysctlfromname("vm.stats.vm.v_wire_count", mib, 4, &len)) < 0)
     return;
   if ((cached =
        _sysctlfromname("vm.stats.vm.v_cache_count", mib, 4, &len)) < 0)
     return;
   if ((free = _sysctlfromname("vm.stats.vm.v_free_count", mib, 4, &len)) < 0)
     return;
   if ((buffered = _sysctlfromname("vfs.bufspace", mib, 2, &len)) < 0)
     return;

   memory->used = ((active + wired + cached) * page_size);
   memory->buffered = buffered;
   memory->cached = (cached * page_size);

   result = _sysctlfromname("vm.swap_total", mib, 2, &len);
   if (result < 0)
     return;
   memory->swap_total = result;

   miblen = 3;
   if (sysctlnametomib("vm.swap_info", mib, &miblen) == -1) return;

   if ((zfs_arc = _sysctlfromname("kstat.zfs.misc.arcstats.c", mib, 5, &len)) != -1)
     {
        memory->zfs_arc_used = zfs_arc;
     }

   struct xswdev xsw;

   for (i = 0; ; i++)
     {
        mib[miblen] = i;
        len = sizeof(xsw);
        if (sysctl(mib, miblen + 1, &xsw, &len, NULL, 0) == -1)
          break;

        memory->swap_used += (unsigned long) xsw.xsw_used * page_size;
     }
#elif defined(__OpenBSD__)
   static int mib[] = { CTL_HW, HW_PHYSMEM64 };
   static int bcstats_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   static int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   int nswap, rnswap;
   struct swapent *swdev = NULL;
   (void) miblen;

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;

   len = sizeof(uvmexp);
   if (sysctl(uvmexp_mib, 2, &uvmexp, &len, NULL, 0) == -1)
     return;

   len = sizeof(bcstats);
   if (sysctl(bcstats_mib, 3, &bcstats, &len, NULL, 0) == -1)
     return;

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

   memory->swap_total *= 1024;
   memory->swap_used *= 1024;
swap_out:
   if (swdev)
     free(swdev);

   memory->cached = (uint64_t)(uvmexp.pagesize * bcstats.numbufpages);
   memory->used = (uint64_t)(uvmexp.active * uvmexp.pagesize);
   memory->buffered = (uint64_t)(uvmexp.pagesize * (uint64_t)(uvmexp.npages - uvmexp.free));
   memory->shared = (uint64_t)(uvmexp.pagesize * uvmexp.wired);
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

   memory->total = total;

   if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
       host_statistics64(mach_port, HOST_VM_INFO,
                 (host_info64_t)&vm_stats, &count) == KERN_SUCCESS)
     {
        memory->used = memory->total - (vm_stats.inactive_count * page_size);
        memory->cached = vm_stats.active_count * page_size;
        memory->shared = vm_stats.wire_count * page_size;
        memory->buffered = vm_stats.inactive_count * page_size;
     }

   total = sizeof(xsu);
   if (sysctlbyname("vm.swapusage", &xsu, &total, NULL, 0) != -1)
     {
        memory->swap_total = xsu.xsu_total;
        memory->swap_used = xsu.xsu_used;
     }
#endif
}

sensor_t **
system_sensors_thermal_get(int *sensor_count)
{
   sensor_t **sensors = NULL;
#if defined(__OpenBSD__)
   sensor_t *sensor;
   int mibs[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int devn, n;
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);

   for (devn = 0;; devn++)
     {
        mibs[2] = devn;

        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENOENT) break;
             continue;
          }

        for (n = 0; n < snsrdev.maxnumt[SENSOR_TEMP]; n++)
          {
             mibs[4] = n;

             if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
               continue;

             if (slen > 0 && (snsr.flags & SENSOR_FINVALID) == 0)
               break;
          }

        if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
          continue;
        if (snsr.type != SENSOR_TEMP)
          continue;

        sensors = realloc(sensors, (1 + *sensor_count) * sizeof(sensor_t *));
        sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));
        sensor->name = strdup(snsrdev.xname);
        sensor->value = (snsr.value - 273150000) / 1000000.0; // (uK -> C)
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   sensor_t *sensor;
   int value;
   size_t len = sizeof(value);

   if ((sysctlbyname("hw.acpi.thermal.tz0.temperature", &value, &len, NULL, 0)) != -1)
     {
        sensors = realloc(sensors, (1 + *sensor_count) * sizeof(sensor_t *));
        sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));
        sensor->name = strdup("hw.acpi.thermal.tz0");
        sensor->value = (float) (value -  2732) / 10;
     }
#elif defined(__linux__)
   sensor_t *sensor;
   char *type, *value;
   char path[PATH_MAX];
   struct dirent **names;
   int i, n;

   n = scandir("/sys/class/thermal", &names, 0, alphasort);
   if (n < 0) return NULL;

   for (i = 0; i < n; i++)
     {
        if (strncmp(names[i]->d_name, "thermal_zone", 12))
          {
             free(names[i]);
             continue;
          }
        snprintf(path, sizeof(path), "/sys/class/thermal/%s/type",
                 names[i]->d_name);

        type = file_contents(path);
        if (type)
          {
             sensors =
                realloc(sensors, (1 + (*sensor_count)) * sizeof(sensor_t *));
             sensors[(*sensor_count)++] =
                 sensor = calloc(1, sizeof(sensor_t));

             sensor->name = strdup(type);
             snprintf(path, sizeof(path), "/sys/class/thermal/%s/temp",
                      names[i]->d_name);

             value = file_contents(path);
             if (!value)
               sensor->invalid = true;
             else
               {
                  sensor->value = (float)atoi(value) / 1000.0;
                  free(value);
               }
             free(type);
          }

        free(names[i]);
     }

   free(names);
#elif defined(__MacOS__)
#endif
   return sensors;
}

static int
_power_battery_count_get(power_t *power)
{
#if defined(__OpenBSD__)
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   int mib[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int i, devn, id;
   for (devn = 0;; devn++) {
        mib[2] = devn;
        if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO)
               continue;
             if (errno == ENOENT)
               break;
          }

        for (i = 0; i < 10; i++) {
             char buf[64];
             snprintf(buf, sizeof(buf), "acpibat%d", i);
             if (!strcmp(buf, snsrdev.xname))
               {
                  id = power->battery_count;
                  power->batteries = realloc(power->batteries, 1 +
                                     power->battery_count * sizeof(bat_t **));
                  power->batteries[id] = calloc(1, sizeof(bat_t));
                  power->batteries[id]->name = strdup(buf);
                  power->batteries[id]->present = true;
                  power->batteries[id]->mibs = malloc(sizeof(int) * 5);
                  int *tmp = power->batteries[id]->mibs;
                  tmp[0] = mib[0];
                  tmp[1] = mib[1];
                  tmp[2] = mib[2];
                  power->battery_count++;
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

   power->batteries = malloc(power->battery_count * sizeof(bat_t **));
   for (int i = 0; i < power->battery_count; i++)
     {
        power->batteries[i] = calloc(1, sizeof(bat_t));
        power->batteries[i]->present = true;
     }
#elif defined(__linux__)
   char *type;
   char path[PATH_MAX];
   struct dirent **names;
   int i, n, id;

   n = scandir("/sys/class/power_supply", &names, 0, alphasort);
   if (n < 0) return power->battery_count;

   for (i = 0; i < n; i++)
     {
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type",
                 names[i]->d_name);

        type = file_contents(path);
        if (type)
          {
             if (!strncmp(type, "Battery", 7))
               {
                  id = power->battery_count;
                  power->batteries = realloc(power->batteries, (1 +
                                     power->battery_count) * sizeof(bat_t **));
                  power->batteries[id] = calloc(1, sizeof(bat_t));
                  power->batteries[id]->name = strdup(names[i]->d_name);
                  power->batteries[id]->present = true;
                  power->battery_count++;
               }
             free(type);
          }

        free(names[i]);
     }

   free(names);
#endif

   return power->battery_count;
}

static void
_battery_state_get(power_t *power)
{
#if defined(__OpenBSD__)
   int *mib;
   double charge_full, charge_current;
   size_t slen = sizeof(struct sensor);
   struct sensor snsr;

   for (int i = 0; i < power->battery_count; i++)
     {
        charge_full = charge_current = 0;

        mib = power->batteries[i]->mibs;
        mib[3] = SENSOR_WATTHOUR;
        mib[4] = 0;

        if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
          charge_full = (double)snsr.value;

        mib[3] = SENSOR_WATTHOUR;
        mib[4] = 3;

        if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
          charge_current = (double)snsr.value;

        if (charge_current == 0 || charge_full == 0)
          {
             mib[3] = SENSOR_AMPHOUR;
             mib[4] = 0;

             if (sysctl(mib, 5, &snsr, &slen, NULL, 0) != -1)
               charge_full = (double)snsr.value;

             mib[3] = SENSOR_AMPHOUR;
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
             if (battio.bif.lfcap == 0)
               power->batteries[i]->charge_full = battio.bif.dcap;
             else
               power->batteries[i]->charge_full = battio.bif.lfcap;
          }
        snprintf(name, sizeof(name), "%s %s", battio.bif.oeminfo, battio.bif.model);
        power->batteries[i]->name = strdup(name);
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BST, &battio) != -1)
          {
             power->batteries[i]->charge_current = battio.bst.cap;
          }
        if (battio.bst.state == ACPI_BATT_STAT_NOT_PRESENT)
          {
             power->batteries[i]->present = false;
          }
     }
   close(fd);

#elif defined(__linux__)
   char path[PATH_MAX];
   struct dirent *dh;
   struct stat st;
   DIR *dir;
   char *model, *vendor;
   char *buf, *naming = NULL;
   int i = 0;
   unsigned long charge_full = 0;
   unsigned long charge_current = 0;

   for (i = 0; i < power->battery_count; i++)
     {
        naming = NULL;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s",
                 power->batteries[i]->name);

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
          continue;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s_full",
                 power->batteries[i]->name, naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_full = atol(buf);
             free(buf);
          }
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/%s_now",
                 power->batteries[i]->name, naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_current = atol(buf);
             free(buf);
          }

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/manufacturer",
                 power->batteries[i]->name);
        vendor = file_contents(path);

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/model_name",
                 power->batteries[i]->name);
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

             free(power->batteries[i]->name);;
             snprintf(name, sizeof(name), "%s %s", vendor, model);
             power->batteries[i]->name = strdup(name);
          }

        power->batteries[i]->charge_full = charge_full;
        power->batteries[i]->charge_current = charge_current;

        if (model)
          free(model);
        if (vendor)
          free(vendor);
        free(naming);
     }
#endif
}

void
system_power_state_get(power_t *power)
{
   int i;
#if defined(__OpenBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int value;
   size_t len;
#elif defined(__linux__)
   char *buf;
#endif

   if (!_power_battery_count_get(power))
     return;

#if defined(__OpenBSD__)
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
        double percent = 100 *
           (power->batteries[i]->charge_current /
                                    power->batteries[i]->charge_full);
        power->batteries[i]->percent = percent;
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
        int mib[] = {
           CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL
        };
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
        if (17 == sscanf(buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                         "%lu %lu %lu %lu %lu\n", dummy_s, &tmp_in, &dummy,
                         &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &tmp_out, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &dummy, &dummy))
          {
             *in += tmp_in;
             *out += tmp_out;
          }
     }

   fclose(f);
}

#endif

void
system_network_transfer_get(network_t *usage)
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
   usage->incoming = last_in - first_in;
   usage->outgoing = last_out - first_out;
}

static void *
_network_transfer_get_thread_cb(void *arg)
{
   network_t *usage = arg;

   system_network_transfer_get(usage);

   return (void *)0;
}

void
system_info_all_free(Sys_Info *info)
{
   sensor_t *snsr;
   int i;

   for (i = 0; i < info->cpu_count; i++)
     {
        free(info->cores[i]);
     }
   free(info->cores);

   for (i = 0; i < info->sensor_count; i++)
     {
        snsr = info->sensors[i];
        if (snsr->name)
          free(snsr->name);
        free(snsr);
     }
   if (info->sensors)
     free(info->sensors);

   for (i = 0; i < info->power.battery_count; i++)
     {
        if (info->power.batteries[i]->name)
          free(info->power.batteries[i]->name);
#if defined(__OpenBSD__)
        if (info->power.batteries[i]->mibs)
          free(info->power.batteries[i]->mibs);
#endif
        free(info->power.batteries[i]);
     }
   if (info->power.batteries)
     free(info->power.batteries);

   free(info);
}

Sys_Info *
system_info_all_get(void)
{
   Sys_Info *info;
   void *ret;
   pthread_t tid;
   int error;

   info = calloc(1, sizeof(Sys_Info));
   if (!info) return NULL;

   info->cores = system_cpu_usage_get(&info->cpu_count);

   system_memory_usage_get(&info->memory);

   error = pthread_create(&tid, NULL, _network_transfer_get_thread_cb,
                   &info->network_usage);
   if (error)
     system_network_transfer_get(&info->network_usage);

   system_power_state_get(&info->power);
   info->sensors = system_sensors_thermal_get(&info->sensor_count);

   if (!error)
     {
        ret = NULL;
        pthread_join(tid, ret);
     }

   return info;
}

