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

#if defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
static net_iface_t **
_freebsd_generic_network_status(int *n)
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

   net_iface_t **ifaces = NULL;

   for (i = 1; i <= count; i++) {
        int mib[] = {
           CTL_NET, PF_LINK, NETLINK_GENERIC, IFMIB_IFDATA, i, IFDATA_GENERAL
        };
        len = sizeof(*ifmd);
        if (sysctl(mib, 6, ifmd, &len, NULL, 0) < 0) continue;

        net_iface_t *iface = malloc(sizeof(net_iface_t));
        if (iface)
          {
             snprintf(iface->name, sizeof(iface->name), "%s",
                      ifmd->ifmd_name);
             iface->xfer.in = ifmd->ifmd_data.ifi_ibytes;
             iface->xfer.out = ifmd->ifmd_data.ifi_obytes;
             void *t = realloc(ifaces, (1 + 1 + *n) * sizeof(net_iface_t *));
             ifaces = t;
             ifaces[(*n)++] = iface;
           }
     }
   free(ifmd);

   return ifaces;
}

#endif

#if defined(__OpenBSD__)
static net_iface_t **
_openbsd_generic_network_status(int *n)
{
   struct ifaddrs *interfaces, *ifa;

   if (getifaddrs(&interfaces) < 0)
     return NULL;

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
     return NULL;

   net_iface_t **ifaces = NULL;

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

        net_iface_t *iface = malloc(sizeof(net_iface_t));
        if (iface)
          {
             snprintf(iface->name, sizeof(iface->name), "%s",
                      ifa->ifa_name);
             iface->xfer.in = ifi->ifi_ibytes;
             iface->xfer.out = ifi->ifi_obytes;
             void *t = realloc(ifaces, (1 + 1 + *n) * sizeof(net_iface_t *));
             ifaces = t;
             ifaces[(*n)++] = iface;
          }
     }
   close(sock);

   return ifaces;
}

#endif

#if defined(__linux__)
static net_iface_t **
_linux_generic_network_status(int *n)
{
   FILE *f;
   char buf[4096], name[128];
   unsigned long int tmp_in, tmp_out, dummy;

   f = fopen("/proc/net/dev", "r");
   if (!f) return NULL;

   net_iface_t **ifaces = NULL;

   while (fgets(buf, sizeof(buf), f))
     {
        if (17 == sscanf(buf, "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                         "%lu %lu %lu %lu %lu\n", name, &tmp_in, &dummy,
                         &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &tmp_out, &dummy, &dummy, &dummy, &dummy, &dummy,
                         &dummy, &dummy))
          {
             net_iface_t *iface = malloc(sizeof(net_iface_t));
             if (iface)
               {
                  name[strlen(name)-1] = '\0';
                  snprintf(iface->name, sizeof(iface->name), "%s", name);
                  iface->xfer.in = tmp_in;
                  iface->xfer.out = tmp_out;
                  void *t = realloc(ifaces, (1 + 1 + *n) * sizeof(net_iface_t *));
                  ifaces = t;
                  ifaces[(*n)++] = iface;
               }
          }
     }

   fclose(f);

   return ifaces;
}

#endif

net_iface_t **
system_network_ifaces_get(int *n)
{
   *n = 0;
#if defined(__linux__)
   return _linux_generic_network_status(n);
#elif defined(__MacOS__) || defined(__FreeBSD__) || defined(__DragonFly__)
   return _freebsd_generic_network_status(n);
#elif defined(__OpenBSD__)
   return _openbsd_generic_network_status(n);
#else
   return NULL;
#endif
}

