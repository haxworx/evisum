#include "evisum_ui_util.h"
#include "evisum_ui.h"

#include <Elementary.h>
#include <stdlib.h>
#include <string.h>

const char *
evisum_size_format(unsigned long long bytes, Eina_Bool simple) {
    unsigned long powi = 1;
    unsigned long long value;
    unsigned int precision = 2, powj = 1;
    int i = 0;
    static const char *units[8] = {
        _("B"), _("KiB"), _("MiB"), _("GiB"), _("TiB"), _("PiB"), _("EiB"), _("ZiB"),
    };

    value = bytes;
    while (value > 1024) {
        if ((value / 1024) < powi) break;
        powi *= 1024;
        ++i;
        if (i == 7) break;
    }

    if (!i) precision = 0;

    while (precision > 0) {
        powj *= 10;
        if ((value / powi) < powj) break;
        --precision;
    }

    if (simple) return eina_slstr_printf("%1.*f %c", precision, (double) value / powi, units[i][0]);

    return eina_slstr_printf("%1.*f %s", precision, (double) value / powi, units[i]);
}

void
evisum_ui_textblock_font_size_set(Evas_Object *tb, int new_size) {
    Evas_Textblock_Style *ts;

    if (!tb) return;

    ts = evas_textblock_style_new();
    evas_textblock_style_set(ts, eina_slstr_printf("font='monospace' DEFAULT='font_size=%d'", new_size));
    evas_object_textblock_style_user_push(tb, ts);
    evas_textblock_style_free(ts);
}

int
evisum_ui_textblock_font_size_get(Evas_Object *tb) {
    const char *style;
    char *p, *p1;
    Evas_Textblock_Style *ts;
    int size = 0;

    if (!tb) return size;

    ts = evas_object_textblock_style_get(tb);
    if (!ts) return size;

    style = evas_textblock_style_get(ts);
    if (!style || !style[0]) return size;

    p = strdup(style);
    if (!p) return size;

    p1 = strstr(p, "font_size=");
    if (p1) {
        p1 += 10;
        size = atoi(p1);
    }

    free(p);

    return size;
}

char *
evisum_ui_text_man2entry(const char *text) {
    Eina_Strbuf *sb;
    const char *p;
    char *out;

    if (!text) return NULL;

    sb = eina_strbuf_new();
    if (!sb) return NULL;

    for (p = text; *p; p++) {
        switch (*p) {
            case '<':
                eina_strbuf_append(sb, "&lt;");
                break;
            case '>':
                eina_strbuf_append(sb, "&gt;");
                break;
            case '&':
                eina_strbuf_append(sb, "&amp;");
                break;
            case '\t':
                eina_strbuf_append(sb, "        ");
                break;
            default:
                eina_strbuf_append_char(sb, *p);
                break;
        }
    }

    out = eina_strbuf_string_steal(sb);
    eina_strbuf_free(sb);

    return out;
}
