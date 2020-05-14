#ifndef __UI_MEMORY_H__
#define __UI_MEMORY_H__

#include "ui.h"
#include "../system/machine.h"

void
ui_tab_memory_add(Ui *ui);

void
ui_tab_memory_update(Ui *ui, Sys_Info *sysinfo);

#endif
