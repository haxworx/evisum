#include "system/process.h"
#include "uid.h"
#include "enigmatic_log.h"
#include <ctype.h>
#include <string.h>

static void
cb_process_free(void *data)
{
   Proc_Info *proc = data;

   DEBUG("del pid => %i => %s", proc->pid, proc->command);

   proc_info_free(proc);
}

static int
cb_process_cmp(const void *a, const void *b)
{
   Proc_Info *p1, *p2;

   p1 = (Proc_Info *) a;
   p2 = (Proc_Info *) b;

   return p1->pid - p2->pid;
}

static void
proc_info_log_fill(const Proc_Info *proc, Proc_Info_Log *out)
{
   const char *src;
   size_t len = 0;

   if (!proc || !out) return;

   memset(out, 0, sizeof(*out));

   out->pid = proc->pid;
   out->ppid = proc->ppid;
   out->uid = proc->uid;
   out->nice = proc->nice;
   out->priority = proc->priority;
   out->cpu_id = proc->cpu_id;
   out->numthreads = proc->numthreads;
   out->cpu_time = proc->cpu_time;
   out->cpu_usage = proc->cpu_usage;
   out->run_time = proc->run_time;
   out->start = proc->start;
   out->mem_size = proc->mem_size;
   out->mem_virt = proc->mem_virt;
   out->mem_rss = proc->mem_rss;
   out->mem_shared = proc->mem_shared;
   out->net_in = proc->net_in;
   out->net_out = proc->net_out;
   out->disk_read = proc->disk_read;
   out->disk_write = proc->disk_write;
   out->numfiles = proc->numfiles;
   out->was_zero = proc->was_zero;
   out->is_kernel = proc->is_kernel;
   out->is_new = proc->is_new;
   out->tid = proc->tid;
   out->fds_count = eina_list_count(proc->fds);
   out->threads_count = eina_list_count(proc->threads);
   out->children_count = eina_list_count(proc->children);

   if (proc->command)
     snprintf(out->command, sizeof(out->command), "%s", proc->command);
   if (proc->arguments)
     snprintf(out->arguments, sizeof(out->arguments), "%s", proc->arguments);
   snprintf(out->state, sizeof(out->state), "%s", proc->state);
   snprintf(out->wchan, sizeof(out->wchan), "%s", proc->wchan);
   if (proc->thread_name)
     snprintf(out->thread_name, sizeof(out->thread_name), "%s", proc->thread_name);

   src = proc->arguments;
   if ((!src) || (!src[0]))
     src = proc->command;
   if (!src) return;

   while (src[len] && !isspace((unsigned char) src[len]))
     len++;

   if (!len)
     snprintf(out->path, sizeof(out->path), "%s", src);
   else
     {
        if (len >= sizeof(out->path))
          len = sizeof(out->path) - 1;
        memcpy(out->path, src, len);
        out->path[len] = '\0';
     }
}

void
enigmatic_log_process_list_write(Enigmatic *enigmatic, Eina_List *list)
{
   Proc_Info_Log *proc_log;
   int n;
   ssize_t len;
   Header *hdr;
   char *buf;

   n = eina_list_count(list);
   if (!n) return;

   len = sizeof(Header) + sizeof(Message);

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);
   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = EVENT_MESSAGE;

   Message *msg = (Message *) &buf[sizeof(Header)];
   msg->type = MESG_REFRESH;
   msg->object_type = PROCESS;
   msg->number = n;

   enigmatic_log_write(enigmatic, buf, len);

   EINA_LIST_FREE(list, proc_log)
     {
        enigmatic_log_write(enigmatic, (char *) proc_log, sizeof(Proc_Info_Log));
        free(proc_log);
     }
   free(buf);
}

static void
enigmatic_log_process_write(Enigmatic *enigmatic, Proc_Info_Log *proc_log)
{
   Header *hdr;
   char *buf;
   ssize_t len;

   len = sizeof(Header) + sizeof(Message);

   buf = malloc(len);
   EINA_SAFETY_ON_NULL_RETURN(buf);
   hdr = (Header *) &buf[0];
   hdr->time = enigmatic->poll_time;
   hdr->event = EVENT_MESSAGE;

   Message *msg = (Message *) &buf[sizeof(Header)];
   msg->type = MESG_ADD;
   msg->object_type = PROCESS;
   msg->number = 1;

   enigmatic_log_write(enigmatic, buf, len);

   enigmatic_log_write(enigmatic, (char *) proc_log, sizeof(Proc_Info_Log));
   free(buf);
}

static void
_process_log_delta(Enigmatic *enigmatic, pid_t pid, Object_Type object_type, int64_t delta, Eina_Bool *changed)
{
   Message msg;

   if (!delta) return;

   msg.type = MESG_MOD;
   msg.object_type = object_type;
   msg.number = pid;
   enigmatic_log_diff(enigmatic, msg, delta);
   *changed = 1;
}

static void
_process_log_string(Enigmatic *enigmatic, pid_t pid, Object_Type object_type, const char *value, Eina_Bool *changed)
{
   Message msg;
   Change change = CHANGE_STRING;
   const char *str = value ? value : "";

   msg.type = MESG_MOD;
   msg.object_type = object_type;
   msg.number = pid;
   enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
   enigmatic_log_write(enigmatic, (char *) &change, sizeof(change));
   enigmatic_log_write(enigmatic, str, strlen(str) + 1);
   *changed = 1;
}

static void
processes_refresh(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *ordered = NULL, *proc_logs = NULL;
   void *d = NULL;
   Proc_Info *proc;
   Proc_Info_Log *proc_log;
   int n;
   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);

   while (eina_iterator_next(it, &d))
     {
        proc = d;
        ordered = eina_list_append(ordered, proc);
     }
   eina_iterator_free(it);

   n = eina_list_count(ordered);
   if (!n) return;

   ordered = eina_list_sort(ordered, n, cb_process_cmp);
   EINA_LIST_FREE(ordered, proc)
     {
        proc_log = calloc(1, sizeof(Proc_Info_Log));
        if (!proc_log) continue;
        proc_info_log_fill(proc, proc_log);
        proc_logs = eina_list_append(proc_logs, proc_log);
      }
   enigmatic_log_process_list_write(enigmatic, proc_logs);
}

Eina_Bool
enigmatic_monitor_processes(Enigmatic *enigmatic, Eina_Hash **cache_hash)
{
   Eina_List *l, *processes;
   Proc_Info *proc, *p1;
   Eina_Bool changed = 0;

   processes = proc_info_all_get();
   if (!*cache_hash)
     {
        *cache_hash = eina_hash_int32_new(cb_process_free);
        EINA_LIST_FOREACH(processes, l, proc)
          {
             DEBUG("add pid => %i => %s", proc->pid, proc->command);
             proc->is_new = 1;
             int32_t pid = proc->pid;
             eina_hash_add(*cache_hash, &pid, proc);
          }
     }

   if (enigmatic->broadcast)
     {
        processes_refresh(enigmatic, cache_hash);
     }

   void *d = NULL;
   Eina_List *purge = NULL;

   Eina_Iterator *it = eina_hash_iterator_data_new(*cache_hash);
   while (eina_iterator_next(it, &d))
     {
        Proc_Info *p2 = d;
        Eina_Bool found = 0;
        EINA_LIST_FOREACH(processes, l, p1)
          {
             if ((p1->pid == p2->pid) && (p1->start == p2->start))
               {
                  found = 1;
                  break;
               }
          }
        if (!found)
          purge = eina_list_prepend(purge, p2);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(purge, proc)
     {
        Message msg;
        msg.type = MESG_DEL;
        msg.object_type = PROCESS;
        msg.number = proc->pid;
        enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);

        int32_t pid = proc->pid;
        eina_hash_del(*cache_hash, &pid, NULL);
      }

   EINA_LIST_FREE(processes, proc)
     {
        Proc_Info_Log old_log, new_log;
        int64_t cpu_time_delta, cpu_usage_now, cpu_usage_prev;

        int32_t pid = proc->pid;
        p1 = eina_hash_find(*cache_hash, &pid);
        if (!p1)
          {
             Proc_Info_Log proc_log;

             proc_info_log_fill(proc, &proc_log);
             enigmatic_log_process_write(enigmatic, &proc_log);

             DEBUG("add pid => %i => %s", proc->pid, proc->command);
             int32_t pid = proc->pid;
             eina_hash_add(*cache_hash, &pid, proc);
             continue;
          }

        proc_info_log_fill(p1, &old_log);
        proc_info_log_fill(proc, &new_log);

        cpu_time_delta = new_log.cpu_time - old_log.cpu_time;
        cpu_usage_prev = (int64_t) old_log.cpu_usage;
        cpu_usage_now = cpu_time_delta / enigmatic->interval;
        _process_log_delta(enigmatic, proc->pid, PROCESS_PPID, (int64_t) new_log.ppid - (int64_t) old_log.ppid, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_UID, (int64_t) new_log.uid - (int64_t) old_log.uid, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_NICE, new_log.nice - old_log.nice, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_PRIORITY, new_log.priority - old_log.priority, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_CPU_ID, new_log.cpu_id - old_log.cpu_id, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_NUM_THREAD, new_log.numthreads - old_log.numthreads, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_CPU_TIME, new_log.cpu_time - old_log.cpu_time, &changed);

        new_log.cpu_usage = cpu_usage_now;
        new_log.was_zero = !cpu_usage_now;
        proc->cpu_usage = cpu_usage_now;
        proc->was_zero = new_log.was_zero;

        _process_log_delta(enigmatic, proc->pid, PROCESS_RUN_TIME, new_log.run_time - old_log.run_time, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_START, new_log.start - old_log.start, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_MEM_SIZE, ((int64_t) (new_log.mem_size / 4096)) - ((int64_t) (old_log.mem_size / 4096)), &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_MEM_RSS, ((int64_t) (new_log.mem_rss / 4096)) - ((int64_t) (old_log.mem_rss / 4096)), &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_MEM_SHARED, ((int64_t) (new_log.mem_shared / 4096)) - ((int64_t) (old_log.mem_shared / 4096)), &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_MEM_VIRT, ((int64_t) (new_log.mem_virt / 4096)) - ((int64_t) (old_log.mem_virt / 4096)), &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_NET_IN, (int64_t) new_log.net_in - (int64_t) old_log.net_in, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_NET_OUT, (int64_t) new_log.net_out - (int64_t) old_log.net_out, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_DISK_READ, (int64_t) new_log.disk_read - (int64_t) old_log.disk_read, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_DISK_WRITE, (int64_t) new_log.disk_write - (int64_t) old_log.disk_write, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_NUM_FILES, new_log.numfiles - old_log.numfiles, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_WAS_ZERO, new_log.was_zero - old_log.was_zero, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_IS_KERNEL, new_log.is_kernel - old_log.is_kernel, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_IS_NEW, new_log.is_new - old_log.is_new, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_TID, new_log.tid - old_log.tid, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_FDS_COUNT, new_log.fds_count - old_log.fds_count, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_THREADS_COUNT, new_log.threads_count - old_log.threads_count, &changed);
        _process_log_delta(enigmatic, proc->pid, PROCESS_CHILDREN_COUNT, new_log.children_count - old_log.children_count, &changed);

        if (strcmp(new_log.command, old_log.command))
          _process_log_string(enigmatic, proc->pid, PROCESS_COMMAND, new_log.command, &changed);
        if (strcmp(new_log.arguments, old_log.arguments))
          _process_log_string(enigmatic, proc->pid, PROCESS_ARGUMENTS, new_log.arguments, &changed);
        if (strcmp(new_log.state, old_log.state))
          _process_log_string(enigmatic, proc->pid, PROCESS_STATE, new_log.state, &changed);
        if (strcmp(new_log.wchan, old_log.wchan))
          _process_log_string(enigmatic, proc->pid, PROCESS_WCHAN, new_log.wchan, &changed);
        if (strcmp(new_log.thread_name, old_log.thread_name))
          _process_log_string(enigmatic, proc->pid, PROCESS_THREAD_NAME, new_log.thread_name, &changed);
        if (strcmp(new_log.path, old_log.path))
          _process_log_string(enigmatic, proc->pid, PROCESS_PATH, new_log.path, &changed);
        if ((cpu_time_delta != 0) || !old_log.was_zero)
          _process_log_delta(enigmatic, proc->pid, PROCESS_CPU_USAGE, cpu_usage_now - cpu_usage_prev, &changed);

        if (!proc->is_new)
          {
             Proc_Info *prev = eina_hash_modify(*cache_hash, &pid, proc);
             if (prev)
               proc_info_free(prev);
          }
     }

   return changed;
}
