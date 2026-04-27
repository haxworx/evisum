#ifndef ENIGMATIC_H
#define ENIGMATIC_H

#define DEBUGGING 0

#define DEBUG(format, ...) \
   do {\
      if (DEBUGGING) \
        fprintf(stderr, format"\n", ##__VA_ARGS__); \
   } while (0);

#define ERROR(format, ...) \
   do {\
      fprintf(stderr, "ERR: " format "\n", ##__VA_ARGS__); \
      exit(1);\
   } while (0);

#include "config.h"
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>

#include "enigmatic_config.h"
#include "Events.h"
#include "system/machine.h"
#include "system/process.h"

#define LOG_FILE_NAME PACKAGE".log"
#define LCK_FILE_NAME PACKAGE".lock"
#define PID_FILE_NAME PACKAGE".pid"

typedef struct
{
   Eina_Hash *cores;
   Meminfo    meminfo;
   Eina_Bool  power;
   Eina_Hash *sensors;
   Eina_Hash *batteries;
   Eina_Hash *network_interfaces;
   Eina_Hash *file_systems;
   Eina_Hash *processes;
} System_Info;

#ifndef ENIGMATIC_BUFFER_TYPEDEF
#define ENIGMATIC_BUFFER_TYPEDEF
typedef struct
{
  uint8_t  *data;
  uint32_t  length;
  uint32_t  index;
} Buffer;
#endif

typedef struct
{
   int      fd;
   int      flags;
   Buffer   buf;
} Log;

typedef struct _Enigmatic Enigmatic;

struct _Enigmatic
{
   Ecore_Thread        *thread;
   Ecore_Event_Handler *handler;
   Eina_List           *client_requests;

   int                  lock_fd;

   Eina_List           *unique_ids;

   System_Info         *info;
   uint32_t             poll_time;
   uint32_t             poll_count;
   Interval             interval;
   Interval             interval_update;
   Eina_Bool            broadcast;
   Eina_Bool            close_on_parent_exit;
   int                  device_refresh_interval;

   Enigmatic_Config    *config;

   Eina_Lock            update_lock;

   pid_t                pid;
   char                *pidfile_path;

   struct
   {
      char             *path;
      Log              *file;
      char              hour;
      char              min;
      Eina_Thread      *rotate_thread;
   } log;

   Ecore_Thread        *battery_thread;
   Ecore_Thread        *power_thread;
   Ecore_Thread        *sensors_thread;
};

#include "enigmatic_util.h"

#endif
