#ifndef ENIGMATIC_CLIENT_H
#define ENIGMATIC_CLIENT_H

#include "Events.h"
#include "enigmatic_util.h"
#include "enigmatic_visibility.h"
#include "intl/gettext.h"
#define _(STR) gettext(STR)

#include "system/machine.h"
#include "system/file_systems.h"
#include "system/process.h"

#include <Eina.h>
#include <Ecore.h>
#include <Eio.h>
#include <stdint.h>
#include <sys/types.h>

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
   uint32_t      time;
   Eina_Bool     last_record;

   Eina_List    *cores;
   Meminfo       meminfo;
   Eina_List    *sensors;
   Eina_Bool     power;
   Eina_List    *batteries;
   Eina_List    *network_interfaces;
   Eina_List    *file_systems;
   Eina_List    *processes;
} Snapshot;

typedef struct _Enigmatic_Client Enigmatic_Client;

typedef struct
{
   uint32_t time;
   void    *data;
} Enigmatic_Client_Event;

typedef void (Snapshot_Callback)(Enigmatic_Client *client, Snapshot *s, void *data);
typedef void (Event_Callback)(Enigmatic_Client *client, Enigmatic_Client_Event *event_info, void *data);

typedef struct
{
   Event_Callback *callback;
   void           *data;
} Event_Callback_Data;

typedef struct
{
   Snapshot_Callback *callback;
   void              *data;
} Event_Snapshot_Data;

struct _Enigmatic_Client
{
   /* Private */
   char                 *filename;
   char                 *directory;
   int                   fd;
   int                   retries;
   uint32_t              file_size;
   Buffer                buf;
   Buffer                zbuf;
   off_t                 offset;
   Eina_Bool             compressed;

   Header                header;
   Message               message;
   Change                change;
   Interval              interval;

   Eina_Bool             follow;
   Eina_Bool             truncated;

   Eio_Monitor          *mon;
   Ecore_Thread         *thread;
   Ecore_Event_Handler  *handler;
   Ecore_Event_Handler  *handler_created;
   Ecore_Event_Handler  *handler_deleted;

   struct
   {
      Eina_Bool enabled;
      uint32_t  start_time;
      uint32_t  end_time;
   } replay;

   struct
   {
      Eina_Bool valid;
      uint32_t  start_time;
      uint32_t  end_time;
   } bounds;

   /* Public */

   Event_Snapshot_Data   event_snapshot;
   Event_Snapshot_Data   event_snapshot_init;

   Event_Callback_Data   event_cpu_add;
   Event_Callback_Data   event_cpu_del;
   Event_Callback_Data   event_battery_add;
   Event_Callback_Data   event_battery_del;
   Event_Callback_Data   event_power_supply_add;
   Event_Callback_Data   event_power_supply_del;
   Event_Callback_Data   event_sensor_add;
   Event_Callback_Data   event_sensor_del;
   Event_Callback_Data   event_network_iface_add;
   Event_Callback_Data   event_network_iface_del;
   Event_Callback_Data   event_file_system_add;
   Event_Callback_Data   event_file_system_del;
   Event_Callback_Data   event_process_add;
   Event_Callback_Data   event_process_del;

   Event_Callback_Data   event_record_delay;

   Snapshot              snapshot;
   int                   changes;
};

typedef enum
{
   EVENT_CPU_ADD           = 1,
   EVENT_CPU_DEL           = 2,
   EVENT_BATTERY_ADD       = 3,
   EVENT_BATTERY_DEL       = 4,
   EVENT_POWER_SUPPLY_ADD  = 5,
   EVENT_POWER_SUPPLY_DEL  = 6,
   EVENT_SENSOR_ADD        = 7,
   EVENT_SENSOR_DEL        = 8,
   EVENT_NETWORK_IFACE_ADD = 9,
   EVENT_NETWORK_IFACE_DEL = 10,
   EVENT_FILE_SYSTEM_ADD   = 11,
   EVENT_FILE_SYSTEM_DEL   = 12,
   EVENT_PROCESS_ADD       = 13,
   EVENT_PROCESS_DEL       = 14,

   EVENT_RECORD_DELAY      = 15,
} Enigmatic_Client_Event_Type;

ENIGMATIC_API Enigmatic_Client *
enigmatic_client_open(void);

ENIGMATIC_API void
enigmatic_client_monitor_add(Enigmatic_Client *client, Snapshot_Callback *cb_event_change_init, Snapshot_Callback *cb_event_change, void *data);

ENIGMATIC_API void
enigmatic_client_event_callback_add(Enigmatic_Client *client, Enigmatic_Client_Event_Type type, Event_Callback *cb_event, void *data);

ENIGMATIC_API void
enigmatic_client_read(Enigmatic_Client *client);

ENIGMATIC_API Enigmatic_Client *
enigmatic_client_path_open(char *filename);

ENIGMATIC_API void
enigmatic_client_del(Enigmatic_Client *client);

ENIGMATIC_API Enigmatic_Client *
enigmatic_client_add(void);

ENIGMATIC_API void
enigmatic_client_follow_enabled_set(Enigmatic_Client *client, Eina_Bool enabled);

ENIGMATIC_API void
enigmatic_client_snapshot_callback_set(Enigmatic_Client *client, Snapshot_Callback *cb_event_change, void *data);

/* Some events occur multiple times between block end. We can check for a snapshot event to reduce
 * the polling granuality when we don't want sub-second data.
 */
ENIGMATIC_API Eina_Bool
enigmatic_client_event_is_snapshot(Enigmatic_Client *client);

ENIGMATIC_API void
enigmatic_client_replay_time_start_set(Enigmatic_Client *client, uint32_t secs);

ENIGMATIC_API void
enigmatic_client_replay_time_end_set(Enigmatic_Client *client, uint32_t secs);

ENIGMATIC_API Eina_Bool
enigmatic_client_replay(Enigmatic_Client *client);

ENIGMATIC_API Eina_Bool
enigmatic_client_time_bounds_get(Enigmatic_Client *client, uint32_t *start_time, uint32_t *end_time);

#endif
