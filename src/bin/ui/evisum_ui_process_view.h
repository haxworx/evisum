#ifndef __EVISUM_UI_PROCESS_H__
#define __EVISUM_UI_PROCESS_H__

#include "evisum_ui.h"

typedef enum
{
   PROC_VIEW_DEFAULT     = 0,
   PROC_VIEW_CHILDREN    = 1,
   PROC_VIEW_THREADS     = 2,
   PROC_VIEW_MANUAL      = 3,
} Evisum_Proc_Action;

void
evisum_ui_process_view_win_add(int pid, Evisum_Proc_Action action);

#endif
