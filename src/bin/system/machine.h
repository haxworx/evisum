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

// Will anyone have more than 8 vdrm/video card devices?
#define MEM_VIDEO_CARD_MAX 8

typedef struct
{
   uint64_t total;
   uint64_t used;
} meminfo_video_t;

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

   uint64_t        video_count;
   meminfo_video_t video[MEM_VIDEO_CARD_MAX];
} meminfo_t;

typedef struct
{
   char   *name;
   char   *child_name;
#if defined(__linux__)
   char   *path;
#elif defined(__OpenBSD__)
   int     mibs[5];
#endif
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
   int     mibs[5];
#endif
} bat_t;

typedef struct
{
   bool     have_ac;
   int      battery_count;

   bat_t  **batteries;
#if defined(__OpenBSD__)
   int      mibs[5];
#endif
} power_t;

typedef struct
{
   char name[255];
   struct
   {
      uint64_t in;
      uint64_t out;
   } xfer;
} net_iface_t;

int
system_cpu_online_count_get(void);

int
system_cpu_count_get(void);

cpu_core_t **
system_cpu_usage_get(int *ncpu);

cpu_core_t **
system_cpu_usage_delayed_get(int *ncpu, int usecs);

cpu_core_t **
system_cpu_state_get(int *ncpu);

int
system_cpu_frequency_get(void);

int
system_cpu_n_frequency_get(int n);

int
system_cpu_n_temperature_get(int n);

int
system_cpu_temperature_min_max_get(int *min, int *max);

int
system_cpu_frequency_min_max_get(int *min, int *max);

void
system_cpu_topology_get(int *ids, int ncpus);

void
system_memory_usage_get(meminfo_t *memory);

sensor_t **
system_sensors_thermal_get(int *count);

void
system_sensors_thermal_free(sensor_t **sensors, int count);

int
system_sensor_thermal_get(sensor_t *sensor);

void
system_sensor_thermal_free(sensor_t *sensor);

void
system_power_state_get(power_t *power);

void
system_power_state_free(power_t *power);

net_iface_t **
system_network_ifaces_get(int *n);

#endif
