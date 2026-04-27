#ifndef ENIGMATIC_MONITOR_POWER_H
#define ENIGMATIC_MONITOR_POWER_H

#include "Enigmatic.h"

Eina_Bool
enigmatic_monitor_power(Enigmatic *enigmatic EINA_UNUSED, Eina_Bool *ac_prev);

void
enigmatic_monitor_power_init(Enigmatic *enigmatic);

void
enigmatic_monitor_power_shutdown(void);

#endif
