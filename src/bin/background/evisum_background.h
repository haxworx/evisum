#ifndef EVISUM_BACKGROUND_H
#define EVISUM_BACKGROUND_H

#include "../ui/evisum_ui.h"
#include <Ecore.h>
#include <sys/types.h>

void evisum_background_poller(void *data, Ecore_Thread *thread);

void evisum_background_init(Evisum_Ui *ui);

void evisum_background_shutdown(Evisum_Ui *ui);

void evisum_background_proc_net_get(Evisum_Ui *ui, pid_t pid, uint64_t *net_in, uint64_t *net_out);

#endif
