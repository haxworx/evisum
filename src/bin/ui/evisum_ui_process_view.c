#include "evisum_ui_process_view.h"
#include "evisum_ui_colors.h"
#include "evisum_ui_widget_exel.h"
#include "../engine/evisum_engine.h"
#include "../background/evisum_background.h"
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

typedef struct {
    Evisum_Ui *ui;
    Evas_Object *win;

    Evas_Object *tab_general;
    Evas_Object *tab_children;
    Evas_Object *tab_thread;
    Evas_Object *tab_manual;

    Evas_Object *general_view;
    Evas_Object *children_view;
    Evas_Object *thread_view;
    Evas_Object *manual_view;

    Evas_Object *current_view;

    int64_t start;
    char *selected_cmd;
    pid_t selected_pid;
    uint32_t poll_count;

    Ecore_Thread *thread;

    Eina_Bool kthreads_has_rss;
    Eina_Bool ignore_initial_resize;

    Eina_Hash *icon_cache;
    Eina_Hash *proc_usage_cache;

    struct {
        Evas_Object *entry_cmd;
        Evas_Object *entry_cmd_args;
        Evas_Object *entry_user;
        Evas_Object *entry_pid;
        Evas_Object *entry_ppid;
        Evas_Object *entry_uid;
        Evas_Object *entry_cpu;
        Evas_Object *entry_threads;
        Evas_Object *entry_files;
        Evas_Object *entry_virt;
        Evas_Object *entry_rss;
        Evas_Object *entry_shared;
        Evas_Object *entry_size;
        Evas_Object *entry_started;
        Evas_Object *entry_run_time;
        Evas_Object *entry_nice;
        Evas_Object *entry_pri;
        Evas_Object *entry_state;
        Evas_Object *entry_cpu_usage;
#if defined(__linux__)
        Evas_Object *entry_net_in;
        Evas_Object *entry_net_out;
        Evas_Object *entry_disk_read;
        Evas_Object *entry_disk_write;
#endif
        Evas_Object *btn_start;
        Evas_Object *btn_stop;
        Evas_Object *btn_kill;
    } general;

    struct {
        Evisum_Ui_Widget_Exel *widget_exel;
        unsigned int fields_mask;
        int field_widths[1];
    } children;

    struct {
        struct {
            int cpu_count;
            unsigned int cores[256];
            Evas_Object *obj;
            Evas_Object *lb;
        } graph;

        Eina_Hash *hash_cpu_times;
        Evisum_Ui_Widget_Exel *widget_exel;
        unsigned int fields_mask;
        int field_widths[5];
        int (*sort_cb)(const void *p1, const void *p2);

        Eina_Bool sort_reverse;
    } threads;

    struct {
        Evas_Object *entry;
        Eina_Bool init;
    } manual;

} Evisum_Ui_Process_View;

static int _process_view_last_width = 0;
static int _process_view_last_height = 0;

typedef struct {
    long cpu_time;
    long cpu_time_prev;
} Thread_Cpu_Info;

typedef struct {
    int64_t start;
    int64_t cpu_time;
#if defined(__linux__)
    uint64_t net_in;
    uint64_t net_out;
    uint64_t disk_read;
    uint64_t disk_write;
#endif
} Proc_Usage_Cache;

typedef struct {
    int tid;
    char *name;
    char *state;
    int cpu_id;
    long cpu_time;
    double cpu_usage;
} Thread_Info;

static Thread_Info *
_evisum_ui_process_view_thread_info_new(Evisum_Ui_Process_View *view, Proc_Info *th) {
    Thread_Info *t;
    Thread_Cpu_Info *inf;
    const char *key;
    double cpu_usage = 0.0;

    t = calloc(1, sizeof(Thread_Info));
    if (!t) return NULL;

    key = eina_slstr_printf("%s:%d", th->thread_name, th->tid);

    inf = eina_hash_find(view->threads.hash_cpu_times, key);
    if (inf) {
        if (inf->cpu_time_prev) cpu_usage = (inf->cpu_time - inf->cpu_time_prev);
        inf->cpu_time_prev = th->cpu_time;
    }

    t->tid = th->tid;
    t->name = strdup(th->thread_name);
    t->cpu_time = th->cpu_time;
    t->state = strdup(th->state);
    t->cpu_id = th->cpu_id;
    t->cpu_usage = cpu_usage;

    return t;
}

Eina_List *
_evisum_ui_process_view_exe_response(const char *command) {
    FILE *p;
    Eina_List *lines;
    char buf[1024];

    p = popen(command, "r");
    if (!p) return NULL;

    lines = NULL;

    while ((fgets(buf, sizeof(buf), p)) != NULL) {
        lines = eina_list_append(lines, evisum_ui_text_man2entry(buf));
    }

    pclose(p);

    return lines;
}

static void
_evisum_ui_process_view_item_del(void *data, Evas_Object *obj EINA_UNUSED) {
    Thread_Info *t = data;

    free(t->name);
    free(t->state);
    free(t);
}

static void
_evisum_ui_process_view_hash_free_cb(void *data) {
    Thread_Cpu_Info *inf = data;
    free(inf);
}

static void
_evisum_ui_process_view_usage_cache_free_cb(void *data) {
    Proc_Usage_Cache *cache = data;
    free(cache);
}

static Evas_Object *
_evisum_ui_process_view_content_get(void *data, Evas_Object *obj, const char *source) {
    Evisum_Ui_Process_View *view;
    Thread_Info *th;
    Evas_Object *row;

    if (strcmp(source, "elm.swallow.content")) return NULL;

    th = data;
    if (!th) return NULL;

    view = evas_object_data_get(obj, "widget_exel_data");
    if (!view) return NULL;

    row = evisum_ui_widget_exel_item_cache_object_get(view->threads.widget_exel);
    if (!row) {
        fprintf(stderr, "Error: Object cache creation failed.\n");
        exit(-1);
    }

    evisum_ui_widget_exel_item_field_text_set(view->threads.widget_exel, row, 0, "tid",
                                              eina_slstr_printf("%d", th->tid));
    evisum_ui_widget_exel_item_field_text_set(view->threads.widget_exel, row, 1, "name", th->name);
    evisum_ui_widget_exel_item_field_text_set(view->threads.widget_exel, row, 2, "state", th->state);
    evisum_ui_widget_exel_item_field_text_set(view->threads.widget_exel, row, 3, "cpu_id",
                                              eina_slstr_printf("%d", th->cpu_id));
    evisum_ui_widget_exel_item_field_progress_set(view->threads.widget_exel, row, 4, "cpu_usage",
                                                  th->cpu_usage > 0 ? th->cpu_usage / 100.0 : 0.0, NULL);

    return row;
}

static void
_evisum_ui_process_view_glist_ensure_n_items(Evisum_Ui_Process_View *view, unsigned int items) {
    Elm_Genlist_Item_Class *itc;
    Evas_Object *glist;
    unsigned int existing;

    glist = evisum_ui_widget_exel_genlist_obj_get(view->threads.widget_exel);
    if (!glist) return;
    existing = elm_genlist_items_count(glist);

    if (items == existing) return;

    itc = elm_genlist_item_class_new();
    itc->item_style = "full";
    itc->func.text_get = NULL;
    itc->func.content_get = _evisum_ui_process_view_content_get;
    itc->func.filter_get = NULL;
    itc->func.del = _evisum_ui_process_view_item_del;

    evisum_ui_widget_exel_genlist_items_ensure(view->threads.widget_exel, items, itc);

    elm_genlist_item_class_free(itc);
}

static int
_evisum_ui_process_view_sort_by_cpu_usage(const void *p1, const void *p2) {
    const Thread_Info *inf1, *inf2;
    double one, two;

    inf1 = p1;
    inf2 = p2;
    one = inf1->cpu_usage;
    two = inf2->cpu_usage;

    if (one > two) return 1;
    else if (one < two) return -1;
    else return 0;
}

static int
_evisum_ui_process_view_sort_by_cpu_id(const void *p1, const void *p2) {
    const Thread_Info *inf1, *inf2;

    inf1 = p1;
    inf2 = p2;

    return inf1->cpu_id - inf2->cpu_id;
}

static int
_evisum_ui_process_view_sort_by_state(const void *p1, const void *p2) {
    const Thread_Info *inf1, *inf2;

    inf1 = p1;
    inf2 = p2;

    return strcmp(inf1->state, inf2->state);
}

static int
_evisum_ui_process_view_sort_by_name(const void *p1, const void *p2) {
    const Thread_Info *inf1, *inf2;

    inf1 = p1;
    inf2 = p2;

    return strcmp(inf1->name, inf2->name);
}

static int
_evisum_ui_process_view_sort_by_tid(const void *p1, const void *p2) {
    const Thread_Info *inf1, *inf2;

    inf1 = p1;
    inf2 = p2;

    return inf1->tid - inf2->tid;
}

static void
_evisum_ui_process_view_thread_view_update(Evisum_Ui_Process_View *view, Proc_Info *proc) {
    Proc_Info *p;
    Thread_Info *t;
    Elm_Object_Item *it;
    Eina_List *l, *threads = NULL;

    _evisum_ui_process_view_glist_ensure_n_items(view, eina_list_count(proc->threads));

    EINA_LIST_FOREACH(proc->threads, l, p) {
        t = _evisum_ui_process_view_thread_info_new(view, p);
        if (t) threads = eina_list_append(threads, t);
    }

    if (view->threads.sort_cb) threads = eina_list_sort(threads, eina_list_count(threads), view->threads.sort_cb);
    if (view->threads.sort_reverse) threads = eina_list_reverse(threads);

    it = evisum_ui_widget_exel_genlist_first_item_get(view->threads.widget_exel);

    EINA_LIST_FREE(threads, t) {
        if (!it) _evisum_ui_process_view_item_del(t, NULL);
        else {
            Thread_Info *prev = evisum_ui_widget_exel_object_item_data_get(it);
            if (prev) _evisum_ui_process_view_item_del(prev, NULL);
            evisum_ui_widget_exel_object_item_data_set(it, t);
            evisum_ui_widget_exel_genlist_item_update(it);
            it = evisum_ui_widget_exel_genlist_item_next_get(it);
        }
    }
}

static void
_evisum_ui_process_view_threads_cpu_usage(Evisum_Ui_Process_View *view, Proc_Info *proc) {
    Eina_List *l;
    Proc_Info *p;

    if (!view->threads.hash_cpu_times)
        view->threads.hash_cpu_times = eina_hash_string_superfast_new(_evisum_ui_process_view_hash_free_cb);

    EINA_LIST_FOREACH(proc->threads, l, p) {
        Thread_Cpu_Info *inf;
        double cpu_usage = 0.0;
        const char *key = eina_slstr_printf("%s:%d", p->thread_name, p->tid);

        if ((inf = eina_hash_find(view->threads.hash_cpu_times, key)) == NULL) {
            inf = calloc(1, sizeof(Thread_Cpu_Info));
            if (inf) {
                inf->cpu_time = p->cpu_time;
                eina_hash_add(view->threads.hash_cpu_times, key, inf);
            }
        } else {
            cpu_usage = (double) (p->cpu_time - inf->cpu_time) * 10;
            inf->cpu_time = p->cpu_time;
        }
        view->threads.graph.cores[p->cpu_id] += cpu_usage;
    }
}

static void
_evisum_ui_process_view_btn_ppid_clicked_cb(void *data, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Process_View *view;
    Evas_Object *entry;
    const char *txt;
    Proc_Info *proc;

    view = data;
    entry = view->general.entry_ppid;
    txt = elm_object_text_get(entry);
    proc = proc_info_by_pid(atoll(txt));

    if (!proc) return;

    evisum_ui_process_view_win_add(view->ui, proc->pid, PROC_VIEW_DEFAULT);
    proc_info_free(proc);
}

static void
_evisum_ui_process_view_item_children_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info) {
    Evisum_Ui_Process_View *view;
    Elm_Object_Item *it;
    Proc_Info *proc;

    view = data;
    it = event_info;

    evisum_ui_widget_exel_genlist_item_selected_set(it, 0);

    proc = evisum_ui_widget_exel_object_item_data_get(it);
    if (!proc) return;

    evisum_ui_process_view_win_add(view->ui, proc->pid, PROC_VIEW_DEFAULT);
}

static char *
_evisum_ui_process_view_children_text_get(void *data, Evas_Object *obj, const char *part)

{
    Proc_Info *child = data;
    char buf[256];

    snprintf(buf, sizeof(buf), "%s (%d) ", child->command, child->pid);

    return strdup(buf);
}

static Evas_Object *
_evisum_ui_process_view_children_icon_get(void *data, Evas_Object *obj, const char *part) {
    Proc_Info *proc;
    Evisum_Ui_Process_View *view;
    Evas_Object *ic = elm_icon_add(obj);

    proc = data;
    view = evas_object_data_get(obj, "widget_exel_data");

    if (!strcmp(part, "elm.swallow.icon")) {
        evisum_ui_icon_set(ic, evisum_icon_cache_find(view->icon_cache, proc));
    }

    evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);

    return ic;
}

static void
_evisum_ui_process_view_children_del(void *data, Evas_Object *obj EINA_UNUSED) {
    Proc_Info *proc = data;

    eina_list_free(proc->children);
    proc->children = NULL;
    proc_info_free(proc);
}

static void
_evisum_ui_process_view_children_populate(Evisum_Ui_Process_View *view, Elm_Object_Item *parent, Eina_List *children) {
    Elm_Genlist_Item_Class *itc;
    Eina_List *l;
    Elm_Object_Item *it;
    Proc_Info *child;

    itc = elm_genlist_item_class_new();
    itc->item_style = "default";
    itc->func.content_get = _evisum_ui_process_view_children_icon_get;
    itc->func.text_get = _evisum_ui_process_view_children_text_get;
    itc->func.filter_get = NULL;
    itc->func.del = _evisum_ui_process_view_children_del;

    EINA_LIST_FOREACH(children, l, child) {
        it = evisum_ui_widget_exel_genlist_item_append(
                view->children.widget_exel, itc, child, parent,
                (child->children ? ELM_GENLIST_ITEM_TREE : ELM_GENLIST_ITEM_NONE), NULL, NULL);
        evisum_ui_widget_exel_genlist_item_update(it);
        if (child->children) {
            child->children = eina_list_sort(child->children, eina_list_count(child->children), proc_sort_by_age);
            _evisum_ui_process_view_children_populate(view, it, child->children);
        }
    }
    elm_genlist_item_class_free(itc);
}

static Eina_Bool
_evisum_ui_process_view_children_view_update(void *data) {
    Eina_List *children, *l;
    Proc_Info *child;
    Evisum_Ui_Process_View *view = data;

    evisum_ui_widget_exel_genlist_clear(view->children.widget_exel);

    if (view->selected_pid == 0) return 0;

    children = proc_info_pid_children_get(view->selected_pid);
    EINA_LIST_FOREACH(children, l, child) {
        if (child->pid == view->selected_pid) {
            child->children = eina_list_sort(child->children, eina_list_count(child->children), proc_sort_by_age);
            _evisum_ui_process_view_children_populate(view, NULL, child->children);
            break;
        }
    }
    child = eina_list_nth(children, 0);
    if (child) {
        Eina_List *root_children = child->children;
        child->children = NULL;
        proc_info_free(child);
        eina_list_free(root_children);
    }
    eina_list_free(children);

    return 1;
}

static void
_evisum_ui_process_view_proc_info_main(void *data, Ecore_Thread *thread) {
    Evisum_Ui_Process_View *view = data;
    uint64_t seq = 0;

    ecore_thread_name_set(thread, "process_view");

    while (!ecore_thread_check(thread)) {
        if (!evisum_background_update_wait(&seq)) continue;
        Proc_Info *proc = proc_info_by_pid(view->selected_pid);
        if (!proc) continue;
        ecore_thread_feedback(thread, proc);
    }
}

static void
_evisum_ui_process_view_graph_summary_update(Evisum_Ui_Process_View *view, Proc_Info *proc) {
    elm_object_text_set(view->threads.graph.lb,
                        eina_slstr_printf(_("<b>"
                                            "CPU: %.0f%%<br>"
                                            "Size: %s<br>"
                                            "Reserved: %s<br>"
                                            "Virtual: %s"
                                            "</>"),
                                          proc->cpu_usage, evisum_size_format(proc->mem_size, 0),
                                          evisum_size_format(proc->mem_rss, 0), evisum_size_format(proc->mem_virt, 0)));
}

static void
_evisum_ui_process_view_graph_update(Evisum_Ui_Process_View *view, Proc_Info *proc) {
    Evas_Object *obj = view->threads.graph.obj;
    unsigned int *pixels, *pix;
    Evas_Coord x, y, w, h;
    int iw, stride;
    Eina_Bool clear = 0;

    evas_object_geometry_get(obj, &x, &y, &w, &h);
    evas_object_image_size_get(obj, &iw, NULL);

    if (iw != w) {
        evas_object_image_size_set(obj, w, view->threads.graph.cpu_count);
        clear = 1;
    }

    pixels = evas_object_image_data_get(obj, 1);
    if (!pixels) return;

    stride = evas_object_image_stride_get(obj);

    for (y = 0; y < view->threads.graph.cpu_count; y++) {
        if (clear) {
            pix = &(pixels[y * (stride / 4)]);
            for (x = 0; x < (w - 1); x++) pix[x] = cpu_colormap[0];
        } else {
            pix = &(pixels[y * (stride / 4)]);
            for (x = 0; x < (w - 1); x++) pix[x] = pix[x + 1];
        }
        unsigned int c1;
        c1 = cpu_colormap[view->threads.graph.cores[y] & 0xff];
        pix = &(pixels[y * (stride / 4)]);
        pix[x] = c1;
    }

    evas_object_image_data_set(obj, pixels);
    evas_object_image_data_update_add(obj, 0, 0, w, view->threads.graph.cpu_count);
    memset(view->threads.graph.cores, 0, 255 * sizeof(unsigned int));
}

static Evas_Object *
_evisum_ui_process_view_graph(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *tb, *obj, *tb2, *lb, *scr, *fr, *rec;

    view->threads.graph.cpu_count = system_cpu_count_get();

    tb = elm_table_add(parent);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_show(tb);

    scr = elm_scroller_add(parent);
    evas_object_size_hint_align_set(scr, FILL, FILL);
    evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
    evas_object_show(scr);

    view->threads.graph.obj = obj = evas_object_image_add(evas_object_evas_get(parent));
    evas_object_size_hint_align_set(obj, FILL, FILL);
    evas_object_size_hint_weight_set(obj, EXPAND, EXPAND);
    evas_object_image_smooth_scale_set(obj, 0);
    evas_object_image_filled_set(obj, 1);
    evas_object_image_alpha_set(obj, 0);
    evas_object_show(obj);

    evas_object_size_hint_min_set(obj, 100, (2 * view->threads.graph.cpu_count) * elm_config_scale_get());
    elm_object_content_set(scr, obj);

    // Overlay
    fr = elm_frame_add(parent);
    elm_object_style_set(fr, "pad_small");
    evas_object_size_hint_align_set(fr, 0.01, 0.03);
    evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
    evas_object_show(fr);

    tb2 = elm_table_add(parent);
    evas_object_size_hint_weight_set(tb2, EXPAND, 0);
    evas_object_size_hint_align_set(tb2, 0.0, 0.0);
    evas_object_show(tb2);

    rec = evas_object_rectangle_add(evas_object_evas_get(parent));
    evas_object_color_set(rec, 0, 0, 0, 64);
    evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(128), ELM_SCALE_SIZE(92));
    evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(128), ELM_SCALE_SIZE(92));
    evas_object_show(rec);

    view->threads.graph.lb = lb = elm_entry_add(parent);
    elm_entry_single_line_set(lb, 1);
    elm_entry_select_allow_set(lb, 1);
    elm_entry_editable_set(lb, 0);
    elm_object_focus_allow_set(lb, 0);
    evas_object_size_hint_weight_set(lb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(lb, 0.5, 0.5);
    evas_object_show(lb);

    elm_table_pack(tb2, rec, 0, 0, 1, 1);
    elm_table_pack(tb2, lb, 0, 0, 1, 1);
    elm_object_content_set(fr, tb2);

    elm_table_pack(tb, scr, 0, 0, 1, 1);
    elm_table_pack(tb, fr, 0, 0, 1, 1);

    return tb;
}

static char *
_evisum_ui_process_view_time_string(int64_t epoch) {
    struct tm *info;
    char buf[256];
    time_t rawtime = (time_t) epoch;

    info = localtime(&rawtime);
    strftime(buf, sizeof(buf), "%F %T", info);

    return strdup(buf);
}

static char *
_evisum_ui_process_view_run_time_string(int64_t secs) {
    char buf[256];
    int rem;

    if (secs < 86400) snprintf(buf, sizeof(buf), "%02" PRIi64 ":%02" PRIi64, secs / 60, secs % 60);
    else {
        rem = secs % 3600;
        snprintf(buf, sizeof(buf), "%02" PRIi64 ":%02d:%02d", secs / 3600, rem / 60, rem % 60);
    }
    return strdup(buf);
}

#if defined(__linux__)
static char *
_evisum_ui_process_view_rate_string(uint64_t rate) {
    const char *unit = _("B/s");
    char buf[256];
    double value = rate;

    if (value > 1073741824.0) {
        value /= 1073741824.0;
        unit = _("GB/s");
    } else if (value > 1048576.0) {
        value /= 1048576.0;
        unit = _("MB/s");
    } else if (value > 1024.0) {
        value /= 1024.0;
        unit = _("KB/s");
    }

    snprintf(buf, sizeof(buf), "%.2f %s", value, unit);
    return strdup(buf);
}
#endif

static void
_evisum_ui_process_view_manual_init_cb(void *data, Ecore_Thread *thread) {
    Eina_List *lines = NULL;
    Eina_Strbuf *sbuf = NULL;
    char buf[4096];
    char *line;
    int n = 1;
    Evisum_Ui_Process_View *view = data;

    setenv("MANWIDTH", "80", 1);
    ecore_thread_feedback(thread, strdup("<code>"));

    if (!strchr(view->selected_cmd, ' ')) {
        snprintf(buf, sizeof(buf), "man %s | col -bx", view->selected_cmd);
        lines = _evisum_ui_process_view_exe_response(buf);
    }
    if (!lines) {
        snprintf(buf, sizeof(buf), _("No documentation found for %s."), view->selected_cmd);
        ecore_thread_feedback(thread, strdup(buf));
    } else sbuf = eina_strbuf_new();
    EINA_LIST_FREE(lines, line) {
        if (n++ > 1) {
            eina_strbuf_append_printf(sbuf, "%s<br>", line);
            if (eina_strbuf_length_get(sbuf) >= 4096) ecore_thread_feedback(thread, eina_strbuf_string_steal(sbuf));
        }
        free(line);
    }
    if (sbuf) {
        if (eina_strbuf_length_get(sbuf)) ecore_thread_feedback(thread, eina_strbuf_string_steal(sbuf));
        eina_strbuf_free(sbuf);
    }
    ecore_thread_feedback(thread, strdup("</code>"));
    unsetenv("MANWIDTH");
    view->manual.init = 1;
}

static void
_evisum_ui_process_view_manual_init_feedback_cb(void *data, Ecore_Thread *thread, void *msgdata) {
    Evas_Object *ent;
    char *s;
    Evisum_Ui_Process_View *view = data;

    ent = view->manual.entry;
    s = msgdata;
    elm_entry_entry_append(ent, s);

    free(s);
}

static void
_evisum_ui_process_view_manual_init(Evisum_Ui_Process_View *view) {
    if (view->manual.init) return;

    if ((!view->selected_cmd) || (!view->selected_cmd[0])) return;

    ecore_thread_feedback_run(_evisum_ui_process_view_manual_init_cb, _evisum_ui_process_view_manual_init_feedback_cb,
                              NULL, NULL, view, 1);
}

static void
_evisum_ui_process_view_general_view_update(Evisum_Ui_Process_View *view, Proc_Info *proc) {
    struct passwd *pwd_entry;
    char *s;

    if (!strcmp(proc->state, _("stopped"))) {
        elm_object_disabled_set(view->general.btn_stop, 1);
        elm_object_disabled_set(view->general.btn_start, 0);
    } else {
        elm_object_disabled_set(view->general.btn_stop, 0);
        elm_object_disabled_set(view->general.btn_start, 1);
    }

    elm_object_text_set(view->general.entry_cmd, eina_slstr_printf("<subtitle>%s</subtitle>", proc->command));
    pwd_entry = getpwuid(proc->uid);
    if (pwd_entry) elm_object_text_set(view->general.entry_user, pwd_entry->pw_name);

    if (proc->arguments) elm_object_text_set(view->general.entry_cmd_args, proc->arguments);
    else elm_object_text_set(view->general.entry_cmd_args, "");

    elm_object_text_set(view->general.entry_pid, eina_slstr_printf("%d", proc->pid));
    elm_object_text_set(view->general.entry_uid, eina_slstr_printf("%d", proc->uid));
    elm_object_text_set(view->general.entry_cpu, eina_slstr_printf("%d", proc->cpu_id));
    elm_object_text_set(view->general.entry_ppid, eina_slstr_printf("%d", proc->ppid));
    elm_object_text_set(view->general.entry_threads, eina_slstr_printf("%d", proc->numthreads));
    elm_object_text_set(view->general.entry_files, eina_slstr_printf("%d", proc->numfiles));
    if (!proc->is_kernel) elm_object_text_set(view->general.entry_virt, evisum_size_format(proc->mem_virt, 0));
    else elm_object_text_set(view->general.entry_virt, _("-"));

    if ((!proc->is_kernel) || (view->kthreads_has_rss))
        elm_object_text_set(view->general.entry_rss, evisum_size_format(proc->mem_rss, 0));
    else elm_object_text_set(view->general.entry_rss, _("-"));

    if (!proc->is_kernel) elm_object_text_set(view->general.entry_shared, evisum_size_format(proc->mem_shared, 0));
    else elm_object_text_set(view->general.entry_shared, _("-"));

    if (!proc->is_kernel) elm_object_text_set(view->general.entry_size, evisum_size_format(proc->mem_size, 0));
    else elm_object_text_set(view->general.entry_size, _("-"));

    s = _evisum_ui_process_view_run_time_string(proc->run_time);
    if (s) {
        elm_object_text_set(view->general.entry_run_time, s);
        free(s);
    }
    s = _evisum_ui_process_view_time_string(proc->start);
    if (s) {
        elm_object_text_set(view->general.entry_started, s);
        free(s);
    }
    elm_object_text_set(view->general.entry_nice, eina_slstr_printf("%d", proc->nice));
    elm_object_text_set(view->general.entry_pri, eina_slstr_printf("%d", proc->priority));
    if (proc->wchan[0] && ((proc->state[0] == 's' && proc->state[1] == 'l')))
        elm_object_text_set(view->general.entry_state, proc->wchan);
    else elm_object_text_set(view->general.entry_state, proc->state);
    elm_object_text_set(view->general.entry_cpu_usage, eina_slstr_printf("%.0f%%", proc->cpu_usage));

#if defined(__linux__)
    if (!proc->is_kernel) {
        s = _evisum_ui_process_view_rate_string(proc->net_in);
        if (s) {
            elm_object_text_set(view->general.entry_net_in, s);
            free(s);
        }

        s = _evisum_ui_process_view_rate_string(proc->net_out);
        if (s) {
            elm_object_text_set(view->general.entry_net_out, s);
            free(s);
        }

        s = _evisum_ui_process_view_rate_string(proc->disk_read);
        if (s) {
            elm_object_text_set(view->general.entry_disk_read, s);
            free(s);
        }

        s = _evisum_ui_process_view_rate_string(proc->disk_write);
        if (s) {
            elm_object_text_set(view->general.entry_disk_write, s);
            free(s);
        }
    } else {
        elm_object_text_set(view->general.entry_net_in, _("-"));
        elm_object_text_set(view->general.entry_net_out, _("-"));
        elm_object_text_set(view->general.entry_disk_read, _("-"));
        elm_object_text_set(view->general.entry_disk_write, _("-"));
    }
#endif
}

static void
_evisum_ui_process_view_proc_gone(Evisum_Ui_Process_View *view) {
    const char *fmt = _("%s (%d) - Not running");

    elm_win_title_set(view->win, eina_slstr_printf(fmt, view->selected_cmd, view->selected_pid));

    elm_object_disabled_set(view->general.btn_start, 1);
    elm_object_disabled_set(view->general.btn_stop, 1);
    elm_object_disabled_set(view->general.btn_kill, 1);

    if (!ecore_thread_check(view->thread)) ecore_thread_cancel(view->thread);
    view->thread = NULL;
}

static void
_evisum_ui_process_view_proc_info_feedback_cb(void *data, Ecore_Thread *thread, void *msg) {
    Evisum_Ui_Process_View *view;
    Proc_Info *proc;
    Proc_Usage_Cache *cache;
    int64_t id;
    int elapsed;
#if defined(__linux__)
    uint64_t net_in_abs = 0;
    uint64_t net_out_abs = 0;
    uint64_t disk_read_abs = 0;
    uint64_t disk_write_abs = 0;
#endif

    view = data;
    proc = msg;
    elapsed = 1;

    if (!proc || (view->start && (proc->start != view->start))) {
        if (proc) proc_info_free(proc);
        _evisum_ui_process_view_proc_gone(view);
        return;
    }

    if (ecore_thread_check(thread)) {
        proc_info_free(proc);
        return;
    }

    _evisum_ui_process_view_threads_cpu_usage(view, proc);

    if ((view->poll_count != 0) && (view->poll_count % 10)) {
        _evisum_ui_process_view_graph_update(view, proc);
        proc_info_free(proc);
        view->poll_count++;
        return;
    }
    if (!view->proc_usage_cache)
        view->proc_usage_cache = eina_hash_int64_new(_evisum_ui_process_view_usage_cache_free_cb);

    id = proc->pid;
    cache = view->proc_usage_cache ? eina_hash_find(view->proc_usage_cache, &id) : NULL;
#if defined(__linux__)
    disk_read_abs = proc->disk_read;
    disk_write_abs = proc->disk_write;
    net_in_abs = proc->net_in;
    net_out_abs = proc->net_out;
#endif
    if (!cache) {
        cache = calloc(1, sizeof(Proc_Usage_Cache));
        if (cache) {
            cache->start = proc->start;
            cache->cpu_time = proc->cpu_time;
#if defined(__linux__)
            cache->net_in = net_in_abs;
            cache->net_out = net_out_abs;
            cache->disk_read = disk_read_abs;
            cache->disk_write = disk_write_abs;
            proc->net_in = 0;
            proc->net_out = 0;
            proc->disk_read = 0;
            proc->disk_write = 0;
#endif
            proc->cpu_usage = 0.0;
            if (view->proc_usage_cache) eina_hash_add(view->proc_usage_cache, &id, cache);
            else free(cache);
        } else {
            proc->cpu_usage = 0.0;
#if defined(__linux__)
            proc->net_in = 0;
            proc->net_out = 0;
            proc->disk_read = 0;
            proc->disk_write = 0;
#endif
        }
    } else if (cache->start != proc->start) {
        cache->start = proc->start;
        cache->cpu_time = proc->cpu_time;
#if defined(__linux__)
        cache->net_in = net_in_abs;
        cache->net_out = net_out_abs;
        cache->disk_read = disk_read_abs;
        cache->disk_write = disk_write_abs;
        proc->net_in = 0;
        proc->net_out = 0;
        proc->disk_read = 0;
        proc->disk_write = 0;
#endif
        proc->cpu_usage = 0.0;
    } else {
        if (cache->cpu_time && (proc->cpu_time >= cache->cpu_time))
            proc->cpu_usage = (double) (proc->cpu_time - cache->cpu_time) / elapsed;
        else proc->cpu_usage = 0.0;
        cache->cpu_time = proc->cpu_time;
#if defined(__linux__)
        if (cache->net_in && (net_in_abs >= cache->net_in)) proc->net_in = (net_in_abs - cache->net_in) / elapsed;
        else proc->net_in = 0;

        if (cache->net_out && (net_out_abs >= cache->net_out)) proc->net_out = (net_out_abs - cache->net_out) / elapsed;
        else proc->net_out = 0;

        if (cache->disk_read && (disk_read_abs >= cache->disk_read))
            proc->disk_read = (disk_read_abs - cache->disk_read) / elapsed;
        else proc->disk_read = 0;

        if (cache->disk_write && (disk_write_abs >= cache->disk_write))
            proc->disk_write = (disk_write_abs - cache->disk_write) / elapsed;
        else proc->disk_write = 0;

        cache->net_in = net_in_abs;
        cache->net_out = net_out_abs;
        cache->disk_read = disk_read_abs;
        cache->disk_write = disk_write_abs;
#endif
    }

    _evisum_ui_process_view_graph_update(view, proc);
    _evisum_ui_process_view_graph_summary_update(view, proc);
    _evisum_ui_process_view_thread_view_update(view, proc);
    _evisum_ui_process_view_general_view_update(view, proc);

    view->poll_count++;

    proc_info_free(proc);
}

static void
_evisum_ui_process_view_btn_start_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    if (view->selected_pid == -1) return;

    kill(view->selected_pid, SIGCONT);
}

static void
_evisum_ui_process_view_btn_stop_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    if (view->selected_pid == -1) return;

    kill(view->selected_pid, SIGSTOP);
}

static void
_evisum_ui_process_view_btn_kill_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    if (view->selected_pid == -1) return;

    kill(view->selected_pid, SIGKILL);
}

static Evas_Object *
_evisum_ui_process_view_entry_add(Evas_Object *parent) {
    Evas_Object *entry = elm_entry_add(parent);
    evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
    evas_object_size_hint_align_set(entry, FILL, FILL);
    elm_entry_single_line_set(entry, 1);
    elm_entry_scrollable_set(entry, 1);
    elm_entry_editable_set(entry, 0);
    elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
    evas_object_show(entry);

    return entry;
}

static Evas_Object *
_evisum_ui_process_view_lb_add(Evas_Object *parent, const char *text) {
    Evas_Object *lb = elm_label_add(parent);
    elm_object_text_set(lb, text);
    evas_object_show(lb);

    return lb;
}

static Evas_Object *
_evisum_ui_process_view_general_tab_add(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *fr, *hbx, *tb, *scr;
    Evas_Object *lb, *entry, *btn, *pad, *ic;
    Evas_Object *rec;
    Proc_Info *proc;
    int i = 0;

    fr = elm_frame_add(parent);
    elm_object_style_set(fr, "pad_medium");
    evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
    evas_object_size_hint_align_set(fr, FILL, FILL);

    scr = elm_scroller_add(parent);
    evas_object_size_hint_weight_set(scr, EXPAND, EXPAND);
    evas_object_size_hint_align_set(scr, FILL, FILL);
    elm_scroller_policy_set(scr, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
    elm_scroller_bounce_set(scr, 0, 1);
    evas_object_show(scr);
    elm_object_content_set(fr, scr);

    tb = elm_table_add(parent);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_show(tb);
    elm_object_focus_allow_set(tb, 1);
    elm_object_content_set(scr, tb);

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(64), ELM_SCALE_SIZE(64));
    evas_object_size_hint_max_set(rec, ELM_SCALE_SIZE(64), ELM_SCALE_SIZE(64));
    evas_object_size_hint_align_set(rec, FILL, 1.0);
    elm_table_pack(tb, rec, 0, i, 1, 1);

    proc = proc_info_by_pid(view->selected_pid);
    if (proc) {
        ic = elm_icon_add(parent);
        evas_object_size_hint_weight_set(ic, EXPAND, EXPAND);
        evas_object_size_hint_align_set(ic, FILL, FILL);
        evisum_ui_icon_set(ic, evisum_icon_cache_find(view->icon_cache, proc));
        evas_object_show(ic);
        proc_info_free(proc);
        elm_table_pack(tb, ic, 0, i, 1, 1);
    }

    lb = _evisum_ui_process_view_lb_add(parent, _("Command:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_cmd = entry = elm_label_add(parent);
    evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
    evas_object_size_hint_align_set(entry, 0.0, 0.5);
    evas_object_show(entry);
    evas_object_hide(lb);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Command line:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_cmd_args = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("PID:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_pid = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Username:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_user = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("UID:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_uid = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    btn = elm_button_add(parent);
    elm_object_style_set(btn, "anchor");
    evas_object_size_hint_weight_set(btn, 0, EXPAND);
    elm_object_text_set(btn, _("PPID:"));
    elm_table_pack(tb, btn, 0, i, 1, 1);
    evas_object_show(btn);

    view->general.entry_ppid = entry = _evisum_ui_process_view_entry_add(parent);
    evas_object_smart_callback_add(btn, "clicked", _evisum_ui_process_view_btn_ppid_clicked_cb, view);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

#if defined(__MacOS__)
    lb = _evisum_ui_process_view_lb_add(parent, _("WQ #:"));
#else
    lb = _evisum_ui_process_view_lb_add(parent, _("CPU #:"));
#endif
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_cpu = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Threads:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_threads = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Open Files:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_files = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Memory :"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_size = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Shared memory:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_shared = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Resident memory:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_rss = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Virtual memory:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_virt = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

#if defined(__linux__)
    lb = _evisum_ui_process_view_lb_add(parent, _(" Disk Read:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_disk_read = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Disk Write:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_disk_write = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Network In:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_net_in = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Network Out:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_net_out = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);
#endif

    lb = _evisum_ui_process_view_lb_add(parent, _(" Start time:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_started = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _(" Run time:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_run_time = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Nice:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_nice = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("Priority:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_pri = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("State:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_state = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    lb = _evisum_ui_process_view_lb_add(parent, _("CPU %:"));
    elm_table_pack(tb, lb, 0, i, 1, 1);
    view->general.entry_cpu_usage = entry = _evisum_ui_process_view_entry_add(parent);
    elm_table_pack(tb, entry, 1, i++, 1, 1);

    hbx = elm_box_add(parent);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);
    evas_object_size_hint_align_set(hbx, 1.0, FILL);
    elm_box_horizontal_set(hbx, 1);
    elm_box_homogeneous_set(hbx, 1);
    evas_object_show(hbx);
    elm_table_pack(tb, hbx, 1, i, 1, 1);

    pad = elm_frame_add(parent);
    evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
    elm_object_style_set(pad, "pad_small");
    evas_object_show(pad);
    elm_box_pack_end(hbx, pad);
    pad = elm_frame_add(parent);
    evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
    elm_object_style_set(pad, "pad_small");
    evas_object_show(pad);
    elm_box_pack_end(hbx, pad);

    btn = evisum_ui_button_add(parent, NULL, _("stop"), "stop", _evisum_ui_process_view_btn_stop_clicked_cb, view);
    evas_object_show(btn);
    view->general.btn_stop = btn;
    elm_box_pack_end(hbx, btn);

    btn = evisum_ui_button_add(parent, NULL, _("start"), "start", _evisum_ui_process_view_btn_start_clicked_cb, view);
    evas_object_show(btn);
    view->general.btn_start = btn;
    elm_box_pack_end(hbx, btn);

    btn = evisum_ui_button_add(parent, NULL, _("kill"), "kill", _evisum_ui_process_view_btn_kill_clicked_cb, view);
    evas_object_show(btn);
    view->general.btn_kill = btn;
    elm_box_pack_end(hbx, btn);

    return fr;
}

static void
_evisum_ui_process_view_threads_list_reorder(Evisum_Ui_Process_View *view) {
    view->poll_count = 0;
    evisum_ui_widget_exel_genlist_page_bring_in(view->threads.widget_exel, 0, 0);
}

typedef enum {
    THREAD_SORT_TID,
    THREAD_SORT_NAME,
    THREAD_SORT_STATE,
    THREAD_SORT_CPU_ID,
    THREAD_SORT_CPU_USAGE,
} Thread_Sort_Type;

typedef int (*Thread_Sort_Cb)(const void *p1, const void *p2);

static Thread_Sort_Cb
_evisum_ui_process_view_thread_sort_cb_get(Thread_Sort_Type type) {
    switch (type) {
        case THREAD_SORT_TID:
            return _evisum_ui_process_view_sort_by_tid;
        case THREAD_SORT_NAME:
            return _evisum_ui_process_view_sort_by_name;
        case THREAD_SORT_STATE:
            return _evisum_ui_process_view_sort_by_state;
        case THREAD_SORT_CPU_ID:
            return _evisum_ui_process_view_sort_by_cpu_id;
        case THREAD_SORT_CPU_USAGE:
        default:
            return _evisum_ui_process_view_sort_by_cpu_usage;
    }
}

static void
_evisum_ui_process_view_btn_threads_sort_clicked_cb(void *data, Evas_Object *obj, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;
    Thread_Sort_Type type;
    Thread_Sort_Cb cb;

    type = (Thread_Sort_Type) (intptr_t) evas_object_data_get(obj, "thread_sort_type");
    cb = _evisum_ui_process_view_thread_sort_cb_get(type);

    if (view->threads.sort_cb == cb) view->threads.sort_reverse = !view->threads.sort_reverse;
    view->threads.sort_cb = cb;
    evisum_ui_widget_exel_sort_header_button_state_set(obj, view->threads.sort_reverse);
    _evisum_ui_process_view_threads_list_reorder(view);
}

static Evas_Object *
_evisum_ui_process_view_threads_tab_add(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *fr, *bx, *bx2, *tb, *rec, *btn, *glist;
    Evas_Object *graph;
    struct {
        const char *label;
        Thread_Sort_Type sort;
        double weight;
        double align;
        int field_id;
        const char *key;
        Evisum_Ui_Widget_Exel_Item_Cell_Type type;
        const char *unit_format;
        const char *field_desc;
        Eina_Bool min_width_btn;
    } cols[] = {
        { _("id"),     THREAD_SORT_TID,       EXPAND, FILL, 0, "tid",       EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     NULL, "Thread ID",
         0                                                                                                                                        },
        { _("name"),   THREAD_SORT_NAME,      EXPAND, FILL, 1, "name",      EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     NULL,
         "Thread Name",                                                                                                                         0 },
        { _("state"),  THREAD_SORT_STATE,     EXPAND, FILL, 2, "state",     EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     NULL,
         "Thread State",                                                                                                                        0 },
        { _("cpu id"), THREAD_SORT_CPU_ID,    0,      FILL, 3, "cpu_id",    EVISUM_UI_WIDGET_EXEL_ITEM_CELL_TEXT,     NULL,
         "Thread CPU ID",                                                                                                                       1 },
        { _("cpu %"),  THREAD_SORT_CPU_USAGE, EXPAND, FILL, 4, "cpu_usage", EVISUM_UI_WIDGET_EXEL_ITEM_CELL_PROGRESS,
         "%1.0f %%",                                                                                                        "Thread CPU Usage", 0 },
    };
    Evas_Smart_Cb sort_clicked_cb = _evisum_ui_process_view_btn_threads_sort_clicked_cb;
    int i = 0;

    fr = elm_frame_add(parent);
    evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
    evas_object_size_hint_align_set(fr, FILL, FILL);
    elm_object_style_set(fr, "pad_small");

    bx = elm_box_add(parent);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    evas_object_show(bx);
    elm_box_homogeneous_set(bx, 1);
    elm_object_content_set(fr, bx);

    graph = _evisum_ui_process_view_graph(parent, view);
    elm_box_pack_end(bx, graph);

    bx2 = elm_box_add(parent);
    evas_object_size_hint_weight_set(bx2, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx2, FILL, FILL);
    evas_object_show(bx2);

    tb = elm_table_add(bx2);
    evas_object_size_hint_weight_set(tb, EXPAND, 0);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    elm_box_pack_end(bx2, tb);
    evas_object_show(tb);

    rec = evas_object_rectangle_add(evas_object_evas_get(tb));
    evas_object_size_hint_min_set(rec, 1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
    evas_object_size_hint_max_set(rec, -1, ELM_SCALE_SIZE(LIST_BTN_HEIGHT));
    elm_table_pack(tb, rec, i++, 0, 1, 1);

    for (unsigned int c = 0; c < sizeof(cols) / sizeof(cols[0]); c++) {
        btn = evisum_ui_widget_exel_sort_header_button_add(tb, cols[c].label, view->threads.sort_reverse,
                                                           cols[c].weight, cols[c].align, sort_clicked_cb, view);
        evas_object_data_set(btn, "thread_sort_type", (void *) (intptr_t) cols[c].sort);
        evisum_ui_widget_exel_field_register(view->threads.widget_exel, cols[c].field_id, btn, cols[c].field_desc,
                                             ELM_SCALE_SIZE(BTN_WIDTH), ELM_SCALE_SIZE(BTN_WIDTH), EINA_TRUE);
        evisum_ui_widget_exel_field_row_def_set(view->threads.widget_exel, cols[c].field_id,
                                                &(Evisum_Ui_Widget_Exel_Item_Cell_Def) {
                                                        .type = cols[c].type,
                                                        .key = cols[c].key,
                                                        .unit_format = cols[c].unit_format,
                                                        .align_x = cols[c].align,
                                                        .weight_x = cols[c].weight,
                                                        .boxed = EINA_FALSE,
                                                        .spacer = 0,
                                                        .icon_size = 0,
                                                });
        evisum_ui_widget_exel_field_width_apply(view->threads.widget_exel, cols[c].field_id);
        if (cols[c].min_width_btn) {
            rec = evas_object_rectangle_add(evas_object_evas_get(tb));
            evas_object_size_hint_min_set(rec, ELM_SCALE_SIZE(BTN_WIDTH), 1);
            elm_table_pack(tb, rec, i, 0, 1, 1);
        }
        elm_table_pack(tb, btn, i++, 0, 1, 1);
        evas_object_show(btn);
    }

    glist = evisum_ui_widget_exel_genlist_add(view->threads.widget_exel, parent);
    elm_box_pack_end(bx2, glist);
    elm_box_pack_end(bx, bx2);

    return fr;
}

static Evas_Object *
_evisum_ui_process_view_children_tab_add(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *fr, *bx, *glist;

    fr = elm_frame_add(parent);
    evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
    evas_object_size_hint_align_set(fr, FILL, FILL);
    elm_object_style_set(fr, "pad_small");

    bx = elm_box_add(parent);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    evas_object_show(bx);
    elm_object_content_set(fr, bx);

    glist = evisum_ui_widget_exel_genlist_add(view->children.widget_exel, parent);
    evas_object_smart_callback_add(glist, "selected", _evisum_ui_process_view_item_children_clicked_cb, view);
    elm_box_pack_end(bx, glist);

    return fr;
}

static Evas_Object *
_evisum_ui_process_view_manual_tab_add(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *fr, *bx, *entry;
    Evas_Object *tb;
    int sz;

    fr = elm_frame_add(parent);
    evas_object_size_hint_weight_set(fr, EXPAND, EXPAND);
    evas_object_size_hint_align_set(fr, FILL, FILL);
    elm_object_style_set(fr, "pad_small");

    bx = elm_box_add(parent);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    evas_object_show(bx);
    elm_object_content_set(fr, bx);

    view->manual.entry = entry = elm_entry_add(bx);
    evas_object_size_hint_weight_set(entry, EXPAND, EXPAND);
    evas_object_size_hint_align_set(entry, FILL, FILL);
    elm_entry_single_line_set(entry, 0);
    elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
    elm_entry_editable_set(entry, 0);
    elm_entry_scrollable_set(entry, 1);
    elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF, ELM_SCROLLER_POLICY_AUTO);
    evas_object_show(entry);
    elm_box_pack_end(bx, entry);

    tb = elm_entry_textblock_get(entry);
    sz = evisum_ui_textblock_font_size_get(tb);
    evisum_ui_textblock_font_size_set(tb, sz - ELM_SCALE_SIZE(1));

    return fr;
}

static void
_evisum_ui_process_view_tab_change(Evisum_Ui_Process_View *view, Evas_Object *page, Evas_Object *obj) {
    elm_object_disabled_set(view->tab_general, 0);
    elm_object_disabled_set(view->tab_children, 0);
    elm_object_disabled_set(view->tab_thread, 0);
    elm_object_disabled_set(view->tab_manual, 0);
    evas_object_hide(view->general_view);
    evas_object_hide(view->children_view);
    evas_object_hide(view->manual_view);
    evas_object_hide(view->thread_view);

    view->current_view = page;
    evas_object_show(page);

    elm_object_disabled_set(obj, 1);
}

static void
_evisum_ui_process_view_tab_general_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    _evisum_ui_process_view_tab_change(view, view->general_view, obj);
    elm_object_focus_set(view->tab_children, 1);
}

static void
_evisum_ui_process_view_tab_children_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED,
                                                void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    _evisum_ui_process_view_children_view_update(view);
    _evisum_ui_process_view_tab_change(view, view->children_view, obj);
    elm_object_focus_set(view->tab_thread, 1);
}

static void
_evisum_ui_process_view_tab_threads_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    _evisum_ui_process_view_tab_change(view, view->thread_view, obj);
    elm_object_focus_set(view->tab_manual, 1);
}

static void
_evisum_ui_process_view_tab_manual_clicked_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED) {
    Evisum_Ui_Process_View *view = data;

    _evisum_ui_process_view_tab_change(view, view->manual_view, obj);
    elm_object_focus_set(view->tab_general, 1);
    _evisum_ui_process_view_manual_init(view);
}

static Evas_Object *
_evisum_ui_process_view_tabs_add(Evas_Object *parent, Evisum_Ui_Process_View *view) {
    Evas_Object *hbx, *pad, *btn;

    hbx = elm_box_add(parent);
    evas_object_size_hint_weight_set(hbx, EXPAND, 0);
    evas_object_size_hint_align_set(hbx, FILL, 0.5);
    elm_box_horizontal_set(hbx, 1);
    evas_object_show(hbx);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_medium");
    evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);
    elm_box_pack_end(hbx, pad);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_small");
    evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);

    btn = evisum_ui_tab_add(parent, &view->tab_general, _("Process"), _evisum_ui_process_view_tab_general_clicked_cb,
                            view);
    elm_object_content_set(pad, btn);
    elm_box_pack_end(hbx, pad);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_small");
    evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);

    btn = evisum_ui_tab_add(parent, &view->tab_children, _("Children"), _evisum_ui_process_view_tab_children_clicked_cb,
                            view);
    elm_object_content_set(pad, btn);
    elm_box_pack_end(hbx, pad);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_small");
    evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);

    btn = evisum_ui_tab_add(parent, &view->tab_thread, _("Threads"), _evisum_ui_process_view_tab_threads_clicked_cb,
                            view);
    elm_object_content_set(pad, btn);
    elm_box_pack_end(hbx, pad);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_small");
    evas_object_size_hint_weight_set(pad, 0.0, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);

    btn = evisum_ui_tab_add(parent, &view->tab_manual, _("Manual"), _evisum_ui_process_view_tab_manual_clicked_cb,
                            view);
    elm_object_content_set(pad, btn);
    elm_box_pack_end(hbx, pad);

    pad = elm_frame_add(parent);
    elm_object_style_set(pad, "pad_medium");
    evas_object_size_hint_weight_set(pad, EXPAND, EXPAND);
    evas_object_size_hint_align_set(pad, FILL, FILL);
    evas_object_show(pad);
    elm_box_pack_end(hbx, pad);

    return hbx;
}

static void
_evisum_ui_process_view_win_del_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
                                   void *event_info EINA_UNUSED) {
    Evas_Object *win;
    Evisum_Ui_Process_View *view;

    view = data;
    win = obj;

    if (view->thread) {
        ecore_thread_cancel(view->thread);
        ecore_thread_wait(view->thread, 0.5);
    }

    if (view->threads.hash_cpu_times) eina_hash_free(view->threads.hash_cpu_times);
    if (view->proc_usage_cache) eina_hash_free(view->proc_usage_cache);
    free(view->selected_cmd);
    if (view->threads.widget_exel) evisum_ui_widget_exel_free(view->threads.widget_exel);
    if (view->children.widget_exel) evisum_ui_widget_exel_free(view->children.widget_exel);

    evisum_icon_cache_del(view->icon_cache);

    evas_object_del(win);

    free(view);
}

static void
_evisum_ui_process_view_win_resize_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
    Evisum_Ui_Process_View *view = data;
    Evas_Coord w, h;

    if (view->ignore_initial_resize) {
        view->ignore_initial_resize = 0;
        evisum_ui_widget_exel_genlist_realized_items_update(view->threads.widget_exel);
        return;
    }

    evas_object_geometry_get(obj, NULL, NULL, &w, &h);
    if (w > 0) _process_view_last_width = w;
    if (h > 0) _process_view_last_height = h;

    evisum_ui_widget_exel_genlist_realized_items_update(view->threads.widget_exel);
}

static void
_evisum_ui_process_view_win_key_down_cb(void *data, Evas *e, Evas_Object *obj, void *event_info) {
    Evas_Event_Key_Down *ev;
    Evisum_Ui_Process_View *view;

    view = data;
    ev = event_info;

    if (!ev || !ev->keyname) return;

    if (!strcmp(ev->keyname, "Escape")) evas_object_del(view->win);
}

static void
_evisum_ui_process_view_activate(Evisum_Ui_Process_View *view, Evisum_Proc_Action action) {
    switch (action) {
        case PROC_VIEW_DEFAULT:
            view->current_view = view->general_view;
            _evisum_ui_process_view_tab_general_clicked_cb(view, view->tab_general, NULL);
            break;
        case PROC_VIEW_CHILDREN:
            view->current_view = view->children_view;
            _evisum_ui_process_view_tab_children_clicked_cb(view, view->tab_children, NULL);
            break;
        case PROC_VIEW_THREADS:
            view->current_view = view->thread_view;
            _evisum_ui_process_view_tab_threads_clicked_cb(view, view->tab_thread, NULL);
            break;
        case PROC_VIEW_MANUAL:
            view->current_view = view->manual_view;
            _evisum_ui_process_view_tab_manual_clicked_cb(view, view->tab_manual, NULL);
            break;
    }
    evas_object_show(view->current_view);
}

void
evisum_ui_process_view_win_add(Evisum_Ui *ui, pid_t pid, Evisum_Proc_Action action) {
    Evas_Object *win, *ic, *bx, *tabs, *tb;
    Proc_Info *proc;

    Evisum_Ui_Process_View *view = calloc(1, sizeof(Evisum_Ui_Process_View));
    if (!view) return;
    view->ui = ui;
    view->selected_pid = pid;
    view->threads.widget_exel = NULL;
    view->children.widget_exel = NULL;
    view->children.fields_mask = 0;
    view->children.field_widths[0] = 1;
    view->threads.fields_mask = 0;
    view->threads.field_widths[0] = 1;
    view->threads.sort_reverse = 1;
    view->threads.sort_cb = _evisum_ui_process_view_sort_by_cpu_usage;
    view->icon_cache = evisum_icon_cache_new();

    proc = proc_info_by_pid(pid);
    if (!proc) view->selected_cmd = strdup(_("Unknown"));
    else {
        view->selected_cmd = strdup(proc->command);
        view->start = proc->start;
        proc_info_free(proc);
    }

    view->win = win = elm_win_util_standard_add("evisum", "evisum");
    elm_win_autodel_set(win, 1);
    ic = elm_icon_add(win);
    elm_icon_standard_set(ic, "evisum");
    elm_win_icon_object_set(win, ic);

    elm_win_title_set(view->win, eina_slstr_printf("%s (%i)", view->selected_cmd, view->selected_pid));

    view->threads.widget_exel = evisum_ui_widget_exel_create(view->win);
    if (!view->threads.widget_exel) {
        evas_object_del(view->win);
        evisum_icon_cache_del(view->icon_cache);
        free(view->selected_cmd);
        free(view);
        return;
    }
    evisum_ui_widget_exel_field_bounds_set(view->threads.widget_exel, 0, 5);
    evisum_ui_widget_exel_resize_hit_width_set(view->threads.widget_exel, 0);
    evisum_ui_widget_exel_state_bind(view->threads.widget_exel, &view->threads.fields_mask, view->threads.field_widths);
    evisum_ui_widget_exel_callbacks_set(view->threads.widget_exel, NULL, NULL, NULL, NULL, NULL, NULL, view);

    view->children.widget_exel = evisum_ui_widget_exel_create(view->win);
    if (!view->children.widget_exel) {
        evisum_ui_widget_exel_free(view->threads.widget_exel);
        evas_object_del(view->win);
        evisum_icon_cache_del(view->icon_cache);
        free(view->selected_cmd);
        free(view);
        return;
    }
    evisum_ui_widget_exel_field_bounds_set(view->children.widget_exel, 0, 1);
    evisum_ui_widget_exel_resize_hit_width_set(view->children.widget_exel, 0);
    evisum_ui_widget_exel_state_bind(view->children.widget_exel, &view->children.fields_mask,
                                     view->children.field_widths);
    evisum_ui_widget_exel_callbacks_set(view->children.widget_exel, NULL, NULL, NULL, NULL, NULL, NULL, view);

    tabs = _evisum_ui_process_view_tabs_add(win, view);

    bx = elm_box_add(win);
    evas_object_size_hint_weight_set(bx, EXPAND, EXPAND);
    evas_object_size_hint_align_set(bx, FILL, FILL);
    evas_object_show(bx);
    elm_box_pack_end(bx, tabs);

    tb = elm_table_add(bx);
    evas_object_size_hint_weight_set(tb, EXPAND, EXPAND);
    evas_object_size_hint_align_set(tb, FILL, FILL);
    evas_object_show(tb);

    view->general_view = _evisum_ui_process_view_general_tab_add(tabs, view);
    view->children_view = _evisum_ui_process_view_children_tab_add(tabs, view);
    view->thread_view = _evisum_ui_process_view_threads_tab_add(tabs, view);
    view->manual_view = _evisum_ui_process_view_manual_tab_add(tabs, view);

    elm_table_pack(tb, view->general_view, 0, 0, 1, 1);
    elm_table_pack(tb, view->children_view, 0, 0, 1, 1);
    elm_table_pack(tb, view->thread_view, 0, 0, 1, 1);
    elm_table_pack(tb, view->manual_view, 0, 0, 1, 1);

    elm_box_pack_end(bx, tb);
    elm_object_content_set(win, bx);

    evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _evisum_ui_process_view_win_del_cb, view);
    evas_object_event_callback_add(win, EVAS_CALLBACK_RESIZE, _evisum_ui_process_view_win_resize_cb, view);
    evas_object_event_callback_add(bx, EVAS_CALLBACK_KEY_DOWN, _evisum_ui_process_view_win_key_down_cb, view);

    view->ignore_initial_resize = 1;
    if (_process_view_last_width > 0 && _process_view_last_height > 0)
        evas_object_resize(win, _process_view_last_width, _process_view_last_height);
    else evas_object_resize(win, ELM_SCALE_SIZE(460), ELM_SCALE_SIZE(600));
    elm_win_center(win, 1, 1);
    evas_object_show(win);

    _evisum_ui_process_view_activate(view, action);
#if defined(__FreeBSD__)
    view->kthreads_has_rss = 1;
#endif

    view->thread = ecore_thread_feedback_run(_evisum_ui_process_view_proc_info_main,
                                             _evisum_ui_process_view_proc_info_feedback_cb, NULL, NULL, view, 0);
}
