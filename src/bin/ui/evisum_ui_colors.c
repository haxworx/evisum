#include "evisum_ui_colors.h"
#include "config.h"
#include <string.h>

unsigned int cpu_colormap[256];
unsigned int freq_colormap[256];
unsigned int temp_colormap[256];

#define EVISUM_THEME_FILE                      PACKAGE_DATA_DIR "/themes/evisum.edj"
#define EVISUM_GRAPH_COLOR_FALLBACK_SIZE       1021
#define EVISUM_GRAPH_COLOR_THEME_MAX           512
#define EVISUM_GRAPH_COLOR_THEME_KEY_PREFIX    "evisum/graph_palette/"
#define EVISUM_GRAPH_WIDGET_BG_KEY             "evisum/graph_widget/background"
#define EVISUM_GRAPH_WIDGET_GRID_V_KEY         "evisum/graph_widget/grid_vertical"
#define EVISUM_GRAPH_WIDGET_GRID_H_KEY         "evisum/graph_widget/grid_horizontal"
#define EVISUM_CPU_DEFAULT_TEXT_KEY            "evisum/cpu_default/text"
#define EVISUM_CPU_DEFAULT_OVERLAY_KEY         "evisum/cpu_default/overlay"
#define EVISUM_CPU_DEFAULT_PADDING_KEY         "evisum/cpu_default/padding"
#define EVISUM_CPU_DEFAULT_EXPLAINER_BG_KEY    "evisum/cpu_default/explainer_bg"
#define EVISUM_CPU_VISUAL_CPU_COLORMAP_PREFIX  "evisum/cpu_visual/cpu_colormap/"
#define EVISUM_CPU_VISUAL_FREQ_COLORMAP_PREFIX "evisum/cpu_visual/freq_colormap/"
#define EVISUM_CPU_VISUAL_TEMP_COLORMAP_PREFIX "evisum/cpu_visual/temp_colormap/"

static uint8_t (*_graph_color_pool)[3];
static Eina_Bool *_graph_color_used;
static uint16_t _graph_color_pool_size;
static Eina_Hash *_graph_color_assignments;
static uint8_t _graph_widget_bg[3] = { 32, 32, 32 };
static uint8_t _graph_widget_grid_v[3] = { 56, 56, 56 };
static uint8_t _graph_widget_grid_h[3] = { 48, 48, 48 };
static uint8_t _cpu_default_text[4] = { 255, 255, 255, 255 };
static uint8_t _cpu_default_overlay[4] = { 0, 0, 0, 64 };
static uint8_t _cpu_default_padding[4] = { 0, 0, 0, 0 };
static uint8_t _cpu_default_explainer_bg[4] = { 0, 0, 0, 128 };
static Color_Point _cpu_colormap_points[COLOR_CPU_NUM + 1];
static Color_Point _freq_colormap_points[COLOR_FREQ_NUM + 1];
static Color_Point _temp_colormap_points[COLOR_TEMP_NUM + 1];

static uint32_t
_hash(const char *name) {
    uint32_t hash = 2166136261u;

    while (*name) {
        hash ^= (uint8_t) *name;
        hash *= 16777619u;
        name++;
    }
    return hash;
}

static void
_hsv_to_rgb_u8(uint16_t hue1536, uint8_t sat, uint8_t val, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t region, fpart;
    uint8_t p, q, t;

    region = hue1536 / 256;
    fpart = hue1536 % 256;

    p = (uint8_t) ((val * (255 - sat)) / 255);
    q = (uint8_t) ((val * (255 - ((sat * fpart) / 255))) / 255);
    t = (uint8_t) ((val * (255 - ((sat * (255 - fpart)) / 255))) / 255);

    switch (region) {
        case 0:
            *r = val;
            *g = t;
            *b = p;
            break;
        case 1:
            *r = q;
            *g = val;
            *b = p;
            break;
        case 2:
            *r = p;
            *g = val;
            *b = t;
            break;
        case 3:
            *r = p;
            *g = q;
            *b = val;
            break;
        case 4:
            *r = t;
            *g = p;
            *b = val;
            break;
        default:
        case 5:
            *r = val;
            *g = p;
            *b = q;
            break;
    }
}

static void
_graph_color_idx_free_cb(void *data) {
    free(data);
}

static Eina_Bool
_graph_color_pool_allocate(uint16_t size) {
    _graph_color_pool = calloc(size, sizeof(*_graph_color_pool));
    _graph_color_used = calloc(size, sizeof(*_graph_color_used));
    if (!_graph_color_pool || !_graph_color_used) {
        free(_graph_color_pool);
        free(_graph_color_used);
        _graph_color_pool = NULL;
        _graph_color_used = NULL;
        _graph_color_pool_size = 0;
        return EINA_FALSE;
    }

    _graph_color_pool_size = size;
    return EINA_TRUE;
}

static int
_hex_nibble(char c) {
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    return -1;
}

static Eina_Bool
_color_hex_parse_rgba(const char *hex, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a) {
    int hi, lo;
    size_t len;

    if (!hex || !r || !g || !b) return EINA_FALSE;
    if (hex[0] == '#') hex++;
    len = strlen(hex);
    if ((len != 6) && (len != 8)) return EINA_FALSE;

    hi = _hex_nibble(hex[0]);
    lo = _hex_nibble(hex[1]);
    if ((hi < 0) || (lo < 0)) return EINA_FALSE;
    *r = (uint8_t) ((hi << 4) | lo);

    hi = _hex_nibble(hex[2]);
    lo = _hex_nibble(hex[3]);
    if ((hi < 0) || (lo < 0)) return EINA_FALSE;
    *g = (uint8_t) ((hi << 4) | lo);

    hi = _hex_nibble(hex[4]);
    lo = _hex_nibble(hex[5]);
    if ((hi < 0) || (lo < 0)) return EINA_FALSE;
    *b = (uint8_t) ((hi << 4) | lo);

    if (a) {
        if (len == 8) {
            hi = _hex_nibble(hex[6]);
            lo = _hex_nibble(hex[7]);
            if ((hi < 0) || (lo < 0)) return EINA_FALSE;
            *a = (uint8_t) ((hi << 4) | lo);
        } else *a = 255;
    }

    return EINA_TRUE;
}

static Eina_Bool
_graph_color_hex_parse(const char *hex, uint8_t *r, uint8_t *g, uint8_t *b) {
    return _color_hex_parse_rgba(hex, r, g, b, NULL);
}

static Eina_Bool
_graph_color_pool_theme_load(void) {
    uint8_t tmp[EVISUM_GRAPH_COLOR_THEME_MAX][3];
    uint16_t count = 0;

    for (uint16_t i = 0; i < EVISUM_GRAPH_COLOR_THEME_MAX; i++) {
        char key[64];
        char *value;

        snprintf(key, sizeof(key), EVISUM_GRAPH_COLOR_THEME_KEY_PREFIX "%03u", i);
        value = edje_file_data_get(EVISUM_THEME_FILE, key);
        if (!value) break;

        if (_graph_color_hex_parse(value, &tmp[count][0], &tmp[count][1], &tmp[count][2])) count++;
        free(value);
    }

    if (count < 2) return EINA_FALSE;
    if (!_graph_color_pool_allocate(count)) return EINA_FALSE;

    for (uint16_t i = 0; i < count; i++) {
        _graph_color_pool[i][0] = tmp[i][0];
        _graph_color_pool[i][1] = tmp[i][1];
        _graph_color_pool[i][2] = tmp[i][2];
    }

    return EINA_TRUE;
}

static void
_graph_widget_color_theme_load(const char *key, uint8_t color[3]) {
    char *value;
    uint8_t r, g, b;

    value = edje_file_data_get(EVISUM_THEME_FILE, key);
    if (!value) return;
    if (_graph_color_hex_parse(value, &r, &g, &b)) {
        color[0] = r;
        color[1] = g;
        color[2] = b;
    }
    free(value);
}

static void
_cpu_default_color_theme_load(const char *key, uint8_t color[4]) {
    char *value;
    uint8_t r, g, b, a;

    value = edje_file_data_get(EVISUM_THEME_FILE, key);
    if (!value) return;
    if (_color_hex_parse_rgba(value, &r, &g, &b, &a)) {
        color[0] = r;
        color[1] = g;
        color[2] = b;
        color[3] = a;
    }
    free(value);
}

static void
_colormap_points_theme_load(const char *prefix, Color_Point *points, unsigned int n) {
    char key[96];
    char *value;
    uint8_t r, g, b;

    for (unsigned int i = 0; i <= n; i++) {
        snprintf(key, sizeof(key), "%s%02u", prefix, i);
        value = edje_file_data_get(EVISUM_THEME_FILE, key);
        if (!value) continue;
        if (_graph_color_hex_parse(value, &r, &g, &b)) points[i].color = ARGB(0xff, r, g, b);
        free(value);
    }
}

static void
_graph_color_pool_init(void) {
    static const uint8_t sat_levels[] = { 210, 225, 240 };
    static const uint8_t val_levels[] = { 228, 214, 242 };

    if (!_graph_color_pool_allocate(EVISUM_GRAPH_COLOR_FALLBACK_SIZE)) return;

    for (uint32_t i = 0; i < _graph_color_pool_size; i++) {
        uint16_t hue = (uint16_t) (((uint64_t) i * 983ULL) % 1536ULL);
        uint8_t sat = sat_levels[i % EINA_C_ARRAY_LENGTH(sat_levels)];
        uint8_t val = val_levels[(i / EINA_C_ARRAY_LENGTH(sat_levels)) % EINA_C_ARRAY_LENGTH(val_levels)];

        _hsv_to_rgb_u8(hue, sat, val, &_graph_color_pool[i][0], &_graph_color_pool[i][1], &_graph_color_pool[i][2]);
    }
}

static void
_evisum_ui_colors_init(const Color_Point *col_in, unsigned int n, unsigned int *col) {
    unsigned int pos, interp, val, dist, d;
    unsigned int a, r, g, b;
    unsigned int a1, r1, g1, b1, v1;
    unsigned int a2, r2, g2, b2, v2;

    // wal colormap_in until colormap table is full
    for (pos = 0, val = 0; pos < n; pos++) {
        // get first color and value position
        v1 = col_in[pos].val;
        a1 = AVAL(col_in[pos].color);
        r1 = RVAL(col_in[pos].color);
        g1 = GVAL(col_in[pos].color);
        b1 = BVAL(col_in[pos].color);
        // get second color and valuje position
        v2 = col_in[pos + 1].val;
        a2 = AVAL(col_in[pos + 1].color);
        r2 = RVAL(col_in[pos + 1].color);
        g2 = GVAL(col_in[pos + 1].color);
        b2 = BVAL(col_in[pos + 1].color);
        // get distance between values (how many entires to fill)
        dist = v2 - v1;
        // walk over the span of colors from point a to point b
        for (interp = v1; interp < v2; interp++) {
            // distance from starting point
            d = interp - v1;
            // calculate linear interpolation between start and given d
            a = ((d * a2) + ((dist - d) * a1)) / dist;
            r = ((d * r2) + ((dist - d) * r1)) / dist;
            g = ((d * g2) + ((dist - d) * g1)) / dist;
            b = ((d * b2) + ((dist - d) * b1)) / dist;
            // write out resulting color value
            col[val] = ARGB(a, r, g, b);
            val++;
        }
    }
}

void
evisum_ui_colors_init(void) {
    memcpy(_cpu_colormap_points, cpu_colormap_in, sizeof(_cpu_colormap_points));
    memcpy(_freq_colormap_points, freq_colormap_in, sizeof(_freq_colormap_points));
    memcpy(_temp_colormap_points, temp_colormap_in, sizeof(_temp_colormap_points));

    if (!_graph_color_assignments) {
        _colormap_points_theme_load(EVISUM_CPU_VISUAL_CPU_COLORMAP_PREFIX, _cpu_colormap_points, COLOR_CPU_NUM);
        _colormap_points_theme_load(EVISUM_CPU_VISUAL_FREQ_COLORMAP_PREFIX, _freq_colormap_points, COLOR_FREQ_NUM);
        _colormap_points_theme_load(EVISUM_CPU_VISUAL_TEMP_COLORMAP_PREFIX, _temp_colormap_points, COLOR_TEMP_NUM);

        _graph_widget_color_theme_load(EVISUM_GRAPH_WIDGET_BG_KEY, _graph_widget_bg);
        _graph_widget_color_theme_load(EVISUM_GRAPH_WIDGET_GRID_V_KEY, _graph_widget_grid_v);
        _graph_widget_color_theme_load(EVISUM_GRAPH_WIDGET_GRID_H_KEY, _graph_widget_grid_h);
        _cpu_default_color_theme_load(EVISUM_CPU_DEFAULT_TEXT_KEY, _cpu_default_text);
        _cpu_default_color_theme_load(EVISUM_CPU_DEFAULT_OVERLAY_KEY, _cpu_default_overlay);
        _cpu_default_color_theme_load(EVISUM_CPU_DEFAULT_PADDING_KEY, _cpu_default_padding);
        _cpu_default_color_theme_load(EVISUM_CPU_DEFAULT_EXPLAINER_BG_KEY, _cpu_default_explainer_bg);

        if (!_graph_color_pool_theme_load()) _graph_color_pool_init();
        _graph_color_assignments = eina_hash_string_superfast_new(_graph_color_idx_free_cb);
    }

    _evisum_ui_colors_init(_cpu_colormap_points, COLOR_CPU_NUM, cpu_colormap);
    _evisum_ui_colors_init(_freq_colormap_points, COLOR_FREQ_NUM, freq_colormap);
    _evisum_ui_colors_init(_temp_colormap_points, COLOR_TEMP_NUM, temp_colormap);
}

void
evisum_graph_color_get(const char *key, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint32_t h;
    uint16_t *assigned;
    uint32_t start, step;
    uint16_t idx = 0;
    Eina_Bool found = EINA_FALSE;

    if (!key || !r || !g || !b) return;

    if (!_graph_color_assignments) evisum_ui_colors_init();
    if (!_graph_color_pool || !_graph_color_pool_size) {
        *r = 255;
        *g = 255;
        *b = 255;
        return;
    }

    assigned = eina_hash_find(_graph_color_assignments, key);
    if (assigned) {
        idx = *assigned;
        if (idx >= _graph_color_pool_size) idx = idx % _graph_color_pool_size;
        *r = _graph_color_pool[idx][0];
        *g = _graph_color_pool[idx][1];
        *b = _graph_color_pool[idx][2];
        return;
    }

    h = _hash(key);
    start = h % _graph_color_pool_size;
    step = (_graph_color_pool_size > 1) ? (1 + ((h >> 16) % (_graph_color_pool_size - 1))) : 1;

    for (uint32_t i = 0; i < _graph_color_pool_size; i++) {
        uint32_t probe = (start + i * step) % _graph_color_pool_size;
        if (_graph_color_used[probe]) continue;
        idx = (uint16_t) probe;
        _graph_color_used[probe] = EINA_TRUE;
        found = EINA_TRUE;
        break;
    }

    if (!found) idx = (uint16_t) start;

    assigned = calloc(1, sizeof(uint16_t));
    if (assigned) {
        *assigned = idx;
        eina_hash_add(_graph_color_assignments, key, assigned);
    }

    *r = _graph_color_pool[idx][0];
    *g = _graph_color_pool[idx][1];
    *b = _graph_color_pool[idx][2];
}

void
evisum_graph_widget_colors_get(uint8_t *bg_r, uint8_t *bg_g, uint8_t *bg_b, uint8_t *grid_v_r, uint8_t *grid_v_g,
                               uint8_t *grid_v_b, uint8_t *grid_h_r, uint8_t *grid_h_g, uint8_t *grid_h_b) {
    if (!_graph_color_assignments) evisum_ui_colors_init();

    if (bg_r) *bg_r = _graph_widget_bg[0];
    if (bg_g) *bg_g = _graph_widget_bg[1];
    if (bg_b) *bg_b = _graph_widget_bg[2];

    if (grid_v_r) *grid_v_r = _graph_widget_grid_v[0];
    if (grid_v_g) *grid_v_g = _graph_widget_grid_v[1];
    if (grid_v_b) *grid_v_b = _graph_widget_grid_v[2];

    if (grid_h_r) *grid_h_r = _graph_widget_grid_h[0];
    if (grid_h_g) *grid_h_g = _graph_widget_grid_h[1];
    if (grid_h_b) *grid_h_b = _graph_widget_grid_h[2];
}

void
evisum_cpu_default_colors_get(uint8_t *text_r, uint8_t *text_g, uint8_t *text_b, uint8_t *overlay_r, uint8_t *overlay_g,
                              uint8_t *overlay_b, uint8_t *overlay_a, uint8_t *padding_r, uint8_t *padding_g,
                              uint8_t *padding_b, uint8_t *padding_a, uint8_t *explainer_bg_r, uint8_t *explainer_bg_g,
                              uint8_t *explainer_bg_b, uint8_t *explainer_bg_a) {
    if (!_graph_color_assignments) evisum_ui_colors_init();

    if (text_r) *text_r = _cpu_default_text[0];
    if (text_g) *text_g = _cpu_default_text[1];
    if (text_b) *text_b = _cpu_default_text[2];

    if (overlay_r) *overlay_r = _cpu_default_overlay[0];
    if (overlay_g) *overlay_g = _cpu_default_overlay[1];
    if (overlay_b) *overlay_b = _cpu_default_overlay[2];
    if (overlay_a) *overlay_a = _cpu_default_overlay[3];

    if (padding_r) *padding_r = _cpu_default_padding[0];
    if (padding_g) *padding_g = _cpu_default_padding[1];
    if (padding_b) *padding_b = _cpu_default_padding[2];
    if (padding_a) *padding_a = _cpu_default_padding[3];

    if (explainer_bg_r) *explainer_bg_r = _cpu_default_explainer_bg[0];
    if (explainer_bg_g) *explainer_bg_g = _cpu_default_explainer_bg[1];
    if (explainer_bg_b) *explainer_bg_b = _cpu_default_explainer_bg[2];
    if (explainer_bg_a) *explainer_bg_a = _cpu_default_explainer_bg[3];
}
