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

#include "macros.h"
#include "machine.h"
#include "machine/machine.bogox"
#include "machine/cpu.bogox"
#include "machine/memory.bogox"
#include "machine/sensors.bogox"
#include "machine/network.bogox"

void
system_info_all_free(Sys_Info *info)
{
   sensor_t *snsr;
   int i;

   for (i = 0; i < info->cpu_count; i++)
     free(info->cores[i]);
   free(info->cores);

   for (i = 0; i < info->sensor_count; i++) {
        snsr = info->sensors[i];
        if (snsr->name)
          free(snsr->name);
        if (snsr->child_name)
          free(snsr->child_name);
        free(snsr);
     }
   if (info->sensors)
     free(info->sensors);

   for (i = 0; i < info->power.battery_count; i++) {
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

   info = calloc(1, sizeof(Sys_Info));
   if (!info) return NULL;

   info->cores = system_cpu_usage_get(&info->cpu_count);

   system_memory_usage_get(&info->memory);

   system_power_state_get(&info->power);

   info->sensors = system_sensors_thermal_get(&info->sensor_count);

   return info;
}

