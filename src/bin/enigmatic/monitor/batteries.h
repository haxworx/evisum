#ifndef ENIGMATIC_MONITOR_BATTERIES_H
#define ENIGMATIC_MONITOR_BATTERIES_H

#include "Enigmatic.h"

Eina_Bool
enigmatic_monitor_batteries(Enigmatic *enigmatic, Eina_Hash **cache_hash);

void
enigmatic_monitor_batteries_init(void);

void
enigmatic_monitor_batteries_shutdown(void);

#endif
