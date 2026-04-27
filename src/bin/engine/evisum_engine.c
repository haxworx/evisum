#include "evisum_engine.h"
#include "enigmatic/client/Enigmatic_Client.h"

#include "enigmatic/enigmatic_util.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>

#define LOCK() eina_lock_take(&_state.lock)
#define UNLOCK() eina_lock_release(&_state.lock)
#define HISTORY_LOG_CACHE_TTL 10
#define HISTORY_CONTIGUOUS_GAP 120

typedef struct {
    Eina_Lock lock;
    Eina_Bool lock_init;
    Eina_Condition cond;
    Eina_Bool cond_init;
    Eina_Bool started;
    uint64_t snapshot_seq;

    Enigmatic_Client *client;
    Enigmatic_Client *history_client;
    Eina_Bool history_enabled;
    uint32_t history_time;
    Eina_List *history_logs;
    time_t history_logs_scan_at;
} Evisum_Engine_State;

typedef struct {
    char *path;
    uint32_t start_time;
    uint32_t end_time;
} Evisum_Engine_History_Log;

static Evisum_Engine_State _state = {0};

static Eina_Bool
_engine_snapshot_acquire(const Snapshot **out);

static void
_engine_snapshot_release(void);

static void
_engine_history_logs_free(Eina_List *logs);

static void
_cb_snapshot_init(Enigmatic_Client *client EINA_UNUSED, Snapshot *s EINA_UNUSED, void *data EINA_UNUSED)
{
    if (!_state.lock_init || !_state.cond_init) return;
    LOCK();
    _state.snapshot_seq++;
    eina_condition_broadcast(&_state.cond);
    UNLOCK();
}

static void
_cb_snapshot(Enigmatic_Client *client EINA_UNUSED, Snapshot *s EINA_UNUSED, void *data EINA_UNUSED)
{
    if (!_state.lock_init || !_state.cond_init) return;
    LOCK();
    _state.snapshot_seq++;
    eina_condition_broadcast(&_state.cond);
    UNLOCK();
}

static Eina_Bool
_engine_start_locked(void)
{
    if (_state.started && _state.client) return EINA_TRUE;

    if (!enigmatic_running()) {
        if (!enigmatic_launch()) return EINA_FALSE;
    }

    _state.client = enigmatic_client_open();
    if (!_state.client) {
        if (!enigmatic_launch()) return EINA_FALSE;
        _state.client = enigmatic_client_open();
        if (!_state.client) return EINA_FALSE;
    }

    enigmatic_client_monitor_add(_state.client, _cb_snapshot_init, _cb_snapshot, NULL);
    _state.started = EINA_TRUE;
    return EINA_TRUE;
}

Eina_Bool
evisum_engine_ensure_started(void)
{
    Eina_Bool ok;

    if (!_state.lock_init) {
        eina_lock_new(&_state.lock);
        eina_condition_new(&_state.cond, &_state.lock);
        _state.lock_init = EINA_TRUE;
        _state.cond_init = EINA_TRUE;
    }

    LOCK();
    ok = _engine_start_locked();
    UNLOCK();

    return ok;
}

void
evisum_engine_shutdown(void)
{
    if (!_state.lock_init) return;

    LOCK();

    if (_state.client) {
        enigmatic_client_del(_state.client);
        _state.client = NULL;
    }
    if (_state.history_client) {
        enigmatic_client_del(_state.history_client);
        _state.history_client = NULL;
    }
    _engine_history_logs_free(_state.history_logs);
    _state.history_logs = NULL;
    _state.history_logs_scan_at = 0;

    _state.started = EINA_FALSE;
    _state.history_enabled = EINA_FALSE;
    _state.history_time = 0;
    if (_state.cond_init) eina_condition_broadcast(&_state.cond);

    UNLOCK();

    if (_state.cond_init) {
        eina_condition_free(&_state.cond);
        _state.cond_init = EINA_FALSE;
    }
    eina_lock_free(&_state.lock);
    _state.lock_init = EINA_FALSE;
}

uint64_t
evisum_engine_update_seq_get(void)
{
    uint64_t seq = 0;
    if (!_state.lock_init) return 0;
    LOCK();
    seq = _state.snapshot_seq;
    UNLOCK();
    return seq;
}

Eina_Bool
evisum_engine_update_wait(uint64_t *seq)
{
    uint64_t target;

    if (!seq) return EINA_FALSE;
    if (!_state.lock_init || !_state.cond_init) return EINA_FALSE;

    LOCK();
    if (!_state.started || !_state.client) {
        UNLOCK();
        return EINA_FALSE;
    }

    target = *seq ? *seq : _state.snapshot_seq;
    while (_state.started && _state.client) {
        if (enigmatic_running() && (_state.snapshot_seq != target)) {
            *seq = _state.snapshot_seq;
            UNLOCK();
            return EINA_TRUE;
        }
        if (!eina_condition_timedwait(&_state.cond, 0.2)) break;
    }

    UNLOCK();
    return EINA_FALSE;
}

Eina_Bool
evisum_engine_daemon_running_get(void)
{
    return enigmatic_running();
}

static Enigmatic_Client *
_engine_history_client_for_path_read(char *path, uint32_t time)
{
    Enigmatic_Client *client;

    client = enigmatic_client_path_open(path);
    if (!client) {
        free(path);
        return NULL;
    }

    enigmatic_client_replay_time_start_set(client, 0);
    if (time) enigmatic_client_replay_time_end_set(client, time);
    enigmatic_client_read(client);

    return client;
}

static void
_engine_history_log_free(Evisum_Engine_History_Log *log)
{
    if (!log) return;
    free(log->path);
    free(log);
}

static void
_engine_history_logs_free(Eina_List *logs)
{
    Evisum_Engine_History_Log *log;

    EINA_LIST_FREE(logs, log) _engine_history_log_free(log);
}

static Evisum_Engine_History_Log *
_engine_history_log_clone(const Evisum_Engine_History_Log *src)
{
    Evisum_Engine_History_Log *log;

    if (!src || !src->path) return NULL;

    log = calloc(1, sizeof(*log));
    if (!log) return NULL;

    log->path = strdup(src->path);
    if (!log->path) {
        free(log);
        return NULL;
    }

    log->start_time = src->start_time;
    log->end_time = src->end_time;

    return log;
}

static Eina_List *
_engine_history_logs_clone(Eina_List *logs)
{
    Eina_List *out = NULL, *l;
    Evisum_Engine_History_Log *log, *copy;

    EINA_LIST_FOREACH(logs, l, log) {
        copy = _engine_history_log_clone(log);
        if (!copy) {
            _engine_history_logs_free(out);
            return NULL;
        }
        out = eina_list_append(out, copy);
    }

    return out;
}

static Eina_Bool
_engine_history_archive_name_is_valid(const char *name)
{
    size_t len;
    const char *p;

    if (!name || !name[0]) return EINA_FALSE;
    if (!strcmp(name, ".") || !strcmp(name, "..")) return EINA_FALSE;
    if (!strcmp(name, "enigmatic.log")) return EINA_FALSE;
    if (!strcmp(name, "enigmatic.lock") || !strcmp(name, "enigmatic.pid")) return EINA_FALSE;

    len = strlen(name);
    if (len > 5 && !strcmp(name + len - 5, ".size")) return EINA_FALSE;

    if (len > 4 && !strcmp(name + len - 4, ".lz4")) len -= 4;
    if (len < 2) return EINA_FALSE;

    for (size_t i = 0; i < len; i++) {
        if (name[i] == '-') continue;
        if (name[i] < '0' || name[i] > '9') return EINA_FALSE;
    }

    p = strchr(name, '-');
    if (p && (p >= name + len)) return EINA_FALSE;

    return EINA_TRUE;
}

static int
_engine_history_log_sort_cb(const void *d1, const void *d2)
{
    const Evisum_Engine_History_Log *a = d1;
    const Evisum_Engine_History_Log *b = d2;

    if (a->start_time < b->start_time) return -1;
    if (a->start_time > b->start_time) return 1;
    if (a->end_time < b->end_time) return -1;
    if (a->end_time > b->end_time) return 1;

    return 0;
}

static Eina_Bool
_engine_history_log_add(Eina_List **logs, char *path)
{
    Enigmatic_Client *client;
    Evisum_Engine_History_Log *log;
    uint32_t start_time = 0, end_time = 0;

    if (!logs || !path) {
        free(path);
        return EINA_FALSE;
    }

    client = _engine_history_client_for_path_read(strdup(path), 0);
    if (!client) {
        free(path);
        return EINA_FALSE;
    }

    if (!enigmatic_client_time_bounds_get(client, &start_time, &end_time) || !start_time || !end_time) {
        enigmatic_client_del(client);
        free(path);
        return EINA_FALSE;
    }

    enigmatic_client_del(client);

    log = calloc(1, sizeof(*log));
    if (!log) {
        free(path);
        return EINA_FALSE;
    }

    log->path = path;
    log->start_time = start_time;
    log->end_time = end_time;

    *logs = eina_list_append(*logs, log);

    return EINA_TRUE;
}

static Eina_List *
_engine_history_logs_scan(uint32_t since)
{
    Eina_List *logs = NULL;
    DIR *dp;
    struct dirent *ent;
    char *dir, *current_path;

    current_path = enigmatic_log_path();
    _engine_history_log_add(&logs, current_path);

    dir = enigmatic_log_directory();
    if (!dir) return logs;

    dp = opendir(dir);
    if (!dp) {
        free(dir);
        return logs;
    }

    while ((ent = readdir(dp))) {
        char path[PATH_MAX];
        struct stat st;

        if (!_engine_history_archive_name_is_valid(ent->d_name)) continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        if (stat(path, &st) == -1) continue;
        if (!S_ISREG(st.st_mode)) continue;
        if (access(path, R_OK) != 0) continue;
        if (since && st.st_mtime && ((uint32_t) st.st_mtime < since)) continue;

        _engine_history_log_add(&logs, strdup(path));
    }

    closedir(dp);
    free(dir);

    return eina_list_sort(logs, eina_list_count(logs), _engine_history_log_sort_cb);
}

static Eina_List *
_engine_history_logs_get(Eina_Bool refresh, uint32_t since)
{
    Eina_List *logs, *copy = NULL;
    time_t now;

    now = time(NULL);

    if (!since && !refresh && _state.lock_init) {
        LOCK();
        if (_state.history_logs && ((now - _state.history_logs_scan_at) < HISTORY_LOG_CACHE_TTL))
            copy = _engine_history_logs_clone(_state.history_logs);
        UNLOCK();
        if (copy) return copy;
    }

    logs = _engine_history_logs_scan(since);
    if (!logs) return NULL;

    if (!since && _state.lock_init) {
        copy = _engine_history_logs_clone(logs);
        LOCK();
        _engine_history_logs_free(_state.history_logs);
        _state.history_logs = copy;
        _state.history_logs_scan_at = now;
        UNLOCK();
    }

    return logs;
}

Eina_Bool
evisum_engine_history_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time)
{
    Eina_List *logs, *l;
    Evisum_Engine_History_Log *log;
    Eina_Bool ok = EINA_FALSE;

    if (start_time) *start_time = 0;
    if (end_time) *end_time = 0;
    if (!evisum_engine_ensure_started()) return EINA_FALSE;

    logs = _engine_history_logs_get(EINA_FALSE, since);
    if (!logs) return EINA_FALSE;

    EINA_LIST_FOREACH(logs, l, log) {
        if (!ok) {
            if (start_time) *start_time = log->start_time;
            if (end_time) *end_time = log->end_time;
            ok = EINA_TRUE;
            continue;
        }
        if (start_time && log->start_time < *start_time) *start_time = log->start_time;
        if (end_time && log->end_time > *end_time) *end_time = log->end_time;
    }

    _engine_history_logs_free(logs);

    return EINA_TRUE;
}

Eina_Bool
evisum_engine_history_contiguous_bounds_since_get(uint32_t since, uint32_t *start_time, uint32_t *end_time)
{
    Eina_List *logs, *l;
    Evisum_Engine_History_Log *log, *tail = NULL;
    uint32_t start, end;

    if (start_time) *start_time = 0;
    if (end_time) *end_time = 0;
    if (!evisum_engine_ensure_started()) return EINA_FALSE;

    logs = _engine_history_logs_get(EINA_FALSE, since);
    if (!logs) return EINA_FALSE;

    l = eina_list_last(logs);
    if (!l) {
        _engine_history_logs_free(logs);
        return EINA_FALSE;
    }

    tail = eina_list_data_get(l);
    if (!tail || !tail->start_time || !tail->end_time || (since && tail->end_time < since)) {
        _engine_history_logs_free(logs);
        return EINA_FALSE;
    }

    start = tail->start_time;
    end = tail->end_time;

    for (l = eina_list_prev(l); l; l = eina_list_prev(l)) {
        log = eina_list_data_get(l);
        if (!log || !log->end_time) continue;
        if (log->end_time + HISTORY_CONTIGUOUS_GAP < start) break;
        if (log->start_time < start) start = log->start_time;
    }

    _engine_history_logs_free(logs);

    if (since && start < since) start = since;
    if (end <= start) end = start + 1;

    if (start_time) *start_time = start;
    if (end_time) *end_time = end;

    return EINA_TRUE;
}

Eina_Bool
evisum_engine_history_bounds_get(uint32_t *start_time, uint32_t *end_time)
{
    return evisum_engine_history_bounds_since_get(0, start_time, end_time);
}

Eina_Bool
evisum_engine_history_time_set(uint32_t time)
{
    Enigmatic_Client *client, *old;
    Eina_List *logs, *l;
    Evisum_Engine_History_Log *log, *selected = NULL;

    if (!time) return EINA_FALSE;
    if (!evisum_engine_ensure_started()) return EINA_FALSE;

    logs = _engine_history_logs_get(EINA_FALSE, 0);
    EINA_LIST_FOREACH(logs, l, log) {
        if ((time >= log->start_time) && (time <= log->end_time)) {
            selected = log;
            break;
        }
    }

    if (!selected) {
        _engine_history_logs_free(logs);
        logs = _engine_history_logs_get(EINA_TRUE, 0);
        EINA_LIST_FOREACH(logs, l, log) {
            if ((time >= log->start_time) && (time <= log->end_time)) {
                selected = log;
                break;
            }
        }
        if (!selected) {
            _engine_history_logs_free(logs);
            return EINA_FALSE;
        }
    }

    client = _engine_history_client_for_path_read(strdup(selected->path), time);
    _engine_history_logs_free(logs);
    if (!client) return EINA_FALSE;

    LOCK();
    old = _state.history_client;
    _state.history_client = client;
    _state.history_enabled = EINA_TRUE;
    _state.history_time = time;
    _state.snapshot_seq++;
    if (_state.cond_init) eina_condition_broadcast(&_state.cond);
    UNLOCK();

    if (old) enigmatic_client_del(old);

    return EINA_TRUE;
}

void
evisum_engine_history_live_set(void)
{
    Enigmatic_Client *old = NULL;

    if (!_state.lock_init) return;

    LOCK();
    old = _state.history_client;
    _state.history_client = NULL;
    _state.history_enabled = EINA_FALSE;
    _state.history_time = 0;
    _state.snapshot_seq++;
    if (_state.cond_init) eina_condition_broadcast(&_state.cond);
    UNLOCK();

    if (old) enigmatic_client_del(old);
}

Eina_Bool
evisum_engine_history_live_get(void)
{
    Eina_Bool live = EINA_TRUE;

    if (!_state.lock_init) return EINA_TRUE;

    LOCK();
    live = !_state.history_enabled;
    UNLOCK();

    return live;
}

uint32_t
evisum_engine_history_time_get(void)
{
    uint32_t time = 0;

    if (!_state.lock_init) return 0;

    LOCK();
    time = _state.history_time;
    UNLOCK();

    return time;
}

uint32_t
evisum_engine_live_time_get(void)
{
    uint32_t time = 0;

    if (!_state.lock_init) return 0;

    LOCK();
    if (_state.client) time = _state.client->snapshot.time;
    UNLOCK();

    return time;
}

static Eina_Bool
_engine_snapshot_acquire(const Snapshot **out)
{
    if (out) *out = NULL;
    if (!out) return EINA_FALSE;
    if (!_state.lock_init) return EINA_FALSE;
    if (!_state.started) return EINA_FALSE;

    LOCK();
    if (!_state.started || !_state.client) {
        UNLOCK();
        return EINA_FALSE;
    }

    if (_state.history_enabled && _state.history_client)
        *out = &_state.history_client->snapshot;
    else
        *out = &_state.client->snapshot;
    return EINA_TRUE;
}

static void
_engine_snapshot_release(void)
{
    if (_state.lock_init) UNLOCK();
}

static Cpu_Core **
_cores_as_array(const Snapshot *snap, int *ncpu)
{
    Eina_List *l;
    Cpu_Core *core;
    int n = 0;
    int i = 0;
    Cpu_Core **arr;

    if (!snap) {
        if (ncpu) *ncpu = 0;
        return NULL;
    }

    n = eina_list_count(snap->cores);
    arr = calloc(n ? n : 1, sizeof(Cpu_Core *));
    if (!arr) {
        if (ncpu) *ncpu = 0;
        return NULL;
    }

    EINA_LIST_FOREACH(snap->cores, l, core) {
        arr[i++] = (Cpu_Core *) core;
    }

    if (ncpu) *ncpu = i;
    return arr;
}

int
system_cpu_online_count_get(void)
{
    const Snapshot *snap;
    int n;

    if (!_engine_snapshot_acquire(&snap)) return 1;
    n = eina_list_count(snap->cores);
    _engine_snapshot_release();
    return n > 0 ? n : 1;
}

int
system_cpu_count_get(void)
{
    return system_cpu_online_count_get();
}

Cpu_Core **
system_cpu_state_get(int *ncpu)
{
    const Snapshot *snap;
    Cpu_Core **arr = NULL;

    if (!_engine_snapshot_acquire(&snap)) {
        if (ncpu) *ncpu = 0;
        return NULL;
    }

    arr = _cores_as_array(snap, ncpu);
    _engine_snapshot_release();
    return arr;
}

Cpu_Core **
system_cpu_usage_delayed_get(int *ncpu, int usecs)
{
    (void) usecs;
    return system_cpu_state_get(ncpu);
}

Cpu_Core **
system_cpu_usage_get(int *ncpu)
{
    return system_cpu_usage_delayed_get(ncpu, 1000000);
}

int
system_cpu_frequency_get(void)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int n = 0;
    long total = 0;

    if (!_engine_snapshot_acquire(&snap)) return 0;

    EINA_LIST_FOREACH(snap->cores, l, core) {
        if (core->freq <= 0) continue;
        total += core->freq;
        n++;
    }

    _engine_snapshot_release();
    return n ? (int) (total / n) : 0;
}

int
system_cpu_n_frequency_get(int n)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int i = 0;
    int out = 0;

    if (!_engine_snapshot_acquire(&snap)) return 0;

    EINA_LIST_FOREACH(snap->cores, l, core) {
        if (i == n) {
            out = core->freq;
            break;
        }
        i++;
    }

    _engine_snapshot_release();
    return out;
}

int
system_cpu_n_temperature_get(int n)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int i = 0;
    int out = -1;

    if (!_engine_snapshot_acquire(&snap)) return -1;

    EINA_LIST_FOREACH(snap->cores, l, core) {
        if (i == n) {
            out = core->temp;
            break;
        }
        i++;
    }

    _engine_snapshot_release();
    return out;
}

int
system_cpu_temperature_min_max_get(int *min, int *max)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int lo = 0, hi = 0;
    Eina_Bool has = EINA_FALSE;

    if (!_engine_snapshot_acquire(&snap)) return -1;

    EINA_LIST_FOREACH(snap->cores, l, core) {
        int t = core->temp;
        if (t < 0) continue;
        if (!has) {
            lo = hi = t;
            has = EINA_TRUE;
        } else {
            if (t < lo) lo = t;
            if (t > hi) hi = t;
        }
    }

    _engine_snapshot_release();

    if (!has) return -1;
    if (min) *min = lo;
    if (max) *max = hi;
    return 0;
}

int
system_cpu_frequency_min_max_get(int *min, int *max)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int lo = 0, hi = 0;
    Eina_Bool has = EINA_FALSE;

    if (!_engine_snapshot_acquire(&snap)) return -1;

    EINA_LIST_FOREACH(snap->cores, l, core) {
        int f = core->freq;
        if (f <= 0) continue;
        if (!has) {
            lo = hi = f;
            has = EINA_TRUE;
        } else {
            if (f < lo) lo = f;
            if (f > hi) hi = f;
        }
    }

    _engine_snapshot_release();

    if (!has) return -1;
    if (min) *min = lo;
    if (max) *max = hi;
    return 0;
}

void
system_cpu_topology_get(int *ids, int ncpus)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    int i = 0;

    if (!ids || ncpus <= 0) return;
    if (!_engine_snapshot_acquire(&snap)) {
        for (i = 0; i < ncpus; i++) ids[i] = i;
        return;
    }

    EINA_LIST_FOREACH(snap->cores, l, core) {
        if (i >= ncpus) break;
        ids[i++] = core->id;
    }
    for (; i < ncpus; i++) ids[i] = i;

    _engine_snapshot_release();
}

void
system_memory_usage_get(Meminfo *memory)
{
    const Snapshot *snap;

    if (!memory) return;
    memset(memory, 0, sizeof(*memory));
    if (!_engine_snapshot_acquire(&snap)) return;
    memory->total = snap->meminfo.total;
    memory->used = snap->meminfo.used;
    memory->cached = snap->meminfo.cached;
    memory->buffered = snap->meminfo.buffered;
    memory->shared = snap->meminfo.shared;
    memory->swap_total = snap->meminfo.swap_total;
    memory->swap_used = snap->meminfo.swap_used;
    memory->zfs_arc_used = snap->meminfo.zfs_arc_used;
    memory->video_count = snap->meminfo.video_count;
    if (memory->video_count > MEM_VIDEO_CARD_MAX) memory->video_count = MEM_VIDEO_CARD_MAX;
    for (unsigned int i = 0; i < memory->video_count; i++) {
        memory->video[i].total = snap->meminfo.video[i].total;
        memory->video[i].used = snap->meminfo.video[i].used;
    }
    _engine_snapshot_release();
}

Eina_Bool
evisum_engine_status_get(Evisum_Engine_Status *status)
{
    const Snapshot *snap;
    Eina_List *l;
    Cpu_Core *core;
    File_System *fs;
    double total = 0.0;
    int ncpu = 0;

    if (!status) return EINA_FALSE;
    memset(status, 0, sizeof(*status));

    if (!_engine_snapshot_acquire(&snap)) return EINA_FALSE;

    status->memory.total = snap->meminfo.total;
    status->memory.used = snap->meminfo.used;
    status->memory.cached = snap->meminfo.cached;
    status->memory.buffered = snap->meminfo.buffered;
    status->memory.shared = snap->meminfo.shared;
    status->memory.swap_total = snap->meminfo.swap_total;
    status->memory.swap_used = snap->meminfo.swap_used;
    status->memory.zfs_arc_used = snap->meminfo.zfs_arc_used;
    status->memory.video_count = snap->meminfo.video_count;
    if (status->memory.video_count > MEM_VIDEO_CARD_MAX) status->memory.video_count = MEM_VIDEO_CARD_MAX;
    for (unsigned int i = 0; i < status->memory.video_count; i++) {
        status->memory.video[i].total = snap->meminfo.video[i].total;
        status->memory.video[i].used = snap->meminfo.video[i].used;
    }

    EINA_LIST_FOREACH(snap->cores, l, core) {
        total += core->percent;
        ncpu++;
    }
    status->cpu_usage = ncpu > 0 ? total / ncpu : 0.0;

    EINA_LIST_FOREACH(snap->file_systems, l, fs) {
        if (fs->type_name[0] && !strcasecmp(fs->type_name, "ZFS")) {
            status->zfs_mounted = EINA_TRUE;
            break;
        }
    }

    _engine_snapshot_release();
    return EINA_TRUE;
}

Sensor **
system_sensors_thermal_get(int *count)
{
    const Snapshot *snap;
    Eina_List *l;
    Sensor *s;
    Sensor **arr;
    int n = 0;
    int i = 0;

    if (count) *count = 0;
    if (!_engine_snapshot_acquire(&snap)) return NULL;

    EINA_LIST_FOREACH(snap->sensors, l, s) {
        if (s->type == THERMAL) n++;
    }

    arr = calloc(n ? n : 1, sizeof(Sensor *));
    if (!arr) {
        _engine_snapshot_release();
        return NULL;
    }

    EINA_LIST_FOREACH(snap->sensors, l, s) {
        if (s->type != THERMAL) continue;
        arr[i++] = s;
    }

    if (count) *count = i;
    _engine_snapshot_release();
    return arr;
}

void
system_sensors_thermal_free(Sensor **sensors, int count)
{
    (void) count;
    if (!sensors) return;
    free(sensors);
}

Network_Interface **
system_network_ifaces_get(int *n)
{
    const Snapshot *snap;
    Eina_List *l;
    Network_Interface *iface;
    Network_Interface **arr;
    int count = 0;
    int i = 0;

    if (n) *n = 0;
    if (!_engine_snapshot_acquire(&snap)) return NULL;

    count = eina_list_count(snap->network_interfaces);
    arr = calloc(count ? count : 1, sizeof(Network_Interface *));
    if (!arr) {
        _engine_snapshot_release();
        return NULL;
    }

    EINA_LIST_FOREACH(snap->network_interfaces, l, iface)
        arr[i++] = iface;

    if (n) *n = i;
    _engine_snapshot_release();
    return arr;
}

Proc_Net **
system_network_process_usage_get(int *n)
{
    if (n) *n = 0;
    return NULL;
}

void
system_network_process_usage_free(Proc_Net **procs, int n)
{
    (void) n;
    if (!procs) return;
    free(procs);
}

static Eina_Bool _show_kthreads = EINA_TRUE;

void
proc_info_kthreads_show_set(Eina_Bool enabled)
{
    _show_kthreads = enabled;
}

Eina_Bool
proc_info_kthreads_show_get(void)
{
    return _show_kthreads;
}

static Proc_Info *
_proc_from_log(const Proc_Info_Log *src)
{
    Proc_Info *p;
    const char *cmd;

    if (!src) return NULL;

    p = calloc(1, sizeof(Proc_Info));
    if (!p) return NULL;

    p->pid = src->pid;
    p->ppid = src->ppid;
    p->uid = src->uid;
    p->nice = src->nice;
    p->priority = src->priority;
    p->cpu_id = src->cpu_id;
    p->numthreads = src->numthreads;
    p->cpu_time = src->cpu_time;
    p->cpu_usage = src->cpu_usage;
    p->run_time = src->run_time;
    p->start = src->start;
    p->mem_size = src->mem_size;
    p->mem_virt = src->mem_virt;
    p->mem_rss = src->mem_rss;
    p->mem_shared = src->mem_shared;
    p->net_in = src->net_in;
    p->net_out = src->net_out;
    p->disk_read = src->disk_read;
    p->disk_write = src->disk_write;
    p->numfiles = src->numfiles;
    p->was_zero = src->was_zero;
    p->is_kernel = src->is_kernel;
    p->is_new = src->is_new;
    p->tid = src->tid;
    snprintf(p->state, sizeof(p->state), "%s", src->state);
    snprintf(p->wchan, sizeof(p->wchan), "%s", src->wchan);

    cmd = src->command[0] ? src->command : (src->path[0] ? src->path : "");
    p->command = strdup(cmd);
    p->arguments = strdup(src->arguments);
    p->thread_name = strdup(src->thread_name[0] ? src->thread_name : cmd);

    return p;
}

void
proc_info_free(Proc_Info *proc)
{
    Proc_Info *child;

    if (!proc) return;

    EINA_LIST_FREE(proc->threads, child) proc_info_free(child);
    EINA_LIST_FREE(proc->children, child) proc_info_free(child);

    free(proc->command);
    free(proc->arguments);
    free(proc->thread_name);
    free(proc);
}

static Eina_List *
_proc_list_get(void)
{
    const Snapshot *snap;
    Eina_List *l;
    Proc_Info_Log *p;
    Eina_List *out = NULL;

    if (!_engine_snapshot_acquire(&snap)) return NULL;

    EINA_LIST_FOREACH(snap->processes, l, p) {
        Proc_Info *c;
        if (!_show_kthreads && p->is_kernel) continue;
        c = _proc_from_log(p);
        if (!c) continue;
        out = eina_list_append(out, c);
    }

    _engine_snapshot_release();
    return out;
}

Eina_List *
proc_info_all_get(void)
{
    return _proc_list_get();
}

Proc_Info *
proc_info_by_pid(pid_t pid)
{
    const Snapshot *snap;
    Eina_List *l;
    Proc_Info_Log *p;
    Proc_Info *ret = NULL;

    if (!_engine_snapshot_acquire(&snap)) return NULL;

    EINA_LIST_FOREACH(snap->processes, l, p) {
        if (p->pid != pid) continue;
        ret = _proc_from_log(p);
        break;
    }

    if (ret) {
        EINA_LIST_FOREACH(snap->processes, l, p) {
            Proc_Info *t;
            if (p->pid != pid) continue;
            if (p->tid <= 0) continue;
            t = _proc_from_log(p);
            if (!t) continue;
            ret->threads = eina_list_append(ret->threads, t);
        }
        if (!ret->threads) {
            Proc_Info *t = calloc(1, sizeof(Proc_Info));
            if (t) {
                t->tid = ret->tid > 0 ? ret->tid : ret->pid;
                t->cpu_id = ret->cpu_id;
                t->cpu_time = ret->cpu_time;
                t->cpu_usage = ret->cpu_usage;
                snprintf(t->state, sizeof(t->state), "%s", ret->state);
                t->thread_name = strdup(ret->command ? ret->command : "thread");
                ret->threads = eina_list_append(ret->threads, t);
            }
        }
    }

    _engine_snapshot_release();
    return ret;
}

Eina_List *
proc_info_all_children_get(void)
{
    Eina_List *procs, *l;
    Proc_Info *p;
    Eina_Hash *by_pid;
    Eina_List *roots = NULL;

    procs = _proc_list_get();
    if (!procs) return NULL;

    by_pid = eina_hash_int64_new(NULL);
    if (!by_pid) {
        EINA_LIST_FREE(procs, p) proc_info_free(p);
        return NULL;
    }

    EINA_LIST_FOREACH(procs, l, p) {
        int64_t k = p->pid;
        eina_hash_add(by_pid, &k, p);
    }

    EINA_LIST_FOREACH(procs, l, p) {
        int64_t parent_k = p->ppid;
        Proc_Info *parent = eina_hash_find(by_pid, &parent_k);
        if (parent && parent != p) parent->children = eina_list_append(parent->children, p);
        else roots = eina_list_append(roots, p);
    }

    eina_hash_free(by_pid);
    eina_list_free(procs);
    return roots;
}

Eina_List *
proc_info_pid_children_get(pid_t pid)
{
    Eina_List *roots, *l;
    Proc_Info *p;
    Eina_List *out = NULL;

    roots = proc_info_all_children_get();
    EINA_LIST_FOREACH(roots, l, p) {
        if (p->pid == pid) {
            out = eina_list_append(out, p);
            break;
        }
    }

    if (!out) {
        EINA_LIST_FREE(roots, p) proc_info_pid_children_free(p);
        return NULL;
    }

    EINA_LIST_FOREACH(roots, l, p) {
        if (p == eina_list_data_get(out)) continue;
        proc_info_pid_children_free(p);
    }
    eina_list_free(roots);

    return out;
}

void
proc_info_pid_children_free(Proc_Info *proc)
{
    Proc_Info *child;
    if (!proc) return;
    EINA_LIST_FREE(proc->children, child) proc_info_pid_children_free(child);
    proc_info_free(proc);
}

#define SORT_NUM(a,b,field) (((a)->field > (b)->field) - ((a)->field < (b)->field))

int proc_sort_by_pid(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,pid); }
int proc_sort_by_uid(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,uid); }
int proc_sort_by_nice(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,nice); }
int proc_sort_by_pri(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,priority); }
int proc_sort_by_cpu(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,cpu_id); }
int proc_sort_by_threads(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,numthreads); }
int proc_sort_by_files(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,numfiles); }
int proc_sort_by_size(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,mem_size); }
int proc_sort_by_virt(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,mem_virt); }
int proc_sort_by_rss(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,mem_rss); }
int proc_sort_by_shared(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,mem_shared); }
int proc_sort_by_net_in(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,net_in); }
int proc_sort_by_net_out(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,net_out); }
int proc_sort_by_disk_read(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,disk_read); }
int proc_sort_by_disk_write(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,disk_write); }
int proc_sort_by_time(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,cpu_time); }
int proc_sort_by_cpu_usage(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,cpu_usage); }
int proc_sort_by_cmd(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return strcmp(a->command ? a->command : "", b->command ? b->command : ""); }
int proc_sort_by_state(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return strcmp(a->state, b->state); }
int proc_sort_by_age(const void *p1, const void *p2) { const Proc_Info *a=p1,*b=p2; return SORT_NUM(a,b,start); }

Eina_List *
file_system_info_all_get(void)
{
    const Snapshot *snap;
    Eina_List *l;
    File_System *in;
    Eina_List *out = NULL;

    if (!_engine_snapshot_acquire(&snap)) return NULL;

    EINA_LIST_FOREACH(snap->file_systems, l, in) {
        File_System *fs = calloc(1, sizeof(File_System));
        if (!fs) continue;
        memcpy(fs, in, sizeof(*fs));
        out = eina_list_append(out, fs);
    }

    _engine_snapshot_release();
    return out;
}

void
file_system_info_free(File_System *fs)
{
    if (!fs) return;
    free(fs);
}

Eina_Bool
file_system_in_use(const char *name)
{
    Eina_List *list;
    File_System *fs;
    Eina_Bool mounted = EINA_FALSE;

    if (!name) return EINA_FALSE;

    list = file_system_info_all_get();
    EINA_LIST_FREE(list, fs) {
        if (fs->type_name[0] && (!strcasecmp(fs->type_name, name))) mounted = EINA_TRUE;
        file_system_info_free(fs);
    }

    return mounted;
}
