#if defined(__MACH__) && defined(__APPLE__)
# define __MacOS__
#endif

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/user.h>
# include <sys/proc.h>
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
# include <unistd.h>
# include <fcntl.h>
# include <kvm.h>
# include <limits.h>
# include <sys/proc.h>
# include <sys/param.h>
# include <sys/resource.h>
#endif

#if defined(__MacOS__)
# include <libproc.h>
# include <sys/proc_info.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>

#include "process.h"
#include <Eina.h>
#include <Ecore.h>
#include <Ecore_File.h>

#if defined(__linux__) && !defined(PF_KTHREAD)
# define PF_KTHREAD 0x00200000
#endif

static const char *
_process_state_name(char state)
{
   const char *statename = NULL;
#if defined(__linux__)

   switch (state)
     {
      case 'D':
        statename = "dsleep";
        break;

      case 'I':
        statename = "idle";
        break;

      case 'R':
        statename = "run";
        break;

      case 'S':
        statename = "sleep";
        break;

      case 'T':
      case 't':
        statename = "stop";
        break;

      case 'X':
        statename = "dead";
        break;

      case 'Z':
        statename = "zomb";
        break;
     }
#else
   switch (state)
     {
      case SIDL:
        statename = "idle";
        break;

      case SRUN:
        statename = "run";
        break;

      case SSLEEP:
        statename = "sleep";
        break;

      case SSTOP:
        statename = "stop";
        break;

#if !defined(__MacOS__)
#if !defined(__OpenBSD__)
      case SWAIT:
        statename = "wait";
        break;

      case SLOCK:
        statename = "lock";
        break;

#endif
      case SZOMB:
        statename = "zomb";
        break;

#endif
#if defined(__OpenBSD__)
      case SDEAD:
        statename = "dead";
        break;

      case SONPROC:
        statename = "onproc";
        break;
#endif
     }
#endif
   return statename;
}

#if defined(__linux__)

static unsigned long
_parse_line(const char *line)
{
   char *p, *tok;

   p = strchr(line, ':') + 1;
   while (isspace(*p))
     p++;
   tok = strtok(p, " ");

   return atol(tok);
}

static Eina_List *
_process_list_linux_get(void)
{
   Eina_List *files, *list;
   FILE *f;
   char *name, *link, state, line[4096], program_name[1024];
   int pid, res, utime, stime, cutime, cstime, uid, psr, pri, nice, numthreads;
   unsigned int mem_size, mem_rss, flags;
   int pagesize = getpagesize();

   list = NULL;

   files = ecore_file_ls("/proc");
   EINA_LIST_FREE(files, name)
     {
        pid = atoi(name);
        free(name);

        if (!pid) continue;

        f = fopen(eina_slstr_printf("/proc/%d/stat", pid), "r");
        if (!f) continue;

        if (fgets(line, sizeof(line), f))
          {
             int dummy;
             char *end, *start = strchr(line, '(') + 1;
             end = strchr(line, ')');
             strncpy(program_name, start, end - start);
             program_name[end - start] = '\0';
             res = sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                          &state, &dummy, &dummy, &dummy, &dummy, &dummy, &flags, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                          &pri, &nice, &numthreads, &dummy, &dummy, &mem_size, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                          &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
          }

        fclose(f);

        if (res != 42) continue;

        if (flags & PF_KTHREAD) continue;

        f = fopen(eina_slstr_printf("/proc/%d/status", pid), "r");
        if (!f) continue;

        while ((fgets(line, sizeof(line), f)) != NULL)
          {
             if (!strncmp(line, "Uid:", 4))
               {
                  uid = _parse_line(line);
                  break;
               }
          }

        fclose(f);

        link = ecore_file_readlink(eina_slstr_printf("/proc/%d/exe", pid));
        if (link)
          {
             snprintf(program_name, sizeof(program_name), "%s", ecore_file_file_get(link));
             free(link);
          }
        else
          {
             f = fopen(eina_slstr_printf("/proc/%d/cmdline", pid), "r");
             if (f)
               {
                  if (fgets(line, sizeof(line), f))
                    {
                       if (ecore_file_exists(line))
                         snprintf(program_name, sizeof(program_name), "%s", ecore_file_file_get(line));
                    }
                 fclose(f);
               }
          }


        char *end = strchr(program_name, ' ');
        if (end) *end = '\0';
        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = pid;
        p->uid = uid;
        p->cpu_id = psr;
        snprintf(p->command, sizeof(p->command), "%s", program_name);
        p->state = _process_state_name(state);
        p->cpu_time = utime + stime;
        p->mem_size = mem_size;
        p->mem_rss = mem_rss * pagesize;
        p->nice = nice;
        p->priority = pri;
        p->numthreads = numthreads;

        list = eina_list_append(list, p);
     }

   if (files)
     eina_list_free(files);

   return list;
}

Proc_Info *
proc_info_by_pid(int pid)
{
   FILE *f;
   char *link, state, line[4096], program_name[1024];
   int res, dummy, utime, stime, cutime, cstime, uid, psr;
   unsigned int mem_size, mem_rss, pri, nice, numthreads;

   f = fopen(eina_slstr_printf("/proc/%d/stat", pid), "r");
   if (!f) return NULL;

   if (fgets(line, sizeof(line), f))
     {
        char *end, *start = strchr(line, '(') + 1;
        end = strchr(line, ')');
        strncpy(program_name, start, end - start);
        program_name[end - start] = '\0';

        res = sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                     &state, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                     &pri, &nice, &numthreads, &dummy, &dummy, &mem_size, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                     &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
     }
   fclose(f);

   if (res != 42) return NULL;

   f = fopen(eina_slstr_printf("/proc/%d/status", pid), "r");
   if (!f) return NULL;

   while ((fgets(line, sizeof(line), f)) != NULL)
     {
        if (!strncmp(line, "Uid:", 4))
          {
             uid = _parse_line(line);
             break;
          }
     }
   fclose(f);

   link = ecore_file_readlink(eina_slstr_printf("/proc/%d/exe", pid));
   if (link)
     {
        snprintf(program_name, sizeof(program_name), "%s", ecore_file_file_get(link));
        free(link);
     }
   else
     {
        f = fopen(eina_slstr_printf("/proc/%d/cmdline", pid), "r");
        if (f)
          {
             if (fgets(line, sizeof(line), f))
               {
                  if (ecore_file_exists(line))
                    snprintf(program_name, sizeof(program_name), "%s", ecore_file_file_get(line));
               }
             fclose(f);
          }
      }

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = uid;
   p->cpu_id = psr;
   snprintf(p->command, sizeof(p->command), "%s", program_name);
   p->state = _process_state_name(state);
   p->cpu_time = utime + stime;
   p->mem_size = mem_size;
   p->mem_rss = mem_rss * getpagesize();
   p->priority = pri;
   p->nice = nice;
   p->numthreads = numthreads;

   return p;
}

#endif

#if defined(__OpenBSD__)

Proc_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc *kp;
   kvm_t *kern;
   char errbuf[_POSIX2_LINE_MAX];
   int count, pagesize, pid_count;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kp = kvm_getprocs(kern, KERN_PROC_PID, pid, sizeof(*kp), &count);
   if (!kp) return NULL;

   if (count == 0) return NULL;
   pagesize = getpagesize();

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = kp->p_pid;
   p->uid = kp->p_uid;
   p->cpu_id = kp->p_cpuid;
   snprintf(p->command, sizeof(p->command), "%s", kp->p_comm);
   p->state = _process_state_name(kp->p_stat);
   p->cpu_time = kp->p_uticks + kp->p_sticks + kp->p_iticks;
   p->mem_size = (kp->p_vm_tsize * pagesize) + (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
   p->mem_rss = kp->p_vm_rssize * pagesize;
   p->priority = kp->p_priority - PZERO;
   p->nice = kp->p_nice - NZERO;
   p->numthreads = -1;

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   for (int i = 0; i < pid_count; i++)
     {
        if (kp[i].p_pid == p->pid)
          p->numthreads++;
     }

   kvm_close(kern);

   return p;
}

static Eina_List *
_process_list_openbsd_get(void)
{
   struct kinfo_proc *kp;
   Proc_Info *p;
   char errbuf[4096];
   kvm_t *kern;
   int pid_count, pagesize;
   Eina_List *l, *list = NULL;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kp = kvm_getprocs(kern, KERN_PROC_ALL, 0, sizeof(*kp), &pid_count);
   if (!kp) return NULL;

   pagesize = getpagesize();

   for (int i = 0; i < pid_count; i++)
     {
        p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = kp[i].p_pid;
        p->uid = kp[i].p_uid;
        p->cpu_id = kp[i].p_cpuid;
        snprintf(p->command, sizeof(p->command), "%s", kp[i].p_comm);
        p->state = _process_state_name(kp[i].p_stat);
        p->cpu_time = kp[i].p_uticks + kp[i].p_sticks + kp[i].p_iticks;
        p->mem_size = (kp[i].p_vm_tsize * pagesize) + (kp[i].p_vm_dsize * pagesize) + (kp[i].p_vm_ssize * pagesize);
        p->mem_rss = kp[i].p_vm_rssize * pagesize;
        p->priority = kp[i].p_priority - PZERO;
        p->nice = kp[i].p_nice - NZERO;
        p->numthreads = -1;
        list = eina_list_append(list, p);
     }

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   EINA_LIST_FOREACH(list, l, p)
     {
        for (int i = 0; i < pid_count; i++)
          {
             if (kp[i].p_pid == p->pid)
               p->numthreads++;
          }
     }

   kvm_close(kern);

   return list;
}

#endif

#if defined(__MacOS__)
static Eina_List *
_process_list_macos_get(void)
{
   Eina_List *list = NULL;

   for (int i = 1; i <= PID_MAX; i++)
     {
        struct proc_taskallinfo taskinfo;
        int size = proc_pidinfo(i, PROC_PIDTASKALLINFO, 0, &taskinfo, sizeof(taskinfo));
        if (size != sizeof(taskinfo)) continue;

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = i;
        p->uid = taskinfo.pbsd.pbi_uid;
        p->cpu_id = -1;
        snprintf(p->command, sizeof(p->command), "%s", taskinfo.pbsd.pbi_comm);
        p->cpu_time = taskinfo.ptinfo.pti_total_user + taskinfo.ptinfo.pti_total_system;
        p->cpu_time /= 10000000;
        p->state = _process_state_name(taskinfo.pbsd.pbi_status);
        p->mem_size = taskinfo.ptinfo.pti_virtual_size;
        p->mem_rss = taskinfo.ptinfo.pti_resident_size;
        p->priority = taskinfo.ptinfo.pti_priority;
        p->nice = taskinfo.pbsd.pbi_nice;
        p->numthreads = taskinfo.ptinfo.pti_threadnum;

        list = eina_list_append(list, p);
     }

   return list;
}

Proc_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc kp;
   struct proc_taskallinfo taskinfo;
   struct proc_workqueueinfo workqueue;
   size_t size;

   size = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &taskinfo, sizeof(taskinfo));
   if (size != sizeof(taskinfo))
     return NULL;

   size = proc_pidinfo(pid, PROC_PIDWORKQUEUEINFO, 0, &workqueue, sizeof(workqueue));
   if (size != sizeof(workqueue))
     return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = taskinfo.pbsd.pbi_uid;
   p->cpu_id = workqueue.pwq_nthreads;
   snprintf(p->command, sizeof(p->command), "%s", taskinfo.pbsd.pbi_comm);
   p->cpu_time = taskinfo.ptinfo.pti_total_user + taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->state = _process_state_name(taskinfo.pbsd.pbi_status);
   p->mem_size = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
   p->priority = taskinfo.ptinfo.pti_priority;
   p->nice = taskinfo.pbsd.pbi_nice;
   p->numthreads = taskinfo.ptinfo.pti_threadnum;

   return p;
}

#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_List *
_process_list_freebsd_fallback_get(void)
{
   Eina_List *list;
   struct rusage *usage;
   struct kinfo_proc kp;
   int mib[4];
   size_t len;
   static int pagesize = 0;

   if (!pagesize)
     pagesize = getpagesize();

   list = NULL;

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   for (int i = 1; i <= PID_MAX; i++)
     {
        mib[3] = i;
        len = sizeof(kp);
        if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
          {
             continue;
          }

        if (kp.ki_flag & P_KPROC)
          continue;

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = kp.ki_pid;
        p->uid = kp.ki_uid;
        snprintf(p->command, sizeof(p->command), "%s", kp.ki_comm);
        p->cpu_id = kp.ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp.ki_lastcpu;

        usage = &kp.ki_rusage;

        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
        p->cpu_time /= 10000;
        p->state = _process_state_name(kp.ki_stat);
        p->mem_size = kp.ki_size;
        p->mem_rss = kp.ki_rssize * pagesize;
        p->nice = kp.ki_nice - NZERO;
        p->priority = kp.ki_pri.pri_level - PZERO;
        p->numthreads = kp.ki_numthreads;

        list = eina_list_append(list, p);
     }

   return list;
}

static Eina_List *
_process_list_freebsd_get(void)
{
   kvm_t *kern;
   Eina_List *list = NULL;
   struct kinfo_proc *kp;
   char errbuf[_POSIX2_LINE_MAX];
   int pid_count;

   kern = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
   if (!kern)
     return _process_list_freebsd_fallback_get();

   kp = kvm_getprocs(kern, KERN_PROC_PROC, 0, &pid_count);
   if (!kp) return _process_list_freebsd_fallback_get();

   for (int i = 0; i < pid_count; i++)
     {
        if (kp[i].ki_flag & P_KPROC)
          continue;

        Proc_Info *p = proc_info_by_pid(kp[i].ki_pid);
        if (!p) continue;

        list = eina_list_append(list, p);
     }

   return list;
}

Proc_Info *
proc_info_by_pid(int pid)
{
   struct rusage *usage;
   struct kinfo_proc kp;
   int mib[4];
   size_t len;
   static int pagesize = 0;

   if (!pagesize) pagesize = getpagesize();

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   mib[3] = pid;

   len = sizeof(kp);
   if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
     return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = kp.ki_pid;
   p->uid = kp.ki_uid;
   snprintf(p->command, sizeof(p->command), "%s", kp.ki_comm);
   p->cpu_id = kp.ki_oncpu;
   if (p->cpu_id == -1)
     p->cpu_id = kp.ki_lastcpu;

   usage = &kp.ki_rusage;

   p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
   p->cpu_time /= 10000;
   p->state = _process_state_name(kp.ki_stat);
   p->mem_size = kp.ki_size;
   p->mem_rss = kp.ki_rssize * pagesize;
   p->nice = kp.ki_nice - NZERO;
   p->priority = kp.ki_pri.pri_level - PZERO;
   p->numthreads = kp.ki_numthreads;

   return p;
}

#endif

Eina_List *
proc_info_all_get(void)
{
   Eina_List *processes;

#if defined(__linux__)
   processes = _process_list_linux_get();
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   processes = _process_list_freebsd_get();
#elif defined(__MacOS__)
   processes = _process_list_macos_get();
#elif defined(__OpenBSD__)
   processes = _process_list_openbsd_get();
#else
   processes = NULL;
#endif

   return processes;
}

