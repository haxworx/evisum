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
   int64_t     start_time;

   uint64_t    mem_size;
   uint64_t    mem_virt;
   uint64_t    mem_rss;
   uint64_t    mem_shared;

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

Eina_List *
proc_info_all_get(void);

Proc_Info *
proc_info_by_pid(int pid);

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
