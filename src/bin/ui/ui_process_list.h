#ifndef __UI_PROCESS_LIST_H__
#define __UI_PROCESS_LIST_H__

#include "ui.h"

typedef enum
{
   SORT_BY_NONE,
   SORT_BY_PID,
   SORT_BY_UID,
   SORT_BY_NICE,
   SORT_BY_PRI,
   SORT_BY_CPU,
   SORT_BY_THREADS,
   SORT_BY_SIZE,
   SORT_BY_RSS,
   SORT_BY_CMD,
   SORT_BY_TIME,
   SORT_BY_STATE,
   SORT_BY_CPU_USAGE,

   SORT_BY_MAX,
} Sort_Type;

void
ui_process_list_win_add(Ui *ui);

#endif
