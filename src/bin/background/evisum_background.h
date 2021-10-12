#ifndef EVISUM_BACKGROUND_H
#define EVISUM_BACKGROUND_H

#include "../ui/evisum_ui.h"
#include <Ecore.h>

void
background_poller_cb(void *data, Ecore_Thread *thread);

void
background_init(Evisum_Ui *ui);

#endif
