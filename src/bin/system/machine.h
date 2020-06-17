#ifndef __MACHINE_H__
#define __MACHINE_H__

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
   unsigned long long total;
   unsigned long long used;
   unsigned long long cached;
   unsigned long long buffered;
   unsigned long long shared;
   unsigned long long swap_total;
   unsigned long long swap_used;

   unsigned long long zfs_arc_used;
} meminfo_t;

typedef struct
{
   char   *name;
   double  value;
   bool    invalid;
} sensor_t;

typedef struct
{
   double  charge_full;
   double  charge_current;
   uint8_t percent;
   bool    present;
} bat_t;

typedef struct
{
   bool     have_ac;
   int      battery_count;

   bat_t  **batteries;
#define MAX_BATTERIES 10
   char    *battery_names[MAX_BATTERIES];
   int     *bat_mibs[MAX_BATTERIES];
   int      ac_mibs[5];
} power_t;

typedef struct Sys_Info Sys_Info;
struct Sys_Info
{
   int           cpu_count;
   cpu_core_t  **cores;
   meminfo_t     memory;
   power_t       power;

   int           sensor_count;
   sensor_t     **sensors;

   unsigned long incoming;
   unsigned long outgoing;
};

Sys_Info *
system_info_all_get(void);

void
system_info_all_free(Sys_Info *);

int
system_cpu_online_count_get();

#endif
