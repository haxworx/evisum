#ifndef __PROC_H__
#define __PROC_H__

/**
 * @file
 * @brief Routines for querying processes.
 */

/**
 * @brief Querying Processes
 * @defgroup Proc
 *
 * @{
 *
 * Query processes.
 *
 */

#include <Eina.h>
#include <stdint.h>
#include <unistd.h>

#if !defined(PID_MAX)
# define PID_MAX     99999
#endif

typedef struct _Proc_Info
{
   pid_t       pid;
   uid_t       uid;
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   long        cpu_time;
   double      cpu_usage;

   uint64_t    mem_size;
   uint64_t    mem_virt;
   uint64_t    mem_rss;
   uint64_t    mem_shared;

   char       *command;
   char       *arguments;
   const char *state;

   int         tid;
   char       *thread_name;

   Eina_List  *threads;
} Proc_Info;

/**
 * Query a full list of running processes and return a list.
 *
 * @return A list of Proc_Info pointers for all processes.
 */
Eina_List *
proc_info_all_get(void);

/**
 * Query a process for its current state.
 *
 * @param pid The process ID to query.
 *
 * @return A Proc_Info pointer containing the process information.
 */
Proc_Info *
proc_info_by_pid(int pid);

/**
 * Free a Proc_Info * pointer;
 *
 * @param proc The object to free.
 */
void
proc_info_free(Proc_Info *proc);


/**
 * Enable or disable the listing of kernel threads.
 *
 * @param enabled Boolean true or false (show/hide).
 */
void
proc_info_kthreads_show_set(Eina_Bool enabled);

/**
 * Return the current policy for listing kernel threads.
 *
 * @return True or false (enabled/disabled).
 */
Eina_Bool
proc_info_kthreads_show_get(void);

/**
 * @}
 */

#endif
