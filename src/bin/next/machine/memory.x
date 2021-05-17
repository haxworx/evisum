
#if defined(__linux__)
static unsigned long
_meminfo_parse_line(const char *line)
{
   char *p, *tok;

   p = strchr(line, ':') + 1;
   while (isspace(*p))
     p++;
   tok = strtok(p, " ");

   return atoll(tok);
}

#endif

void
memory_info(Meminfo *memory)
{
   memset(memory, 0, sizeof(Meminfo));
#if defined(__linux__)
   FILE *f;
   unsigned long swap_free = 0, tmp_free = 0, tmp_slab = 0;
   char line[256];
   int i, fields = 0;

   f = fopen("/proc/meminfo", "r");
   if (!f) return;

   while (fgets(line, sizeof(line), f) != NULL)
     {
        if (!strncmp("MemTotal:", line, 9))
          {
             memory->total = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("MemFree:", line, 8))
          {
             tmp_free = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Cached:", line, 7))
          {
             memory->cached = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Slab:", line, 5))
          {
             tmp_slab = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Buffers:", line, 8))
          {
             memory->buffered = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("Shmem:", line, 6))
          {
             memory->shared = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("SwapTotal:", line, 10))
          {
             memory->swap_total = _meminfo_parse_line(line);
             fields++;
          }
        else if (!strncmp("SwapFree:", line, 9))
          {
             swap_free = _meminfo_parse_line(line);
             fields++;
          }

        if (fields >= 8)
          break;
     }

   memory->cached += tmp_slab;
   memory->used = memory->total - tmp_free - memory->cached - memory->buffered;
   memory->swap_used = memory->swap_total - swap_free;

   memory->total *= 1024;
   memory->used *= 1024;
   memory->buffered *= 1024;
   memory->cached *= 1024;
   memory->shared *= 1024;
   memory->swap_total *= 1024;
   memory->swap_used *= 1024;

   fclose(f);
   for (i = 0; i < MEM_VIDEO_CARD_MAX; i++)
     {
        struct stat st;
        char buf[256];

        // if no more drm devices exist - end of card list
        snprintf(buf, sizeof(buf),
                 "/sys/class/drm/card%i/device", i);
        if (stat(buf, &st) != 0) break;
        // not all drivers expose this, so video devices with no exposed video
        // ram info will appear as 0 sized... much like swap.
        snprintf(buf, sizeof(buf),
                 "/sys/class/drm/card%i/device/mem_info_vram_total", i);
        f = fopen(buf, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f))
               memory->video[i].total = atoll(buf);
             fclose(f);
          }
        snprintf(buf, sizeof(buf),
                 "/sys/class/drm/card%i/device/mem_info_vram_used", i);
        f = fopen(buf, "r");
        if (f)
          {
             if (fgets(buf, sizeof(buf), f))
               memory->video[i].used = atoll(buf);
             fclose(f);
          }
     }
   memory->video_count = i;

#elif defined(__FreeBSD__) || defined(__DragonFly__)
   unsigned int free = 0, active = 0, inactive = 0, wired = 0;
   unsigned int cached = 0, buffered = 0, zfs_arc = 0;
   long int result = 0;
   int page_size = getpagesize();
   int mib[5] = { CTL_HW, HW_PHYSMEM, 0, 0, 0 };
   size_t miblen, len = 0;

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;

   if ((active = _sysctlfromname("vm.stats.vm.v_active_count", mib, 4, &len)) == -1)
     return;

   if ((inactive = _sysctlfromname("vm.stats.vm.v_inactive_count", mib, 4, &len)) == -1)
     return;

   if ((wired = _sysctlfromname("vm.stats.vm.v_wire_count", mib, 4, &len)) == -1)
     return;

   if ((cached = _sysctlfromname("vm.stats.vm.v_cache_count", mib, 4, &len)) == -1)
     return;

   if ((free = _sysctlfromname("vm.stats.vm.v_free_count", mib, 4, &len)) == -1)
     return;

   if ((buffered = _sysctlfromname("vfs.bufspace", mib, 2, &len)) == -1)
     return;

   if ((zfs_arc = _sysctlfromname("kstat.zfs.misc.arcstats.c", mib, 5, &len)) != -1)
     memory->zfs_arc_used = zfs_arc;

   memory->used = ((active + wired + cached) * page_size);
   memory->buffered = buffered;
   memory->cached = (cached * page_size);

   result = _sysctlfromname("vm.swap_total", mib, 2, &len);
   if (result == -1)
     return;

   memory->swap_total = result;

   miblen = 3;
   if (sysctlnametomib("vm.swap_info", mib, &miblen) == -1) return;

   struct xswdev xsw;

   for (int i = 0; ; i++) {
        mib[miblen] = i;
        len = sizeof(xsw);
        if (sysctl(mib, miblen + 1, &xsw, &len, NULL, 0) == -1)
          break;

        memory->swap_used += (unsigned long) xsw.xsw_used * page_size;
     }
#elif defined(__OpenBSD__)
   static int mib[] = { CTL_HW, HW_PHYSMEM64 };
   static int bcstats_mib[] = { CTL_VFS, VFS_GENERIC, VFS_BCACHESTAT };
   struct bcachestats bcstats;
   static int uvmexp_mib[] = { CTL_VM, VM_UVMEXP };
   struct uvmexp uvmexp;
   int nswap, rnswap;
   struct swapent *swdev = NULL;
   size_t len;

   len = sizeof(memory->total);
   if (sysctl(mib, 2, &memory->total, &len, NULL, 0) == -1)
     return;

   len = sizeof(uvmexp);
   if (sysctl(uvmexp_mib, 2, &uvmexp, &len, NULL, 0) == -1)
     return;

   len = sizeof(bcstats);
   if (sysctl(bcstats_mib, 3, &bcstats, &len, NULL, 0) == -1)
     return;

   nswap = swapctl(SWAP_NSWAP, 0, 0);
   if (nswap == 0)
     goto swap_out;

   swdev = calloc(nswap, sizeof(*swdev));
   if (swdev == NULL)
     goto swap_out;

   rnswap = swapctl(SWAP_STATS, swdev, nswap);
   if (rnswap == -1)
     goto swap_out;

   for (int i = 0; i < nswap; i++) {
        if (swdev[i].se_flags & SWF_ENABLE)
          {
             memory->swap_used += (swdev[i].se_inuse / (1024 / DEV_BSIZE));
             memory->swap_total += (swdev[i].se_nblks / (1024 / DEV_BSIZE));
          }
     }

   memory->swap_total *= 1024;
   memory->swap_used *= 1024;
swap_out:
   if (swdev)
     free(swdev);
   memory->cached = MEMSIZE(uvmexp.pagesize) * MEMSIZE(bcstats.numbufpages);
   memory->used = MEMSIZE(uvmexp.pagesize) * MEMSIZE(uvmexp.active);
   memory->buffered = MEMSIZE(uvmexp.pagesize) * (MEMSIZE(uvmexp.npages) - MEMSIZE(uvmexp.free));
   memory->shared = MEMSIZE(uvmexp.pagesize) * MEMSIZE(uvmexp.wired);
#elif defined(__MacOS__)
   int mib[2] = { CTL_HW, HW_MEMSIZE };
   size_t total;
   vm_size_t page_size;
   mach_port_t mach_port;
   mach_msg_type_number_t count;
   vm_statistics64_data_t vm_stats;
   struct xsw_usage xsu;

   size_t len = sizeof(size_t);
   if (sysctl(mib, 2, &total, &len, NULL, 0) == -1)
     return;
   mach_port = mach_host_self();
   count = sizeof(vm_stats) / sizeof(natural_t);

   memory->total = total;

   if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
       host_statistics64(mach_port, HOST_VM_INFO,
                 (host_info64_t)&vm_stats, &count) == KERN_SUCCESS)
     {
        memory->used = (vm_stats.active_count + vm_stats.wire_count) * page_size;
        memory->cached = vm_stats.active_count * page_size;
        memory->shared = vm_stats.wire_count * page_size;
        memory->buffered = vm_stats.inactive_count * page_size;
     }

   total = sizeof(xsu);
   if (sysctlbyname("vm.swapusage", &xsu, &total, NULL, 0) != -1)
     {
        memory->swap_total = xsu.xsu_total;
        memory->swap_used = xsu.xsu_used;
     }
#endif
}

