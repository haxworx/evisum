/*
 * Copyright (c) 2018 Alastair Roy Poole <netstar@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

void
system_sensor_thermal_free(sensor_t *sensor)
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

void
system_sensors_thermal_free(sensor_t **sensors, int count)
{
   for (int i = 0; i < count; i++)
     {
        sensor_t *sensor = sensors[i];
        system_sensor_thermal_free(sensor);
     }
   if (sensors)
     free(sensors);
}

int
system_sensor_thermal_get(sensor_t *sensor)
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

sensor_t **
system_sensors_thermal_get(int *sensor_count)
{
   sensor_t **sensors = NULL;
   *sensor_count = 0;
#if defined(__OpenBSD__)
   sensor_t *sensor;
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

        void *t = realloc(sensors, (1 + *sensor_count) * sizeof(sensor_t *));
        sensors = t;
        sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));
        sensor->name = strdup(snsrdev.xname);
        sensor->value = (snsr.value - 273150000) / 1000000.0; // (uK -> C)
        memcpy(sensor->mibs, &mibs, sizeof(mibs));
     }
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   sensor_t *sensor;
   int value;
   char buf[256];
   size_t len = sizeof(value);

   if ((sysctlbyname("hw.acpi.thermal.tz0.temperature", &value, &len, NULL, 0)) != -1)
     {
        void *t = realloc(sensors, (1 + *sensor_count) * sizeof(sensor_t *));
        sensors = t;
        sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));
        sensor->name = strdup("hw.acpi.thermal.tz0.temperature");
        sensor->value = (float) (value -  2732) / 10;
     }

   int n = system_cpu_count_get();

   for (int i = 0; i < n; i++)
     {
        len = sizeof(value);
        snprintf(buf, sizeof(buf), "dev.cpu.%i.temperature", i);
        if ((sysctlbyname(buf, &value, &len, NULL, 0)) != -1)
          {
             void *t = realloc(sensors, (1 + *sensor_count) * sizeof(sensor_t *));
             sensors = t;
             sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));
             sensor->name = strdup(buf);
             sensor->value = (float) (value - 2732) / 10;
          }
     }

#elif defined(__linux__)
   sensor_t *sensor;
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

                  void *t = realloc(sensors, (1 + (*sensor_count)) * sizeof(sensor_t *));
                  sensors = t;
                  sensors[(*sensor_count)++] = sensor = calloc(1, sizeof(sensor_t));

                  snprintf(buf, sizeof(buf), "%s/name", link);
                  sensor->name = file_contents(buf);
                  if (sensor->name)
                    strimmer(sensor->name);

                  snprintf(buf, sizeof(buf), "%s/temp%d_label", link, id);
                  sensor->child_name = file_contents(buf);
                  if (sensor->child_name)
                    strimmer(sensor->child_name);

                  snprintf(buf, sizeof(buf), "%s/temp%d_input", link, id);
                  sensor->path = strdup(buf);
                  char *d = file_contents(buf);
                  if (d)
                    {
                       sensor->value = atoi(d);
                       if (sensor->value) sensor->value /= 1000;
                       free(d);
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
#elif defined(__linux__)

#elif defined(__FreeBSD__)

#endif
puts("AYE");
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

static int
_power_battery_count_get(power_t *power)
{
#if defined(__OpenBSD__)
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int n_units, fd;
   char name[256];

   fd = open("/dev/acpi", O_RDONLY);
   if (fd != -1)
     {
        if (ioctl(fd, ACPIIO_BATT_GET_UNITS, &n_units) != -1)
          power->battery_count = n_units;
        close(fd);
     }

   power->batteries = malloc(power->battery_count * sizeof(bat_t **));
   for (int i = 0; i < power->battery_count; i++) {
        power->batteries[i] = calloc(1, sizeof(bat_t));
        snprintf(name, sizeof(name), "hw.acpi.battery.%i", i);
        power->batteries[i]->name = strdup(name);
        power->batteries[i]->present = true;
     }
#elif defined(__linux__)
   char *type;
   struct dirent **names;
   char *buf;
   char path[PATH_MAX];
   int i, n, id;

   n = scandir("/sys/class/power_supply", &names, 0, alphasort);
   if (n < 0) return power->battery_count;

   for (i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/type", names[i]->d_name);
        type = file_contents(path);
        if (type)
          {
             if (!strncmp(type, "Battery", 7))
               {
                  id = power->battery_count;
                  void *t = realloc(power->batteries, (1 +
                                    power->battery_count) * sizeof(bat_t **));
                  power->batteries = t;
                  power->batteries[id] = calloc(1, sizeof(bat_t));
                  power->batteries[id]->name = strdup(names[i]->d_name);
                  snprintf(path, sizeof(path), "/sys/class/power_supply/%s/manufacturer", names[i]->d_name);
                  power->batteries[id]->vendor = file_contents(path);
                  snprintf(path, sizeof(path), "/sys/class/power_supply/%s/model_name", names[i]->d_name);
                  power->batteries[id]->model = file_contents(path);
                  snprintf(path, sizeof(path), "/sys/class/power_supply/%s/present", names[i]->d_name);
                  power->batteries[id]->present = 1;
                  buf = file_contents(path);
                  if (buf)
                    {
                       power->batteries[id]->present = atoi(buf);
                       free(buf);
                    }

                  power->battery_count++;
               }
             free(type);
          }

        free(names[i]);
     }

   free(names);
#endif

   return 0; 
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

#else
#endif

    bat->charge_full = charge_full;
    bat->charge_current = charge_current;

    bat->percent = 100 * (charge_full / charge_current);
}

static void
_battery_state_get(power_t *power)
{
#if defined(__OpenBSD__)
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int fd, i;
   union acpi_battery_ioctl_arg battio;

   if ((fd = open("/dev/acpi", O_RDONLY)) == -1) return;

   for (i = 0; i < power->battery_count; i++) {
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BIX, &battio) != -1)
          {
             if (battio.bif.lfcap == 0)
               power->batteries[i]->charge_full = battio.bif.dcap;
             else
               power->batteries[i]->charge_full = battio.bif.lfcap;
          }
        power->batteries[i]->vendor = strdup(battio.bix.oeminfo);
        power->batteries[i]->model = strdup(battio.bix.model);
        battio.unit = i;
        if (ioctl(fd, ACPIIO_BATT_GET_BST, &battio) != -1)
          power->batteries[i]->charge_current = battio.bst.cap;

        if (battio.bst.state == ACPI_BATT_STAT_NOT_PRESENT)
          power->batteries[i]->present = false;
     }
   close(fd);

#elif defined(__linux__)
   char path[PATH_MAX];
   struct dirent *dh;
   struct stat st;
   DIR *dir;
   char *buf, *link = NULL, *naming = NULL;


   for (int i = 0; i < power->battery_count; i++) {
        naming = NULL;

        snprintf(path, sizeof(path), "/sys/class/power_supply/%s", power->batteries[i]->name);

        if ((stat(path, &st) < 0) || (!S_ISDIR(st.st_mode)))
          continue;

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
                  power->batteries[i]->charge_full = atol(buf);
                  free(buf);
               }
             snprintf(path, sizeof(path), "%s/%s_now", link, naming);
             buf = file_contents(path);
             if (buf)
               {
                  power->batteries[i]->charge_current = atol(buf);
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
                  power->batteries[i]->charge_full = 100;
                  if (buf[0] == 'F')
                    power->batteries[i]->charge_current = 100;
                  else if (buf[0] == 'H')
                    power->batteries[i]->charge_current = 75;
                  else if (buf[0] == 'N')
                    power->batteries[i]->charge_current = 50;
                  else if (buf[0] == 'L')
                    power->batteries[i]->charge_current = 25;
                  else if (buf[0] == 'C')
                    power->batteries[i]->charge_current = 5;
                  free(buf);
               }
          }
     }
done:
   if (link) free(link);
#endif
}

void
system_power_state_get(power_t *power)
{
   memset(power, 0, sizeof(power_t));
#if defined(__OpenBSD__)
   struct sensor snsr;
   size_t slen = sizeof(struct sensor);
#endif

   if (!_power_battery_count_get(power))
     return;

#if defined(__OpenBSD__)
   power->mibs[3] = 9;
   power->mibs[4] = 0;

   if (sysctl(power->mibs, 5, &snsr, &slen, NULL, 0) != -1)
     power->have_ac = (int)snsr.value;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
   int val, fd;

   fd = open("/dev/acpi", O_RDONLY);
   if (fd != -1)
     {
        if (ioctl(fd, ACPIIO_ACAD_GET_STATUS, &val) != -1)
          power->have_ac = val;
        close(fd);
     }
#elif defined(__linux__)
   char *buf = file_contents("/sys/class/power_supply/AC/online");
   if (buf)
     {
        power->have_ac = atoi(buf);
        free(buf);
     }
#endif

   _battery_state_get(power);
}

void
system_power_state_free(power_t *power)
{
}
