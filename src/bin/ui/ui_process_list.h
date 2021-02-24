#ifndef __UI_PROCESS_LIST_H__
#define __UI_PROCESS_LIST_H__

#include "ui.h"

typedef enum
{
   PROC_SORT_BY_NONE,
   PROC_SORT_BY_CMD,
   PROC_SORT_BY_UID,
   PROC_SORT_BY_PID,
   PROC_SORT_BY_THREADS,
   PROC_SORT_BY_CPU,
   PROC_SORT_BY_PRI,
   PROC_SORT_BY_NICE,
   PROC_SORT_BY_SIZE,
   PROC_SORT_BY_RSS,
   PROC_SORT_BY_STATE,
   PROC_SORT_BY_TIME,
   PROC_SORT_BY_CPU_USAGE,
   PROC_SORT_BY_MAX,
} Proc_Sort;

typedef enum
{
   PROC_FIELD_CMD       = 1,
   PROC_FIELD_UID       = 2,
   PROC_FIELD_PID       = 3,
   PROC_FIELD_THREADS   = 4,
   PROC_FIELD_CPU       = 5,
   PROC_FIELD_PRI       = 6,
   PROC_FIELD_NICE      = 7,
   PROC_FIELD_SIZE      = 8,
   PROC_FIELD_RSS       = 9,
   PROC_FIELD_STATE     = 10,
   PROC_FIELD_TIME      = 11,
   PROC_FIELD_CPU_USAGE = 12,
   PROC_FIELD_MAX = 13,
} Proc_Field;

void
ui_process_list_win_add(Ui *ui);

#endif
