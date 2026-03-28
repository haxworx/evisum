#include "evisum_ui_util.h"

#include <Elementary.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#define EVISUM_THEME_FILE              PACKAGE_DATA_DIR "/themes/evisum.edj"
#define EVISUM_THEME_ICON_GROUP_PREFIX "evisum/icons/"

static void
_icon_cache_free_cb(void *data) {
    free(data);
}

Eina_Hash *
evisum_icon_cache_new(void) {
    return eina_hash_string_superfast_new(_icon_cache_free_cb);
}

void
evisum_icon_cache_del(Eina_Hash *icon_cache) {
    eina_hash_free(icon_cache);
}

const char *
evisum_icon_cache_find(Eina_Hash *icon_cache, const Proc_Info *proc) {
    Efreet_Desktop *e;
    const char *name, *cmd;
    char *exists;

    if (proc->is_kernel)
#if defined(__linux__)
        return "linux";
#else
        return "freebsd";
#endif

    cmd = proc->command;

    exists = eina_hash_find(icon_cache, cmd);
    if (exists) return exists;

    if (!strncmp(cmd, "enlightenment", 13)) return "e";

    e = efreet_util_desktop_exec_find(cmd);
    if (!e) {
        Proc_Info *pproc = proc_info_by_pid(proc->ppid);
        const char *command;

        if (!pproc) return "application";
        command = evisum_icon_cache_find(icon_cache, pproc);
        proc_info_free(pproc);
        return command;
    }

    if (e->icon) name = e->icon;
    else name = cmd;

    eina_hash_add(icon_cache, cmd, strdup(name));

    efreet_desktop_free(e);

    return name;
}

const char *
evisum_ui_icon_name_get(const char *name) {
    const char *group;

    if (!name) return NULL;

    group = eina_slstr_printf(EVISUM_THEME_ICON_GROUP_PREFIX "%s", name);
    if (edje_file_group_exists(EVISUM_THEME_FILE, group)) return group;

    return name;
}

const char *
evisum_image_path_get(const char *name) {
    const char *group;

    if (!name) return NULL;

    group = eina_slstr_printf(EVISUM_THEME_ICON_GROUP_PREFIX "%s", name);
    if (edje_file_group_exists(EVISUM_THEME_FILE, group)) return group;

    return NULL;
}

void
evisum_ui_icon_set(Evas_Object *ic, const char *name) {
    const char *icon;
    Evas_Object *img;

    if (!ic || !name) return;

    icon = evisum_ui_icon_name_get(name);
    if (!icon) return;

    if (!strncmp(icon, EVISUM_THEME_ICON_GROUP_PREFIX, strlen(EVISUM_THEME_ICON_GROUP_PREFIX))) {
        if (elm_image_file_set(ic, EVISUM_THEME_FILE, icon)) {
            elm_image_resizable_set(ic, EINA_TRUE, EINA_TRUE);
            elm_image_smooth_set(ic, EINA_TRUE);
            evas_object_color_set(ic, 255, 255, 255, 255);
            img = elm_image_object_get(ic);
            if (img) {
                const char *type = evas_object_type_get(img);
                evas_object_color_set(img, 255, 255, 255, 255);
                if (type && !strcmp(type, "image")) evas_object_image_alpha_set(img, EINA_TRUE);
            }
            return;
        }
    }

    elm_icon_standard_set(ic, icon);
}
