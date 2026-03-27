#include "config.h"
#include "evisum_config.h"

#include "evisum_ui.h"
#include "evisum_ui_widget_exel.h"
#include "ui/evisum_ui_process_list.h"
#include "ui/evisum_ui_process_view.h"

#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <pwd.h>

/* If you're reading this comment, God help you. */

extern int EVISUM_EVENT_CONFIG_CHANGED;

typedef struct {
    Proc_Sort type;
    int (*sort_cb)(const void *p1, const void *p2);
} Sorter;

typedef struct {
    Ecore_Thread *thread;
    Eina_Hash *icon_cache;
    Ecore_Event_Handler *handler;
    Eina_Hash *proc_usage_cache;
    Eina_Bool skip_wait;
    Eina_Bool skip_update;
    Eina_Bool update_every_item;
    Eina_Bool first_run;
    Sorter sorters[PROC_SORT_BY_MAX];
    pid_t selected_pid;
    int poll_count;

    Ecore_Timer *resize_timer;
    Evas_Object *win;
    Evas_Object *main_menu;
    Ecore_Timer *main_menu_timer;
    Evas_Object *menu;
    Eina_Bool transparent;

    struct {
        char *text;
        size_t len;
        Ecore_Timer *timer;
        Evas_Object *pop;
        Evas_Object *entry;
        Eina_Bool visible;
        double keytime;
    } search;

    Evas_Object *tb_main;

    Elm_Genlist_Item_Class itc;

    Evas_Object *btn_menu;
    Evisum_Ui_Widget_Exel *widget_exel;

    struct {
        Eina_Bool visible;
        Evas_Object *fr;
        Evas_Object *hbx;
        Evas_Object *pb_cpu;
        Evas_Object *pb_mem;
    } summary;

    Elm_Layout *indicator;
    Evisum_Ui *ui;

} Evisum_Ui_Process_List_View;

#define PROC_COL_RESIZE_HIT_WIDTH 8
#define PROC_COL_MIN_WIDTH        48

typedef struct {
    int64_t cpu_time;
    uint64_t net_in;
    uint64_t net_out;
} Proc_Usage_Cache;

static void _evisum_ui_process_list_content_reset(Evisum_Ui_Process_List_View *view);
static void _evisum_ui_process_list_content_update_all(Evisum_Ui_Process_List_View *view);
static void _evisum_ui_process_list_btn_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED);
static Evas_Object *_evisum_ui_process_list_sort_button_add(Evas_Object *parent, Evisum_Ui_Process_List_View *view,
                                                            Proc_Sort type, double weight_x);

typedef struct {
    const char *name;
    const char *desc;
    Proc_Sort sort;
} Proc_Field_Info;

static const Proc_Field_Info _proc_field_info[PROC_FIELD_MAX] = {
    [PROC_FIELD_CMD] = { "command", "Command",         PROC_SORT_BY_CMD       },
    [PROC_FIELD_UID] = { "user",    "User",            PROC_SORT_BY_UID       },
    [PROC_FIELD_PID] = { "pid",     "Process ID",      PROC_SORT_BY_PID       },
    [PROC_FIELD_THREADS] = { "thr",     "Threads",         PROC_SORT_BY_THREADS   },
    [PROC_FIELD_CPU] = { "cpu",     "CPU #",           PROC_SORT_BY_CPU       },
    [PROC_FIELD_PRI] = { "pri",     "Priority",        PROC_SORT_BY_PRI       },
    [PROC_FIELD_NICE] = { "nice",    "Nice",            PROC_SORT_BY_NICE      },
    [PROC_FIELD_FILES] = { "files",   "Open Files",      PROC_SORT_BY_FILES     },
    [PROC_FIELD_SIZE] = { "size",    "Memory Size",     PROC_SORT_BY_SIZE      },
    [PROC_FIELD_VIRT] = { "virt",    "Memory Virtual",  PROC_SORT_BY_VIRT      },
    [PROC_FIELD_RSS] = { "res",     "Memory Reserved", PROC_SORT_BY_RSS       },
    [PROC_FIELD_SHARED] = { "shr",     "Memory Shared",   PROC_SORT_BY_SHARED    },
    [PROC_FIELD_STATE] = { "state",   "State",           PROC_SORT_BY_STATE     },
    [PROC_FIELD_TIME] = { "time",    "Time",            PROC_SORT_BY_TIME      },
    [PROC_FIELD_CPU_USAGE] = { "cpu %",   "CPU Usage",       PROC_SORT_BY_CPU_USAGE },
    [PROC_FIELD_NET_IN] = { "net in",  "Network In",      PROC_SORT_BY_NET_IN    },
    [PROC_FIELD_NET_OUT] = { "net out", "Network Out",     PROC_SORT_BY_NET_OUT   },
};

static const Proc_Field_Info *
_evisum_ui_process_list_field_meta_get(Proc_Field id) {
    if (id < PROC_FIELD_CMD || id >= PROC_FIELD_MAX) return NULL;
    if (!_proc_field_info[id].name || !_proc_field_info[id].desc) return NULL;
    return &_proc_field_info[id];
}

static Eina_Bool
_evisum_ui_process_list_fields_enabled(const Evisum_Ui_Process_List_View *view, Proc_Field id) {
    if (!view || !view->ui) return 0;
    return evisum_ui_widget_exel_field_enabled_get(view->widget_exel, id);
}

static int
_evisum_ui_process_list_field_default_width_get(Proc_Field id) {
    if (id == PROC_FIELD_CMD) return 2.0 * ELM_SCALE_SIZE(BTN_WIDTH);
    if (id == PROC_FIELD_UID) return 1.8 * ELM_SCALE_SIZE(BTN_WIDTH);
    if (id == PROC_FIELD_CPU_USAGE) return 1.75 * ELM_SCALE_SIZE(BTN_WIDTH);
    if (id == PROC_FIELD_NET_IN || id == PROC_FIELD_NET_OUT) return 1.5 * ELM_SCALE_SIZE(BTN_WIDTH);

    return ELM_SCALE_SIZE(BTN_WIDTH);
}

static int
_evisum_ui_process_list_field_min_width_get(Proc_Field id) {
    if (id == PROC_FIELD_CMD) return ELM_SCALE_SIZE(BTN_WIDTH);

    return ELM_SCALE_SIZE(PROC_COL_MIN_WIDTH);
}

static Proc_Sort
_evisum_ui_process_list_field_sort_get(Proc_Field id) {
    const Proc_Field_Info *info;

    if (id < PROC_FIELD_CMD || id >= PROC_FIELD_MAX) return PROC_SORT_BY_NONE;
    info = _evisum_ui_process_list_field_meta_get(id);
    if (!info) return PROC_SORT_BY_NONE;

    return info->sort;
}

static void
_evisum_ui_process_list_field_proportions_apply(Evisum_Ui_Process_List_View *view) {
    evisum_ui_widget_exel_field_proportions_apply(view->widget_exel);
}

static void
_evisum_ui_process_list_cmd_width_sync(Evisum_Ui_Process_List_View *view, Evas_Coord width) {
    evisum_ui_widget_exel_cmd_width_sync(view->widget_exel, PROC_FIELD_CMD, width);
    _evisum_ui_process_list_content_update_all(view);
}

static void
_evisum_ui_process_list_fields_update_deferred_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;
    view->skip_wait = 1;
    view->skip_update = 0;
}

static void
_evisum_ui_process_list_fields_cache_reset_done_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;

    evisum_ui_widget_exel_deferred_call_schedule(view->widget_exel, 1.0,
                                                 _evisum_ui_process_list_fields_update_deferred_cb, view);
}

static void
_evisum_ui_process_list_fields_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj,
                                           void *event_info) {
    Evas_Event_Mouse_Up *ev;
    Evisum_Ui_Process_List_View *view;

    ev = event_info;
    view = data;

    evisum_ui_widget_exel_fields_menu_show(view->widget_exel, obj, ev);
}

static unsigned int
_evisum_ui_process_list_fields_reference_mask_get_cb(void *data) {
    (void) data;
    return config()->proc.fields;
}

static void
_evisum_ui_process_list_fields_changed_cb(void *data, Eina_Bool changed) {
    Evisum_Ui_Process_List_View *view = data;

    view->skip_update = 1;
    (void) changed;
    evisum_ui_config_save(view->ui);
}

static void
_evisum_ui_process_list_fields_applied_cb(void *data, Eina_Bool changed) {
    Evisum_Ui_Process_List_View *view = data;

    if (!changed) return;

    if (evisum_ui_effects_enabled_get(view->ui))
        elm_object_signal_emit(view->indicator, "fields,change", "evisum/indicator");

    _evisum_ui_process_list_content_reset(view);
}

static void
_evisum_ui_process_list_fields_resize_live_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;
    evisum_ui_widget_exel_genlist_realized_items_update(view->widget_exel);
}

static void
_evisum_ui_process_list_fields_resize_done_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;
    evisum_ui_config_save(view->ui);
}

static void
_evisum_ui_process_list_fields_init(Evisum_Ui_Process_List_View *view) {
    Evisum_Ui_Widget_Exel_Params p = {
        .win = view->win,
        .parent = view->win,
        .field_first = PROC_FIELD_CMD,
        .field_max = PROC_FIELD_MAX,
        .resize_hit_width = PROC_COL_RESIZE_HIT_WIDTH,
        .fields_mask = &view->ui->proc.fields,
        .field_widths = view->ui->proc.field_widths,
        .reference_mask_get_cb = _evisum_ui_process_list_fields_reference_mask_get_cb,
        .fields_changed_cb = _evisum_ui_process_list_fields_changed_cb,
        .fields_applied_cb = _evisum_ui_process_list_fields_applied_cb,
        .resize_live_cb = _evisum_ui_process_list_fields_resize_live_cb,
        .resize_done_cb = _evisum_ui_process_list_fields_resize_done_cb,
        .data = view,
    };

    view->widget_exel = evisum_ui_widget_exel_add(&p);

    for (int i = PROC_FIELD_CMD; i < PROC_FIELD_MAX; i++) {
        Proc_Sort type;
        Evas_Object *btn;
        const Proc_Field_Info *meta;
        const char *name, *desc;

        type = _evisum_ui_process_list_field_sort_get(i);
        btn = _evisum_ui_process_list_sort_button_add(view->win, view, type, i == PROC_FIELD_CMD ? 1.0 : 0.0);
        evas_object_show(btn);

        meta = _evisum_ui_process_list_field_meta_get(i);
        name = meta ? _(meta->name) : "BUG";
        desc = meta ? _(meta->desc) : "BUG";

        elm_object_tooltip_text_set(btn, desc);
        elm_object_text_set(btn, name);

        evisum_ui_widget_exel_field_register(view->widget_exel, i, btn, desc,
                                             _evisum_ui_process_list_field_default_width_get(i),
                                             _evisum_ui_process_list_field_min_width_get(i), i == PROC_FIELD_CMD);
        evisum_ui_widget_exel_field_resize_attach(view->widget_exel, btn, i);
        evisum_ui_widget_exel_field_width_apply(view->widget_exel, i);
        evas_object_event_callback_add(btn, EVAS_CALLBACK_MOUSE_UP, _evisum_ui_process_list_fields_mouse_up_cb, view);
    }
}

static void
_evisum_ui_process_list_content_reset(Evisum_Ui_Process_List_View *view) {
    int j = 0;

    _evisum_ui_process_list_field_proportions_apply(view);

    elm_table_clear(view->tb_main, 0);
    elm_table_pack(view->tb_main, view->btn_menu, j++, 0, 1, 1);
    j = evisum_ui_widget_exel_fields_pack_visible(view->widget_exel, view->tb_main, j);
    elm_table_pack(view->tb_main, evisum_ui_widget_exel_genlist_obj_get(view->widget_exel), 0, 1, j, 1);
    elm_table_pack(view->tb_main, view->summary.fr, 0, 2, j, 1);
    evas_object_show(view->summary.fr);
    evisum_ui_widget_exel_genlist_clear(view->widget_exel);
    evisum_ui_widget_exel_item_cache_reset(view->widget_exel, _evisum_ui_process_list_fields_cache_reset_done_cb, view);
}

static void
_evisum_ui_process_list_item_unrealized_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evas_Object *o;
    Evisum_Ui_Process_List_View *view;
    Eina_List *contents = NULL;

    view = data;

    evisum_ui_widget_exel_genlist_item_all_contents_unset(event_info, &contents);

    EINA_LIST_FREE(contents, o) {
        if (!evisum_ui_widget_exel_item_cache_item_release(view->widget_exel, o)) {
            evas_object_del(o);
        }
    }
}

static void
_evisum_ui_process_list_item_del(void *data, Evas_Object *obj EINA_UNUSED) {
    Proc_Info *proc = data;
    proc_info_free(proc);
}

static Evas_Object *
_evisum_ui_process_list_item_create(Evas_Object *obj) {
    typedef struct {
        Proc_Field type;
        const char *key;
        Evisum_Ui_Widget_Exel_Item_Cell_Type cell_type;
        double align_x;
    } Proc_List_Row_Field;
    static const Proc_List_Row_Field fields[] = {
        { PROC_FIELD_UID,       "uid",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     0.0 },
        { PROC_FIELD_PID,       "pid",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_THREADS,   "thr",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_CPU,       "cpu",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_PRI,       "prio",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_NICE,      "nice",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_FILES,     "files",   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_NET_IN,    "net_in",  EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_NET_OUT,   "net_out", EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_SIZE,      "size",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_VIRT,      "virt",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_RSS,       "rss",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_SHARED,    "share",   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_STATE,     "state",   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_TIME,      "time",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     1.0 },
        { PROC_FIELD_CPU_USAGE, "cpu_u",   EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS, 1.0 },
    };
    Evisum_Ui_Widget_Exel_Item_Cell_Def defs[1 + (sizeof(fields) / sizeof(fields[0]))];
    unsigned int n = 0;
    Evisum_Ui_Process_List_View *view = evas_object_data_get(obj, "process_list_view");

    defs[n++] = (Evisum_Ui_Widget_Exel_Item_Cell_Def) {
        .type = EVISUM_UI_WIDGET_EXEL_ITEM_CELL_CMD,
        .key = "cmd",
        .aux_key = "icon",
        .align_x = 0.0,
        .weight_x = EXPAND,
        .boxed = 0,
        .spacer = ELM_SCALE_SIZE(4),
        .icon_size = 16,
    };

    for (unsigned int i = 0; i < (sizeof(fields) / sizeof(fields[0])); i++) {
        if (!_evisum_ui_process_list_fields_enabled(view, fields[i].type)) continue;

        defs[n++] = (Evisum_Ui_Widget_Exel_Item_Cell_Def) {
            .type = fields[i].cell_type,
            .key = fields[i].key,
            .aux_key = NULL,
            .unit_format = (fields[i].type == PROC_FIELD_CPU_USAGE) ? "%1.1f %%" : NULL,
            .align_x = fields[i].align_x,
            .weight_x = EXPAND,
            .boxed = 1,
            .spacer = ELM_SCALE_SIZE(6),
            .icon_size = 0,
        };
    }

    return evisum_ui_widget_exel_item_row_add(obj, defs, n);
}

static void
_evisum_ui_process_list_run_time_set(char *buf, size_t n, int64_t secs) {
    int rem;

    if (secs < 86400) snprintf(buf, n, "%02" PRIi64 ":%02" PRIi64, secs / 60, secs % 60);
    else {
        rem = secs % 3600;
        snprintf(buf, n, "%02" PRIi64 ":%02d:%02d", secs / 3600, rem / 60, rem % 60);
    }
}

static void
_evisum_ui_process_list_net_rate_set(char *buf, size_t n, uint64_t rate) {
    const char *unit = "B/s";
    double value = rate;

    if (value > 1048576.0) {
        value /= 1048576.0;
        unit = "MB/s";
    } else if (value > 1024.0) {
        value /= 1024.0;
        unit = "KB/s";
    }

    snprintf(buf, n, "%.2f %s", value, unit);
}

static void
_evisum_ui_process_list_content_update_all(Evisum_Ui_Process_List_View *view) {
    view->update_every_item = 1;
    view->poll_count = 0;
}

static Evas_Object *
_evisum_ui_process_list_content_get(void *data, Evas_Object *obj, const char *source) {
    Proc_Info *proc;
    struct passwd *pwd_entry;
    Evas_Object *cell, *o;
    char buf[128];
    Evas_Coord w, ow, bw;
    Evisum_Ui *ui;
    Evisum_Ui_Process_List_View *view;
    const char *icon_name, *icon_old, *icon_new;

    view = evas_object_data_get(obj, "process_list_view");
    if (!view) return NULL;
    ui = view->ui;
    proc = (void *) data;

    if (strcmp(source, "elm.swallow.content")) return NULL;
    if (!proc) return NULL;

    Evas_Object *row = evisum_ui_widget_exel_item_cache_object_get(view->widget_exel);
    if (!row) {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
    }

    evas_object_geometry_get(view->btn_menu, NULL, NULL, &bw, NULL);
    w = evisum_ui_widget_exel_field_width_get(view->widget_exel, PROC_FIELD_CMD);
    w += bw;

    snprintf(buf, sizeof(buf), "%s", proc->command);
    cell = evisum_ui_widget_exel_item_object_get(row, "cmd");
    evisum_ui_widget_exel_item_text_object_if_changed_set(cell, buf);
    evas_object_geometry_get(cell, NULL, NULL, &ow, NULL);
    ow += bw;
    _evisum_ui_process_list_cmd_width_sync(view, ow);

    icon_name = evisum_icon_cache_find(view->icon_cache, proc);
    icon_new = evisum_ui_icon_name_get(icon_name);
    icon_old = NULL;
    o = evisum_ui_widget_exel_item_object_get(row, "icon");
    elm_image_file_get(o, &icon_old, NULL);
    if ((!icon_old) || strcmp(icon_old, icon_new)) evisum_ui_icon_set(o, icon_name);
    evisum_ui_widget_exel_item_column_width_apply(
            o, w, evisum_ui_widget_exel_field_is_last_visible(view->widget_exel, PROC_FIELD_CMD));
    evas_object_show(o);

    cell = evisum_ui_widget_exel_item_field_cell_get(view->widget_exel, row, PROC_FIELD_UID, "uid");
    if (cell) {
        pwd_entry = getpwuid(proc->uid);
        if (pwd_entry) snprintf(buf, sizeof(buf), "%s", pwd_entry->pw_name);
        else snprintf(buf, sizeof(buf), "%i", proc->uid);
        evisum_ui_widget_exel_item_text_object_if_changed_set(cell, buf);

        evas_object_geometry_get(cell, NULL, NULL, &ow, NULL);
        w = evisum_ui_widget_exel_field_width_get(view->widget_exel, PROC_FIELD_UID);
        if (ow > w) {
            evisum_ui_widget_exel_field_min_width_set(view->widget_exel, PROC_FIELD_UID, ow);
            _evisum_ui_process_list_content_update_all(view);
        }
    }

    snprintf(buf, sizeof(buf), "%d", proc->pid);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_PID, "pid", buf);

    snprintf(buf, sizeof(buf), "%d", proc->numthreads);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_THREADS, "thr", buf);

    snprintf(buf, sizeof(buf), "%d", proc->cpu_id);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_CPU, "cpu", buf);

    snprintf(buf, sizeof(buf), "%d", proc->priority);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_PRI, "prio", buf);

    snprintf(buf, sizeof(buf), "%d", proc->nice);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_NICE, "nice", buf);

    snprintf(buf, sizeof(buf), "%d", proc->numfiles);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_FILES, "files", buf);

    if (!proc->is_kernel) snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_size, 1));
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_SIZE, "size", buf);

    if (!proc->is_kernel) snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_virt, 1));
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_VIRT, "virt", buf);

    if ((!proc->is_kernel) || (ui->kthreads_has_rss))
        snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_rss, 1));
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_RSS, "rss", buf);

    if (!proc->is_kernel) snprintf(buf, sizeof(buf), "%s", evisum_size_format(proc->mem_shared, 1));
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_SHARED, "share", buf);

    if ((ui->proc.has_wchan) && (proc->state[0] == 's' && proc->state[1] == 'l'))
        snprintf(buf, sizeof(buf), "%s", proc->wchan);
    else snprintf(buf, sizeof(buf), "%s", proc->state);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_STATE, "state", buf);

    _evisum_ui_process_list_run_time_set(buf, sizeof(buf), proc->run_time);
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_TIME, "time", buf);

    snprintf(buf, sizeof(buf), "%1.1f %%", proc->cpu_usage);
    evisum_ui_widget_exel_item_field_progress_set(view->widget_exel, row, PROC_FIELD_CPU_USAGE, "cpu_u",
                                                  proc->cpu_usage / 100.0, buf);

    if (!proc->is_kernel) _evisum_ui_process_list_net_rate_set(buf, sizeof(buf), proc->net_in);
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_NET_IN, "net_in", buf);

    if (!proc->is_kernel) _evisum_ui_process_list_net_rate_set(buf, sizeof(buf), proc->net_out);
    else {
        buf[0] = '-';
        buf[1] = '\0';
    }
    evisum_ui_widget_exel_item_field_text_set(view->widget_exel, row, PROC_FIELD_NET_OUT, "net_out", buf);

    return row;
}

static void
_evisum_ui_process_list_summary_update(Evisum_Ui_Process_List_View *view) {
    Evisum_Ui *ui;
    Eina_Strbuf *buf;

    ui = view->ui;

    if ((!ui->proc.show_statusbar) || (!view->summary.pb_cpu)) return;

    buf = eina_strbuf_new();

    elm_progressbar_value_set(view->summary.pb_cpu, ui->cpu_usage / 100.0);

    elm_progressbar_value_set(view->summary.pb_mem, (ui->mem_total / 100) / ui->mem_total);
    eina_strbuf_append_printf(buf, "%s / %s ", evisum_size_format(ui->mem_used, 0),
                              evisum_size_format(ui->mem_total, 0));
    elm_object_part_text_set(view->summary.pb_mem, "elm.text.status", eina_strbuf_string_get(buf));

    eina_strbuf_free(buf);
}

static void
_evisum_ui_process_list_summary_add(Evisum_Ui_Process_List_View *view) {
    Evisum_Ui *ui = view->ui;
    Evas_Object *hbx, *ic, *pb, *bx;

    if (!ui->proc.show_statusbar) return;

    hbx = view->summary.hbx;

    ic = elm_icon_add(hbx);
    evisum_ui_icon_set(ic, "cpu");
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    evas_object_size_hint_weight_set(ic, 0, EXPAND);
    elm_box_pack_end(hbx, ic);
    evas_object_show(ic);

    view->summary.pb_cpu = pb = elm_progressbar_add(hbx);
    elm_progressbar_unit_format_set(pb, "%1.2f %%");
    elm_progressbar_span_size_set(pb, 120);
    elm_box_pack_end(hbx, pb);
    evas_object_show(pb);

    ic = elm_icon_add(hbx);
    evisum_ui_icon_set(ic, "memory");
    evas_object_size_hint_min_set(ic, ELM_SCALE_SIZE(16), ELM_SCALE_SIZE(16));
    evas_object_size_hint_weight_set(ic, 0, EXPAND);
    elm_box_pack_end(hbx, ic);
    evas_object_show(ic);

    view->summary.pb_mem = pb = elm_progressbar_add(hbx);
    elm_progressbar_span_size_set(pb, 120);
    evas_object_show(pb);
    elm_box_pack_end(hbx, pb);

    bx = elm_box_add(view->win);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    elm_box_pack_end(hbx, bx);
    evas_object_show(bx);

    view->first_run = 0;
    view->summary.visible = 1;
}

static Eina_List *
_evisum_ui_process_list_sort(Eina_List *list, Evisum_Ui_Process_List_View *view) {
    Evisum_Ui *ui;
    Sorter s;
    ui = view->ui;

    s = view->sorters[ui->proc.sort_type];

    list = eina_list_sort(list, eina_list_count(list), s.sort_cb);

    if (ui->proc.sort_reverse) list = eina_list_reverse(list);

    return list;
}

static Eina_List *
_evisum_ui_process_list_uid_trim(Eina_List *list, uid_t uid) {
    Proc_Info *proc;
    Eina_List *l, *l_next;

    EINA_LIST_FOREACH_SAFE(list, l, l_next, proc) {
        if (proc->uid != uid) {
            proc_info_free(proc);
            list = eina_list_remove_list(list, l);
        }
    }

    return list;
}

static void
_evisum_ui_process_list_usage_cache_free_cb(void *data) {
    Proc_Usage_Cache *cache = data;
    free(cache);
}

static Eina_Bool
_evisum_ui_process_list_process_ignore(Evisum_Ui_Process_List_View *view, Proc_Info *proc) {
    Evisum_Ui *ui = view->ui;
    const char *command;

    if (proc->pid == ui->program_pid) return 1;

    if (!view->search.text || !view->search.len) return 0;

    command = proc->command;
    if (!command || !command[0]) return 1;

    if ((strncasecmp(command, view->search.text, view->search.len) != 0) && (!strstr(command, view->search.text)))
        return 1;

    return 0;
}

static Eina_List *
_evisum_ui_process_list_search_trim(Eina_List *list, Evisum_Ui_Process_List_View *view) {
    Eina_List *l, *l_next;
    Proc_Info *proc;

    EINA_LIST_FOREACH_SAFE(list, l, l_next, proc) {
        if (_evisum_ui_process_list_process_ignore(view, proc)) {
            proc_info_free(proc);
            list = eina_list_remove_list(list, l);
        } else {
            Proc_Usage_Cache *cache;
            int64_t id = proc->pid;

            if ((cache = eina_hash_find(view->proc_usage_cache, &id))) {
                uint64_t net_in = proc->net_in;
                uint64_t net_out = proc->net_out;
                int elapsed = view->ui->proc.poll_delay;
                if (elapsed <= 0) elapsed = 1;

                if (cache->cpu_time) proc->cpu_usage = (double) (proc->cpu_time - cache->cpu_time) / elapsed;
                cache->cpu_time = proc->cpu_time;

                if (cache->net_in && net_in >= cache->net_in) proc->net_in = (net_in - cache->net_in) / elapsed;
                else proc->net_in = 0;

                if (cache->net_out && net_out >= cache->net_out) proc->net_out = (net_out - cache->net_out) / elapsed;
                else proc->net_out = 0;

                cache->net_in = net_in;
                cache->net_out = net_out;
            } else {
                cache = calloc(1, sizeof(Proc_Usage_Cache));
                if (cache) {
                    cache->cpu_time = proc->cpu_time;
                    cache->net_in = proc->net_in;
                    cache->net_out = proc->net_out;
                    proc->net_in = 0;
                    proc->net_out = 0;
                    eina_hash_add(view->proc_usage_cache, &id, cache);
                }
            }
        }
    }

    return list;
}

static Eina_List *
_evisum_ui_process_list_get(Evisum_Ui_Process_List_View *view) {
    Eina_List *list;
    Evisum_Ui *ui;

    ui = view->ui;

    list = proc_info_all_get();

    if (ui->proc.show_user) list = _evisum_ui_process_list_uid_trim(list, getuid());

    list = _evisum_ui_process_list_search_trim(list, view);
    list = _evisum_ui_process_list_sort(list, view);

    return list;
}

static void
_evisum_ui_process_list_process_list(void *data, Ecore_Thread *thread) {
    Evisum_Ui_Process_List_View *view;
    Eina_List *list;
    Evisum_Ui *ui;
    Proc_Info *proc;
    int delay = 1;

    view = data;
    ui = view->ui;

    ecore_thread_name_set(thread, "process_list");

    while (!ecore_thread_check(thread)) {
        for (int i = 0; i < delay * 8; i++) {
            if (ecore_thread_check(thread)) return;
            if (view->skip_wait) {
                view->skip_wait = 0;
                break;
            }
            usleep(125000);
        }
        list = _evisum_ui_process_list_get(view);
        if (!view->skip_update) ecore_thread_feedback(thread, list);
        else {
            EINA_LIST_FREE(list, proc)
            proc_info_free(proc);
        }
        view->skip_update = 0;
        delay = ui->proc.poll_delay;
    }
}

static void
_evisum_ui_process_list_indicator(Evisum_Ui_Process_List_View *view) {
    if ((!view->skip_update) && (!view->resize_timer) && (view->poll_count > 5)) {
        elm_object_signal_emit(view->indicator, "indicator,show", "evisum/indicator");
    }
}

static void
_evisum_ui_process_list_feedback_cb(void *data, Ecore_Thread *thread EINA_UNUSED, void *msg EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view;
    Eina_List *list;
    Proc_Info *proc;
    Elm_Object_Item *it;
    unsigned int n;

    view = data;
    list = msg;

    n = eina_list_count(list);

    evisum_ui_widget_exel_genlist_items_ensure(view->widget_exel, n, &view->itc);

    it = evisum_ui_widget_exel_genlist_first_item_get(view->widget_exel);
    EINA_LIST_FREE(list, proc) {
        Proc_Info *prev = evisum_ui_widget_exel_object_item_data_get(it);
        if (prev) proc_info_free(prev);

        evisum_ui_widget_exel_object_item_data_set(it, proc);
        if (view->update_every_item) evisum_ui_widget_exel_genlist_item_update(it);

        it = evisum_ui_widget_exel_genlist_item_next_get(it);
    }

    if (!view->update_every_item) evisum_ui_widget_exel_genlist_realized_items_update(view->widget_exel);
    view->update_every_item = 0;

    _evisum_ui_process_list_summary_update(view);

    Eina_List *real = evisum_ui_widget_exel_genlist_realized_items_get(view->widget_exel);
    n = evisum_ui_widget_exel_item_cache_active_count_get(view->widget_exel);
    if (n > eina_list_count(real) * 2) {
        evisum_ui_widget_exel_item_cache_steal(view->widget_exel, real);
    }
    eina_list_free(real);

    if (view->first_run) _evisum_ui_process_list_summary_add(view);

    view->poll_count++;

    if (evisum_ui_effects_enabled_get(view->ui)) _evisum_ui_process_list_indicator(view);
}

static Evas_Object *
_evisum_ui_process_list_sort_button_add(Evas_Object *parent, Evisum_Ui_Process_List_View *view, Proc_Sort type,
                                        double weight_x) {
    Evisum_Ui *ui = view->ui;
    Evas_Object *btn;

    btn = evisum_ui_widget_exel_sort_header_button_add(parent, "",
                                                       (ui->proc.sort_type == type) ? ui->proc.sort_reverse : 0,
                                                       weight_x, FILL, _evisum_ui_process_list_btn_clicked_cb, view);
    evas_object_data_set(btn, "type", (void *) (intptr_t) type);
    return btn;
}

static Eina_Bool
_evisum_ui_process_list_btn_clicked_state_save(Evisum_Ui_Process_List_View *view, Evas_Object *btn, Proc_Sort type) {
    Evisum_Ui *ui = view->ui;

    if (evisum_ui_widget_exel_sort_ignore_consume(view->widget_exel)) return 0;

    if (evisum_ui_widget_exel_fields_menu_visible_get(view->widget_exel)) {
        evisum_ui_widget_exel_fields_menu_dismiss(view->widget_exel);
        return 0;
    }

    if (ui->proc.sort_type == type) ui->proc.sort_reverse = !ui->proc.sort_reverse;
    evisum_ui_widget_exel_sort_header_button_state_set(btn, ui->proc.sort_reverse);

    evisum_ui_widget_exel_genlist_page_bring_in(view->widget_exel, 0, 0);

    return 1;
}

static void
_evisum_ui_process_list_field_header_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                                 void *event_info) {
    Evisum_Ui_Process_List_View *view = data;
    Evas_Event_Mouse_Up *ev = event_info;
    (void) e;
    (void) obj;
    evisum_ui_widget_exel_field_resize_mouse_up(view->widget_exel, ev);
}

static void
_evisum_ui_process_list_field_header_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                                   void *event_info) {
    Evisum_Ui_Process_List_View *view = data;
    Evas_Event_Mouse_Move *ev = event_info;
    (void) e;
    (void) obj;
    evisum_ui_widget_exel_field_resize_mouse_move(view->widget_exel, ev);
}

static void
_evisum_ui_process_list_btn_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view;
    Evisum_Ui *ui;
    Proc_Sort type;
    int t;

    view = data;
    ui = view->ui;

    t = (intptr_t) evas_object_data_get(obj, "type");
    type = (t & 0xff);

    if (!_evisum_ui_process_list_btn_clicked_state_save(view, obj, type)) return;

    ui->proc.sort_type = type;
    view->skip_update = 0;
    view->skip_wait = 1;
}

static void
_evisum_ui_process_list_item_menu_dismissed_cb(void *data EINA_UNUSED, Evas_Object *obj, void *ev EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    evas_object_del(obj);

    view->menu = NULL;
}

static void
_evisum_ui_process_list_item_menu_start_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    kill(view->selected_pid, SIGCONT);
}

static void
_evisum_ui_process_list_item_menu_stop_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    kill(view->selected_pid, SIGSTOP);
}

static void
_evisum_ui_process_list_item_menu_kill_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    if (evisum_ui_effects_enabled_get(view->ui)) {
        elm_object_signal_emit(view->indicator, "process,kill", "evisum/indicator");
    }

    kill(view->selected_pid, SIGKILL);
}

static void
_evisum_ui_process_list_item_menu_cancel_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    elm_menu_close(view->menu);
    view->menu = NULL;
}

static void
_evisum_ui_process_list_item_menu_debug_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Proc_Info *proc;
    const char *terminals[] = {
        "terminology", "gnome-terminal", "xfce4-terminal", "konsole", NULL,
    };
    const char *terminal = NULL;
    Evisum_Ui_Process_List_View *view = data;

    _evisum_ui_process_list_item_menu_cancel_cb(view, NULL, NULL);

    proc = proc_info_by_pid(view->selected_pid);
    if (!proc) return;

    for (int i = 0; terminals[i]; i++) {
        if (ecore_file_app_installed(terminals[i])) {
            terminal = terminals[i];
            break;
        }
    }

    if (terminal && ecore_file_app_installed("gdb"))
        ecore_exe_run(eina_slstr_printf("%s -e gdb attach %d", terminal, proc->pid), NULL);

    proc_info_free(proc);
}

static Elm_Object_Item *
_evisum_ui_process_list_item_menu_add(Evas_Object *menu, Elm_Object_Item *parent, const char *icon, const char *label,
                                      Evas_Smart_Cb func, const void *data) {
    return evisum_ui_widget_exel_menu_item_add(menu, parent, icon, label, func, data);
}

static void
_evisum_ui_process_list_item_menu_actions_add(Evas_Object *menu, Elm_Object_Item *menu_it,
                                              Evisum_Ui_Process_List_View *view) {
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "bug", _("Debug"), _evisum_ui_process_list_item_menu_debug_cb,
                                          view);
}

static void
_evisum_ui_process_list_item_menu_manual_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    _evisum_ui_process_list_item_menu_cancel_cb(view, NULL, NULL);

    evisum_ui_process_view_win_add(view->selected_pid, PROC_VIEW_MANUAL);
}

static void
_evisum_ui_process_list_item_menu_threads_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    _evisum_ui_process_list_item_menu_cancel_cb(view, NULL, NULL);

    evisum_ui_process_view_win_add(view->selected_pid, PROC_VIEW_THREADS);
}

static void
_evisum_ui_process_list_item_menu_children_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    _evisum_ui_process_list_item_menu_cancel_cb(view, NULL, NULL);

    evisum_ui_process_view_win_add(view->selected_pid, PROC_VIEW_CHILDREN);
}

static void
_evisum_ui_process_list_item_menu_general_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    _evisum_ui_process_list_item_menu_cancel_cb(view, NULL, NULL);

    evisum_ui_process_view_win_add(view->selected_pid, PROC_VIEW_DEFAULT);
}

static void
_evisum_ui_process_list_item_menu_info_add(Evas_Object *menu, Elm_Object_Item *menu_it,
                                           Evisum_Ui_Process_List_View *view) {
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "info", _("General"),
                                          _evisum_ui_process_list_item_menu_general_cb, view);
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "proc", _("Children"),
                                          _evisum_ui_process_list_item_menu_children_cb, view);
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "threads", _("Threads"),
                                          _evisum_ui_process_list_item_menu_threads_cb, view);
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "manual", _("Manual"),
                                          _evisum_ui_process_list_item_menu_manual_cb, view);
}

static Evas_Object *
_evisum_ui_process_list_item_menu_create(Evisum_Ui_Process_List_View *view, Proc_Info *proc) {
    Elm_Object_Item *menu_it, *menu_it2;
    Evas_Object *menu;
    Eina_Bool stopped;

    if (!proc) return NULL;

    view->selected_pid = proc->pid;

    view->menu = menu = elm_menu_add(view->win);
    if (!menu) return NULL;

    evas_object_smart_callback_add(menu, "dismissed", _evisum_ui_process_list_item_menu_dismissed_cb, view);

    stopped = !(!strcmp(proc->state, "stop"));

    menu_it = _evisum_ui_process_list_item_menu_add(menu, NULL, evisum_icon_cache_find(view->icon_cache, proc),
                                                    proc->command, NULL, NULL);

    menu_it2 = _evisum_ui_process_list_item_menu_add(menu, menu_it, "actions", _("Actions"), NULL, NULL);
    _evisum_ui_process_list_item_menu_actions_add(menu, menu_it2, view);
    elm_menu_item_separator_add(menu, menu_it);

    menu_it2 = _evisum_ui_process_list_item_menu_add(menu, menu_it, "start", _("Start"),
                                                     _evisum_ui_process_list_item_menu_start_cb, view);

    elm_object_item_disabled_set(menu_it2, stopped);
    menu_it2 = _evisum_ui_process_list_item_menu_add(menu, menu_it, "stop", _("Stop"),
                                                     _evisum_ui_process_list_item_menu_stop_cb, view);

    elm_object_item_disabled_set(menu_it2, !stopped);
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "kill", "Kill", _evisum_ui_process_list_item_menu_kill_cb,
                                          view);

    elm_menu_item_separator_add(menu, menu_it);
    menu_it2 = _evisum_ui_process_list_item_menu_add(menu, menu_it, "info", _("Info"), NULL, view);
    _evisum_ui_process_list_item_menu_info_add(menu, menu_it2, view);

    elm_menu_item_separator_add(menu, menu_it);
    _evisum_ui_process_list_item_menu_add(menu, menu_it, "cancel", _("Cancel"),
                                          _evisum_ui_process_list_item_menu_cancel_cb, view);

    return menu;
}

static void
_evisum_ui_process_list_item_pid_secondary_clicked_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
                                                      void *event_info) {
    Evas_Object *menu;
    Evas_Event_Mouse_Up *ev;
    Evisum_Ui_Process_List_View *view;
    Elm_Object_Item *it;
    Proc_Info *proc;

    ev = event_info;
    if (ev->button != 3) return;
    view = data;

    (void) obj;
    it = evisum_ui_widget_exel_genlist_at_xy_item_get(view->widget_exel, ev->output.x, ev->output.y, NULL);
    proc = evisum_ui_widget_exel_object_item_data_get(it);
    if (!proc) return;

    menu = _evisum_ui_process_list_item_menu_create(view, proc);
    if (!menu) return;

    elm_menu_move(menu, ev->canvas.x, ev->canvas.y);
    evas_object_show(menu);
}

static void
_evisum_ui_process_list_item_pid_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Elm_Object_Item *it;
    Proc_Info *proc;
    Evisum_Ui_Process_List_View *view;

    view = data;
    it = event_info;

    evisum_ui_widget_exel_genlist_item_selected_set(it, 0);
    if (view->menu) return;

    proc = evisum_ui_widget_exel_object_item_data_get(it);
    if (!proc) return;

    view->selected_pid = proc->pid;
    evisum_ui_process_view_win_add(proc->pid, PROC_VIEW_DEFAULT);
}

static void
_evisum_ui_process_list_glist_scrolled_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    view->skip_update = 1;
    view->skip_wait = 0;
}

static void
_evisum_ui_process_list_glist_scroll_stopped_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view;
    Evas_Coord oy;
    static Evas_Coord prev_oy;

    view = data;

    evisum_ui_widget_exel_genlist_region_get(view->widget_exel, NULL, &oy, NULL, NULL);

    if (oy != prev_oy) {
        view->skip_wait = 1;
        view->skip_update = 0;
        evisum_ui_widget_exel_genlist_realized_items_update(view->widget_exel);
    }
    prev_oy = oy;
}

static Eina_Bool
_evisum_ui_process_list_main_menu_timer_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;

    evas_object_del(view->main_menu);
    view->main_menu_timer = NULL;
    view->main_menu = NULL;

    return 0;
}

static void
_evisum_ui_process_list_main_menu_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED, void *ev EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view = data;

    elm_ctxpopup_dismiss(view->main_menu);
    if (view->main_menu_timer) _evisum_ui_process_list_main_menu_timer_cb(view);
    else view->main_menu_timer = ecore_timer_add(0.2, _evisum_ui_process_list_main_menu_timer_cb, view);
}

static void
_evisum_ui_process_list_btn_menu_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view;
    Evisum_Ui *ui;

    view = data;
    ui = view->ui;

    if (!view->main_menu) view->main_menu = evisum_ui_main_menu_create(ui, ui->proc.win, obj);
    else _evisum_ui_process_list_main_menu_dismissed_cb(view, NULL, NULL);
}

static Evas_Object *
_evisum_ui_process_list_content_add(Evisum_Ui_Process_List_View *view, Evas_Object *parent) {
    Evas_Object *tb, *btn;
    Evas_Object *fr, *hbx;
    Evisum_Ui_Widget_Exel_Genlist_Params gp = {
        .homogeneous = 1,
        .focus_allow = 1,
        .multi_select = 0,
        .bounce_h = 0,
        .bounce_v = 0,
        .select_mode = ELM_OBJECT_SELECT_MODE_DEFAULT,
        .policy_h = ELM_SCROLLER_POLICY_AUTO,
        .policy_v = ELM_SCROLLER_POLICY_AUTO,
        .weight_x = EXPAND,
        .weight_y = EXPAND,
        .align_x = FILL,
        .align_y = FILL,
        .data_key = "process_list_view",
        .data = view,
    };

    tb = elm_table_add(parent);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_show(tb);

    view->btn_menu = btn = evisum_ui_widget_exel_icon_button_add(tb, "menu", _("Menu"), 0.0, FILL,
                                                                 _evisum_ui_process_list_btn_menu_clicked_cb, view);
    evas_object_show(btn);

    _evisum_ui_process_list_fields_init(view);
    evisum_ui_widget_exel_genlist_add(view->widget_exel, parent, &gp);

    view->itc.item_style = "full";
    view->itc.func.text_get = NULL;
    view->itc.func.content_get = _evisum_ui_process_list_content_get;
    view->itc.func.filter_get = NULL;
    view->itc.func.del = _evisum_ui_process_list_item_del;

    evisum_ui_widget_exel_genlist_smart_callback_add(view->widget_exel, "selected",
                                                     _evisum_ui_process_list_item_pid_clicked_cb, view);
    evisum_ui_widget_exel_genlist_event_callback_add(view->widget_exel, EVAS_CALLBACK_MOUSE_UP,
                                                     _evisum_ui_process_list_item_pid_secondary_clicked_cb, view);
    evisum_ui_widget_exel_genlist_smart_callback_add(view->widget_exel, "unrealized",
                                                     _evisum_ui_process_list_item_unrealized_cb, view);
    evisum_ui_widget_exel_genlist_smart_callback_add(view->widget_exel, "scroll",
                                                     _evisum_ui_process_list_glist_scrolled_cb, view);
    evisum_ui_widget_exel_genlist_smart_callback_add(view->widget_exel, "scroll,anim,stop",
                                                     _evisum_ui_process_list_glist_scroll_stopped_cb, view);
    evisum_ui_widget_exel_genlist_smart_callback_add(view->widget_exel, "scroll,drag,stop",
                                                     _evisum_ui_process_list_glist_scroll_stopped_cb, view);

    view->summary.fr = fr = elm_frame_add(parent);
    elm_object_style_set(fr, "pad_small");
    evas_object_size_hint_weight_set(fr, EXPAND, 0);
    evas_object_size_hint_align_set(fr, FILL, FILL);

    view->summary.hbx = hbx = elm_box_add(parent);
    elm_box_horizontal_set(hbx, 1);
    evas_object_size_hint_weight_set(hbx, 1.0, 0);
    evas_object_size_hint_align_set(hbx, FILL, FILL);
    evas_object_show(hbx);

    elm_object_content_set(fr, hbx);

    return tb;
}

static Eina_Bool
_evisum_ui_process_list_search_empty_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;

    if (!view->search.len) {
        elm_object_focus_allow_set(view->search.entry, 0);
        evas_object_lower(view->search.pop);
        evas_object_hide(view->search.pop);
        view->search.visible = 0;
        view->search.timer = NULL;
        view->skip_update = 0;
        view->skip_wait = 1;
        return 0;
    }

    if (view->search.keytime && ((ecore_loop_time_get() - view->search.keytime) > 0.2)) {
        view->skip_update = 0;
        view->skip_wait = 1;
        view->search.keytime = 0;
    }

    return 1;
}

static void
_evisum_ui_process_list_search_clear(Evisum_Ui_Process_List_View *view) {
    free(view->search.text);
    view->search.text = NULL;
    view->search.len = 0;
    view->skip_update = 0;
}

static void
_evisum_ui_process_list_search_set_text(Evisum_Ui_Process_List_View *view, const char *text) {
    _evisum_ui_process_list_search_clear(view);

    if (!text) return;

    view->search.text = strdup(text);
    if (!view->search.text) return;

    view->search.len = strlen(view->search.text);
}

static void
_evisum_ui_process_list_search_popup_hide(Evisum_Ui_Process_List_View *view) {
    elm_object_focus_allow_set(view->search.entry, 0);
    evas_object_lower(view->search.pop);
    evas_object_hide(view->search.pop);
    view->search.visible = 0;
}

static void
_evisum_ui_process_list_search_popup_show(Evisum_Ui_Process_List_View *view) {
    Evas_Coord w, h;

    evas_object_geometry_get(view->win, NULL, NULL, &w, &h);
    evas_object_move(view->search.pop, w / 2, h / 2);
    evas_object_raise(view->search.pop);
    elm_object_focus_allow_set(view->search.entry, 1);
    elm_object_focus_set(view->search.entry, 1);
    evas_object_show(view->search.pop);
    view->search.visible = 1;
}

static void
_evisum_ui_process_list_search_key_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info) {
    Evas_Event_Key_Down *ev;
    const char *text;
    Evisum_Ui_Process_List_View *view;

    view = data;
    ev = event_info;

    if (ev && !strcmp(ev->keyname, "Escape")) elm_object_text_set(view->search.entry, "");

    text = elm_object_text_get(obj);
    if (!text) return;

    view->skip_update = 1;
    view->search.keytime = ecore_loop_time_get();
    _evisum_ui_process_list_search_set_text(view, text);
    if (!view->search.timer) view->search.timer = ecore_timer_add(0.05, _evisum_ui_process_list_search_empty_cb, view);
}

static void
_evisum_ui_process_list_search_add(Evisum_Ui_Process_List_View *view) {
    Evas_Object *tb, *fr, *rec, *entry;

    view->search.pop = tb = elm_table_add(view->win);
    evas_object_lower(tb);

    rec = evas_object_rectangle_add(evas_object_evas_get(view->win));
    evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
    evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(220), ELM_SCALE_SIZE(128));
    elm_table_pack(tb, rec, 0, 0, 1, 1);

    fr = elm_frame_add(view->win);
    elm_object_text_set(fr, _("Search"));
    evas_object_size_hint_weight_set(fr, 0, 0);
    evas_object_size_hint_align_set(fr, FILL, 0.5);

    view->search.entry = entry = elm_entry_add(fr);
    evas_object_size_hint_weight_set(entry, 0, 0);
    evas_object_size_hint_align_set(entry, 0.5, 0.5);
    elm_entry_single_line_set(entry, 1);
    elm_entry_scrollable_set(entry, 1);
    elm_entry_editable_set(entry, 1);
    elm_object_focus_allow_set(entry, 0);
    evas_object_show(entry);
    elm_object_content_set(fr, entry);
    elm_table_pack(tb, fr, 0, 0, 1, 1);
    evas_object_show(fr);

    evas_object_event_callback_add(entry, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_process_list_search_key_down_cb, view);
}

static void
_evisum_ui_process_list_win_key_down_search(Evisum_Ui_Process_List_View *view, Evas_Event_Key_Down *ev) {
    if (!strcmp(ev->keyname, "Escape")) {
        elm_object_text_set(view->search.entry, "");
        _evisum_ui_process_list_search_clear(view);
        view->skip_wait = 0;
        _evisum_ui_process_list_search_popup_hide(view);
    } else if (ev->string && strcmp(ev->keyname, "BackSpace")) {
        if ((isspace(ev->string[0])) || (iscntrl(ev->string[0]))) return;
        size_t len = strlen(ev->string);
        if (len) {
            elm_entry_entry_append(view->search.entry, ev->string);
            elm_entry_cursor_pos_set(view->search.entry, len);
            _evisum_ui_process_list_search_key_down_cb(view, NULL, view->search.entry, NULL);
        }
        _evisum_ui_process_list_search_popup_show(view);
    }
}

static void
_evisum_ui_process_list_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
    Evas_Event_Key_Down *ev;
    Evisum_Ui_Process_List_View *view;
    Evas_Coord x, y, w, h;

    view = data;
    ev = event_info;

    if (!ev || !ev->keyname) return;

    if (!view) return;

    evisum_ui_widget_exel_genlist_region_get(view->widget_exel, &x, &y, &w, &h);

    if (!strcmp(ev->keyname, "Escape") && !view->search.visible) {
        evas_object_del(view->win);
        return;
    } else if (!strcmp(ev->keyname, "Prior"))
        evisum_ui_widget_exel_genlist_region_bring_in(view->widget_exel, x, y - 512, w, h);
    else if (!strcmp(ev->keyname, "Next"))
        evisum_ui_widget_exel_genlist_region_bring_in(view->widget_exel, x, y + 512, w, h);
    else _evisum_ui_process_list_win_key_down_search(view, ev);

    view->skip_wait = 1;
}

static Eina_Bool
_evisum_ui_process_list_resize_cb(void *data) {
    Evisum_Ui_Process_List_View *view = data;

    view->skip_wait = 0;
    view->resize_timer = NULL;

    return 0;
}

static void
_evisum_ui_process_list_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Process_List_View *view;
    Evisum_Ui *ui;

    view = data;
    ui = view->ui;

    evisum_ui_widget_exel_genlist_realized_items_update(view->widget_exel);

    view->skip_wait = 1;

    if (view->resize_timer) ecore_timer_reset(view->resize_timer);
    else view->resize_timer = ecore_timer_add(0.2, _evisum_ui_process_list_resize_cb, view);

    evas_object_lower(view->search.pop);
    if (view->main_menu) _evisum_ui_process_list_main_menu_dismissed_cb(view, NULL, NULL);
    evisum_ui_widget_exel_fields_menu_dismiss(view->widget_exel);

    evas_object_geometry_get(obj, NULL, NULL, &ui->proc.width, &ui->proc.height);

    if (!evisum_ui_effects_enabled_get(ui)) return;

    evas_object_move(view->indicator, ui->proc.width - ELM_SCALE_SIZE(32), ui->proc.height - ELM_SCALE_SIZE(32));
    evas_object_show(view->indicator);
}

static void
_evisum_ui_process_list_win_alpha_set(Evisum_Ui_Process_List_View *view) {
    Evas_Object *bg, *win;
    Evisum_Ui *ui;
    int r, g, b, a;
    double fade;

    win = view->win;
    ui = view->ui;

    bg = evas_object_data_get(win, "bg");
    if (!bg) return;

    fade = ui->proc.alpha / 100.0;

    // FIXME: Base window colour from theme.
    if (ui->proc.transparent) {
        r = b = g = a = 255;
        evas_object_color_set(view->tb_main, r * fade, g * fade, b * fade, fade * a);
        r = b = g = a = 255;
        evas_object_color_set(bg, r * fade, g * fade, b * fade, fade * a);
        elm_bg_color_set(bg, -1, -1, -1);
    } else {
        r = b = g = a = 255;
        evas_object_color_set(view->tb_main, r, g, b, a);
        r = b = g = a = 255;
        evas_object_color_set(bg, r, g, b, a);
        elm_bg_color_set(bg, -1, -1, -1);
    }

    if (ui->proc.transparent != view->transparent) {
        elm_win_alpha_set(win, ui->proc.transparent);
    }
    view->transparent = ui->proc.transparent;
}

static Eina_Bool
_evisum_ui_process_list_config_changed_cb(void *data, int type EINA_UNUSED, void *event EINA_UNUSED) {
    Eina_Iterator *it;
    Evisum_Ui *ui;
    Evisum_Ui_Process_List_View *view;
    void *d = NULL;

    view = data;
    ui = view->ui;

    it = eina_hash_iterator_data_new(view->proc_usage_cache);
    while (eina_iterator_next(it, &d)) {
        Proc_Usage_Cache *cache = d;
        cache->cpu_time = 0;
        cache->net_in = 0;
        cache->net_out = 0;
    }
    eina_iterator_free(it);

    evisum_ui_widget_exel_genlist_policy_set(view->widget_exel, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
    view->skip_wait = 1;

    if ((!view->summary.visible) && (ui->proc.show_statusbar)) _evisum_ui_process_list_summary_add(view);
    else if ((view->summary.visible) && (!ui->proc.show_statusbar)) {
        elm_box_clear(view->summary.hbx);
        view->summary.visible = 0;
    }

    _evisum_ui_process_list_win_alpha_set(view);

    return 1;
}

static void
_evisum_ui_process_list_win_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_List_View *view;
    Evisum_Ui *ui;

    view = data;
    ui = view->ui;

    evas_object_geometry_get(obj, &ui->proc.x, &ui->proc.y, NULL, NULL);
}

static void
_evisum_ui_process_list_win_del_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                   void *event_info EINA_UNUSED) {
    Evisum_Ui *ui;
    Evisum_Ui_Process_List_View *view;

    view = data;
    ui = view->ui;

    evisum_ui_config_save(ui);

    evisum_ui_widget_exel_deferred_call_cancel(view->widget_exel);

    if (view->search.timer) ecore_timer_del(view->search.timer);

    if (view->thread) ecore_thread_cancel(view->thread);

    if (view->thread) ecore_thread_wait(view->thread, 0.5);

    ecore_event_handler_del(view->handler);
    evisum_icon_cache_del(view->icon_cache);

    view->thread = NULL;
    ui->proc.win = NULL;

    free(view->search.text);

    if (view->widget_exel) evisum_ui_widget_exel_free(view->widget_exel);

    eina_hash_free(view->proc_usage_cache);

    free(view);
    view = NULL;
}

static void
_evisum_ui_process_list_effects_add(Evisum_Ui_Process_List_View *view, Evas_Object *win) {
    Elm_Layout *lay;
    Evas_Object *pb;

    if (evisum_ui_effects_enabled_get(view->ui)) {
        pb = elm_progressbar_add(win);
        elm_object_style_set(pb, "wheel");
        elm_progressbar_pulse_set(pb, 1);
        elm_progressbar_pulse(pb, 1);
        evas_object_show(pb);

        view->indicator = lay = elm_layout_add(win);
        elm_layout_file_set(lay, PACKAGE_DATA_DIR "/themes/evisum.edj", "proc");
        elm_layout_content_set(lay, "evisum/indicator", pb);
        evas_object_show(lay);
    }

    _evisum_ui_process_list_win_alpha_set(view);
    evas_object_show(win);
}

static void
_evisum_ui_process_list_init(Evisum_Ui_Process_List_View *view) {
    view->sorters[PROC_SORT_BY_NONE].sort_cb = proc_sort_by_pid;
    view->sorters[PROC_SORT_BY_UID].sort_cb = proc_sort_by_uid;
    view->sorters[PROC_SORT_BY_PID].sort_cb = proc_sort_by_pid;
    view->sorters[PROC_SORT_BY_THREADS].sort_cb = proc_sort_by_threads;
    view->sorters[PROC_SORT_BY_CPU].sort_cb = proc_sort_by_cpu;
    view->sorters[PROC_SORT_BY_PRI].sort_cb = proc_sort_by_pri;
    view->sorters[PROC_SORT_BY_NICE].sort_cb = proc_sort_by_nice;
    view->sorters[PROC_SORT_BY_FILES].sort_cb = proc_sort_by_files;
    view->sorters[PROC_SORT_BY_SIZE].sort_cb = proc_sort_by_size;
    view->sorters[PROC_SORT_BY_VIRT].sort_cb = proc_sort_by_virt;
    view->sorters[PROC_SORT_BY_RSS].sort_cb = proc_sort_by_rss;
    view->sorters[PROC_SORT_BY_SHARED].sort_cb = proc_sort_by_shared;
    view->sorters[PROC_SORT_BY_CMD].sort_cb = proc_sort_by_cmd;
    view->sorters[PROC_SORT_BY_STATE].sort_cb = proc_sort_by_state;
    view->sorters[PROC_SORT_BY_TIME].sort_cb = proc_sort_by_time;
    view->sorters[PROC_SORT_BY_CPU_USAGE].sort_cb = proc_sort_by_cpu_usage;
    view->sorters[PROC_SORT_BY_NET_IN].sort_cb = proc_sort_by_net_in;
    view->sorters[PROC_SORT_BY_NET_OUT].sort_cb = proc_sort_by_net_out;
}

void
evisum_ui_process_list_win_add(Evisum_Ui *ui) {
    Evas_Object *win, *icon;
    Evas_Object *tb;

    if (ui->proc.win) {
        elm_win_raise(ui->proc.win);
        return;
    }

    Evisum_Ui_Process_List_View *view = calloc(1, sizeof(Evisum_Ui_Process_List_View));
    if (!view) return;

    view->selected_pid = -1;
    view->first_run = 1;
    view->ui = ui;
    view->handler
            = ecore_event_handler_add(EVISUM_EVENT_CONFIG_CHANGED, _evisum_ui_process_list_config_changed_cb, view);
    _evisum_ui_process_list_init(view);

    ui->proc.win = view->win = win = elm_win_add(NULL, "evisum", ELM_WIN_BASIC);
    elm_win_autodel_set(win, 1);
    elm_win_title_set(win, _("Evisum"));
    icon = elm_icon_add(win);
    elm_icon_standard_set(icon, "evisum");
    elm_win_icon_object_set(win, icon);
    evisum_ui_background_add(win);

    if ((ui->proc.width > 1) && (ui->proc.height > 1)) evas_object_resize(win, ui->proc.width, ui->proc.height);
    else evas_object_resize(win, EVISUM_WIN_WIDTH * elm_config_scale_get(), EVISUM_WIN_HEIGHT * elm_config_scale_get());

    if ((ui->proc.x > 0) && (ui->proc.y > 0)) evas_object_move(win, ui->proc.x, ui->proc.y);
    else elm_win_center(win, 1, 1);

    view->tb_main = tb = _evisum_ui_process_list_content_add(view, win);
    elm_win_resize_object_add(win, tb);
    elm_object_content_set(win, tb);

    evisum_ui_widget_exel_item_cache_attach(view->widget_exel, _evisum_ui_process_list_item_create, 30);
    view->icon_cache = evisum_icon_cache_new();
    view->proc_usage_cache = eina_hash_int64_new(_evisum_ui_process_list_usage_cache_free_cb);

    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_process_list_win_del_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_process_list_win_resize_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOVE, _evisum_ui_process_list_win_move_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOUSE_MOVE, _evisum_ui_process_list_field_header_mouse_move_cb,
                                   view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_MOUSE_UP, _evisum_ui_process_list_field_header_mouse_up_cb, view);
    evas_object_event_callback_add(tb, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_process_list_win_key_down_cb, view);

    _evisum_ui_process_list_search_add(view);
    _evisum_ui_process_list_effects_add(view, win);
    _evisum_ui_process_list_content_reset(view);

    _evisum_ui_process_list_win_resize_cb(view, NULL, win, NULL);
    view->thread = ecore_thread_feedback_run(_evisum_ui_process_list_process_list, _evisum_ui_process_list_feedback_cb,
                                             NULL, NULL, view, 0);
}
