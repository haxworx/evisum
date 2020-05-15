#if defined(__MACH__) && defined(__APPLE__)
# define __MacOS__
#endif

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <sys/user.h>
# include <sys/proc.h>
# include <libgen.h>
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
_mem_size(Proc_Info *proc)
{
   FILE *f;
   char buf[1024];
   unsigned int dummy, size, shared, resident, data, text;

   f = fopen(eina_slstr_printf("/proc/%d/statm", proc->pid), "r");
   if (!f) return;

   if (fgets(buf, sizeof(buf), f))
     {
        if (sscanf(buf, "%u %u %u %u %u %u %u",
                   &size, &resident, &shared, &text,
                   &dummy, &data, &dummy) == 7)
          {
             proc->mem_rss = resident * getpagesize();
             proc->mem_shared = shared * getpagesize();
             proc->mem_size = proc->mem_rss - proc->mem_shared;
             proc->mem_virt = size * getpagesize();
          }
     }

   fclose(f);
}

static void
_cmd_args(Proc_Info *p, char *name, size_t len)
{
   char line[4096];
   int pid = p->pid;

   char *link = ecore_file_readlink(eina_slstr_printf("/proc/%d/exe", pid));
   if (link)
     {
        snprintf(name, len, "%s", ecore_file_file_get(link));
        free(link);
     }

   FILE *f = fopen(eina_slstr_printf("/proc/%d/cmdline", pid), "r");
   if (f)
     {
        if (fgets(line, sizeof(line), f))
          {
             Eina_Strbuf *buf = eina_strbuf_new();
             const char *n;

             if (ecore_file_exists(line))
               snprintf(name, len, "%s", ecore_file_file_get(line));

             n = line;
             while (*n && (*n + 1))
               {
                  eina_strbuf_append(buf, n);
                  n = strchr(n, '\0') + 1;
                  if (*n && (*n + 1)) eina_strbuf_append(buf, " ");
               }
             p->arguments = eina_strbuf_release(buf);
          }
        fclose(f);
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

typedef struct {
   int pid, utime, stime, cutime, cstime;
   int psr, pri, nice, numthreads;
   char state;
   unsigned int mem_rss, flags;
   unsigned long mem_virt;
   char name[1024];
} Stat;

static Eina_Bool
_stat(const char *path, Stat *st)
{
   FILE *f;
   char line[4096];
   int dummy, res = 0;

   memset(st, 0, sizeof(Stat));

   f = fopen(path, "r");
   if (!f) return EINA_FALSE;

   if (fgets(line, sizeof(line), f))
     {
        char *end, *start = strchr(line, '(') + 1;
        end = strchr(line, ')');

        strncpy(st->name, start, end - start);
        st->name[end - start] = '\0';
        res = sscanf(end + 2, "%c %d %d %d %d %d %u %u %u %u %u %d %d %d"
              " %d %d %d %u %u %d %lu %u %u %u %u %u %u %u %d %d %d %d %u"
              " %d %d %d %d %d %d %d %d %d",
              &st->state, &dummy, &dummy, &dummy, &dummy, &dummy, &st->flags,
              &dummy, &dummy, &dummy, &dummy, &st->utime, &st->stime, &st->cutime,
              &st->cstime, &st->pri, &st->nice, &st->numthreads, &dummy, &dummy,
              &st->mem_virt, &st->mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy,
              &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
              &dummy, &dummy, &st->psr, &dummy, &dummy, &dummy, &dummy, &dummy);
     }
   fclose(f);

   if (res != 42) return EINA_FALSE;

   return EINA_TRUE;
}

static Eina_List *
_process_list_linux_get(void)
{
   Eina_List *files, *list;
   char *n;
   Stat st;

   list = NULL;

   files = ecore_file_ls("/proc");
   EINA_LIST_FREE(files, n)
     {
        int pid = atoi(n);
        free(n);

        if (!pid) continue;

        if (!_stat(eina_slstr_printf("/proc/%d/stat", pid), &st))
          continue;

        if (st.flags & PF_KTHREAD) continue;

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = pid;
        p->uid = _uid(pid);
        p->cpu_id = st.psr;
        p->state = _process_state_name(st.state);
        p->cpu_time = st.utime + st.stime;
        p->nice = st.nice;
        p->priority = st.pri;
        p->numthreads = st.numthreads;
        _mem_size(p);
        _cmd_args(p, st.name, sizeof(st.name));

        list = eina_list_append(list, p);
     }

   if (files)
     eina_list_free(files);

   return list;
}

static void
_proc_thread_info(Proc_Info *p)
{
   Eina_List *files;
   char *n;
   Stat st;

   files = ecore_file_ls(eina_slstr_printf("/proc/%d/task", p->pid));
   EINA_LIST_FREE(files, n)
     {
        int tid = atoi(n);
        free(n);
        if (!_stat(eina_slstr_printf("/proc/%d/task/%d/stat", p->pid, tid), &st))
          continue;

        Proc_Info *t = calloc(1, sizeof(Proc_Info));
        if (!t) continue;
        t->cpu_id = st.psr;
        t->state = _process_state_name(st.state);
        t->cpu_time = st.utime + st.stime;
        t->nice = st.nice;
        t->priority = st.pri;
        t->numthreads = st.numthreads;
        t->mem_virt = st.mem_virt;
        t->mem_rss = st.mem_rss;

        t->tid = tid;
        t->thread_name = strdup(st.name);

        p->threads = eina_list_append(p->threads, t);
     }

   if (files)
     eina_list_free(files);
}

Proc_Info *
proc_info_by_pid(int pid)
{
   Stat st;

   if (!_stat(eina_slstr_printf("/proc/%d/stat", pid), &st))
     return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = _uid(pid);
   p->cpu_id = st.psr;
   p->state = _process_state_name(st.state);
   p->cpu_time = st.utime + st.stime;
   p->priority = st.pri;
   p->nice = st.nice;
   p->numthreads = st.numthreads;
   _mem_size(p);
   _cmd_args(p, st.name, sizeof(st.name));

   _proc_thread_info(p);

   return p;
}

#endif

#if defined(__OpenBSD__)

Proc_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc *kp, *kpt;
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
   p->mem_virt = p->mem_size = (kp->p_vm_tsize * pagesize) +
      (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
   p->mem_rss = kp->p_vm_rssize * pagesize;
   p->mem_shared = kp->p_uru_ixrss;
   p->priority = kp->p_priority - PZERO;
   p->nice = kp->p_nice - NZERO;

   if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
     {
        Eina_Strbuf *buf = eina_strbuf_new();
        for (int i = 0; args[i]; i++)
          {
             eina_strbuf_append(buf, args[i]);
             if (args[i + 1])
               eina_strbuf_append(buf, " ");
          }
        p->arguments = eina_strbuf_string_steal(buf);
        eina_strbuf_free(buf);

        if (args[0] && ecore_file_exists(args[0]))
          p->command = strdup(ecore_file_file_get(args[0]));
     }

   if (!p->command)
     p->command = strdup(kp->p_comm);

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   for (int i = 0; i < pid_count; i++)
     {
        if (kp[i].p_pid != p->pid) continue;

        kpt = &kp[i];
        p->numthreads++;

        Proc_Info *t = calloc(1, sizeof(Proc_Info));
        if (!t) continue;

        t->pid = kpt->p_pid;
        t->uid = kpt->p_uid;
        t->cpu_id = kpt->p_cpuid;
        t->state = _process_state_name(kpt->p_stat);
        t->cpu_time = kpt->p_uticks + kpt->p_sticks + kpt->p_iticks;
        t->mem_virt = p->mem_size = (kpt->p_vm_tsize * pagesize) +
           (kpt->p_vm_dsize * pagesize) + (kpt->p_vm_ssize * pagesize);
        t->mem_rss = kpt->p_vm_rssize * pagesize;
        t->mem_shared = kpt->p_uru_ixrss;
        t->priority = kpt->p_priority - PZERO;
        t->nice = kpt->p_nice - NZERO;

        int tid = kpt->p_tid;
        if (tid < 0) tid = 0;
        t->command = strdup(eina_slstr_printf("%s:%d", kpt->p_comm, tid));

        p->threads = eina_list_append(p->threads, t);
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
        p->mem_size = p->mem_virt = (kp->p_vm_tsize * pagesize) +
           (kp->p_vm_dsize * pagesize) + (kp->p_vm_ssize * pagesize);
        p->mem_rss = kp->p_vm_rssize * pagesize;
        p->mem_shared = kp->p_uru_ixrss;
        p->priority = kp->p_priority - PZERO;
        p->nice = kp->p_nice - NZERO;

        if ((args = kvm_getargv(kern, kp, sizeof(name)-1)))
          {
             Eina_Strbuf *buf = eina_strbuf_new();
             for (int i = 0; args[i]; i++)
               {
                  eina_strbuf_append(buf, args[i]);
                  if (args[i + 1])
                    eina_strbuf_append(buf, " ");
               }
             p->arguments = eina_strbuf_string_steal(buf);
             eina_strbuf_free(buf);

             if (args[0] && ecore_file_exists(args[0]))
                p->command = strdup(ecore_file_file_get(args[0]));
          }
        if (!p->command)
          p->command = strdup(kp->p_comm);

        list = eina_list_append(list, p);
     }

   kvm_close(kern);

   return list;
}

#endif

#if defined(__MacOS__)
static void
_cmd_get(Proc_Info *p, int pid)
{
   char *cp, *args, **argv;
   int mib[3], argmax, argc;
   size_t size;

   mib[0] = CTL_KERN;
   mib[1] = KERN_ARGMAX;

   size = sizeof(argmax);

   if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1) return;

   mib[0] = CTL_KERN;
   mib[1] = KERN_PROCARGS2;
   mib[2] = pid;

   size = (size_t) argmax;
   args = malloc(argmax);
   if (!args) return;

   /*
    * Make a sysctl() call to get the raw argument space of the process.
    * The layout is documented in start.s, which is part of the Csu
    * project.  In summary, it looks like:
    *
    * /---------------\ 0x00000000
    * :               :
    * :               :
    * |---------------|
    * | argc          |
    * |---------------|
    * | arg[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | arg[argc - 1] |
    * |---------------|
    * | 0             |
    * |---------------|
    * | env[0]        |
    * |---------------|
    * :               :
    * :               :
    * |---------------|
    * | env[n]        |
    * |---------------|
    * | 0             |
    * |---------------| <-- Beginning of data returned by sysctl() is here.
    * | argc          |
    * |---------------|
    * | exec_path     |
    * |:::::::::::::::|
    * |               |
    * | String area.  |
    * |               |
    * |---------------| <-- Top of stack.
    * :               :
    * :               :
    * \---------------/ 0xffffffff
    */

   if (sysctl(mib, 3, args, &size, NULL, 0) == -1) return;

   memcpy(&argc, args, sizeof(argc));
   cp = args + sizeof(argc);

   /* Skip exec path */
   for (;cp < &args[size]; cp++)
     {
        if (*cp == '\0') break;
     }

   if (cp == &args[size]) return;

   /* Skip any padded NULLs. */
   for (;cp < &args[size]; cp++)
     {
        if (*cp == '\0') break;
     }

   if (cp == &args[size]) return;

   argv = malloc(1 + argc * sizeof(char *));
   if (!argv) return;

   int i = 0;
   argv[i] = cp;

   for (cp = args + sizeof(int); cp < &args[size] && i < argc; cp++)
     {
        if (*cp == '\0')
          {
             while (*cp == '\0') cp++;
             argv[i++] = cp;
          }
     }

   if (i == 0) i++;

   argv[i] = NULL;

   p->command = strdup(basename(argv[0]));

   Eina_Strbuf *buf = eina_strbuf_new();

   for (i = 0; i < argc; i++)
     eina_strbuf_append_printf(buf, "%s ", argv[i]);

   if (argc > 0)
     p->arguments = eina_strbuf_release(buf);
   else
     eina_strbuf_free(buf);

   free(args);
   free(argv);
}

static Proc_Info *
_proc_pidinfo(size_t pid)
{
   struct proc_taskallinfo taskinfo;
   int size = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &taskinfo, sizeof(taskinfo));
   if (size != sizeof(taskinfo)) return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = taskinfo.pbsd.pbi_uid;
   p->cpu_id = -1;
   p->cpu_time = taskinfo.ptinfo.pti_total_user +
      taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->state = _process_state_name(taskinfo.pbsd.pbi_status);
   p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
   p->mem_shared = 0;
   p->priority = taskinfo.ptinfo.pti_priority;
   p->nice = taskinfo.pbsd.pbi_nice;
   p->numthreads = taskinfo.ptinfo.pti_threadnum;
   _cmd_get(p, pid);

   return p;
}

static Eina_List *
_process_list_macos_fallback_get(void)
{
   Eina_List *list = NULL;

   for (int i = 1; i <= PID_MAX; i++)
     {
        Proc_Info *p = _proc_pidinfo(i);
        if (p)
          list = eina_list_append(list, p);
     }

   return list;
}

static Eina_List *
_process_list_macos_get(void)
{
   Eina_List *list = NULL;
   pid_t *pids = NULL;
   int size, pid_count;

   size = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
   if (size == -1)
     return _process_list_macos_fallback_get();

   pids = malloc(size * sizeof(pid_t));
   if (!pids) return NULL;

   size = proc_listpids(PROC_ALL_PIDS, 0, pids, size * sizeof(pid_t));
   if (size == -1)
     {
        free(pids);
        return _process_list_macos_fallback_get();
     }

   pid_count = size / sizeof(pid_t);
   for (int i = 0; i < pid_count; i++)
     {
        pid_t pid = pids[i];
        Proc_Info *p = _proc_pidinfo(pid);
        if (p)
          list = eina_list_append(list, p);
     }

   free(pids);

   return list;
}

Proc_Info *
proc_info_by_pid(int pid)
{
   struct proc_taskallinfo taskinfo;
   struct proc_workqueueinfo workqueue;
   size_t size;

   size = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &taskinfo, sizeof(taskinfo));
   if (size != sizeof(taskinfo))
     return NULL;

   size = proc_pidinfo(pid, PROC_PIDWORKQUEUEINFO, 0, &workqueue, sizeof(workqueue));
   if (size != sizeof(workqueue))
     memset(&workqueue, 0, sizeof(struct proc_workqueueinfo));

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->uid = taskinfo.pbsd.pbi_uid;
   p->cpu_id = workqueue.pwq_nthreads;
   p->cpu_time = taskinfo.ptinfo.pti_total_user +
      taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->state = _process_state_name(taskinfo.pbsd.pbi_status);
   p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
   p->mem_shared = 0;
   p->priority = taskinfo.ptinfo.pti_priority;
   p->nice = taskinfo.pbsd.pbi_nice;
   p->numthreads = taskinfo.ptinfo.pti_threadnum;
   _cmd_get(p, pid);

   return p;
}

#endif

#if defined(__FreeBSD__) || defined(__DragonFly__)

static int
_pid_max(void)
{
   size_t len;
   static int pid_max = 0;

   if (pid_max != 0) return pid_max;

   len = sizeof(pid_max);
   if (sysctlbyname("kern.pid_max", &pid_max, &len, NULL, 0) == -1)
     {
#if defined(__FreeBSD__)
        pid_max = 99999;
#elif defined(__DragonFly__)
        pid_max = 999999;
#else
        pid_max = PID_MAX;
#endif
     }

   return pid_max;
}

static Eina_List *
_process_list_freebsd_fallback_get(void)
{
   Eina_List *list;
   struct rusage *usage;
   struct kinfo_proc kp;
   int mib[4];
   size_t len;
   static int pid_max, pagesize = 0;

   if (!pagesize)
     pagesize = getpagesize();

   pid_max = _pid_max();

   list = NULL;

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   for (int i = 1; i <= pid_max; i++)
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

        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) +
           usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) +
           usage->ru_stime.tv_usec;
        p->cpu_time /= 10000;
        p->state = _process_state_name(kp.ki_stat);
        p->mem_virt = kp.ki_size;
        p->mem_rss = kp.ki_rssize * pagesize;
        p->mem_size = p->mem_virt;
        p->nice = kp.ki_nice - NZERO;
        p->priority = kp.ki_pri.pri_level - PZERO;
        p->numthreads = kp.ki_numthreads;

        list = eina_list_append(list, p);
     }

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
                  char *base = strdup(args[0]);
                  if (base)
                    {
                       char *spc = strchr(base, ' ');
                       if (spc) *spc = '\0';

                       if (ecore_file_exists(base))
                         {
                            snprintf(name, sizeof(name), "%s", basename(base));
                            have_command = EINA_TRUE;
                         }
                       free(base);
                    }
               }
             Eina_Strbuf *buf = eina_strbuf_new();
             for (int i = 0; args[i] != NULL; i++)
               {
                  eina_strbuf_append(buf, args[i]);
                  if (args[i + 1])
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


static Eina_List *
_process_list_freebsd_get(void)
{
   kvm_t *kern;
   Eina_List *list = NULL;
   struct kinfo_proc *kps, *kp;
   struct rusage *usage;
   char errbuf[_POSIX2_LINE_MAX];
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

        _cmd_get(p, kp);

        usage = &kp->ki_rusage;
        p->cpu_time = (usage->ru_utime.tv_sec * 1000000) +
           usage->ru_utime.tv_usec + (usage->ru_stime.tv_sec * 1000000) +
           usage->ru_stime.tv_usec;
        p->cpu_time /= 10000;
        p->state = _process_state_name(kp->ki_stat);
        p->mem_virt = kp->ki_size;
        p->mem_rss = kp->ki_rssize * pagesize;
        p->mem_size = p->mem_virt;
        p->nice = kp->ki_nice - NZERO;
        p->priority = kp->ki_pri.pri_level - PZERO;
        p->numthreads = kp->ki_numthreads;

        list = eina_list_append(list, p);
     }

   kvm_close(kern);

   return list;
}

static Proc_Info *
_proc_info_by_pid_fallback(int pid)
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

   p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec +
      (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
   p->cpu_time /= 10000;
   p->state = _process_state_name(kp.ki_stat);
   p->mem_virt = kp.ki_size;
   p->mem_rss = kp.ki_rssize * pagesize;
   p->mem_size = p->mem_virt;
   p->nice = kp.ki_nice - NZERO;
   p->priority = kp.ki_pri.pri_level - PZERO;
   p->numthreads = kp.ki_numthreads;
   p->tid = kp.ki_tid;

   return p;
}

static Proc_Info *
_proc_thread_info(struct kinfo_proc *kp, Eina_Bool is_thread)
{
   struct rusage *usage;
   Proc_Info *p;
   static int pagesize = 0;

   if (!pagesize) pagesize = getpagesize();

   p = calloc(1, sizeof(Proc_Info));

   p->pid = kp->ki_pid;
   p->uid = kp->ki_uid;

   if (!is_thread)
     _cmd_get(p, kp);

   p->cpu_id = kp->ki_oncpu;
   if (p->cpu_id == -1)
     p->cpu_id = kp->ki_lastcpu;

   usage = &kp->ki_rusage;

   p->cpu_time = (usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec +
       (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec;
   p->cpu_time /= 10000;
   p->state = _process_state_name(kp->ki_stat);
   p->mem_virt = kp->ki_size;
   p->mem_rss = kp->ki_rssize * pagesize;
   p->mem_size = p->mem_virt;
   p->nice = kp->ki_nice - NZERO;
   p->priority = kp->ki_pri.pri_level - PZERO;
   p->numthreads = kp->ki_numthreads;

   p->tid = kp->ki_tid;
   p->thread_name = strdup(kp->ki_tdname);

   return p;
}

Proc_Info *
proc_info_by_pid(int pid)
{
   kvm_t *kern;
   struct kinfo_proc *kps, *kp;
   char errbuf[_POSIX2_LINE_MAX];
   int pid_count;

   kern = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
   if (!kern)
     return _proc_info_by_pid_fallback(pid);

   kps = kvm_getprocs(kern, KERN_PROC_ALL, 0, &pid_count);
   if (!kps)
     {
        kvm_close(kern);
        return _proc_info_by_pid_fallback(pid);
     }

   Proc_Info *p = NULL;

   for (int i = 0; i < pid_count; i++)
     {
        if (kps[i].ki_flag & P_KPROC)
          continue;
        if (kps[i].ki_pid != pid)
          continue;

        kp = &kps[i];
        Proc_Info *t = _proc_thread_info(kp, EINA_TRUE);
        if (!p)
          p = _proc_thread_info(kp, EINA_FALSE);

        p->threads = eina_list_append(p->threads, t);
     }

   kvm_close(kern);

   if (!p) return _proc_info_by_pid_fallback(pid);

   return p;
}
#endif

void
proc_info_free(Proc_Info *proc)
{
   Proc_Info *t;

   EINA_LIST_FREE(proc->threads, t)
     {
        proc_info_free(t);
     }

   if (proc->threads)
     eina_list_free(proc->threads);

   if (proc->command)
     free(proc->command);
   if (proc->arguments)
     free(proc->arguments);
   if (proc->thread_name)
     free(proc->thread_name);

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

