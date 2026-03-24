#include "evisum_ui_colors.h"
#include "config.h"

unsigned int cpu_colormap[256];
unsigned int freq_colormap[256];
unsigned int temp_colormap[256];

#define EVISUM_GRAPH_COLOR_POOL_SIZE 1021

static uint8_t _graph_color_pool[EVISUM_GRAPH_COLOR_POOL_SIZE][3];
static Eina_Bool _graph_color_used[EVISUM_GRAPH_COLOR_POOL_SIZE];
static Eina_Hash *_graph_color_assignments;

static uint32_t
_hash(const char *name)
{
   uint32_t hash = 2166136261u;

   while (*name)
     {
        hash ^= (uint8_t) *name;
        hash *= 16777619u;
        name++;
     }
   return hash;
}

static void
_hsv_to_rgb_u8(uint16_t hue1536, uint8_t sat, uint8_t val,
               uint8_t *r, uint8_t *g, uint8_t *b)
{
   uint8_t region, fpart;
   uint8_t p, q, t;

   region = hue1536 / 256;
   fpart = hue1536 % 256;

   p = (uint8_t) ((val * (255 - sat)) / 255);
   q = (uint8_t) ((val * (255 - ((sat * fpart) / 255))) / 255);
   t = (uint8_t) ((val * (255 - ((sat * (255 - fpart)) / 255))) / 255);

   switch (region)
     {
      case 0: *r = val; *g = t;   *b = p;   break;
      case 1: *r = q;   *g = val; *b = p;   break;
      case 2: *r = p;   *g = val; *b = t;   break;
      case 3: *r = p;   *g = q;   *b = val; break;
      case 4: *r = t;   *g = p;   *b = val; break;
      default:
      case 5: *r = val; *g = p;   *b = q;   break;
     }
}

static void
_graph_color_idx_free_cb(void *data)
{
   free(data);
}

static void
_graph_color_pool_init(void)
{
   static const uint8_t sat_levels[] = { 210, 225, 240 };
   static const uint8_t val_levels[] = { 228, 214, 242 };

   for (uint32_t i = 0; i < EVISUM_GRAPH_COLOR_POOL_SIZE; i++)
     {
        uint16_t hue = (uint16_t) (((uint64_t) i * 983ULL) % 1536ULL);
        uint8_t sat = sat_levels[i % EINA_C_ARRAY_LENGTH(sat_levels)];
        uint8_t val = val_levels[(i / EINA_C_ARRAY_LENGTH(sat_levels)) %
                                 EINA_C_ARRAY_LENGTH(val_levels)];

        _hsv_to_rgb_u8(hue, sat, val,
                       &_graph_color_pool[i][0],
                       &_graph_color_pool[i][1],
                       &_graph_color_pool[i][2]);
     }
}

static void
_evisum_ui_colors_init(const Color_Point *col_in, unsigned int n, unsigned int *col)
{
   unsigned int pos, interp, val, dist, d;
   unsigned int a, r, g, b;
   unsigned int a1, r1, g1, b1, v1;
   unsigned int a2, r2, g2, b2, v2;

   // wal colormap_in until colormap table is full
   for (pos = 0, val = 0; pos < n; pos++)
     {
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
        for (interp = v1; interp < v2; interp++)
          {
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

void evisum_ui_colors_init(void)
{
    _evisum_ui_colors_init(cpu_colormap_in, COLOR_CPU_NUM, cpu_colormap);
    _evisum_ui_colors_init(freq_colormap_in, COLOR_FREQ_NUM, freq_colormap);
    _evisum_ui_colors_init(temp_colormap_in, COLOR_TEMP_NUM, temp_colormap);

    if (!_graph_color_assignments)
      {
         _graph_color_pool_init();
         _graph_color_assignments = eina_hash_string_superfast_new(_graph_color_idx_free_cb);
      }
}

void
evisum_graph_color_get(const char *key, uint8_t *r, uint8_t *g, uint8_t *b)
{
   uint32_t h;
   uint16_t *assigned;
   uint32_t start, step;
   uint16_t idx = 0;
   Eina_Bool found = EINA_FALSE;

   if (!key || !r || !g || !b)
     return;

   if (!_graph_color_assignments)
     evisum_ui_colors_init();

   assigned = eina_hash_find(_graph_color_assignments, key);
   if (assigned)
     {
        idx = *assigned;
        *r = _graph_color_pool[idx][0];
        *g = _graph_color_pool[idx][1];
        *b = _graph_color_pool[idx][2];
        return;
     }

   h = _hash(key);
   start = h % EVISUM_GRAPH_COLOR_POOL_SIZE;
   step = 1 + ((h >> 16) % (EVISUM_GRAPH_COLOR_POOL_SIZE - 1));

   for (uint32_t i = 0; i < EVISUM_GRAPH_COLOR_POOL_SIZE; i++)
     {
        uint32_t probe = (start + i * step) % EVISUM_GRAPH_COLOR_POOL_SIZE;
        if (_graph_color_used[probe])
          continue;
        idx = (uint16_t) probe;
        _graph_color_used[probe] = EINA_TRUE;
        found = EINA_TRUE;
        break;
     }

   if (!found)
     idx = (uint16_t) start;

   assigned = calloc(1, sizeof(uint16_t));
   if (assigned)
     {
        *assigned = idx;
        eina_hash_add(_graph_color_assignments, key, assigned);
     }

   *r = _graph_color_pool[idx][0];
   *g = _graph_color_pool[idx][1];
   *b = _graph_color_pool[idx][2];
}
