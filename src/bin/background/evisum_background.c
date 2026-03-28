#include "evisum_background.h"
#include "../system/filesystems.h"
#include "../system/machine.h"

#include <Eina.h>
#include <stdint.h>

// We don't use a hash for processes. Each proc_info_ call is blocking.
// This avoids a lot of problems due to complexities.
//
// Evisum 2.0 will address this.

typedef struct {
    uint64_t in;
    uint64_t out;
} Proc_Net_Stats;

static Proc_Net_Stats *
_proc_net_stats_get_or_add(Eina_Hash *hash, int64_t pid) {
    Proc_Net_Stats *stats = eina_hash_find(hash, &pid);
    if (stats) return stats;

    stats = calloc(1, sizeof(*stats));
    if (!stats) return NULL;

    eina_hash_add(hash, &pid, stats);
    return stats;
}

static Eina_Hash *
_proc_net_stats_collect(void) {
    Eina_Hash *hash = NULL;
    proc_net_t **procs = NULL;
    int n = 0;

    hash = eina_hash_int64_new(free);
    if (!hash) return NULL;

    procs = system_network_process_usage_get(&n);
    for (int i = 0; i < n; i++) {
        int64_t pid;
        Proc_Net_Stats *stats;

        if (!procs[i]) continue;
        if ((!procs[i]->in) && (!procs[i]->out)) continue;

        pid = procs[i]->pid;
        stats = _proc_net_stats_get_or_add(hash, pid);
        if (!stats) continue;

        stats->in = procs[i]->in;
        stats->out = procs[i]->out;
    }

    system_network_process_usage_free(procs, n);
    return hash;
}

static void
_proc_net_stats_refresh(Evisum_Ui *ui) {
    Eina_Hash *next;

    next = _proc_net_stats_collect();
    if (!next) return;

    eina_lock_take(&ui->background.proc_net_lock);
    if (ui->background.proc_net_stats) eina_hash_free(ui->background.proc_net_stats);
    ui->background.proc_net_stats = next;
    eina_lock_release(&ui->background.proc_net_lock);
}

void
evisum_background_init(Evisum_Ui *ui) {
    meminfo_t memory;

    if (!ui->background.proc_net_lock_init) {
        eina_lock_new(&ui->background.proc_net_lock);
        ui->background.proc_net_lock_init = EINA_TRUE;
    }
    _proc_net_stats_refresh(ui);

    system_memory_usage_get(&memory);
    ui->mem_total = memory.total;
    ui->mem_used = memory.used;
}

void
evisum_background_poller(void *data, Ecore_Thread *thread) {
    meminfo_t memory;
    Evisum_Ui *ui = data;

    ecore_thread_name_set(thread, "background");

    while (!ecore_thread_check(thread)) {
        int ncpu;
        double percent = 0.0;
        cpu_core_t **cores = system_cpu_usage_delayed_get(&ncpu, 250000);
        for (int i = 0; i < ncpu; i++) {
            percent += cores[i]->percent;
            free(cores[i]);
        }
        free(cores);

        system_memory_usage_get(&memory);
        ui->mem_used = memory.used;
        if (file_system_in_use("ZFS")) ui->mem_used += memory.zfs_arc_used;

        _proc_net_stats_refresh(ui);
        ui->cpu_usage = percent / system_cpu_online_count_get();
    }
}

void
evisum_background_proc_net_get(Evisum_Ui *ui, pid_t pid, uint64_t *net_in, uint64_t *net_out) {
    int64_t key;
    Proc_Net_Stats *stats;

    if (net_in) *net_in = 0;
    if (net_out) *net_out = 0;
    if (!ui) return;
    if (pid <= 0) return;
    if (!ui->background.proc_net_lock_init) return;

    key = pid;
    eina_lock_take(&ui->background.proc_net_lock);
    stats = ui->background.proc_net_stats ? eina_hash_find(ui->background.proc_net_stats, &key) : NULL;
    if (stats) {
        if (net_in) *net_in = stats->in;
        if (net_out) *net_out = stats->out;
    }
    eina_lock_release(&ui->background.proc_net_lock);
}

void
evisum_background_shutdown(Evisum_Ui *ui) {
    if (!ui) return;
    if (!ui->background.proc_net_lock_init) return;

    eina_lock_take(&ui->background.proc_net_lock);
    if (ui->background.proc_net_stats) {
        eina_hash_free(ui->background.proc_net_stats);
        ui->background.proc_net_stats = NULL;
    }
    eina_lock_release(&ui->background.proc_net_lock);
    eina_lock_free(&ui->background.proc_net_lock);
    ui->background.proc_net_lock_init = EINA_FALSE;
}
