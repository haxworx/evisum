#ifndef EVISUM_BACKGROUND_H
#define EVISUM_BACKGROUND_H

#include "../ui/evisum_ui.h"
#include <Ecore.h>
#include <stdint.h>
#include <sys/types.h>

void evisum_background_poller(void *data, Ecore_Thread *thread);

void evisum_background_init(Evisum_Ui *ui);

void evisum_background_shutdown(Evisum_Ui *ui);

Eina_Bool evisum_background_update_wait(uint64_t *seq);

#endif
