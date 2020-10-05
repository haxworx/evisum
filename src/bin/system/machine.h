#ifndef __MACHINE_H__
#define __MACHINE_H__

/* All functions and data types implementing these APIs have no additional
 * system dependencies deliberately.
 *
 * See machine.c and the files includes in machine/ sub directory.
 */

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
   unsigned long total;
   unsigned long idle;
   float         percent;
} cpu_core_t;

typedef struct
{
   uint64_t total;
   uint64_t used;
   uint64_t cached;
   uint64_t buffered;
   uint64_t shared;
   uint64_t swap_total;
   uint64_t swap_used;

   uint64_t zfs_arc_used;
} meminfo_t;

typedef struct
{
   char   *name;
   char   *child_name;
   double  value;
   bool    invalid;
} sensor_t;

typedef struct
{
   char   *name;
   double  charge_full;
   double  charge_current;
   uint8_t percent;
   bool    present;
#if defined(__OpenBSD__)
   int    *mibs;
#endif
} bat_t;

typedef struct
{
   bool     have_ac;
   int      battery_count;

   bat_t  **batteries;
   int      ac_mibs[5];
} power_t;

typedef struct
{
   uint64_t incoming;
   uint64_t outgoing;
} network_t;

typedef struct Sys_Info Sys_Info;
struct Sys_Info
{
   int           cpu_count;
   cpu_core_t  **cores;

   meminfo_t     memory;
   power_t       power;

   int           sensor_count;
   sensor_t    **sensors;

   network_t     network_usage;
};

Sys_Info *
system_info_all_get(void);

void
system_info_all_free(Sys_Info *);

int
system_cpu_online_count_get(void);

int
system_cpu_count_get(void);

cpu_core_t **
system_cpu_usage_get(int *ncpu);

cpu_core_t **
system_cpu_usage_delayed_get(int *ncpu, int usecs);

int
system_cpu_frequency_get(void);

int
system_cpu_n_frequency_get(int n);

int
system_cpu_frequency_min_max_get(int *min, int *max);

void
system_memory_usage_get(meminfo_t *memory);

sensor_t **
system_sensors_thermal_get(int *count);

void
system_power_state_get(power_t *power);

void
system_network_transfer_get(network_t *usage);

#endif
