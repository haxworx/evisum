#ifndef __EVISUM_UI_PROCESS_LIST_H__
#define __EVISUM_UI_PROCESS_LIST_H__

#include "evisum_ui.h"

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
   PROC_SORT_BY_FILES,
   PROC_SORT_BY_SIZE,
   PROC_SORT_BY_VIRT,
   PROC_SORT_BY_RSS,
   PROC_SORT_BY_SHARED,
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
   PROC_FIELD_FILES     = 8,
   PROC_FIELD_SIZE      = 9,
   PROC_FIELD_VIRT      = 10,
   PROC_FIELD_RSS       = 11,
   PROC_FIELD_SHARED    = 12,
   PROC_FIELD_STATE     = 13,
   PROC_FIELD_TIME      = 14,
   PROC_FIELD_CPU_USAGE = 15,
   PROC_FIELD_MAX       = 16,
} Proc_Field;

void
evisum_ui_process_list_win_add(Evisum_Ui *ui);

#endif
