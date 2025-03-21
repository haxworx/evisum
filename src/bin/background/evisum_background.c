#include "evisum_background.h"
#include "../system/filesystems.h"

#include <Eina.h>

void
background_init(Evisum_Ui *ui)
{
   meminfo_t memory;

   system_memory_usage_get(&memory);
   ui->mem_total = memory.total;
   ui->mem_used = memory.used;
}

void
background_poller_cb(void *data, Ecore_Thread *thread)
{
   meminfo_t memory;
   Evisum_Ui *ui = data;

   while (!ecore_thread_check(thread))
     {
        int ncpu;
        double percent = 0.0;
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 250000);
        for (int i = 0; i < ncpu; i++)
          {
             percent += cores[i]->percent;
             free(cores[i]);
          }
        free(cores);

        system_memory_usage_get(&memory);
        ui->mem_used = memory.used;
        if (file_system_in_use("ZFS"))
          ui->mem_used += memory.zfs_arc_used;

        ui->cpu_usage = percent / system_cpu_online_count_get();
     }
}

