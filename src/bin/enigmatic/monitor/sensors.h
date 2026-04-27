#ifndef ENIGMATIC_MONITOR_SENSORS_H
#define ENIGMATIC_MONITOR_SENSORS_H

#include "Enigmatic.h"

Eina_Bool
enigmatic_monitor_sensors(Enigmatic *enigmatic, Eina_Hash **cache_hash);

void
enigmatic_monitor_sensors_init(void);

void
enigmatic_monitor_sensors_shutdown(void);

#endif
