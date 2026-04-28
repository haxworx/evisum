#ifndef EVISUM_ENGINE_H
#define EVISUM_ENGINE_H

#include <Eina.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include "enigmatic/system/machine.h"
#include "enigmatic/system/file_systems.h"
#include "enigmatic/system/process.h"

typedef struct {
    pid_t pid;
    uint64_t in;
    uint64_t out;
} Proc_Net;

typedef struct {
    double cpu_usage;
    Meminfo memory;
    Eina_Bool zfs_mounted;
} Evisum_Engine_Status;

Eina_Bool evisum_engine_ensure_started(void);
void evisum_engine_shutdown(void);
Eina_Bool evisum_engine_status_get(Evisum_Engine_Status *status);
uint64_t evisum_engine_update_seq_get(void);
Eina_Bool evisum_engine_update_wait(uint64_t *seq);
Eina_Bool evisum_engine_daemon_running_get(void);
pid_t evisum_engine_daemon_pid_get(void);
Eina_Bool evisum_engine_history_bounds_get(uint32_t *start_time, uint32_t *end_time);
Eina_Bool evisum_engine_history_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time);
Eina_Bool evisum_engine_history_contiguous_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time);
Eina_Bool evisum_engine_history_time_set(uint32_t time);
void evisum_engine_history_live_set(void);
Eina_Bool evisum_engine_history_live_get(void);
uint32_t evisum_engine_history_time_get(void);
uint32_t evisum_engine_live_time_get(void);

int system_cpu_online_count_get(void);
int system_cpu_count_get(void);
Cpu_Core **system_cpu_usage_get(int *ncpu);
Cpu_Core **system_cpu_usage_delayed_get(int *ncpu, int usecs);
Cpu_Core **system_cpu_state_get(int *ncpu);
int system_cpu_frequency_get(void);
int system_cpu_n_frequency_get(int n);
int system_cpu_n_temperature_get(int n);
int system_cpu_temperature_min_max_get(int *min, int *max);
int system_cpu_frequency_min_max_get(int *min, int *max);
void system_cpu_topology_get(int *ids, int ncpus);
void system_memory_usage_get(Meminfo *memory);
Sensor **system_sensors_thermal_get(int *count);
void system_sensors_thermal_free(Sensor **sensors, int count);
Network_Interface **system_network_ifaces_get(int *n);
Proc_Net **system_network_process_usage_get(int *n);
void system_network_process_usage_free(Proc_Net **procs, int n);

Eina_List *proc_info_all_get(void);
Proc_Info *proc_info_by_pid(pid_t pid);
void proc_info_free(Proc_Info *proc);
void proc_info_kthreads_show_set(Eina_Bool enabled);
Eina_Bool proc_info_kthreads_show_get(void);
Eina_List *proc_info_all_children_get(void);
Eina_List *proc_info_pid_children_get(pid_t pid);
void proc_info_pid_children_free(Proc_Info *procs);

int proc_sort_by_pid(const void *p1, const void *p2);
int proc_sort_by_uid(const void *p1, const void *p2);
int proc_sort_by_nice(const void *p1, const void *p2);
int proc_sort_by_pri(const void *p1, const void *p2);
int proc_sort_by_cpu(const void *p1, const void *p2);
int proc_sort_by_threads(const void *p1, const void *p2);
int proc_sort_by_files(const void *p1, const void *p2);
int proc_sort_by_size(const void *p1, const void *p2);
int proc_sort_by_virt(const void *p1, const void *p2);
int proc_sort_by_rss(const void *p1, const void *p2);
int proc_sort_by_shared(const void *p1, const void *p2);
int proc_sort_by_net_in(const void *p1, const void *p2);
int proc_sort_by_net_out(const void *p1, const void *p2);
int proc_sort_by_disk_read(const void *p1, const void *p2);
int proc_sort_by_disk_write(const void *p1, const void *p2);
int proc_sort_by_time(const void *p1, const void *p2);
int proc_sort_by_cpu_usage(const void *p1, const void *p2);
int proc_sort_by_cmd(const void *p1, const void *p2);
int proc_sort_by_state(const void *p1, const void *p2);
int proc_sort_by_age(const void *p1, const void *p2);

Eina_List *file_system_info_all_get(void);
void file_system_info_free(File_System *fs);
Eina_Bool file_system_in_use(const char *name);

#endif
