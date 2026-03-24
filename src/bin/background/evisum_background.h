#ifndef EVISUM_BACKGROUND_H
#define EVISUM_BACKGROUND_H

#include "../ui/evisum_ui.h"
#include <Ecore.h>

void
evisum_background_poller(void *data, Ecore_Thread *thread);

void
evisum_background_init(Evisum_Ui *ui);

#endif
