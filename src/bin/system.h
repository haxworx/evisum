#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
   float         percent;
   unsigned long total;
   unsigned long idle;
} cpu_core_t;

typedef struct
{
   unsigned long total;
   unsigned long used;
   unsigned long cached;
   unsigned long buffered;
   unsigned long shared;
   unsigned long swap_total;
   unsigned long swap_used;
} meminfo_t;

#define MAX_BATTERIES 10

typedef struct
{
   double  charge_full;
   double  charge_current;
   uint8_t percent;
} bat_t;

typedef struct
{
   bool     have_ac;
   int      battery_count;

   bat_t  **batteries;

   // Necessary evils.
   char    *battery_names[MAX_BATTERIES];
   int     *bat_mibs[MAX_BATTERIES];
   int      ac_mibs[5];
} power_t;

#define INVALID_TEMP -999

typedef struct results_t results_t;
struct results_t
{
   int           cpu_count;
   cpu_core_t  **cores;

   meminfo_t     memory;

   power_t       power;

   unsigned long incoming;
   unsigned long outgoing;
   int           temperature;
};

results_t *
 system_stats_get(void);

int
 system_cpu_memory_get(double *percent_cpu, long *memory_total, long *memory_used);

bool
 system_network_transfer_get(unsigned long *incoming, unsigned long *outgoing);

int
 system_temperature_cpu_get(void);

void
 system_power_state_get(power_t *power);

int
 system_cpu_online_count_get();

#endif