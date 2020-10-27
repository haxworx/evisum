#ifndef _EVISUM_H_
#define _EVISUM_H_

typedef enum
{
   EVISUM_ACTION_DEFAULT = 0,
   EVISUM_ACTION_PROCESS = 1,
   EVISUM_ACTION_CPU     = 2,
   EVISUM_ACTION_MEM     = 3,
   EVISUM_ACTION_STORAGE = 4,
   EVISUM_ACTION_SENSORS = 5,
} Evisum_Action;

#endif
