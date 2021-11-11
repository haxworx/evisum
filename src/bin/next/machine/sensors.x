Eina_Bool
sensor_update(Sensor *sensor)
{
#if defined(__linux__)
   char *d = file_contents(sensor->path);
   if (d)
     {
        double val = atof(d);
        if (sensor->type == THERMAL)
          sensor->value = val /= 1000;
        else if (sensor->type == FANRPM)
          sensor->value = val;
        free(d);
        return 1;
     }
   return 0;
#elif defined(__OpenBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);

   if (sensor->invalid)
     {
        sensor->value = 0;
        return 0;
     }

   if (sysctl(sensor->mibs, 5, &snsr, &slen, NULL, 0) == -1) return 0;

   if (sensor->type == THERMAL)
     sensor->value = (snsr.value - 273150000) / 1000000.0;
   else if (sensor->type == FANRPM)
     sensor->value = snsr.value;

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
   enum sensor_type type;

   for (devn = 0;; devn++)
      {
        mibs[2] = devn;

        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENOENT) break;
             continue;
          }
        for (type = 0; type < SENSOR_MAX_TYPES; type++)
          {
             mibs[3] = type;
             for (n = 0; n < snsrdev.sensors_count; n++) {
                  mibs[4] = n;

                  if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) == -1)
                    continue;

                  if (!slen || ((snsr.type != SENSOR_TEMP) && (snsr.type != SENSOR_FANRPM)))
                    continue;

                  if ((snsr.flags & SENSOR_FINVALID)) continue;

                  sensor = calloc(1, sizeof(Sensor));
                  if (sensor)
                    {
                       strlcpy(sensor->name, snsrdev.xname, sizeof(sensor->name));
                       if (snsr.type == SENSOR_TEMP)
                         {
                            snprintf(sensor->child_name, sizeof(sensor->child_name), "temp%i", n);
                            sensor->type = THERMAL;
                         }
                       else if (snsr.type == SENSOR_FANRPM)
                         {
                            snprintf(sensor->child_name, sizeof(sensor->child_name), "fan%i", n);
                            sensor->type = FANRPM;
                         }
                       memcpy(sensor->mibs, &mibs, sizeof(mibs));

                       sensors = eina_list_append(sensors, sensor);
                    }
               }
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
             strlcpy(sensor->name, "hw.acpi.thermal.tz0.temperature", sizeof(sensor->name));
             strlcpy(sensor->child_name, "0", sizeof(sensor->child_name));
             sensor->value = (float) (value -  2732) / 10;
             sensors = eina_list_append(sensors, sensor);
          }
     }

   int n = cores_count();

   for (int i = 0; i < n; i++)
     {
        len = sizeof(value);
        snprintf(buf, sizeof(buf), "dev.cpu.%i.temperature", i);
        if ((sysctlbyname(buf, &value, &len, NULL, 0)) != -1)
          {
             sensor = calloc(1, sizeof(Sensor));
             if (sensor)
               {
                  strlcpy(sensor->name, buf, sizeof(sensor->name));
                  snprintf(sensor->child_name, sizeof(sensor->child_name), "%i", i);
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
             if ((!strncmp(names[i]->d_name, "temp", 4)) || (!strncmp(names[i]->d_name, "fan", 3)))
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
                  snprintf(buf, sizeof(buf), "%s/fan%i_input", link, id);
                  if (ecore_file_exists(buf))
                    {
                       sensor = calloc(1, sizeof(Sensor));
                       if (sensor)
                         {
                            char *d;
                            sensor->type = FANRPM;
                            eina_strlcpy(sensor->path, buf, sizeof(sensor->path));
                            snprintf(buf, sizeof(buf), "%s/name", link);
                            d = file_contents(buf);
                            if (d)
                              {
                                 eina_strlcpy(sensor->name, d, sizeof(sensor->name));
                                 free(d);
                              }
                            snprintf(buf, sizeof(buf), "%s/fan%i_label", link, id);
                            if (ecore_file_exists(buf))
                              {
                                 d = file_contents(buf);
                                 if (d)
                                   {
                                      eina_strlcpy(sensor->child_name, d, sizeof(sensor->child_name));
                                      free(d);
                                   }
                              }
                            else
                              snprintf(sensor->child_name, sizeof(sensor->child_name), "fan%i", id);

                            sensors = eina_list_append(sensors, sensor);
                         }
                    }

                  snprintf(buf, sizeof(buf), "%s/temp%i_input", link, id);
                  if (ecore_file_exists(buf))
                    {
                       sensor = calloc(1, sizeof(Sensor));
                       if (sensor)
                         {
                            char *d;
                            sensor->type = THERMAL;
                            eina_strlcpy(sensor->path, buf, sizeof(sensor->path));
                            snprintf(buf, sizeof(buf), "%s/name", link);
                            d = file_contents(buf);
                            if (d)
                              {
                                 eina_strlcpy(sensor->name, d, sizeof(sensor->name));
                                 free(d);
                              }
                            snprintf(buf, sizeof(buf), "%s/temp%i_label", link, id);
                            if (ecore_file_exists(buf))
                              {
                                 d = file_contents(buf);
                                 if (d)
                                   {
                                      eina_strlcpy(sensor->child_name, d, sizeof(sensor->child_name));
                                      free(d);
                                   }
                              }
                             else
                               snprintf(sensor->child_name, sizeof(sensor->child_name), "temp%i", id);

                             sensors = eina_list_append(sensors, sensor);
                          }
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

void
sensors_update(Eina_List *sensors)
{
   Eina_List *l;
   Sensor *sensor;

   EINA_LIST_FOREACH(sensors, l, sensor)
     sensor_update(sensor);
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
                  strlcpy(bat->name, snsrdev.xname, sizeof(bat->name));
                  strlcpy(bat->model, "Unknown", sizeof(bat->model));
                  strlcpy(bat->vendor, "Unknown", sizeof(bat->vendor));
                  bat->mibs[0] = mibs[0];
                  bat->mibs[1] = mibs[1];
                  bat->mibs[2] = mibs[2];
                  bat->present = 1;
                  list = eina_list_append(list, bat);
               }
          }
     }
#elif defined(__FreeBSD__)
   int n_units, fd;
   union acpi_battery_ioctl_arg battio;

   fd = open("/dev/acpi", O_RDONLY);
   if (fd == -1) return NULL;

   if (ioctl(fd, ACPIIO_BATT_GET_UNITS, &n_units) == -1)
     return NULL;

   for (int i = 0; i < n_units; i++)
     {
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BIX, &battio) != -1)
          {
             Battery *bat = calloc(1, sizeof(Battery));
             if (bat)
               {
                  snprintf(bat->name, sizeof(bat->name), "bat%i", i);
                  if (battio.bst.state == ACPI_BATT_STAT_NOT_PRESENT)
                    bat->present = 0;
                  else
                    bat->present = 1;
                  strlcpy(bat->vendor, battio.bix.oeminfo, sizeof(bat->vendor));
                  strlcpy(bat->model, battio.bix.model, sizeof(bat->model));
                  bat->unit = i;
                  list = eina_list_append(list, bat);
               }
          }
     }
   close(fd);
#elif defined(__linux__)
   char *type;
   struct dirent **names;
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
                       char *d;
                       eina_strlcpy(bat->name, names[i]->d_name, sizeof(bat->name));
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/manufacturer", names[i]->d_name);
                       d = file_contents(path);
                       if (d)
                         {
                            eina_strlcpy(bat->vendor, d, sizeof(bat->vendor));
                            free(d);
                         }
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/model_name", names[i]->d_name);
                       d = file_contents(path);
                       if (d)
                         {
                            eina_strlcpy(bat->model, d, sizeof(bat->model));
                            free(d);
                         }
                       snprintf(path, sizeof(path), "/sys/class/power_supply/%s/present", names[i]->d_name);
                       bat->present = 1;
                       d = file_contents(path);
                       if (d)
                         {
                            bat->present = atoi(d);
                            free(d);
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
battery_update(Battery *bat)
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

#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int fd;
   union acpi_battery_ioctl_arg battio;

   if (!bat->present) return;

   fd = open("/dev/acpi", O_RDONLY);
   if (fd == -1) return;

   battio.unit = bat->unit;
   if (ioctl(fd, ACPIIO_BATT_GET_BIX, &battio) != -1)
     {
        if (battio.bif.lfcap == 0)
          charge_full = battio.bif.dcap;
        else
          charge_full = battio.bif.lfcap;
        battio.unit = bat->unit;
        if (ioctl(fd, ACPIIO_BATT_GET_BST, &battio) != -1)
          charge_current = battio.bst.cap;
     }

   close(fd);
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

    if (charge_full)
      bat->percent = 100 * (charge_current / charge_full);
    else
      bat->percent = 0;
}

Eina_Bool
power_ac_present(void)
{
   Eina_Bool have_ac = 0;
#if defined(__OpenBSD__)
   struct sensordev snsrdev;
   size_t sdlen = sizeof(struct sensordev);
   static int mibs[5] = { CTL_HW, HW_SENSORS, 0, 0, 0 };
   int devn;

   for (devn = 0; !mibs[3] ; devn++)
     {
        mibs[2] = devn;
        if (sysctl(mibs, 3, &snsrdev, &sdlen, NULL, 0) == -1)
          {
             if (errno == ENXIO) continue;
             if (errno == ENOENT) break;
          }
        if (!strncmp(snsrdev.xname, "acpiac", 6))
          {
             mibs[3] = 9;
             mibs[4] = 0;
             break;
          }
     }
   if (mibs[3] == 9)
     {
        struct sensor snsr;
        size_t slen = sizeof(struct sensor);
        if (sysctl(mibs, 5, &snsr, &slen, NULL, 0) != -1)
          have_ac = (int)snsr.value;
     }
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
   static const char *found = NULL;
   static const char *known[] = {
      "/sys/class/power_supply/AC/online",
      "/sys/class/power_supply/ACAD/online",
      "/sys/class/power_supply/ADP0/online",
   };

   for (int i = 0; (!found) && (i < sizeof(known) / sizeof(char *)); i++)
     {
        if (ecore_file_exists(known[i]))
          {
             found = known[i];
             break;
          }
     }

   if (found)
     {
        char *buf = file_contents(found);
        if (buf)
          {
             have_ac = atoi(buf);
             free(buf);
          }
     }
#endif

   return have_ac;
}

void
batteries_update(Eina_List *batteries)
{
   Eina_List *l;
   Battery *bat;

   EINA_LIST_FOREACH(batteries, l, bat)
      battery_update(bat);
}

