#if defined(__MACH__) && defined(__APPLE__)
# define __MacOS__
#endif

#include "intl/gettext.h"
#define _(STR) gettext(STR)

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

#if defined(__FreeBSD__)
# define _WANT_FILE
# include <sys/file.h>
# include <sys/filedesc.h>
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

#include "macros.h"

static Eina_Bool _show_kthreads = 0;

void
proc_info_kthreads_show_set(Eina_Bool enabled)
{
   _show_kthreads = enabled;
}

Eina_Bool
proc_info_kthreads_show_get(void)
{
   return _show_kthreads;
}

static const char * _states[128];

static void
_states_init(void)
{
#if defined(__linux__)
   _states['D']     = _("dsleep");
   _states['I']     = _("idle");
   _states['R']     = _("running");
   _states['S']     = _("sleeping");
   _states['T']     = _("stopped");
   _states['X']     = _("dead");
   _states['Z']     = _("zombie");
#else
   _states[SIDL]    = _("idle");
   _states[SRUN]    = _("running");
   _states[SSLEEP]  = _("sleeping");
   _states[SSTOP]   = _("stopped");
#if !defined(__MacOS__)
#if !defined(__OpenBSD__)
   _states[SWAIT]   = _("wait");
   _states[SLOCK]   = _("lock");
   _states[SZOMB]   = _("zombie");
#endif
#if defined(__OpenBSD__)
   _states[SDEAD]   = _("zombie");
   _states[SONPROC] = _("running");
#endif
#endif
#endif
}
#ifndef __OpenBSD__
static
#endif
const char *
_process_state_name(char state)
{
   static int init = 0;
   if (!init)
     {
        _states_init();
        init = 1;
     }
   return _states[toupper(state)];
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
   char buf[4096];
   unsigned int dummy, size, shared, resident, data, text;
   static int pagesize = 0;

   if (!pagesize) pagesize = getpagesize();

   snprintf(buf, sizeof(buf), "/proc/%d/statm", proc->pid);
   f = fopen(buf, "r");
   if (!f) return;

   if (fgets(buf, sizeof(buf), f))
     {
        if (sscanf(buf, "%u %u %u %u %u %u %u",
                   &size, &resident, &shared, &text,
                   &dummy, &data, &dummy) == 7)
          {
             proc->mem_rss = MEMSIZE(resident) * MEMSIZE(pagesize);
             proc->mem_shared = MEMSIZE(shared) * MEMSIZE(pagesize);
             proc->mem_size = proc->mem_rss - proc->mem_shared;
             proc->mem_virt = MEMSIZE(size) * MEMSIZE(pagesize);
          }
     }

   fclose(f);
}

static void
_cmd_args(Proc_Info *p, char *name, size_t len)
{
   char path[PATH_MAX];
   char line[4096];
   int pid = p->pid;

   snprintf(path, sizeof(path), "/proc/%d/exe", pid);
   char *link = ecore_file_readlink(path);
   if (link)
     {
        snprintf(name, len, "%s", ecore_file_file_get(link));
        free(link);
     }

   snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
   FILE *f = fopen(path, "r");
   if (f)
     {
        if (fgets(line, sizeof(line), f))
          {
             int sz = ftell(f);
             Eina_Strbuf *buf = eina_strbuf_new();

             if (ecore_file_exists(line))
               snprintf(name, len, "%s", ecore_file_file_get(line));

             const char *cp = line;
             for (int i = 0; i < sz; i++)
               {
                  if (line[i] == '\0')
                    {
                       if (*cp)
                         eina_strbuf_append(buf, cp);
                       if ((i + 1) < sz)
                         {
                            i++;
                            cp = &line[i];
                            if (*cp)
                              eina_strbuf_append(buf, " ");
                         }
                    }
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
   char buf[4096];
   int uid = 0;

   snprintf(buf, sizeof(buf),"/proc/%d/status", pid);
   f = fopen(buf, "r");
   if (!f) return -1;

   while ((fgets(buf, sizeof(buf), f)) != NULL)
     {
        if (!strncmp(buf, "Uid:", 4))
          {
             uid = _parse_line(buf);
             break;
          }
     }

   fclose(f);

   return uid;
}

static int64_t
_boot_time(void)
{
   FILE *f;
   int64_t boot_time;
   char buf[4096];
   double uptime = 0.0;

   f = fopen("/proc/uptime", "r");
   if (!f) return 0;

   if (fgets(buf, sizeof(buf), f))
     sscanf(buf, "%lf", &uptime);
   else boot_time = 0;

   fclose(f);

   if (uptime > 0.0)
     boot_time = time(NULL) - (time_t) uptime;

   return boot_time;
}

typedef struct
{
   int pid, ppid, utime, stime, cutime, cstime;
   int psr, pri, nice, numthreads;
   long long int start_time, run_time;
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
   char name[1024];
   int dummy, len = 0;
   static long tck = 0;
   static int64_t boot_time = 0;

   if (!boot_time) boot_time = _boot_time();

   memset(st, 0, sizeof(Stat));

   f = fopen(path, "r");
   if (!f) return 0;

   if (fgets(line, sizeof(line), f))
     {

        len = sscanf(line, "%d %s %c %d %d %d %d %d %u %u %u %u %u %d %d %d"
              " %d %d %d %u %u %lld %lu %u %u %u %u %u %u %u %d %d %d %d %u"
              " %d %d %d %d %d %d %d %d %d", &dummy, name,
              &st->state, &st->ppid, &dummy, &dummy, &dummy, &dummy, &st->flags,
              &dummy, &dummy, &dummy, &dummy, &st->utime, &st->stime, &st->cutime,
              &st->cstime, &st->pri, &st->nice, &st->numthreads, &dummy, &st->start_time,
              &st->mem_virt, &st->mem_rss, &dummy, &dummy, &dummy, &dummy, &dummy,
              &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
              &dummy, &dummy, &st->psr, &dummy, &dummy, &dummy, &dummy, &dummy);
     }
   fclose(f);

   if (len != 44) return 0;

   len = strlen(name);
   if (len)
     {
        name[len-1] = '\0';
        snprintf(st->name, sizeof(st->name), "%s", &name[1]);
     }

   if (!tck) tck = sysconf(_SC_CLK_TCK);

   st->start_time /= tck;
   st->start_time += boot_time;
   st->run_time = (st->utime + st->stime) / tck;

   return 1;
}

static int
_n_files(Proc_Info *p)
{
   char buf[256];
   Eina_List *files;
   char *f;

   snprintf(buf, sizeof(buf), "/proc/%d/fd", p->pid);

   files = ecore_file_ls(buf);
   EINA_LIST_FREE(files, f)
     {
        p->numfiles++;
        free(f);
     }
   return p->numfiles;
}

static Eina_List *
_process_list_linux_get(void)
{
   Eina_List *files, *list;
   const char *state;
   char *n;
   char buf[4096];
   Stat st;

   list = NULL;

   files = ecore_file_ls("/proc");
   EINA_LIST_FREE(files, n)
     {
        int pid = atoi(n);
        free(n);

        if (!pid) continue;

        snprintf(buf, sizeof(buf), "/proc/%d/stat", pid);
        if (!_stat(buf, &st))
          continue;

        if (st.flags & PF_KTHREAD && !proc_info_kthreads_show_get())
          continue;

        Proc_Info *p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        p->pid = pid;
        p->ppid = st.ppid;
        p->uid = _uid(pid);
        p->cpu_id = st.psr;
        p->start_time = st.start_time;
        p->run_time = st.run_time;
        state = _process_state_name(st.state);
        snprintf(p->state, sizeof(p->state), "%s", state);
        p->cpu_time = st.utime + st.stime;
        p->nice = st.nice;
        p->priority = st.pri;
        p->numthreads = st.numthreads;
        p->numfiles = _n_files(p);
        if (st.flags & PF_KTHREAD)
          p->is_kernel = 1;
        _mem_size(p);
        _cmd_args(p, st.name, sizeof(st.name));

        list = eina_list_append(list, p);
     }

   return list;
}

static void
_proc_thread_info(Proc_Info *p)
{
   Eina_List *files;
   const char *state;
   char *n;
   char buf[4096];
   Stat st;

   snprintf(buf, sizeof(buf), "/proc/%d/task", p->pid);
   files = ecore_file_ls(buf);
   EINA_LIST_FREE(files, n)
     {
        int tid = atoi(n);
        free(n);
        snprintf(buf, sizeof(buf), "/proc/%d/task/%d/stat", p->pid, tid);
        if (!_stat(buf, &st))
          continue;

        Proc_Info *t = calloc(1, sizeof(Proc_Info));
        if (!t) continue;
        t->cpu_id = st.psr;
        state = _process_state_name(st.state);
        snprintf(t->state, sizeof(t->state), "%s", state);
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
}

Proc_Info *
proc_info_by_pid(int pid)
{
   const char *state;
   Stat st;
   char buf[4096];

   snprintf(buf, sizeof(buf), "/proc/%d/stat", pid);
   if (!_stat(buf, &st))
     return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->ppid = st.ppid;
   p->uid = _uid(pid);
   p->cpu_id = st.psr;
   p->start_time = st.start_time;
   p->run_time = st.run_time;
   state = _process_state_name(st.state);
   snprintf(p->state, sizeof(p->state), "%s", state);
   p->cpu_time = st.utime + st.stime;
   p->priority = st.pri;
   p->nice = st.nice;
   p->numthreads = st.numthreads;
   p->numfiles = _n_files(p);
   if (st.flags & PF_KTHREAD) p->is_kernel = 1;
   _mem_size(p);
   _cmd_args(p, st.name, sizeof(st.name));

   _proc_thread_info(p);

   return p;
}

#endif

#if defined(__OpenBSD__)

static void
_proc_get(Proc_Info *p, struct kinfo_proc *kp)
{
   static int pagesize = 0;
   const char *state;

   if (!pagesize) pagesize = getpagesize();

   p->pid = kp->p_pid;
   p->ppid = kp->p_ppid;
   p->uid = kp->p_uid;
   p->cpu_id = kp->p_cpuid;
   p->start_time = kp->p_ustart_sec;
   p->run_time = kp->p_uutime_sec + kp->p_ustime_sec +
                 (kp->p_uutime_usec / 1000000) + (kp->p_ustime_usec / 1000000);

   state = _process_state_name(kp->p_stat);
   snprintf(p->state, sizeof(p->state), "%s", state);
   snprintf(p->wchan, sizeof(p->wchan), "%s", kp->p_wmesg);
   p->cpu_time = kp->p_uticks + kp->p_sticks + kp->p_iticks;
   p->mem_virt = p->mem_size = (MEMSIZE(kp->p_vm_tsize) * MEMSIZE(pagesize)) +
      (MEMSIZE(kp->p_vm_dsize) * MEMSIZE(pagesize)) + (MEMSIZE(kp->p_vm_ssize) * MEMSIZE(pagesize));
   p->mem_rss = MEMSIZE(kp->p_vm_rssize) * MEMSIZE(pagesize);
   p->priority = kp->p_priority - PZERO;
   p->nice = kp->p_nice - NZERO;
   p->tid = kp->p_tid;
}

static void
_kvm_get(Proc_Info *p, kvm_t *kern, struct kinfo_proc *kp)
{
   char **args;
   char name[4096];

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
   struct kinfo_file *kf;
   int n;

   if ((kf = kvm_getfiles(kern, KERN_FILE_BYPID, -1, sizeof(struct kinfo_file), &n)))
     {
        for (int i = 0; i < n; i++)
          {
             if (kf[i].p_pid == kp->p_pid)
               {
                  if (kf[i].fd_fd >= 0) p->numfiles++;
               }
          }
     }

   if (!p->command)
     p->command = strdup(kp->p_comm);
}

Proc_Info *
proc_info_by_pid(int pid)
{
   struct kinfo_proc *kp, *kpt;
   kvm_t *kern;
   char errbuf[_POSIX2_LINE_MAX];
   int count, pid_count;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kp = kvm_getprocs(kern, KERN_PROC_PID, pid, sizeof(*kp), &count);
   if (!kp) return NULL;

   if (count == 0) return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   _proc_get(p, kp);
   _kvm_get(p, kern, kp);

   kp = kvm_getprocs(kern, KERN_PROC_SHOW_THREADS, 0, sizeof(*kp), &pid_count);

   for (int i = 0; i < pid_count; i++)
     {
        if (kp[i].p_pid != p->pid) continue;

        kpt = &kp[i];

        if (kpt->p_tid <= 0) continue;

        Proc_Info *t = calloc(1, sizeof(Proc_Info));
        if (!t) continue;

        _proc_get(t, kpt);

        t->tid = kpt->p_tid;
        t->thread_name = strdup(kpt->p_comm);

        p->threads = eina_list_append(p->threads, t);
     }

   p->numthreads = eina_list_count(p->threads);

   kvm_close(kern);

   return p;
}

static Eina_List *
_process_list_openbsd_get(void)
{
   struct kinfo_proc *kps, *kp;
   Proc_Info *p;
   char errbuf[4096];
   kvm_t *kern;
   int pid_count;
   Eina_List *list = NULL;

   kern = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
   if (!kern) return NULL;

   kps = kvm_getprocs(kern, KERN_PROC_ALL, 0, sizeof(*kps), &pid_count);
   if (!kps) return NULL;

   for (int i = 0; i < pid_count; i++)
     {
        p = calloc(1, sizeof(Proc_Info));
        if (!p) return NULL;

        kp = &kps[i];

        Proc_Info *p = proc_info_by_pid(kp->p_pid);
        if (p)
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

   /* See libtop.c (top) for the origin of this comment, which is necessary as
    * there is little other documentation...thanks Apple.
    *
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
   const char *state;
   struct proc_taskallinfo taskinfo;
   int size = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &taskinfo, sizeof(taskinfo));
   if (size != sizeof(taskinfo)) return NULL;

   Proc_Info *p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = pid;
   p->ppid = taskinfo.pbsd.pbi_ppid;
   p->uid = taskinfo.pbsd.pbi_uid;
   p->cpu_id = -1;
   p->cpu_time = taskinfo.ptinfo.pti_total_user +
      taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->start_time = taskinfo.pbsd.pbi_start_tvsec;
   state = _process_state_name(taskinfo.pbsd.pbi_status);
   snprintf(p->state, sizeof(p->state), "%s", state);
   p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
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
   const char *state;
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
   p->ppid = taskinfo.pbsd.pbi_ppid;
   p->cpu_id = workqueue.pwq_nthreads;
   p->cpu_time = taskinfo.ptinfo.pti_total_user +
      taskinfo.ptinfo.pti_total_system;
   p->cpu_time /= 10000000;
   p->start_time = taskinfo.pbsd.pbi_start_tvsec;
   state = _process_state_name(taskinfo.pbsd.pbi_status);
   snprintf(p->state, sizeof(p->state), "%s", state);
   p->mem_size = p->mem_virt = taskinfo.ptinfo.pti_virtual_size;
   p->mem_rss = taskinfo.ptinfo.pti_resident_size;
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

static void
_kvm_get(Proc_Info *p, struct kinfo_proc *kp)
{
   kvm_t * kern;
   char name[4096];
   Eina_Bool have_command = 0;

   kern = kvm_open(NULL, "/dev/mem", NULL, O_RDONLY, "kvm_open");
   if (!kern) goto nokvm;
   char **args;
   if ((args = kvm_getargv(kern, kp, sizeof(name)-1)) && (args[0]))
     {
        char *base = strdup(args[0]);
        if (base)
          {
             char *spc = strchr(base, ' ');
             if (spc) *spc = '\0';

             if (ecore_file_exists(base))
               {
                  snprintf(name, sizeof(name), "%s", basename(base));
                  have_command = 1;
               }
             free(base);
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

   struct filedesc filed;
   struct fdescenttbl *fdt;
   unsigned int n;

   if (!kvm_read(kern, (unsigned long)kp->ki_fd, &filed, sizeof(filed)))
     goto kvmerror;

   if (!kvm_read(kern, (unsigned long)filed.fd_files, &n, sizeof(n)))
     goto kvmerror;

   unsigned int size = sizeof(*fdt) + n * sizeof(struct filedescent);
   fdt = malloc(size);
   if (fdt)
     {
        if (kvm_read(kern, (unsigned long)filed.fd_files, fdt, size))
          {
             for (int i = 0; i < n; i++)
               {
                  if (!fdt->fdt_ofiles[i].fde_file) continue;
                  p->numfiles++;
               }
          }
        free(fdt);
     }

kvmerror:
   if (kern)
     kvm_close(kern);
nokvm:
   if (!have_command)
     snprintf(name, sizeof(name), "%s", kp->ki_comm);

   p->command = strdup(name);
}

static Proc_Info *
_proc_thread_info(struct kinfo_proc *kp, Eina_Bool is_thread)
{
   struct rusage *usage;
   const char *state;
   Proc_Info *p;
   static int pagesize = 0;

   if (!pagesize) pagesize = getpagesize();

   p = calloc(1, sizeof(Proc_Info));
   if (!p) return NULL;

   p->pid = kp->ki_pid;
   p->ppid = kp->ki_ppid;
   p->uid = kp->ki_uid;

   if (!is_thread)
     _kvm_get(p, kp);

   p->cpu_id = kp->ki_oncpu;
   if (p->cpu_id == -1)
     p->cpu_id = kp->ki_lastcpu;

   usage = &kp->ki_rusage;

   p->cpu_time = ((usage->ru_utime.tv_sec * 1000000) + usage->ru_utime.tv_usec +
       (usage->ru_stime.tv_sec * 1000000) + usage->ru_stime.tv_usec) / 10000;
   p->run_time = (kp->ki_runtime + 500000) / 1000000;
   state = _process_state_name(kp->ki_stat);
   snprintf(p->state, sizeof(p->state), "%s", state);
   snprintf(p->wchan, sizeof(p->wchan), "%s", kp->ki_wmesg);
   p->mem_virt = kp->ki_size;
   p->mem_rss = MEMSIZE(kp->ki_rssize) * MEMSIZE(pagesize);
   p->start_time = kp->ki_start.tv_sec;
   p->mem_size = p->mem_virt;
   p->nice = kp->ki_nice - NZERO;
   p->priority = kp->ki_pri.pri_level - PZERO;
   p->numthreads = kp->ki_numthreads;

   p->tid = kp->ki_tid;
   p->thread_name = strdup(kp->ki_tdname);
   if (kp->ki_flag & P_KPROC) p->is_kernel = 1;

   return p;
}

static Eina_List *
_process_list_freebsd_fallback_get(void)
{
   Eina_List *list;
   struct kinfo_proc kp;
   int mib[4];
   size_t len;
   static int pid_max;

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
          continue;

        if (kp.ki_flag & P_KPROC && !proc_info_kthreads_show_get())
          continue;

        Proc_Info *p = _proc_thread_info(&kp, 0);
        if (p)
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
   char errbuf[_POSIX2_LINE_MAX];
   int pid_count;

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
        if (kps[i].ki_flag & P_KPROC && !proc_info_kthreads_show_get())
          continue;

        kp = &kps[i];

        Proc_Info *p = _proc_thread_info(kp, 0);
        if (p)
          list = eina_list_append(list, p);
     }

   kvm_close(kern);

   return list;
}

static Proc_Info *
_proc_info_by_pid_fallback(int pid)
{
   struct kinfo_proc kp;
   int mib[4];
   size_t len;

   len = sizeof(int);
   if (sysctlnametomib("kern.proc.pid", mib, &len) == -1)
     return NULL;

   mib[3] = pid;

   len = sizeof(kp);
   if (sysctl(mib, 4, &kp, &len, NULL, 0) == -1)
     return NULL;

   Proc_Info *p = _proc_thread_info(&kp, 0);

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

   // XXX: run_time does not include interrupts.

   for (int i = 0; i < pid_count; i++)
     {
        if (kps[i].ki_flag & P_KPROC && !proc_info_kthreads_show_get())
          continue;
        if (kps[i].ki_pid != pid)
          continue;

        kp = &kps[i];
        Proc_Info *t = _proc_thread_info(kp, 1);
        if (!p)
          {
             p = _proc_thread_info(kp, 0);
             p->cpu_time = 0;
          }

        p->cpu_time += t->cpu_time;
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

   if (!proc) return;

   EINA_LIST_FREE(proc->threads, t)
     proc_info_free(t);

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

static Eina_Bool
_child_add(Eina_List *parents, Proc_Info *child)
{
   Eina_List *l;
   Proc_Info *parent;

   EINA_LIST_FOREACH(parents, l, parent)
     {
        if (parent->pid == child->ppid)
          {
             parent->children = eina_list_append(parent->children, child);
             return 1;
          }
     }

   return 0;
}

Eina_List *
proc_info_all_children_get()
{
   Proc_Info *proc;
   Eina_List *l;
   Eina_List *procs;

   procs = proc_info_all_get();

   EINA_LIST_FOREACH(procs, l, proc)
     {
        int ok =_child_add(procs,  proc);
        (void) ok;
     }

    return procs;
}

Eina_List *
_append_wanted(Eina_List *wanted, Eina_List *tree)
{
   Eina_List *l;
   Proc_Info *parent;

   EINA_LIST_FOREACH(tree, l, parent)
     {
        wanted = eina_list_append(wanted, parent);
        if (parent->children)
          wanted = _append_wanted(wanted, parent->children);
     }
   return wanted;
}

Eina_List *
proc_info_pid_children_get(pid_t pid)
{
   Proc_Info *proc;
   Eina_List *l, *procs, *wanted = NULL;

   procs = proc_info_all_children_get();

   EINA_LIST_FOREACH(procs, l, proc)
     {
        if (!wanted && proc->pid == pid)
          {
             wanted = eina_list_append(wanted, proc);
             if (proc->children)
               wanted = _append_wanted(wanted, proc->children);
          }
     }

   EINA_LIST_FREE(procs, proc)
     {
        if (!eina_list_data_find(wanted, proc))
          {
             proc_info_free(proc);
          }
     }

    return wanted;
}

void
proc_info_all_children_free(Eina_List *pstree)
{
   Proc_Info *parent, *child;

   EINA_LIST_FREE(pstree, parent)
     {
        EINA_LIST_FREE(parent->children, child)
          proc_info_pid_children_free(child);
        proc_info_free(parent);
     }
}

void
proc_info_pid_children_free(Proc_Info *proc)
{
   Proc_Info *child;

   EINA_LIST_FREE(proc->children, child)
     proc_info_free(child);

   proc_info_free(proc);
}

int
proc_sort_by_pid(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->pid - inf2->pid;
}

int
proc_sort_by_uid(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->uid - inf2->uid;
}

int
proc_sort_by_nice(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->nice - inf2->nice;
}

int
proc_sort_by_pri(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->priority - inf2->priority;
}

int
proc_sort_by_cpu(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->cpu_id - inf2->cpu_id;
}

int
proc_sort_by_threads(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numthreads - inf2->numthreads;
}

int
proc_sort_by_files(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return inf1->numfiles - inf2->numfiles;
}

int
proc_sort_by_size(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_size;
   size2 = inf2->mem_size;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

int
proc_sort_by_virt(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_virt;
   size2 = inf2->mem_virt;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

int
proc_sort_by_rss(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_rss;
   size2 = inf2->mem_rss;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

int
proc_sort_by_shared(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t size1, size2;

   inf1 = p1; inf2 = p2;

   size1 = inf1->mem_shared;
   size2 = inf2->mem_shared;

   if (size1 > size2)
     return 1;
   if (size1 < size2)
     return -1;

   return 0;
}

int
proc_sort_by_time(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   int64_t t1, t2;

   inf1 = p1; inf2 = p2;

   t1 = inf1->run_time;
   t2 = inf2->run_time;

   if (t1 > t2)
     return 1;
   if (t1 < t2)
     return -1;

   return 0;
}

int
proc_sort_by_cpu_usage(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;
   double one, two;

   inf1 = p1; inf2 = p2;

   one = inf1->cpu_usage;
   two = inf2->cpu_usage;

   if (one > two)
     return 1;
   else if (one < two)
     return -1;
   else return 0;
}

int
proc_sort_by_cmd(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcasecmp(inf1->command, inf2->command);
}

int
proc_sort_by_state(const void *p1, const void *p2)
{
   const Proc_Info *inf1, *inf2;

   inf1 = p1; inf2 = p2;

   return strcmp(inf1->state, inf2->state);
}

int
proc_sort_by_age(const void *p1, const void *p2)
{
   const Proc_Info *c1 = p1, *c2 = p2;

   return c1->start_time - c2->start_time;
}

