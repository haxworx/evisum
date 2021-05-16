#ifndef EVISUM_SYSTEM_H
#define EVISUM_SYSTEM_H

typedef struct _Evisum_System
{
   struct
   {
      int        count;
      Eina_List *cores;
      int        freq_min, freq_max;
      int        temp_min, temp_max;
   } cpu;

} Evisum_System;

#endif
