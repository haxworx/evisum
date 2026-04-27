#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>

#define HEADER_MAGIC 0xf00dcafe

typedef enum
{
   EVENT_ERROR       = 0,
   EVENT_MESSAGE     = 1,
   EVENT_BROADCAST   = 2,
   EVENT_BLOCK_END   = 3,
   EVENT_LAST_RECORD = 4,
   EVENT_EOF         = 5,
} Event;

typedef enum
{
   CPU_CORE           = 1,
   CPU_CORE_PERC      = 2,
   CPU_CORE_TEMP      = 3,
   CPU_CORE_FREQ      = 4,
   MEMORY             = 5,
   MEMORY_TOTAL       = 6,
   MEMORY_USED        = 7,
   MEMORY_CACHED      = 8,
   MEMORY_BUFFERED    = 9,
   MEMORY_SHARED      = 10,
   MEMORY_SWAP_TOTAL  = 11,
   MEMORY_SWAP_USED   = 12,
   MEMORY_VIDEO_TOTAL = 13,
   MEMORY_VIDEO_USED  = 14,
   SENSOR             = 15,
   SENSOR_VALUE       = 16,
   POWER              = 17,
   POWER_VALUE        = 18,
   BATTERY            = 19,
   BATTERY_FULL       = 20,
   BATTERY_CURRENT    = 21,
   BATTERY_PERCENT    = 22,
   NETWORK            = 23,
   NETWORK_INCOMING   = 24,
   NETWORK_OUTGOING   = 25,
   FILE_SYSTEM        = 26,
   FILE_SYSTEM_TOTAL  = 27,
   FILE_SYSTEM_USED   = 28,
   PROCESS            = 29,
   PROCESS_PPID         = 30,
   PROCESS_UID          = 31,
   PROCESS_NICE         = 32,
   PROCESS_PRIORITY     = 33,
   PROCESS_CPU_ID       = 34,
   PROCESS_NUM_THREAD   = 35,
   PROCESS_CPU_TIME     = 36,
   PROCESS_RUN_TIME     = 37,
   PROCESS_START        = 38,
   PROCESS_MEM_SIZE     = 39,
   PROCESS_MEM_RSS      = 40,
   PROCESS_MEM_SHARED   = 41,
   PROCESS_MEM_VIRT     = 42,
   PROCESS_NET_IN       = 43,
   PROCESS_NET_OUT      = 44,
   PROCESS_DISK_READ    = 45,
   PROCESS_DISK_WRITE   = 46,
   PROCESS_COMMAND      = 47,
   PROCESS_ARGUMENTS    = 48,
   PROCESS_STATE        = 49,
   PROCESS_WCHAN        = 50,
   PROCESS_NUM_FILES    = 51,
   PROCESS_WAS_ZERO     = 52,
   PROCESS_IS_KERNEL    = 53,
   PROCESS_IS_NEW       = 54,
   PROCESS_TID          = 55,
   PROCESS_THREAD_NAME  = 56,
   PROCESS_FDS_COUNT    = 57,
   PROCESS_THREADS_COUNT = 58,
   PROCESS_CHILDREN_COUNT = 59,
   PROCESS_PATH         = 60,
   PROCESS_CPU_USAGE    = 61,
} Object_Type;

typedef enum
{
   INTERVAL_NORMAL = 1,
   INTERVAL_MEDIUM = 3,
   INTERVAL_SLOW   = 5,
} Interval;

typedef enum
{
   CHANGE_FLOAT  = 1,
   CHANGE_I8     = 2,
   CHANGE_I16    = 3,
   CHANGE_I32    = 4,
   CHANGE_I64    = 5,
   CHANGE_I128   = 6,
   CHANGE_STRING = 7
} Change;

typedef enum
{
   MESG_ERROR     = 1,
   MESG_REFRESH   = 2,
   MESG_ADD       = 3,
   MESG_MOD       = 4,
   MESG_DEL       = 5,
} Message_Type;

typedef struct
{
   Message_Type  type;
   Object_Type   object_type;
   unsigned int  number;
} Message;

typedef struct
{
   Event        event;
   uint32_t     time;
} Header;

#endif
