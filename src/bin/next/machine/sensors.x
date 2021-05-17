void
sensor_free(Sensor *sensor)
{
   if (sensor->name)
     free(sensor->name);
   if (sensor->child_name)
     free(sensor->child_name);
#if defined(__linux__)
   if (sensor->path)
     free(sensor->path);
#endif
   free(sensor);
}

Eina_Bool
sensor_check(Sensor *sensor)
{
#if defined(__linux__)
   char *d = file_contents(sensor->path);
   if (d)
     {
        double val = atof(d);
        if (val)
          sensor->value = val /= 1000;
        free(d);
        return 1;
     }
   return 0;
#elif defined(__OpenBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);

   if (sysctl(sensor->mibs, 5, &snsr, &slen, NULL, 0) == -1) return 0;

   sensor->value = (snsr.value - 273150000) / 1000000.0;

   return 1;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int value;
   size_t len = sizeof(value);
   if ((sysctlbyname(sensor->name, &value, &len, NULL, 0)) != -1)
     {
        sensor->value = (float) (value - 2732) / 10;
        return 1;
     }
#endif
   return 0;
}

Eina_List *
sensors_find(void)
{
   Eina_List *sensors = NULL;
#if defined(__OpenBSD__)
   Sensor *sensor;
   int mibs[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int devn, n;
   struct sensor snsr;
   struct sensordev snsrdev;
   size_t slen = sizeof(struct sensor);
   size_t sdlen = sizeof(struct sensordev);

   for (devn = 0;; devn++)
      {
        mibs[2] = devn;

        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENOENT) break;
             continue;
          }

        for (n = 0; n < snsrdev.maxnumt[SENSOR_TEMP]; n++) {
             mibs[4] = n;

             if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
               continue;

             if (slen > 0 && (snsr.flags & SENSOR_FINVALID) == 0)
               break;
          }

        if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
          continue;
        if (snsr.type != SENSOR_TEMP)
          continue;

        sensor = calloc(1, sizeof(Sensor));
        if (sensor)
          {
             sensor->name = strdup(snsrdev.xname);
             sensor->value = (snsr.value - 273150000) / 1000000.0; // (uK -> C)
             memcpy(sensor->mibs, &mibs, sizeof(mibs));

             sensors = eina_list_append(sensors, sensor);
          }
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   Sensor *sensor;
   int value;
   char buf[256];
   size_t len = sizeof(value);

   if ((sysctlbyname("hw.acpi.thermal.tz0.temperature", &value, &len, NULL, 0)) != -1)
     {
        sensor = calloc(1, sizeof(Sensor));
        if (sensor)
          {
             sensor->name = strdup("hw.acpi.thermal.tz0.temperature");
             sensor->value = (float) (value -  2732) / 10;
             sensors = eina_list_append(sensors, sensor);
          }
     }

   int n = system_cpu_count_get();

   for (int i = 0; i < n; i++)
     {
        len = sizeof(value);
        snprintf(buf, sizeof(buf), "dev.cpu.%i.temperature", i);
        if ((sysctlbyname(buf, &value, &len, NULL, 0)) != -1)
          {
             sensor = calloc(1, sizeof(Sensor));
             if (sensor)
               {
                  sensor->name = strdup(buf);
                  sensor->value = (float) (value - 2732) / 10;
                  sensors = eina_list_append(sensors, sensor);
               }
          }
     }
#elif defined(__linux__)
   Sensor *sensor;
   DIR *dir;
   struct dirent *dh;
   char buf[4096];
   int seen[128];

   dir = opendir("/sys/class/hwmon");
   if (!dir) return NULL;

   while ((dh = readdir(dir)) != NULL)
     {
        struct dirent **names = NULL;

        snprintf(buf, sizeof(buf), "/sys/class/hwmon/%s", dh->d_name);
        char *link = realpath(buf, NULL);
        if (!link) continue;

        int n = scandir(link, &names, 0, alphasort);
        if (n < 0) continue;

        int idx = 0;
        memset(&seen, 0, sizeof(seen));

        for (int i = 0; i < n; i++)
          {
             if (!strncmp(names[i]->d_name, "temp", 4))
               {
                  int id = atoi(names[i]->d_name + 4);
                  if ((!id) || (id > sizeof(seen)))
                    {
                       free(names[i]);
                       continue;
                    }

                  int found = 0;

                  for (int j = 0; seen[j] != 0 && j < sizeof(seen); j++)
                     if (seen[j] == id) found = 1;

                  if (found)
                    {
                       free(names[i]);
                       continue;
                    }

                  sensor = calloc(1, sizeof(Sensor));
                  if (sensor)
                    {
                       snprintf(buf, sizeof(buf), "%s/name", link);
                       sensor->name = file_contents(buf);

                       snprintf(buf, sizeof(buf), "%s/temp%d_label", link, id);
                       sensor->child_name = file_contents(buf);

                       snprintf(buf, sizeof(buf), "%s/temp%d_input", link, id);
                       sensor->path = strdup(buf);
                       char *d = file_contents(buf);
                       if (d)
                         {
                            sensor->value = atoi(d);
                            if (sensor->value) sensor->value /= 1000;
                            free(d);
                         }
                        sensors = eina_list_append(sensors, sensor);
                     }
                  seen[idx++] = id;
               }
             free(names[i]);
          }

        free(names);
        free(link);
     }

   closedir(dir);

#elif defined(__MacOS__)
#endif
   return sensors;
}

Eina_List *
batteries_find(void)
{
   Eina_List *list = NULL;
#if defined(__OpenBSD__)
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   int mibs[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int devn;

   for (devn = 0; ; devn++)
     {
        mibs[2] = devn;
        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO) continue;
             if (errno == ENOENT) break;
          }
        if (!strncmp(snsrdev.xname, "acpibat", 7))
          {
             Battery *bat = calloc(1, sizeof(Battery));
             if (bat)
               {
                  bat->name = strdup(snsrdev.xname);
                  bat->model = strdup("Unknown");
                  bat->vendor = strdup("Unknown");
                  bat->mibs[0] = mibs[0];
                  bat->mibs[1] = mibs[1];
                  bat->mibs[2] = mibs[2];
                  bat->present = 1;
                  list = eina_list_append(list, bat);
               }
          }
     }
#elif defined(__FreeBSD__)
#elif defined(__linux__)
   char *type;
   struct dirent **names;
   char *buf;
   char path[PATH_MAX];
   int i, n;

   n = scandir("/sys/class/power_supply", &names, 0, alphasort);
   if (n < 0) return 0;

   for (i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", names[i]->d_name);
        type = file_contents(path);
        if (type)
          {
             if (!strncmp(type, "Battery", 7))
               {
                  Battery *bat = calloc(1, sizeof(Battery));
                  if (bat)
                    {
                       bat->name = strdup(names[i]->d_name);
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/manufacturer", names[i]->d_name);
                       bat->vendor = file_contents(path);
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/model_name", names[i]->d_name);
                       bat->model = file_contents(path);
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/present", names[i]->d_name);
                       bat->present = 1;
                       buf = file_contents(path);
                       if (buf)
                         {
                            bat->present = atoi(buf);
                            free(buf);
                         }
                       list = eina_list_append(list, bat);
                    }
               }
             free(type);
          }
        free(names[i]);
     }

   free(names);
#endif
   return list;
}

void
battery_free(Battery *bat)
{
   free(bat->name);
   free(bat->model);
   free(bat->vendor);
   free(bat);
}

void
battery_check(Battery *bat)
{
   double charge_full = 0, charge_current = 0;
#if defined(__OpenBSD__)
   size_t slen = sizeof(struct sensor);
   struct sensor snsr;

   bat->mibs[3] = SENSOR_WATTHOUR;
   bat->mibs[4] = 0;

   if (sysctl(bat->mibs, 5, &snsr, &slen, NULL, 0) != -1)
     charge_full = (double) snsr.value;

   bat->mibs[4] = 3;

   if (sysctl(bat->mibs, 5, &snsr, &slen, NULL, 0) != -1)
     charge_current = (double) snsr.value;

   if ((!charge_current) || (!charge_full))
     {

        bat->mibs[3] = SENSOR_AMPHOUR;
        bat->mibs[4] = 0;

        if (sysctl(bat->mibs, 5, &snsr, &slen, NULL, 0) != -1)
          charge_full = (double) snsr.value;

        bat->mibs[4] = 3;

        if (sysctl(bat->mibs, 5, &snsr, &slen, NULL, 0) != -1)
          charge_current = (double) snsr.value;
     }

#elif defined(__FreeBSD__)

#elif defined(__linux__)
   char path[PATH_MAX];
   struct dirent *dh;
   struct stat st;
   DIR *dir;
   char *buf, *link = NULL, *naming = NULL;

   naming = NULL;

   snprintf(path, sizeof(path), "/sys/class/power_supply/%s", bat->name);

   if ((stat(path, &st) < 0) || (!S_ISDIR(st.st_mode)))
     return;

   link = realpath(path, NULL);
   if (!link) return;

   dir = opendir(path);
   if (!dir)
     goto done;

   while ((dh = readdir(dir)) != NULL)
     {
        char *e;
        if (dh->d_name[0] == '.') continue;

        if ((e = strstr(dh->d_name, "_full\0")))
         {
            naming = strndup(dh->d_name, e - dh->d_name);
            break;
         }
     }
   closedir(dir);

   if (naming)
     {
        snprintf(path, sizeof(path), "%s/%s_full", link, naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_full = atol(buf);
             free(buf);
          }
        snprintf(path, sizeof(path), "%s/%s_now", link, naming);
        buf = file_contents(path);
        if (buf)
          {
             charge_current = atol(buf);
             free(buf);
          }
        free(naming);
     }
   else
     {
        // Fallback to "coarse" representation.
        snprintf(path, sizeof(path), "%s/capacity_level", link);
        buf = file_contents(path);
        if (buf)
          {
             if (buf[0] == 'F')
               bat->charge_current = 100;
             else if (buf[0] == 'H')
               bat->charge_current = 75;
             else if (buf[0] == 'N')
               bat->charge_current = 50;
             else if (buf[0] == 'L')
               bat->charge_current = 25;
             else if (buf[0] == 'C')
              bat->charge_current = 5;
             free(buf);
          }
     }
done:
   if (link) free(link);

#else
#endif

    bat->charge_full = charge_full;
    bat->charge_current = charge_current;

    if (charge_full && charge_current)
      bat->percent = 100 * (charge_full / charge_current);
}


Eina_Bool
power_ac(void)
{
   Eina_Bool have_ac = 0;
#if defined(__OpenBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);

   power->mibs[3] = 9;
   power->mibs[4] = 0;

   if (sysctl(power->mibs, 5, &snsr, &slen, NULL, 0) != -1)
     have_ac = (int)snsr.value;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int val, fd;

   fd = open("/dev/acpi", O_RDONLY);
   if (fd != -1)
     {
        if (ioctl(fd, ACPIIO_ACAD_GET_STATUS, &val) != -1)
          have_ac = val;
        close(fd);
     }
#elif defined(__linux__)
   char *buf = file_contents("/sys/class/power_supply/AC/online");
   if (buf)
     {
        have_ac = atoi(buf);
        free(buf);
     }
#endif

   return have_ac;
}

