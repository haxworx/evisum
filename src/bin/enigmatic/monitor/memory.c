#include "memory.h"
#include "system/file_systems.h"
#include "enigmatic_log.h"

static void
memory_refresh(Enigmatic *enigmatic, Meminfo *mem)
{
   Message msg;
   msg.type = MESG_REFRESH;
   msg.object_type = MEMORY;
   msg.number = 1;
   enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, mem, sizeof(Meminfo));
}

Eina_Bool
enigmatic_monitor_memory(Enigmatic *enigmatic, Meminfo *mem)
{
   Meminfo mem_now;

   memory_info(&mem_now);
   if (file_system_in_use("ZFS"))
     mem_now.used += mem_now.zfs_arc_used;
   if (enigmatic->broadcast)
     {
        memory_refresh(enigmatic, &mem_now);
        memcpy(mem, &mem_now, sizeof(Meminfo));
        return 1;
     }

   Message msg;
   msg.type = MESG_MOD;
   if (mem_now.total != mem->total)
     {
        msg.object_type = MEMORY_TOTAL;
        enigmatic_log_diff(enigmatic, msg, (mem_now.total - mem->total) / 4096);

        mem->total = mem_now.total;
     }
   if (mem_now.used != mem->used)
     {
        msg.object_type = MEMORY_USED;
        enigmatic_log_diff(enigmatic, msg, (mem_now.used - mem->used) / 4096);

        mem->used = mem_now.used;
     }
   if (mem_now.cached != mem->cached)
     {
        msg.object_type = MEMORY_CACHED;
        enigmatic_log_diff(enigmatic, msg, (mem_now.cached - mem->cached) / 4096);

        mem->cached = mem_now.cached;
     }
   if (mem_now.buffered != mem->buffered)
     {
        msg.object_type = MEMORY_BUFFERED;
        enigmatic_log_diff(enigmatic, msg, (mem_now.buffered - mem->buffered) / 4096);

        mem->buffered = mem_now.buffered;
     }
   if (mem_now.shared != mem->shared)
     {
        msg.object_type = MEMORY_SHARED;
        enigmatic_log_diff(enigmatic, msg, (mem_now.shared - mem->shared) / 4096);

        mem->shared = mem_now.shared;
     }
   if (mem_now.swap_total != mem->swap_total)
     {
        msg.object_type = MEMORY_SWAP_TOTAL;
        enigmatic_log_diff(enigmatic, msg, (mem_now.swap_total - mem->swap_total) / 4096);

        mem->swap_total = mem_now.swap_total;
     }
   if (mem_now.swap_used != mem->swap_used)
     {
        msg.object_type = MEMORY_SWAP_USED;
        enigmatic_log_diff(enigmatic, msg, (mem_now.swap_used - mem->swap_used) / 4096);

        mem->swap_used = mem_now.swap_used;
     }

   for (int i = 0; i < mem->video_count; i++)
     {
        Meminfo_Video *video = &mem->video[i];
        Meminfo_Video *video_now = &mem_now.video[i];
        if (video_now->total != video->total)
          {
             msg.object_type = MEMORY_VIDEO_TOTAL;
             msg.number = i;
             enigmatic_log_diff(enigmatic, msg, (video_now->total - video->total) / 4096);

             video->total = video_now->total;
          }

        if (video_now->used != video->used)
          {
             msg.object_type = MEMORY_VIDEO_USED;
             msg.number = i;
             enigmatic_log_diff(enigmatic, msg, (video_now->used - video->used) / 4096);

             video->used = video_now->used;
          }
     }

   return 0;
}


