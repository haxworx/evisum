#ifndef MACHINE_H
#define MACHINE_H

#include <Eina.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
   unsigned long total;
   unsigned long idle;
   float         percent;
   int           id;
} Cpu_Core;

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
} Sensor;

typedef struct
{
   char   *name;
   char   *vendor;
   char   *model;
   double  charge_full;
   double  charge_current;
   double  percent;
   bool    present;
#if defined(__OpenBSD__)
   int     mibs[5];
#endif
} Battery;

typedef struct
{
   bool     have_ac;
#if defined(__OpenBSD__)
   int      mibs[5];
#endif
} AC_Adapter;

typedef struct
{
   char     name[255];
   uint64_t total_in;
   uint64_t total_out;

   uint64_t peak_in;
   uint64_t peak_out;

   uint64_t in;
   uint64_t out;
} Network_Interface;

Eina_Bool
power_ac(void);

Eina_List *
batteries_find(void);

void
battery_free(Battery *bat);

void
battery_check(Battery *bat);

Eina_List *
sensors_find(void);

void
sensor_free(Sensor *sensor);

Eina_Bool
sensor_check(Sensor *sensor);

Eina_List *
network_interfaces_find(void);

int
system_cpu_online_count_get(void);

int
system_cpu_count_get(void);

Cpu_Core **
system_cpu_usage_get(int *ncpu);
 
Cpu_Core **
system_cpu_usage_delayed_get(int *ncpu, int usecs);
 
Cpu_Core **
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

#endif
