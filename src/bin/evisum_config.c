#include "config.h"
#include "evisum_config.h"
#include "ui/evisum_ui_process_list.h"
#include <Eina.h>
#include <Ecore_File.h>
#include <Efreet.h>

Evisum_Config *_evisum_config;

static Eet_Data_Descriptor *_evisum_conf_descriptor = NULL;

enum {
    PROC_FIELD_WIDTH_CMD = 1,
    PROC_FIELD_WIDTH_UID = 2,
    PROC_FIELD_WIDTH_PID = 3,
    PROC_FIELD_WIDTH_THREADS = 4,
    PROC_FIELD_WIDTH_CPU = 5,
    PROC_FIELD_WIDTH_PRI = 6,
    PROC_FIELD_WIDTH_NICE = 7,
    PROC_FIELD_WIDTH_FILES = 8,
    PROC_FIELD_WIDTH_SIZE = 9,
    PROC_FIELD_WIDTH_VIRT = 10,
    PROC_FIELD_WIDTH_DISK_WRITE = 11,
    PROC_FIELD_WIDTH_DISK_READ = 12,
    PROC_FIELD_WIDTH_NET_IN = 13,
    PROC_FIELD_WIDTH_NET_OUT = 14,
    PROC_FIELD_WIDTH_RSS = 15,
    PROC_FIELD_WIDTH_SHARED = 16,
    PROC_FIELD_WIDTH_STATE = 17,
    PROC_FIELD_WIDTH_TIME = 18,
    PROC_FIELD_WIDTH_CPU_USAGE = 19
};

static const char *
_config_file_path(void) {
    const char *path = eina_slstr_printf("%s/evisum", efreet_config_home_get());

    if (!ecore_file_exists(path)) ecore_file_mkpath(path);

    path = eina_slstr_printf("%s/evisum/evisum.cfg", efreet_config_home_get());

    return path;
}

void
config_init(void) {
    Eet_Data_Descriptor_Class eddc;
    efreet_init();

    EET_EINA_STREAM_DATA_DESCRIPTOR_CLASS_SET(&eddc, Evisum_Config);
    _evisum_conf_descriptor = eet_data_descriptor_stream_new(&eddc);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "version", version, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "effects", effects, EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "backgrounds", backgrounds, EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.width", proc.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.height", proc.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.x", proc.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.y", proc.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.restart", proc.restart, EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_kthreads", proc.show_kthreads,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_user", proc.show_user,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.sort_type", proc.sort_type,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.sort_reverse", proc.sort_reverse,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.poll_delay", proc.poll_delay,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.fields", proc.fields, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.cmd",
                                  proc.field_widths[PROC_FIELD_WIDTH_CMD], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.uid",
                                  proc.field_widths[PROC_FIELD_WIDTH_UID], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.pid",
                                  proc.field_widths[PROC_FIELD_WIDTH_PID], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.threads",
                                  proc.field_widths[PROC_FIELD_WIDTH_THREADS], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.cpu",
                                  proc.field_widths[PROC_FIELD_WIDTH_CPU], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.pri",
                                  proc.field_widths[PROC_FIELD_WIDTH_PRI], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.nice",
                                  proc.field_widths[PROC_FIELD_WIDTH_NICE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.files",
                                  proc.field_widths[PROC_FIELD_WIDTH_FILES], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.size",
                                  proc.field_widths[PROC_FIELD_WIDTH_SIZE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.virt",
                                  proc.field_widths[PROC_FIELD_WIDTH_VIRT], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.rss",
                                  proc.field_widths[PROC_FIELD_WIDTH_RSS], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.shared",
                                  proc.field_widths[PROC_FIELD_WIDTH_SHARED], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.state",
                                  proc.field_widths[PROC_FIELD_WIDTH_STATE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.time",
                                  proc.field_widths[PROC_FIELD_WIDTH_TIME], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.cpu_usage",
                                  proc.field_widths[PROC_FIELD_WIDTH_CPU_USAGE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.net_in",
                                  proc.field_widths[PROC_FIELD_WIDTH_NET_IN], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.net_out",
                                  proc.field_widths[PROC_FIELD_WIDTH_NET_OUT], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.disk_read",
                                  proc.field_widths[PROC_FIELD_WIDTH_DISK_READ], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_widths.disk_write",
                                  proc.field_widths[PROC_FIELD_WIDTH_DISK_WRITE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.cmd",
                                  proc.field_order[PROC_FIELD_CMD], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.uid",
                                  proc.field_order[PROC_FIELD_UID], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.pid",
                                  proc.field_order[PROC_FIELD_PID], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.threads",
                                  proc.field_order[PROC_FIELD_THREADS], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.cpu",
                                  proc.field_order[PROC_FIELD_CPU], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.pri",
                                  proc.field_order[PROC_FIELD_PRI], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.nice",
                                  proc.field_order[PROC_FIELD_NICE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.files",
                                  proc.field_order[PROC_FIELD_FILES], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.size",
                                  proc.field_order[PROC_FIELD_SIZE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.virt",
                                  proc.field_order[PROC_FIELD_VIRT], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.disk_write",
                                  proc.field_order[PROC_FIELD_DISK_WRITE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.disk_read",
                                  proc.field_order[PROC_FIELD_DISK_READ], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.net_in",
                                  proc.field_order[PROC_FIELD_NET_IN], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.net_out",
                                  proc.field_order[PROC_FIELD_NET_OUT], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.rss",
                                  proc.field_order[PROC_FIELD_RSS], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.shared",
                                  proc.field_order[PROC_FIELD_SHARED], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.state",
                                  proc.field_order[PROC_FIELD_STATE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.time",
                                  proc.field_order[PROC_FIELD_TIME], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.field_order.cpu_usage",
                                  proc.field_order[PROC_FIELD_CPU_USAGE], EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.show_statusbar", proc.show_statusbar,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.transparent", proc.transparent,
                                  EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "proc.alpha", proc.alpha, EET_T_UCHAR);

    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.width", cpu.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.height", cpu.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.x", cpu.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.y", cpu.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.restart", cpu.restart, EET_T_UCHAR);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "cpu.visual", cpu.visual, EET_T_STRING);

    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.width", mem.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.height", mem.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.x", mem.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.y", mem.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "mem.restart", mem.restart, EET_T_UCHAR);

    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.width", disk.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.height", disk.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.x", disk.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.y", disk.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "disk.restart", disk.restart, EET_T_UCHAR);

    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.width", sensors.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.height", sensors.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.x", sensors.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.y", sensors.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "sensors.restart", sensors.restart,
                                  EET_T_UCHAR);

    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.width", network.width, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.height", network.height, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.x", network.x, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.y", network.y, EET_T_INT);
    EET_DATA_DESCRIPTOR_ADD_BASIC(_evisum_conf_descriptor, Evisum_Config, "network.restart", network.restart,
                                  EET_T_UCHAR);
}

void
config_shutdown(void) {
    efreet_shutdown();
}

Evisum_Config *
config(void) {
    return _evisum_config;
}

static Eina_Bool
_config_check(Evisum_Config *cfg) {
    if (!cfg) return EINA_FALSE;

    if (cfg->version > CONFIG_VERSION) return EINA_FALSE;

    if (cfg->proc.poll_delay <= 0) return EINA_FALSE;

    return EINA_TRUE;
}

static void
_config_free(Evisum_Config *cfg) {
    if (!cfg) return;

    if (cfg->cpu.visual) eina_stringshare_del(cfg->cpu.visual);
    free(cfg);
}

static Evisum_Config *
_config_init() {
    Evisum_Config *cfg = calloc(1, sizeof(Evisum_Config));
    cfg->version = CONFIG_VERSION;
    cfg->proc.poll_delay = 3;
    cfg->proc.show_kthreads = 0;
    cfg->proc.show_statusbar = 1;
    cfg->proc.show_user = 1;
    cfg->proc.sort_type = PROC_SORT_BY_CMD;
    cfg->proc.transparent = 0;
    cfg->proc.fields = (1u << PROC_FIELD_CMD)
                       | (1u << PROC_FIELD_PID)
                       | (1u << PROC_FIELD_THREADS)
                       | (1u << PROC_FIELD_DISK_READ)
                       | (1u << PROC_FIELD_DISK_WRITE)
                       | (1u << PROC_FIELD_NET_IN)
                       | (1u << PROC_FIELD_NET_OUT)
                       | (1u << PROC_FIELD_FILES)
                       | (1u << PROC_FIELD_SIZE)
                       | (1u << PROC_FIELD_CPU_USAGE);
    cfg->proc.alpha = 100;
    for (int i = PROC_FIELD_CMD; i < PROC_FIELD_MAX; i++) cfg->proc.field_order[i] = i;

    cfg->cpu.visual = eina_stringshare_add("default");

    return cfg;
}

Evisum_Config *
config_load(void) {
    Eet_File *f;
    Evisum_Config *cfg = NULL;

    const char *path = _config_file_path();
    if (!ecore_file_exists(path)) {
        cfg = _config_init();

        f = eet_open(path, EET_FILE_MODE_WRITE);
        if (f) {
            eet_data_write(f, _evisum_conf_descriptor, "Config", cfg, EINA_TRUE);
            eet_close(f);
        } else fprintf(stderr, "WARN: config create failed, using defaults in-memory.\n");
    } else {
        f = eet_open(path, EET_FILE_MODE_READ);
        if (f) {
            cfg = eet_data_read(f, _evisum_conf_descriptor, "Config");
            eet_close(f);
        }

        if ((!cfg) || (!_config_check(cfg)) || (cfg->version < CONFIG_VERSION)) {
            if (cfg) _config_free(cfg);
            if (!f) fprintf(stderr, "WARN: config read failed, reinitialising defaults.\n");
            else fprintf(stderr, "WARN: invalid/corrupt config, reinitialising defaults.\n");

            cfg = _config_init();
            f = eet_open(path, EET_FILE_MODE_WRITE);
            if (f) {
                eet_data_write(f, _evisum_conf_descriptor, "Config", cfg, EINA_TRUE);
                eet_close(f);
            } else fprintf(stderr, "WARN: config rewrite failed, continuing with defaults in-memory.\n");
        }
    }
    _evisum_config = cfg;

    return cfg;
}

Eina_Bool
config_save(Evisum_Config *cfg) {
    Eet_File *f;
    const char *path = _config_file_path();
    f = eet_open(path, EET_FILE_MODE_WRITE);
    if (!f) exit(127);
    eet_data_write(f, _evisum_conf_descriptor, "Config", cfg, EINA_TRUE);
    eet_close(f);

    return 1;
}
