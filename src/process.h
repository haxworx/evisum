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

#define CMD_NAME_MAX 256

typedef struct _Proc_Stats
{
   pid_t       pid;
   uid_t       uid;
   int8_t      nice;
   int8_t      priority;
   int         cpu_id;
   int32_t     numthreads;
   int64_t     mem_size;
   int64_t     mem_rss;
   double      cpu_usage;
   char        command[CMD_NAME_MAX];
   const char *state;

   // Not used yet in UI.
   long        cpu_time;
} Proc_Stats;

/**
 * Query a full list of running processes and return a list.
 *
 * @return A list of proc_t members for all processes.
 */
Eina_List *
proc_info_all_get(void);

/**
 * Query a process for its current state.
 *
 * @param pid The process ID to query.
 *
 * @return A proc_t pointer containing the process information.
 */
Proc_Stats *
proc_info_by_pid(int pid);

/**
 * @}
 */

#endif
