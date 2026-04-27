#ifndef __PROC_H__
#define __PROC_H__

#include <Eina.h>
#include <stdint.h>
#include <unistd.h>

#if !defined(PID_MAX)
# define PID_MAX     99999
#endif

typedef struct _Proc_Info
{
   pid_t       pid;
   pid_t       ppid;
   uid_t       uid;
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   int64_t     cpu_time;
   double      cpu_usage;
   int64_t     run_time;
   int64_t     start;

   uint64_t    mem_size;
   uint64_t    mem_virt;
   uint64_t    mem_rss;
   uint64_t    mem_shared;
   uint64_t    net_in;
   uint64_t    net_out;
   uint64_t    disk_read;
   uint64_t    disk_write;

   char       *command;
   char       *arguments;
   char        state[32];
   char        wchan[32];

   Eina_List   *fds;
   int         numfiles;

   Eina_Bool   was_zero;
   Eina_Bool   is_kernel;
   Eina_Bool   is_new;
   int         tid;
   char       *thread_name;

   Eina_List  *threads;
   Eina_List  *children;
} Proc_Info;

#define PROC_INFO_LOG_COMMAND_SIZE      256
#define PROC_INFO_LOG_ARGUMENTS_SIZE    4096
#define PROC_INFO_LOG_STATE_SIZE        32
#define PROC_INFO_LOG_WCHAN_SIZE        32
#define PROC_INFO_LOG_THREAD_NAME_SIZE  64
#define PROC_INFO_LOG_PATH_SIZE         4096

typedef struct _Proc_Info_Log
{
   pid_t       pid;
   pid_t       ppid;
   uid_t       uid;
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   int64_t     cpu_time;
   double      cpu_usage;
   int64_t     run_time;
   int64_t     start;

   uint64_t    mem_size;
   uint64_t    mem_virt;
   uint64_t    mem_rss;
   uint64_t    mem_shared;
   uint64_t    net_in;
   uint64_t    net_out;
   uint64_t    disk_read;
   uint64_t    disk_write;

   char        command[PROC_INFO_LOG_COMMAND_SIZE];
   char        arguments[PROC_INFO_LOG_ARGUMENTS_SIZE];
   char        state[PROC_INFO_LOG_STATE_SIZE];
   char        wchan[PROC_INFO_LOG_WCHAN_SIZE];
   int         numfiles;

   Eina_Bool   was_zero;
   Eina_Bool   is_kernel;
   Eina_Bool   is_new;
   int         tid;
   char        thread_name[PROC_INFO_LOG_THREAD_NAME_SIZE];

   int32_t     fds_count;
   int32_t     threads_count;
   int32_t     children_count;

   char        path[PROC_INFO_LOG_PATH_SIZE];
} Proc_Info_Log;

Eina_List *
proc_info_all_get(void);

Proc_Info *
proc_info_by_pid(pid_t pid);

void
proc_info_free(Proc_Info *proc);

void
proc_info_kthreads_show_set(Eina_Bool enabled);

Eina_Bool
proc_info_kthreads_show_get(void);

Eina_List *
proc_info_all_children_get(void);

Eina_List *
proc_info_pid_children_get(pid_t pid);

void
proc_info_pid_children_free(Proc_Info *procs);

int
proc_sort_by_pid(const void *p1, const void *p2);

int
proc_sort_by_uid(const void *p1, const void *p2);

int
proc_sort_by_nice(const void *p1, const void *p2);

int
proc_sort_by_pri(const void *p1, const void *p2);

int
proc_sort_by_cpu(const void *p1, const void *p2);

int
proc_sort_by_threads(const void *p1, const void *p2);

int
proc_sort_by_files(const void *p1, const void *p2);

int
proc_sort_by_size(const void *p1, const void *p2);

int
proc_sort_by_virt(const void *p1, const void *p2);

int
proc_sort_by_rss(const void *p1, const void *p2);

int
proc_sort_by_shared(const void *p1, const void *p2);

int
proc_sort_by_net_in(const void *p1, const void *p2);

int
proc_sort_by_net_out(const void *p1, const void *p2);

int
proc_sort_by_disk_read(const void *p1, const void *p2);

int
proc_sort_by_disk_write(const void *p1, const void *p2);

int
proc_sort_by_time(const void *p1, const void *p2);

int
proc_sort_by_cpu_usage(const void *p1, const void *p2);

int
proc_sort_by_cmd(const void *p1, const void *p2);

int
proc_sort_by_state(const void *p1, const void *p2);

int
proc_sort_by_age(const void *p1, const void *p2);

#endif
