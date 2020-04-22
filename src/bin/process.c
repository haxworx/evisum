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
# include <libgen.h>
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

static void
_mem_size(Proc_Info *proc, int pid)
{
   FILE *f;
   char buf[1024];
   unsigned int dummy, size, shared, resident, data, text;

   f = fopen(eina_slstr_printf("/proc/%d/statm", pid), "r");
   if (!f) return;

   if (fgets(buf, sizeof(buf), f))
     {
        if (sscanf(buf, "%u %u %u %u %u %u %u", &size, &resident, &shared, &text,
                   &dummy, &data, &dummy) == 7)
          {
             proc->mem_size = (size * getpagesize()) - proc->mem_rss;
             proc->mem_shared = shared * getpagesize();
          }
     }

   fclose(f);
}

static void
_cmd_args(Proc_Info *p, int pid, char *name, size_t len)
{
   char line[4096];

   char *link = ecore_file_readlink(eina_slstr_printf("/proc/%d/exe", pid));
   if (link)
     {
        snprintf(name, len, "%s", ecore_file_file_get(link));
        free(link);
     }
   else
     {
        FILE *f = fopen(eina_slstr_printf("/proc/%d/cmdline", pid), "r");
        if (f)
          {
             if (fgets(line, sizeof(line), f))
               {
                  if (ecore_file_exists(line))
                    snprintf(name, len, "%s", ecore_file_file_get(line));
                  p->arguments = strdup(line);
               }
            fclose(f);
          }
     }

   char *end = strchr(name, ' ');
   if (end) *end = '\0';

   p->command = strdup(name);
}

static int
_uid(int pid)
{
   FILE *f;
   int uid;
   char line[1024];

   f = fopen(eina_slstr_printf("/proc/%d/status", pid), "r");
   if (!f) return -1;

   while ((fgets(line, sizeof(line), f)) != NULL)
     {
        if (!strncmp(line, "Uid:", 4))
          {
             uid = _parse_line(line);
             break;
          }
     }

   fclose(f);

   return uid;
}

static Eina_List *
_process_list_linux_get(void)
{
   Eina_List *files, *list;
   FILE *f;
   char *n;
   char state, line[4096], name[1024];
   int pid, res, utime, stime, cutime, cstime, psr, pri, nice, numthreads, dummy;
   unsigned int mem_virt, mem_rss, flags;
   int pagesize = getpagesize();

   res = 0;
   list = NULL;

   files = ecore_file_ls("/proc");
   EINA_LIST_FREE(files, n)
     {
        pid = atoi(n);
        free(n);

        if (!pid) continue;

        f = fopen(eina_slstr_printf("/proc/%d/stat", pid), "r");
        if (!f) continue;

        if (fgets(line, sizeof(line), f))
          {
             char *end, *start = strchr(line, '(') + 1;
             end = strchr(line, ')');

             strncpy(name, start, end - start);
             name[end - start] = '\0';

             res = sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                          &state, &dummy, &dummy, &dummy, &dummy, &dummy, &flags, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                          &pri, &nice, &numthreads, &dummy, &dummy, &mem_virt, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                          &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
          }
        fclose(f);

        if (res != 42) continue;
        if (flags & PF_KTHREAD) continue;

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = pid;
        p->uid = _uid(pid);
        p->cpu_id = psr;
        p->state = _process_state_name(state);
        p->cpu_time = utime + stime;
        p->nice = nice;
        p->priority = pri;
        p->numthreads = numthreads;

        p->mem_virt = mem_virt;
        p->mem_rss = mem_rss * pagesize;
        _mem_size(p, pid);

        _cmd_args(p, pid, name, sizeof(name));

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
   char line[4096], name[1024], state;
   int res, dummy, utime, stime, cutime, cstime, psr;
   unsigned int mem_virt, mem_rss, pri, nice, numthreads;

   f = fopen(eina_slstr_printf("/proc/%d/stat", pid), "r");
   if (!f) return NULL;

   if (fgets(line, sizeof(line), f))
     {
        char *end, *start = strchr(line, '(') + 1;
        end = strchr(line, ')');
        strncpy(name, start, end - start);
        name[end - start] = '\0';

        res = sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %d %u %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u %d %d %d %d %d %d %d %d %d",
                     &state, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &utime, &stime, &cutime, &cstime,
                     &pri, &nice, &numthreads, &dummy, &dummy, &mem_virt, &mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                     &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &psr, &dummy, &dummy, &dummy, &dummy, &dummy);
     }
   fclose(f);

   if (res != 42) return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = _uid(pid);
   p->cpu_id = psr;
   p->state = _process_state_name(state);
   p->cpu_time = utime + stime;

   p->mem_virt = mem_virt;
   p->mem_rss = mem_rss * getpagesize();
   _mem_size(p, pid);

   p->priority = pri;
   p->nice = nice;
   p->numthreads = numthreads;

   _cmd_args(p, pid, name, sizeof(name));

   return p;
}

#endif

#if defined(__OpenBSD__)

Proc_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc *kp;
   kvm_t *kern;
   char **args;
   char errbuf[_POSIX2_LINE_MAX];
   char name[1024];
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
   p->state = _process_state_name(kp->p_stat);
   p->cpu_time = kp->p_uticks + kp->p_sticks + kp->p_iticks;
   p->mem_virt = p->mem_size = (kp->p_vm_tsize * pagesize) + (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
   p->mem_rss = kp->p_vm_rssize * pagesize;
   p->mem_shared = kp->p_uru_ixrss;
   p->priority = kp->p_priority - PZERO;
   p->nice = kp->p_nice - NZERO;
   p->numthreads = -1;
   p->command = strdup(kp->p_comm);

   if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
     {
        Eina_Strbuf *buf = eina_strbuf_new();
        for (int i = 0; args[i]; i++)
          {
             eina_strbuf_append(buf, args[i]);
             eina_strbuf_append(buf, " ");
          }
        p->arguments = eina_strbuf_string_steal(buf);
        eina_strbuf_free(buf);
     }

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
   struct kinfo_proc *kps, *kp;
   Proc_Info *p;
   char **args;
   char errbuf[4096];
   char name[1024];
   kvm_t *kern;
   int pid_count, pagesize;
   Eina_List *list = NULL;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kps = kvm_getprocs(kern, KERN_PROC_ALL, 0, sizeof(*kps), &pid_count);
   if (!kps) return NULL;

   pagesize = getpagesize();

   for (int i = 0; i < pid_count; i++)
     {
        p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        kp = &kps[i];
        p->pid = kp->p_pid;
        p->uid = kp->p_uid;
        p->cpu_id = kp->p_cpuid;
        p->state = _process_state_name(kp->p_stat);
        p->cpu_time = kp->p_uticks + kp->p_sticks + kp->p_iticks;
        p->mem_size = p->mem_virt = (kp->p_vm_tsize * pagesize) + (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
        p->mem_rss = kp->p_vm_rssize * pagesize;
        p->mem_shared = kp->p_uru_ixrss;
        p->priority = kp->p_priority - PZERO;
        p->nice = kp->p_nice - NZERO;
        p->numthreads = -1;
        p->command = strdup(kp->p_comm);
        if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
          {
             Eina_Strbuf *buf = eina_strbuf_new();
             for (int i = 0; args[i]; i++)
               {
                  eina_strbuf_append(buf, args[i]);
                  eina_strbuf_append(buf, " ");
               }
             p->arguments = eina_strbuf_string_steal(buf);
             eina_strbuf_free(buf);
          }
        list = eina_list_append(list, p);
     }

   /* We don't need to count the threads for our usage in Evisum.

     If necessary this can be re-enabled. Our single process query is
     sufficient.

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   EINA_LIST_FOREACH(list, l, p)
     {
        for (int i = 0; i < pid_count; i++)
          {
             if (kp[i].p_pid == p->pid)
               p->numthreads++;
          }
     }
   */

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
        p->command = strdup(taskinfo.pbsd.pbi_comm);
        p->cpu_time = taskinfo.ptinfo.pti_total_user + taskinfo.ptinfo.pti_total_system;
        p->cpu_time /= 10000000;
        p->state = _process_state_name(taskinfo.pbsd.pbi_status);
        p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
        p->mem_rss = taskinfo.ptinfo.pti_resident_size;
        p->mem_shared = 0;
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
   p->command = strdup(taskinfo.pbsd.pbi_comm);
   p->cpu_time = taskinfo.ptinfo.pti_total_user + taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->state = _process_state_name(taskinfo.pbsd.pbi_status);
   p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
   p->mem_shared = 0;
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
        p->command = strdup(kp.ki_comm);
        p->cpu_id = kp.ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp.ki_lastcpu;

        usage = &kp.ki_rusage;

        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
        p->cpu_time /= 10000;
        p->state = _process_state_name(kp.ki_stat);
        p->mem_size = p->mem_virt = kp.ki_size;
        p->mem_rss = kp.ki_rssize * pagesize;
        p->mem_shared = kp.ki_rusage.ru_ixrss;
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
   struct kinfo_proc *kps, *kp;
   struct rusage *usage;
   char **args;
   char errbuf[_POSIX2_LINE_MAX];
   char name[1024];
   int pid_count;
   static int pagesize = 0;

   if (!pagesize) pagesize = getpagesize();

   kern = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
   if (!kern)
     return _process_list_freebsd_fallback_get();

   kps = kvm_getprocs(kern, KERN_PROC_PROC, 0, &pid_count);
   if (!kps)
     {
        kvm_close(kern);
        return _process_list_freebsd_fallback_get();
     }

   for (int i = 0; i < pid_count; i++)
     {
        Eina_Bool have_command = EINA_FALSE;

        if (kps[i].ki_flag & P_KPROC)
          continue;

        kp = &kps[i];

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) continue;
        p->pid = kp->ki_pid;
        p->uid = kp->ki_uid;

        p->cpu_id = kp->ki_oncpu;
        if (p->cpu_id == -1)
          p->cpu_id = kp->ki_lastcpu;

        if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
          {
             if (args[0])
               {
                  char *base = basename(args[0]);
                  if (base && base != args[0])
                    {
                       snprintf(name, sizeof(name), "%s", base);
                       char *spc = strchr(name, ' ');
                       if (!spc)
                         have_command = EINA_TRUE;
                    }
               }
             Eina_Strbuf *buf = eina_strbuf_new();
             for (int i = 0; args[i] != NULL; i++)
               {
                  eina_strbuf_append(buf, args[i]);
                  eina_strbuf_append(buf, " ");
               }
             p->arguments = eina_strbuf_string_steal(buf);
             eina_strbuf_free(buf);
          }

        if (!have_command)
         snprintf(name, sizeof(name), "%s", kp->ki_comm);

        p->command = strdup(name);

        usage = &kp->ki_rusage;
        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
        p->cpu_time /= 10000;
        p->state = _process_state_name(kp->ki_stat);
        p->mem_size = p->mem_virt = kp->ki_size;
        p->mem_rss = kp->ki_rssize * pagesize;
        p->mem_shared = kp->ki_rusage.ru_ixrss;
        p->nice = kp->ki_nice - NZERO;
        p->priority = kp->ki_pri.pri_level - PZERO;
        p->numthreads = kp->ki_numthreads;

        list = eina_list_append(list, p);
     }

   kvm_close(kern);

   return list;
}

static void
_cmd_get(Proc_Info *p, struct kinfo_proc *kp)
{
   kvm_t * kern;
   char **args;
   char name[1024];
   Eina_Bool have_command = EINA_FALSE;

   kern = kvm_open(NULL, "/dev/null", NULL, O_RDONLY, "kvm_open");
   if (kern != NULL)
     {
        if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
          {
             if (args[0])
               {
                  char *base = basename(args[0]);
                  if (base && base != args[0])
                    {
                       snprintf(name, sizeof(name), "%s", base);
                       char *spc = strchr(name, ' ');
                       if (!spc)
                         have_command = EINA_TRUE;
                    }
               }
             Eina_Strbuf *buf = eina_strbuf_new();
             for (int i = 0; args[i] != NULL; i++)
               {
                  eina_strbuf_append(buf, args[i]);
                  eina_strbuf_append(buf, " ");
               }
             p->arguments = eina_strbuf_string_steal(buf);
             eina_strbuf_free(buf);
          }
        kvm_close(kern);
     }

   if (!have_command)
     snprintf(name, sizeof(name), "%s", kp->ki_comm);

   p->command = strdup(name);
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
   _cmd_get(p, &kp);
   p->cpu_id = kp.ki_oncpu;
   if (p->cpu_id == -1)
     p->cpu_id = kp.ki_lastcpu;

   usage = &kp.ki_rusage;

   p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
   p->cpu_time /= 10000;
   p->state = _process_state_name(kp.ki_stat);
   p->mem_size = p->mem_virt = kp.ki_size;
   p->mem_rss = kp.ki_rssize * pagesize;
   p->mem_shared = kp.ki_rusage.ru_ixrss;
   p->nice = kp.ki_nice - NZERO;
   p->priority = kp.ki_pri.pri_level - PZERO;
   p->numthreads = kp.ki_numthreads;

   return p;
}

#endif

void
proc_info_free(Proc_Info *proc)
{
   if (proc->command)
     free(proc->command);
   if (proc->arguments)
     free(proc->arguments);
   free(proc);
}

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

