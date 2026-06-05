#ifndef UPTIME_H
#define UPTIME_H

#include <time.h>
#include "enigmatic_visibility.h"

ENIGMATIC_API time_t
enigmatic_system_uptime_get(void);

#endif
