#ifndef MACHINE_H
#define MACHINE_H

#include <Eina.h>
#include <stdint.h>
#include <stdbool.h>

#define CPUFREQ_INVALID -1

typedef struct
{
   char          name[16];
   unsigned long total;
   unsigned long idle;
   int8_t        percent;
   uint16_t      id;
   uint16_t      top_id;
   int           freq;
   int           temp;
   int           unique_id;
} Cpu_Core;

#define MEM_VIDEO_CARD_MAX 8

typedef struct
{
   uint64_t total;
   uint64_t used;
} Meminfo_Video;

typedef struct
{
   uint64_t        total;
   uint64_t        used;
   uint64_t        cached;
   uint64_t        buffered;
   uint64_t        shared;
   uint64_t        swap_total;
   uint64_t        swap_used;

   uint64_t        zfs_arc_used;

   uint64_t        video_count;
   Meminfo_Video   video[MEM_VIDEO_CARD_MAX];
} Meminfo;

#define THERMAL_INVALID -999

typedef enum
{
   THERMAL = 0,
   FANRPM  = 1,
} Sensor_Type;

typedef struct
{
   char        name[255];
   char        child_name[255];
#if defined(__linux__)
   char        path[PATH_MAX];
#elif defined(__OpenBSD__)
   int         mibs[5];
#endif
   double      value;
   bool        invalid;
   int         id;
   Sensor_Type type;
   int         unique_id;
} Sensor;

typedef struct
{
   char      name[255];
   char      vendor[64];
   char      model[64];
   uint32_t  charge_full;
   uint32_t  charge_current;
   float     percent;
   bool      present;
#if defined(__OpenBSD__)
   int       mibs[5];
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   char      unit;
#endif
   int       unique_id;
} Battery;

typedef struct
{
   char          name[255];
   uint64_t      total_in;
   uint64_t      total_out;

   uint64_t      peak_in;
   uint64_t      peak_out;

   uint64_t      in;
   uint64_t      out;

   int           unique_id;
} Network_Interface;

// Power

Eina_Bool
power_ac_present(void);

Eina_List *
batteries_find(void);

void
batteries_update(Eina_List *batteries);

void
battery_update(Battery *bat);

// Sensors

Eina_List *
sensors_find(void);

void
sensors_update(Eina_List *sensors);

Eina_Bool
sensor_update(Sensor *sensor);

// Network

Eina_List *
network_interfaces_find(void);

// Memory

void
memory_info(Meminfo *memory);

// CPU Cores

Eina_List *
cores_find(void);

void
cores_update(Eina_List *cores);

int
cores_count(void);

int
cores_online_count(void);

void
cores_topology(Eina_List *cores);

int
cores_frequency(void);

int
core_id_frequency(int id);

int
core_id_temperature(int id);

int
cores_temperature_min_max(int *min, int *max);

int
cores_frequency_min_max(int *min, int *max);

#endif
