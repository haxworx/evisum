#if defined(__OpenBSD__)
# define CPU_STATES      6
#else
# define CPU_STATES      5
#endif

int
cores_count(void)
{
   static int cores = 0;

   if (cores != 0)
     return cores;

#if defined(__linux__)
   char buf[4096];
   FILE *f;
   int line = 0;

   f = fopen("/proc/stat", "r");
   if (!f) return 0;

   while (fgets(buf, sizeof(buf), f))
     {
        if (line)
          {
             if (!strncmp(buf, "cpu", 3))
               cores++;
             else
               break;
          }
        line++;
     }

   fclose(f);
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
   size_t len;
   int mib[2] = { CTL_HW, HW_NCPU };

   len = sizeof(cores);
   if (sysctl(mib, 2, &cores, &len, NULL, 0) < 0)
     return 0;
#endif
   return cores;
}

int
cores_online_count(void)
{
#if defined(__OpenBSD__)
   int cores = 0;
   size_t len;
   int mib[2] = { CTL_HW, HW_NCPUONLINE };

   len = sizeof(cores);
   if (sysctl(mib, 2, &cores, &len, NULL, 0) < 0)
     return cores_count();

   return cores;
#else
   return cores_count();
#endif
}

void
cores_update(Eina_List *cores)
{
   unsigned long total, idle;
   Cpu_Core *core;
   int ncpu = eina_list_count(cores);
   if (!ncpu) return;

#if defined(__FreeBSD__) || defined(__DragonFly__)
   size_t size;
   int i, j;

   size = sizeof(unsigned long) * (CPU_STATES * ncpu);
   unsigned long cpu_times[ncpu][CPU_STATES];

   if (sysctlbyname("kern.cp_times", cpu_times, &size, NULL, 0) < 0)
     return;

   for (i = 0; i < ncpu; i++)
     {
        core = eina_list_nth(cores, i);
        unsigned long *cpu = cpu_times[i];

        total = 0;
        for (j = CP_USER; j <= CP_IDLE; j++)
          total += cpu[j];

        idle = cpu[CP_IDLE];

        core->total = total;
        core->idle = idle;
        core->freq = core_id_frequency(core->id);
        core->temp = core_id_temperature(core->id);
     }
#elif defined(__OpenBSD__)
   static struct cpustats cpu_times[CPU_STATES];
   static int cpu_time_mib[] = { CTL_KERN, KERN_CPUSTATS, 0 };
   size_t size;
   int i, j;

   memset(&cpu_times, 0, CPU_STATES * sizeof(struct cpustats));

   for (i = 0; i < ncpu; i++)
     {
        core = eina_list_nth(cores, i);
        size = sizeof(struct cpustats);
        cpu_time_mib[2] = i;
        if (sysctl(cpu_time_mib, 3, &cpu_times[i], &size, NULL, 0) < 0)
          return;

        total = 0;
        for (j = 0; j < CPU_STATES; j++)
          total += cpu_times[i].cs_time[j];

        idle = cpu_times[i].cs_time[CP_IDLE];

        core->total = total;
        core->idle = idle;
        core->freq = core_id_frequency(core->id);
        core->temp = core_id_temperature(core->id);
     }
#elif defined(__linux__)
   char *buf, name[128];
   int i;

   buf = file_contents("/proc/stat");
   if (!buf) return;

   for (i = 0; i < ncpu; i++)
     {
        core = eina_list_nth(cores, i);
        snprintf(name, sizeof(name), "cpu%d", i);
        char *line = strstr(buf, name);
        if (line)
          {
             line = strchr(line, ' ') + 1;
             unsigned long cpu_times[4] = { 0 };

             if (4 != sscanf(line, "%lu %lu %lu %lu", &cpu_times[0],
                   &cpu_times[1], &cpu_times[2], &cpu_times[3]))
               return;

             total = cpu_times[0] + cpu_times[1] + cpu_times[2] + cpu_times[3];
             idle = cpu_times[3];

             core->total = total;
             core->idle = idle;
             core->freq = core_id_frequency(core->id);
             core->temp = core_id_temperature(core->id);
          }
     }
   free(buf);
#elif defined(__MacOS__)
   mach_msg_type_number_t count;
   processor_cpu_load_info_t load;
   mach_port_t mach_port;
   unsigned int cores_count;
   int i;

   cores_count = eina_list_count(cores);

   count = HOST_CPU_LOAD_INFO_COUNT;
   mach_port = mach_host_self();
   if (host_processor_info(mach_port, PROCESSOR_CPU_LOAD_INFO, &cores_count,
                (processor_info_array_t *)&load, &count) != KERN_SUCCESS)
     exit(-1);

   for (i = 0; i < ncpu; i++)
     {
        core = eina_list_nth(cores, i);

        total = load[i].cpu_ticks[CPU_STATE_USER] +
           load[i].cpu_ticks[CPU_STATE_SYSTEM] +
           load[i].cpu_ticks[CPU_STATE_IDLE] +
           load[i].cpu_ticks[CPU_STATE_NICE];
        idle = load[i].cpu_ticks[CPU_STATE_IDLE];

        core->total = total;
        core->idle = idle;
     }
#endif
}

Eina_List *
cores_find(void)
{
   Eina_List *cores = NULL;
   Cpu_Core *core;
   int i, ncpu;

   ncpu = cores_count();
   for (i = 0; i < ncpu; i++)
     {
        core = calloc(1, sizeof(Cpu_Core));
        core->id = i;
        snprintf(core->name, sizeof(core->name), "cpu%i", i);
        cores = eina_list_append(cores, core);
     }
   cores_topology(cores);
   return cores;
}

static int  _cpu_temp_min = 0;
static int  _cpu_temp_max = 100;
static char _core_temps[256][512];
static char _hwmon_path[256];

int
_core_n_temperature_read(int n)
{
   int temp = THERMAL_INVALID;
#if defined(__linux__)

   char *b = file_contents(_core_temps[n]);
   if (b)
     {
        temp = atoi(b) / 1000;
        free(b);
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
    int value;
    size_t len = sizeof(value);

    if (!_core_temps[n][0])
      snprintf(_core_temps[n], sizeof(_core_temps[n]), "dev.cpu.%d.temperature", n);

    if ((sysctlbyname(_core_temps[n], &value, &len, NULL, 0)) != -1)
      {
         temp = (value - 2732) / 10;
      }
#endif

   return temp;
}

#if defined(__linux__)

typedef struct _thermal_drv
{
   const char *name;
   void (*init)(void);
   char min;
   char max;
} thermal_drv;

static void
_coretemp_init(void)
{
   char buf[4096];
   int count = cores_count();

   for (int j = 0; j < count; j++)
     {
        snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/topology/core_id", j);
        char *b = file_contents(buf);
        if (b)
          {
             int core_id = atoi(b);
             snprintf(_core_temps[j], sizeof(_core_temps[j]), "%s/temp%d_input", _hwmon_path, 2 + core_id);
             free(b);
          }
     }
}

static void
_generic_init(void)
{
   int i, count = cores_count();

   for (i = 0; i < count; i++)
     snprintf(_core_temps[i], sizeof(_core_temps[i]), "%s/temp1_input", _hwmon_path);
}

#endif

int
core_id_temperature(int id)
{
#if defined(__linux__)
   static int init = 0;

   // This list is not exhastive by any means, if you have the
   // hardware and can provide a better init, please do. WIP.
   // Min max (where applicable)
   thermal_drv drivers[] = {
      { "coretemp", _coretemp_init, 0, 90 },    /* Intel Coretemp */
      { "k10temp", _generic_init, 0, 90 },       /* AMD K10 */
      { "cpu_thermal", _generic_init, 0, 90 },   /* BCM2835/BCM2711 (RPI3/4) */
      { "cup", _generic_init, 0, 100 },          /* RK3399 */
      { "soc_thermal", _generic_init, 0, 100 },  /* RK3326 */
      { "cpu0_thermal", _generic_init, 0, 100 }, /* AllWinner A64 */
      { "soc_dts0", _generic_init, 0, 90 },      /* Intel Baytrail */
   };

   if (!init)
     {
        char buf[4096];
        struct dirent *dh;
        DIR *dir;

        memset(&_core_temps, 0, sizeof(_core_temps));
        memset(&_hwmon_path, 0, sizeof(_hwmon_path));

        dir = opendir("/sys/class/hwmon");
        if (!dir)
          {
             init = 1;
             return THERMAL_INVALID;
          }

        while ((dh = readdir(dir)) != NULL)
          {
             if (dh->d_name[0] == '.') continue;
             snprintf(buf, sizeof(buf), "/sys/class/hwmon/%s", dh->d_name);
             char *link = realpath(buf, NULL);
             if (!link) continue;

             snprintf(buf, sizeof(buf), "%s/name", link);
             char *b = file_contents(buf);
             if (b)
               {
                  for (int i = 0; i < sizeof(drivers) / sizeof(thermal_drv); i++)
                    {
                       if (!strncmp(b, drivers[i].name, strlen(drivers[i].name)))
                         {
                            snprintf(_hwmon_path, sizeof(_hwmon_path), "%s", link);
                            drivers[i].init();
                         }
                    }
                  free(b);
               }
             free(link);
             if (_hwmon_path[0]) break;
          }

        closedir(dir);
        init = 1;
     }

   if (!_hwmon_path[0]) return THERMAL_INVALID;

   return _core_n_temperature_read(id);
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   static int init = 0;

   if (!init)
     {
        memset(&_core_temps, 0, sizeof(_core_temps));
        init = 1;
     }

    return _core_n_temperature_read(id);
#endif
   return THERMAL_INVALID;
}

int
cores_temperature_min_max(int *min, int *max)
{

   *min = _cpu_temp_min;
   *max = _cpu_temp_max;

   return 0;
}

int
core_id_frequency(int id)
{
#if defined(__linux__)
   int freq = CPUFREQ_INVALID;
   FILE *f;
   char buf[4096];
   int tmp;

   snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", id);
   f = fopen(buf, "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f))
          {
             tmp = strtol(buf, NULL, 10);
             if (!((tmp == LONG_MIN || tmp == LONG_MAX) && errno == ERANGE))
               freq = tmp;
          }
        fclose(f);
        if (freq != CPUFREQ_INVALID) return freq;
     }

   return freq;
#elif defined(__FreeBSD__) || defined(__DragonFly__) || defined(__OpenBSD__)
   return cores_frequency();
#endif

   return CPUFREQ_INVALID;
}

int
cores_frequency_min_max(int *min, int *max)
{
   int freq_min = 0x7fffffff, freq_max = 0;
#if defined(__linux__)
   char *s;

   s = file_contents("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq");
   if (s)
     {
        freq_min = atoi(s);
        free(s);
        s = file_contents("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq");
        if (s)
          {
             freq_max = atoi(s);
             free(s);
             *min = freq_min;
             *max = freq_max;
             if (freq_min < freq_max)
               return 0;
          }
     }

   s = file_contents("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies");
   if (!s) return -1;

   char *t = strtok(s, " ");
   while (t)
     {
        int freq = atoi(t);
        if (freq != 0)
          {
             if (freq > freq_max) freq_max = freq;
             if (freq < freq_min) freq_min = freq;
          }
        t = strtok(NULL, " ");
     }

   free(s);

   if (freq_min == 0x7fffffff || freq_max == 0) return -1;

   *min = freq_min;
   *max = freq_max;

   return 0;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   char buf[4096];
   size_t len = sizeof(buf);
   char *t, *s;

   if (sysctlbyname("dev.cpu.0.freq_levels", buf, &len, NULL, 0) != -1)
     {
        s = buf;
        while (s)
          {
             t = strchr(s, '/');
             if (!t) break;
             *t = '\0';
             int freq = atoi(s) * 1000;
             if (freq > freq_max) freq_max = freq;
             if (freq < freq_min) freq_min = freq;

             s = strchr(t + 1, ' ');
          }
        if (freq_min == 0x7fffffff || freq_max == 0) return -1;

        *min = freq_min;
        *max = freq_max;

        return 0;
     }

#elif defined(__OpenBSD__)
   *min = 0;
   *max = 100;

   return 0;
#endif
   (void) freq_min; (void) freq_max;
   return -1;
}

int
cores_frequency(void)
{
   int freq = CPUFREQ_INVALID;

#if defined(__FreeBSD__) || defined(__DragonFly__)
   size_t len = sizeof(freq);
   if (sysctlbyname("dev.cpu.0.freq", &freq, &len, NULL, 0) != -1)
     freq *= 1000;
#elif defined(__OpenBSD__)
   int mib[2] = { CTL_HW, HW_CPUSPEED };
   size_t len = sizeof(freq);
   if (sysctl(mib, 2, &freq, &len, NULL, 0) != -1)
     freq *= 1000;
#elif defined(__linux__)
   FILE *f;
   char buf[4096];
   int tmp;

   f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
   if (f)
     {
        if (fgets(buf, sizeof(buf), f))
          {
             tmp = strtol(buf, NULL, 10);
             if (!((tmp == LONG_MIN || tmp == LONG_MAX) && errno == ERANGE))
               freq = tmp;
          }
        fclose(f);
        if (freq != CPUFREQ_INVALID) return freq;
     }

   f = fopen("/proc/cpuinfo", "r");
   if (!f) return freq;

   while (fgets(buf, sizeof(buf), f))
     {
        if (!strncasecmp(buf, "cpu MHz", 7))
          {
             char *s = strchr(buf, ':') + 1;
             tmp = strtol(s, NULL, 10);
             if (!((tmp == LONG_MIN || tmp == LONG_MAX) && errno == ERANGE))
               freq = tmp * 1000;
             break;
          }
     }

   fclose(f);
#else

#endif
   return freq;
}

#if defined(__linux__)

typedef struct
{
   short id;
   short core_id;
} core_top_t;

static int
_cmp(const void *a, const void *b)
{
   core_top_t *aa = (core_top_t *) a;
   core_top_t *bb = (core_top_t *) b;

   if (aa->core_id == bb->core_id) return 0;
   else if (aa->core_id < bb->core_id) return -1;
   else return 1;
}

#endif

void
cores_topology(Eina_List *cores)
{
#if defined(__linux__)
   char buf[4096];
   int ncpu = eina_list_count(cores);
   core_top_t *cores_top = malloc(ncpu * sizeof(core_top_t));

   for (int i = 0; i < ncpu; i++)
     {
        cores_top[i].id = i;
        cores_top[i].core_id = i;
        snprintf(buf, sizeof(buf), "/sys/devices/system/cpu/cpu%i/topology/core_id", i);
        char *b = file_contents(buf);
        if (b)
          {
             cores_top[i].core_id = atoi(b);
             free(b);
          }
     }

   qsort(cores_top, ncpu, sizeof(core_top_t), _cmp);

   for (int i = 0; i < ncpu; i++)
     {
        Cpu_Core *core = eina_list_nth(cores, i);
        core->top_id = cores_top[i].id;
     }
   free(cores_top);
#else
   Cpu_Core *core;
   Eina_List *l;

   EINA_LIST_FOREACH(cores, l, core)
     core->top_id = core->id;
#endif

}
