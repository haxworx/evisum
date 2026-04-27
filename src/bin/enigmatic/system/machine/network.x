#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
static Eina_List *
_freebsd_generic_network_status(void)
{
   struct ifmibdata *ifmd;
   size_t len;
   int i, count;
   len = sizeof(count);

   if (sysctlbyname("net.link.generic.system.ifcount", &count, &len, NULL, 0) == -1)
     return NULL;

   ifmd = malloc(sizeof(struct ifmibdata));
   if (!ifmd)
     return NULL;

   Eina_List *list = NULL;

   for (i = 1; i <= count; i++) {
        int mib[] = {
           CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL
        };
        len = sizeof(*ifmd);
        if (sysctl(mib, 6, ifmd, &len, NULL, 0) < 0) continue;

        Network_Interface *iface = calloc(1, sizeof(Network_Interface));
        if (iface)
          {
             snprintf(iface->name, sizeof(iface->name), "%s",
                      ifmd->ifmd_name);
             iface->total_in = ifmd->ifmd_data.ifi_ibytes;
             iface->total_out = ifmd->ifmd_data.ifi_obytes;
             list = eina_list_append(list, iface);
           }
     }
   free(ifmd);

   return list;
}

#endif

#if defined(__OpenBSD__)
static Eina_List *
_openbsd_generic_network_status(void)
{
   struct ifaddrs *interfaces, *ifa;

   if (getifaddrs(&interfaces) < 0)
     return NULL;

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
     return NULL;

   Eina_List *list = NULL;

   for (ifa = interfaces; ifa; ifa = ifa->ifa_next) {
        struct ifreq ifreq;
        struct if_data if_data;

        // TBC: Interfaces can have multiple addresses.
        // Using this netmask check we should be obtaining
        // the overall interface data across addresses.

        if (ifa->ifa_netmask != 0) continue;

        ifreq.ifr_data = (void *)&if_data;
        strncpy(ifreq.ifr_name, ifa->ifa_name, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFDATA, &ifreq) < 0)
          return NULL;

        const struct if_data *ifi = &if_data;
        if (!LINK_STATE_IS_UP(ifi->ifi_link_state))
          continue;

        Network_Interface *iface = calloc(1, sizeof(Network_Interface));
        if (iface)
          {
             snprintf(iface->name, sizeof(iface->name), "%s",
                      ifa->ifa_name);
             iface->total_in = ifi->ifi_ibytes;
             iface->total_out = ifi->ifi_obytes;
             list = eina_list_append(list, iface);
          }
     }
   close(sock);

   return list;
}

#endif

#if defined(__linux__)
static Eina_List *
_linux_generic_network_status(void)
{
   FILE *f;
   char buf[4096], name[128];
   unsigned long int tmp_in, tmp_out, dummy;
   Eina_List *list = NULL;

   f = fopen("/proc/net/dev", "r");
   if (!f) return NULL;

   while (fgets(buf, sizeof(buf), f))
     {
        if (17 == sscanf(buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                         "%lu %lu %lu %lu %lu\n", name, &tmp_in, &dummy,
                         &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &tmp_out, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &dummy, &dummy))
          {
             Network_Interface *iface = calloc(1, sizeof(Network_Interface));
             if (iface)
               {
                  name[strlen(name)-1] = '\0';
                  snprintf(iface->name, sizeof(iface->name), "%s", name);
                  iface->total_in = tmp_in;
                  iface->total_out = tmp_out;
                  list = eina_list_append(list, iface);
               }
          }
     }

   fclose(f);

   return list;
}

#endif

Eina_List *
network_interfaces_find(void)
{
#if defined(__linux__)
   return _linux_generic_network_status();
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
   return _freebsd_generic_network_status();
#elif defined(__OpenBSD__)
   return _openbsd_generic_network_status();
#else
   return NULL;
#endif
}

