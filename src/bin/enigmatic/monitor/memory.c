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

   Message msg = { .type = MESG_MOD };
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

   size_t old_index = 0, new_index = 0;
   for (unsigned int id = 0; id < MEM_VIDEO_CARD_MAX; id++)
     {
        Meminfo_Video *old = NULL, *now = NULL;

        if (old_index < mem->video_count && mem->video[old_index].id == id)
          old = &mem->video[old_index++];
        if (new_index < mem_now.video_count && mem_now.video[new_index].id == id)
          now = &mem_now.video[new_index++];

        msg.number = id;
        if (now && !old)
          {
             msg.type = MESG_ADD;
             msg.object_type = MEMORY;
             enigmatic_log_obj_write(enigmatic, EVENT_MESSAGE, msg, now, sizeof(*now));
          }
        else if (old && !now)
          {
             msg.type = MESG_DEL;
             msg.object_type = MEMORY;
             enigmatic_log_header(enigmatic, EVENT_MESSAGE, msg);
          }
        else if (now)
          {
             msg.type = MESG_MOD;
             if (old->total != now->total)
               {
                  msg.object_type = MEMORY_VIDEO_TOTAL;
                  enigmatic_log_diff(enigmatic, msg, now->total - old->total);
               }
             if (old->used != now->used)
               {
                  msg.object_type = MEMORY_VIDEO_USED;
                  enigmatic_log_diff(enigmatic, msg, now->used - old->used);
               }
          }
     }
   memcpy(mem->video, mem_now.video, sizeof(mem->video));
   mem->video_count = mem_now.video_count;

   return 0;
}


