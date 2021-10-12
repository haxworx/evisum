#include "evisum_background.h"
#include "../system/filesystems.h"

#include <Eina.h>

void
background_init(Evisum_Ui *ui)
{
   meminfo_t memory;
   power_t power;
   Battery *bat;

   system_memory_usage_get(&memory);
   ui->mem_total = memory.total;
   ui->mem_used = memory.used;

   system_power_state_get(&power);
   if (power.battery_count)
     {
        ui->have_power = power.have_ac;
        for (int i = 0; i < power.battery_count; i++)
          {
             if (!power.batteries[i]->present) continue;
             bat = calloc(1, sizeof(Battery));
             bat->index = i;
             snprintf(bat->model, sizeof(bat->model), "%s", power.batteries[i]->model);
             snprintf(bat->vendor, sizeof(bat->vendor), "%s", power.batteries[i]->vendor);
             bat->usage = power.batteries[i]->percent;
             ui->batteries = eina_list_append(ui->batteries, bat);
          }
     }
   system_power_state_free(&power);
}

void
background_poller_cb(void *data, Ecore_Thread *thread)
{
   meminfo_t memory;
   power_t power;
   int32_t poll_count = 0;
   Battery *bat;
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

        if ((!(poll_count % 4)) && (ui->batteries))
          {
             Eina_List *l;
             system_power_state_get(&power);
             ui->have_power = power.have_ac;
             for (int i = 0; i < power.battery_count; i++)
               {
                  if (!power.batteries[i]->present) continue;
                  l = eina_list_nth_list(ui->batteries, i);
                  if (!l) continue;
                  bat = eina_list_data_get(l);
                  bat->usage = power.batteries[i]->percent;
               }
             system_power_state_free(&power);
          }

        poll_count++;
     }

   EINA_LIST_FREE(ui->batteries, bat)
     free(bat);
}

