/*
 * Copyright (c) 2018 Alastair Roy Poole <netstar@netstar.im>
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

#if defined(__linux__)
#include <stddef.h>
#include <netinet/in.h>
#include <linux/inet_diag.h>
#include <linux/sock_diag.h>
#include <linux/tcp.h>
#include <linux/rtnetlink.h>
#endif

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
             if (!t)
               {
                  free(iface);
                  continue;
               }
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
     {
        return NULL;
     }

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
          continue;

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
             if (!t)
               {
                  free(iface);
                  continue;
               }
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
                  size_t len = strlen(name);
                  if (!len)
                    {
                       free(iface);
                       continue;
                    }
                  name[len - 1] = '\0';
                  snprintf(iface->name, sizeof(iface->name), "%s", name);
                  iface->xfer.in = tmp_in;
                  iface->xfer.out = tmp_out;
                  void *t = realloc(ifaces, (1 + 1 + *n) * sizeof(net_iface_t *));
                  if (!t)
                    {
                       free(iface);
                       continue;
                    }
                  ifaces = t;
                  ifaces[(*n)++] = iface;
               }
          }
     }

   fclose(f);

   return ifaces;
}

typedef struct {
   unsigned long inode;
   uint64_t in;
   uint64_t out;
} Linux_Proc_Socket_Stat;

static int
_linux_proc_socket_stat_cmp(const void *a, const void *b)
{
   const Linux_Proc_Socket_Stat *sa = a;
   const Linux_Proc_Socket_Stat *sb = b;

   if (sa->inode < sb->inode) return -1;
   if (sa->inode > sb->inode) return 1;
   return 0;
}

static bool
_linux_proc_socket_stat_add(Linux_Proc_Socket_Stat **stats, int *count, int *capacity, unsigned long inode, uint64_t in, uint64_t out)
{
   Linux_Proc_Socket_Stat *tmp;
   int next_capacity;

   if (*count >= *capacity)
     {
        next_capacity = *capacity ? *capacity * 2 : 256;
        tmp = realloc(*stats, next_capacity * sizeof(**stats));
        if (!tmp) return false;
        *stats = tmp;
        *capacity = next_capacity;
     }

   (*stats)[*count].inode = inode;
   (*stats)[*count].in = in;
   (*stats)[*count].out = out;
   (*count)++;

   return true;
}

static void
_linux_proc_socket_diag_collect(int family, Linux_Proc_Socket_Stat **stats, int *count, int *capacity)
{
   int fd;
   ssize_t sent;
   struct
   {
      struct nlmsghdr nlh;
      struct inet_diag_req_v2 req;
   } request;
   char buffer[8192];

   fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
   if (fd < 0) return;

   memset(&request, 0, sizeof(request));
   request.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(request.req));
   request.nlh.nlmsg_type = SOCK_DIAG_BY_FAMILY;
   request.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
   request.req.sdiag_family = family;
   request.req.sdiag_protocol = IPPROTO_TCP;
   request.req.idiag_states = -1;
   request.req.idiag_ext = (1 << (INET_DIAG_INFO - 1));

   sent = send(fd, &request, request.nlh.nlmsg_len, 0);
   if (sent < 0)
     {
        close(fd);
        return;
     }

   while (1)
     {
        ssize_t len = recv(fd, buffer, sizeof(buffer), 0);
        struct nlmsghdr *nlh;

        if (len <= 0) break;

        for (nlh = (struct nlmsghdr *) buffer; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len))
          {
             struct inet_diag_msg *diag;
             struct rtattr *attr;
             int attr_len;

             if (nlh->nlmsg_type == NLMSG_DONE)
               {
                  close(fd);
                  return;
               }
             if (nlh->nlmsg_type == NLMSG_ERROR)
               {
                  close(fd);
                  return;
               }
             if (nlh->nlmsg_type != SOCK_DIAG_BY_FAMILY) continue;
             if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*diag))) continue;

             diag = NLMSG_DATA(nlh);
             attr_len = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*diag));
             if (attr_len <= 0) continue;

             for (attr = (struct rtattr *) (diag + 1); RTA_OK(attr, attr_len); attr = RTA_NEXT(attr, attr_len))
               {
                  struct tcp_info *tcpi;
                  size_t tcpi_len;
                  uint64_t in = 0, out = 0;

                  if (attr->rta_type != INET_DIAG_INFO) continue;

                  tcpi = RTA_DATA(attr);
                  tcpi_len = RTA_PAYLOAD(attr);

                  if (tcpi_len >= (offsetof(struct tcp_info, tcpi_bytes_received) + sizeof(tcpi->tcpi_bytes_received)))
                    in = tcpi->tcpi_bytes_received;
                  if (tcpi_len >= (offsetof(struct tcp_info, tcpi_bytes_acked) + sizeof(tcpi->tcpi_bytes_acked)))
                    out = tcpi->tcpi_bytes_acked;

                  if ((!in) && (!out)) continue;
                  if (!_linux_proc_socket_stat_add(stats, count, capacity, diag->idiag_inode, in, out))
                    {
                       close(fd);
                       return;
                    }
               }
          }
     }

   close(fd);
}

static Linux_Proc_Socket_Stat *
_linux_proc_socket_stats_get(int *count)
{
   Linux_Proc_Socket_Stat *stats;
   int capacity = 0, out_count = 0;

   *count = 0;

   stats = NULL;
   _linux_proc_socket_diag_collect(AF_INET, &stats, &out_count, &capacity);
   _linux_proc_socket_diag_collect(AF_INET6, &stats, &out_count, &capacity);

   if (!out_count) return stats;

   qsort(stats, out_count, sizeof(*stats), _linux_proc_socket_stat_cmp);

   int write = 0;
   for (int read = 0; read < out_count; read++)
     {
        if ((write > 0) && (stats[read].inode == stats[write - 1].inode))
          {
             stats[write - 1].in += stats[read].in;
             stats[write - 1].out += stats[read].out;
             continue;
          }
        if (write != read) stats[write] = stats[read];
        write++;
     }

   *count = write;
   return stats;
}

static const Linux_Proc_Socket_Stat *
_linux_proc_socket_stat_find(const Linux_Proc_Socket_Stat *stats, int count, unsigned long inode)
{
   Linux_Proc_Socket_Stat key;
   key.inode = inode;
   key.in = 0;
   key.out = 0;
   return bsearch(&key, stats, count, sizeof(*stats), _linux_proc_socket_stat_cmp);
}

static bool
_linux_proc_net_usage_add(proc_net_t ***procs, int *n, pid_t pid, uint64_t in, uint64_t out)
{
   proc_net_t **tmp;
   proc_net_t *proc;

   tmp = realloc(*procs, (*n + 1) * sizeof(**procs));
   if (!tmp) return false;
   *procs = tmp;

   proc = calloc(1, sizeof(*proc));
   if (!proc) return false;

   proc->pid = pid;
   proc->in = in;
   proc->out = out;

   (*procs)[*n] = proc;
   (*n)++;

   return true;
}

static proc_net_t **
_linux_process_network_usage_get(int *n)
{
   DIR *dir;
   struct dirent *entry;
   Linux_Proc_Socket_Stat *sockets;
   int socket_count;
   proc_net_t **procs = NULL;

   *n = 0;

   sockets = _linux_proc_socket_stats_get(&socket_count);
   if ((!sockets) || (!socket_count))
     {
        free(sockets);
        return NULL;
     }

   dir = opendir("/proc");
   if (!dir)
     {
        free(sockets);
        return NULL;
     }

   while ((entry = readdir(dir)))
     {
        char *end = NULL;
        unsigned long pid_ul;
        pid_t pid;
        char fd_dir[PATH_MAX];
        DIR *fd;
        struct dirent *fd_entry;
        uint64_t in = 0, out = 0;

        if (!isdigit((unsigned char) entry->d_name[0])) continue;

        pid_ul = strtoul(entry->d_name, &end, 10);
        if (!end || *end) continue;
        if (pid_ul > INT_MAX) continue;
        pid = (pid_t) pid_ul;

        snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
        fd = opendir(fd_dir);
        if (!fd) continue;

        while ((fd_entry = readdir(fd)))
          {
             char link_path[PATH_MAX];
             char target[256];
             ssize_t len;
             unsigned long inode;
             const Linux_Proc_Socket_Stat *sock;

             if (fd_entry->d_name[0] == '.') continue;

             size_t base_len = strlen(fd_dir);
             size_t name_len = strlen(fd_entry->d_name);
             if ((base_len + 1 + name_len + 1) > sizeof(link_path)) continue;
             memcpy(link_path, fd_dir, base_len);
             link_path[base_len] = '/';
             memcpy(link_path + base_len + 1, fd_entry->d_name, name_len);
             link_path[base_len + 1 + name_len] = '\0';
             len = readlink(link_path, target, sizeof(target) - 1);
             if (len <= 0) continue;

             target[len] = '\0';
             if (strncmp(target, "socket:[", 8)) continue;

             inode = strtoul(target + 8, &end, 10);
             if (!end || (*end != ']')) continue;

             sock = _linux_proc_socket_stat_find(sockets, socket_count, inode);
             if (!sock) continue;

             in += sock->in;
             out += sock->out;
          }

        closedir(fd);

        if ((!in) && (!out)) continue;
        if (!_linux_proc_net_usage_add(&procs, n, pid, in, out)) break;
     }

   closedir(dir);
   free(sockets);

   return procs;
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

proc_net_t **
system_network_process_usage_get(int *n)
{
   *n = 0;
#if defined(__linux__)
   return _linux_process_network_usage_get(n);
#else
   return NULL;
#endif
}

void
system_network_process_usage_free(proc_net_t **procs, int n)
{
   if (!procs) return;

   for (int i = 0; i < n; i++)
     free(procs[i]);

   free(procs);
}
