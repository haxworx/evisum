#ifndef MACHINE_H
#define MACHINE_H

#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
   unsigned long total;
   unsigned long idle;
   float         percent;
   int           id;
   int           top_id;
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

typedef enum
{
   THERMAL = 0,
   FANRPM  = 1,
} Sensor_Type;

typedef struct
{
   char       *name;
   char       *child_name;
#if defined(__linux__)
   char       *path;
#elif defined(__OpenBSD__)
   int         mibs[5];
#endif
   double      value;
   bool        invalid;
   int         id;
   Sensor_Type type;
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
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   char    unit;
#endif
} Battery;

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
power_ac_present(void);

Eina_List *
batteries_find(void);

void
battery_free(Battery *bat);

void
battery_update(Battery *bat);

Eina_List *
sensors_find(void);

void
memory_info(Meminfo *memory);

void
sensor_free(Sensor *sensor);

Eina_Bool
sensor_update(Sensor *sensor);

Eina_List *
network_interfaces_find(void);

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
core_id_frequency(int d);

int
core_id_temperature(int id);

int
cores_temperature_min_max(int *min, int *max);

int
cores_frequency_min_max(int *min, int *max);

#endif
