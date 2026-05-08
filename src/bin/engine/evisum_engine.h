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
#include "enigmatic/Events.h"

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

/* Starts the engine if it is not already running. */
Eina_Bool evisum_engine_ensure_started(void);

/* Stops the engine and cleans up its resources. */
void evisum_engine_shutdown(void);

/* Gets the latest overall engine status. */
Eina_Bool evisum_engine_status_get(Evisum_Engine_Status *status);

/* Gets the number that changes when fresh engine data arrives. */
uint64_t evisum_engine_update_seq_get(void);

/* Waits until newer engine data is available. */
Eina_Bool evisum_engine_update_wait(uint64_t *seq);

/* Checks whether the background engine service is running. */
Eina_Bool evisum_engine_daemon_running_get(void);

/* Set the update interval of the background engine. */
Eina_Bool evisum_engine_interval_set(Interval interval);

/* Gets the process id of the background engine service. */
pid_t evisum_engine_daemon_pid_get(void);

/* Gets the first and last times available in history. */
Eina_Bool evisum_engine_history_bounds_get(uint32_t *start_time, uint32_t *end_time);

/* Gets the history range available after a given time. */
Eina_Bool evisum_engine_history_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time);

/* Gets the continuous history range available after a given time. */
Eina_Bool evisum_engine_history_contiguous_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time);

/* Checks whether history data exists for a given time. */
Eina_Bool evisum_engine_history_time_available_get(uint32_t time);

/* Switches the engine view to a chosen history time. */
Eina_Bool evisum_engine_history_time_set(uint32_t time);

/* Switches the engine view back to live data. */
void evisum_engine_history_live_set(void);

/* Checks whether the engine is currently showing live data. */
Eina_Bool evisum_engine_history_live_get(void);

/* Gets the history time currently being shown. */
uint32_t evisum_engine_history_time_get(void);

/* Gets the current live engine time. */
uint32_t evisum_engine_live_time_get(void);

/* Counts the CPU cores currently online. */
int system_cpu_online_count_get(void);

/* Counts all CPU cores known to the system. */
int system_cpu_count_get(void);

/* Gets the latest CPU usage for each core. */
Cpu_Core **system_cpu_usage_get(int *ncpu);

/* Gets CPU usage after waiting briefly, for a fresher sample. */
Cpu_Core **system_cpu_usage_delayed_get(int *ncpu, int usecs);

/* Gets the current state of each CPU core. */
Cpu_Core **system_cpu_state_get(int *ncpu);

/* Gets the current overall CPU frequency. */
int system_cpu_frequency_get(void);

/* Gets the current frequency for one CPU core. */
int system_cpu_n_frequency_get(int n);

/* Gets the current temperature for one CPU core. */
int system_cpu_n_temperature_get(int n);

/* Gets the lowest and highest CPU temperatures. */
int system_cpu_temperature_min_max_get(int *min, int *max);

/* Gets the lowest and highest CPU frequencies. */
int system_cpu_frequency_min_max_get(int *min, int *max);

/* Gets the CPU topology ids for the available cores. */
void system_cpu_topology_get(int *ids, int ncpus);

/* Gets the current memory usage. */
void system_memory_usage_get(Meminfo *memory);

/* Gets the available thermal sensor readings. */
Sensor **system_sensors_thermal_get(int *count);

/* Frees thermal sensor readings returned by the engine. */
void system_sensors_thermal_free(Sensor **sensors, int count);

/* Gets the current network interfaces. */
Network_Interface **system_network_ifaces_get(int *n);

/* Gets network traffic grouped by process. */
Proc_Net **system_network_process_usage_get(int *n);

/* Frees process network traffic data returned by the engine. */
void system_network_process_usage_free(Proc_Net **procs, int n);

/* Gets information about all visible processes. */
Eina_List *proc_info_all_get(void);

/* Gets information about one process. */
Proc_Info *proc_info_by_pid(pid_t pid);

/* Frees process information returned by the engine. */
void proc_info_free(Proc_Info *proc);

/* Sets whether kernel threads should be shown. */
void proc_info_kthreads_show_set(Eina_Bool enabled);

/* Checks whether kernel threads are shown. */
Eina_Bool proc_info_kthreads_show_get(void);

/* Gets all processes with their child relationships. */
Eina_List *proc_info_all_children_get(void);

/* Gets the child processes for one process. */
Eina_List *proc_info_pid_children_get(pid_t pid);

/* Frees child process information returned by the engine. */
void proc_info_pid_children_free(Proc_Info *procs);

/* Compares two processes by process id. */
int proc_sort_by_pid(const void *p1, const void *p2);

/* Compares two processes by user id. */
int proc_sort_by_uid(const void *p1, const void *p2);

/* Compares two processes by nice value. */
int proc_sort_by_nice(const void *p1, const void *p2);

/* Compares two processes by priority. */
int proc_sort_by_pri(const void *p1, const void *p2);

/* Compares two processes by CPU time. */
int proc_sort_by_cpu(const void *p1, const void *p2);

/* Compares two processes by thread count. */
int proc_sort_by_threads(const void *p1, const void *p2);

/* Compares two processes by open file count. */
int proc_sort_by_files(const void *p1, const void *p2);

/* Compares two processes by total size. */
int proc_sort_by_size(const void *p1, const void *p2);

/* Compares two processes by virtual memory size. */
int proc_sort_by_virt(const void *p1, const void *p2);

/* Compares two processes by resident memory size. */
int proc_sort_by_rss(const void *p1, const void *p2);

/* Compares two processes by shared memory size. */
int proc_sort_by_shared(const void *p1, const void *p2);

/* Compares two processes by incoming network traffic. */
int proc_sort_by_net_in(const void *p1, const void *p2);

/* Compares two processes by outgoing network traffic. */
int proc_sort_by_net_out(const void *p1, const void *p2);

/* Compares two processes by disk reads. */
int proc_sort_by_disk_read(const void *p1, const void *p2);

/* Compares two processes by disk writes. */
int proc_sort_by_disk_write(const void *p1, const void *p2);

/* Compares two processes by running time. */
int proc_sort_by_time(const void *p1, const void *p2);

/* Compares two processes by current CPU usage. */
int proc_sort_by_cpu_usage(const void *p1, const void *p2);

/* Compares two processes by command name. */
int proc_sort_by_cmd(const void *p1, const void *p2);

/* Compares two processes by current state. */
int proc_sort_by_state(const void *p1, const void *p2);

/* Compares two processes by age. */
int proc_sort_by_age(const void *p1, const void *p2);

/* Gets information about all file systems. */
Eina_List *file_system_info_all_get(void);

/* Frees file system information returned by the engine. */
void file_system_info_free(File_System *fs);

/* Checks whether a file system is currently in use. */
Eina_Bool file_system_in_use(const char *name);

#endif
