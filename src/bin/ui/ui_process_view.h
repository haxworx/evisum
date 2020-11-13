#ifndef __UI_PROCESS_H__
#define __UI_PROCESS_H__

#include "ui.h"

void
ui_process_win_add(Evas_Object *parent_win, int pid, const char *cmd, int poll_delay);

#endif
