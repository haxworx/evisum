#include "evisum_background.h"
#include "../engine/evisum_engine.h"

typedef struct {
    Eina_Lock lock;
    Eina_Condition cond;
    Eina_Bool init;
    Eina_Bool running;
    uint64_t seq;
} Evisum_Background_State;

static Evisum_Background_State _bg = {0};

#define BG_LOCK() eina_lock_take(&_bg.lock)
#define BG_UNLOCK() eina_lock_release(&_bg.lock)

void
evisum_background_init(Evisum_Ui *ui) {
    Evisum_Engine_Status status = {0};

    if (!_bg.init) {
        eina_lock_new(&_bg.lock);
        eina_condition_new(&_bg.cond, &_bg.lock);
        _bg.init = EINA_TRUE;
    }

    BG_LOCK();
    _bg.running = EINA_TRUE;
    _bg.seq = 0;
    BG_UNLOCK();

    evisum_engine_ensure_started();

    if (!evisum_engine_status_get(&status)) return;
    ui->mem_total = status.memory.total;
    ui->mem_used = status.memory.used;
    if (status.zfs_mounted) ui->mem_used += status.memory.zfs_arc_used;
    ui->cpu_usage = status.cpu_usage;
    ui->mem.zfs_mounted = status.zfs_mounted;
}

void
evisum_background_poller(void *data, Ecore_Thread *thread) {
    Evisum_Engine_Status status = {0};
    Evisum_Ui *ui = data;
    uint64_t seq = 0;
    int ticks = 0;

    ecore_thread_name_set(thread, "background");

    while (!ecore_thread_check(thread)) {
        if (!evisum_engine_update_wait(&seq)) continue;

        BG_LOCK();
        _bg.seq++;
        eina_condition_broadcast(&_bg.cond);
        BG_UNLOCK();

        ticks++;
        if (ticks < 10) continue;
        ticks = 0;
        if (!evisum_engine_status_get(&status)) continue;
        ui->mem_total = status.memory.total;
        ui->mem_used = status.memory.used;
        if (status.zfs_mounted) ui->mem_used += status.memory.zfs_arc_used;
        ui->cpu_usage = status.cpu_usage;
        ui->mem.zfs_mounted = status.zfs_mounted;
    }
}

void
evisum_background_shutdown(Evisum_Ui *ui EINA_UNUSED) {
    if (!_bg.init) return;

    BG_LOCK();
    _bg.running = EINA_FALSE;
    eina_condition_broadcast(&_bg.cond);
    BG_UNLOCK();

    eina_condition_free(&_bg.cond);
    eina_lock_free(&_bg.lock);
    _bg.init = EINA_FALSE;
}

Eina_Bool
evisum_background_update_wait(uint64_t *seq)
{
    uint64_t target;

    if (!seq || !_bg.init) return EINA_FALSE;

    BG_LOCK();
    if (!_bg.running) {
        BG_UNLOCK();
        return EINA_FALSE;
    }

    target = *seq ? *seq : _bg.seq;
    while (_bg.running) {
        if (_bg.seq != target) {
            *seq = _bg.seq;
            BG_UNLOCK();
            return EINA_TRUE;
        }
        if (!eina_condition_timedwait(&_bg.cond, 0.2)) break;
    }

    BG_UNLOCK();
    return EINA_FALSE;
}
