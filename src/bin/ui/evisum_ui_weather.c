/*
 * Author: Codex (OpenAI), controlled and directed by Alastair Poole.
 *
 * We tried a bitmap-based weather approach, but it was too CPU-intensive.
 * This implementation keeps the textures small and procedural, reuses canvas
 * objects, and limits animation updates to 30 frames per second.
 *
 * The UI Effects option was kept because effects like these were always part
 * of the plan. AI assistance made it practical to build and refine them
 * quickly, with Alastair directing the design and implementation.
 */

#include "evisum_ui_weather.h"
#include "evisum_ui_weather_season.h"
#include "../engine/evisum_engine.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DROPS 320
#define BOLT_SEGMENTS 18
#define SPLASHES 18
#define WEATHER_LAYER 100

typedef struct
{
   Evas_Object *line;
   double x, y, speed, depth;
} Rain_Drop;

typedef struct
{
   Evas_Object *win, *clip, *flash;
   Evas_Object *bolt[BOLT_SEGMENTS], *glow[BOLT_SEGMENTS], *splash[SPLASHES];
   Rain_Drop rain[DROPS];
   Ecore_Animator *animator;
   uint32_t random;
   Eina_Bool snow;
   double season_checked;
   double last, sampled, time, load, target, next_lightning, lightning;
} Weather;

static double
_random(Weather *weather)
{
   weather->random = weather->random * 1664525u + 1013904223u;
   return (weather->random >> 8) / 16777216.0;
}

static double
_clamp(double value, double low, double high)
{
   return value < low ? low : (value > high ? high : value);
}

/* Preview winter without changing the system date or saved preferences. */
static Eina_Bool
_winter_preview_get(void)
{
   const char *season = getenv("EVISUM_WEATHER_SEASON");
   return season && !strcmp(season, "winter");
}

static Eina_Bool
_snow_get(void)
{
   return _winter_preview_get() || evisum_ui_weather_winter_get(time(NULL));
}

static void
_overlay_setup(Weather *weather, Evas_Object *obj)
{
   evas_object_layer_set(obj, WEATHER_LAYER);
   evas_object_pass_events_set(obj, EINA_TRUE);
   evas_object_clip_set(obj, weather->clip);
}

/* Small, unfiltered sprites keep the precipitation pixelated. */
static void
_rain_texture(uint32_t *pixels)
{
   int y;
   for (y = 0; y < 16; y++)
     {
        int x = 5 - y / 3;
        int alpha = 45 + y * 13;
        pixels[y * 6 + x] = ((uint32_t) alpha << 24) |
          ((alpha * 165 / 255) << 16) | ((alpha * 215 / 255) << 8) | alpha;
        if (y > 11) pixels[y * 6 + x + 1] = pixels[y * 6 + x];
     }
}

static void
_precipitation_texture_set(Weather *weather, Eina_Bool snow)
{
   uint32_t pixels[6 * 16] = { 0 };
   int i, width = snow ? 9 : 6, height = snow ? 9 : 16;

   if (snow)
     {
        /* A tiny six-armed crystal, with a bright heart and icy tips. */
        static const char *flake[] = {
           "....x....", ".x..x..x.", "..x.x.x..", "...xxx...", "xxxxxxxxx",
           "...xxx...", "..x.x.x..", ".x..x..x.", "....x...."
        };
        for (i = 0; i < width * height; i++)
          if (flake[i / width][i % width] == 'x')
            pixels[i] = i == 40 ? 0xfffffff5 : 0xffcce9fa;
     }
   else _rain_texture(pixels);
   for (i = 0; i < DROPS; i++)
     {
        evas_object_image_size_set(weather->rain[i].line, width, height);
        evas_object_image_data_copy_set(weather->rain[i].line, pixels);
     }
   weather->snow = snow;
}

static void
_lightning_make(Weather *weather, int x, int y, int w, int h)
{
   double bx = x + w * (0.18 + _random(weather) * 0.64);
   double by = y + h * 0.07;
   double branch_x = bx, branch_y = by;
   int i;

   for (i = 0; i < BOLT_SEGMENTS; i++)
     {
        double nx, ny;
        if (i == 12) { bx = branch_x; by = branch_y; }
        nx = bx + (_random(weather) - 0.5) * w * (i < 12 ? 0.085 : 0.12);
        ny = by + h * (0.025 + _random(weather) * 0.025);
        evas_object_line_xy_set(weather->bolt[i], bx, by, nx, ny);
        evas_object_line_xy_set(weather->glow[i], bx + 1, by, nx + 1, ny);
        bx = nx;
        by = ny;
        if (i == 4) { branch_x = bx; branch_y = by; }
     }
   weather->lightning = 0.0;
}

static Eina_Bool
_weather_animate(void *data)
{
   Weather *weather = data;
   double now = ecore_loop_time_get(), dt, storm, rain, wind;
   Evas_Coord x, y, w, h;
   int i, count;

   dt = now - weather->last;
   if (dt < 1.0 / 30.0) return ECORE_CALLBACK_RENEW;
   weather->last = now;
   if (!evas_object_visible_get(weather->win) || elm_win_iconified_get(weather->win))
     return ECORE_CALLBACK_RENEW;
   dt = _clamp(dt, 0.0, 0.1);
   weather->time += dt;
   if (now - weather->season_checked >= 60.0)
     {
        Eina_Bool snow = _snow_get();
        if (snow != weather->snow) _precipitation_texture_set(weather, snow);
        weather->season_checked = now;
     }
   if (now - weather->sampled >= 1.0)
     {
        Evisum_Engine_Status status = { 0 };
        if (evisum_engine_status_get(&status))
          weather->target = _clamp(status.cpu_usage, 0.0, 100.0);
        weather->sampled = now;
     }
   weather->load += (weather->target - weather->load) *
     (1.0 - exp(-dt / 1.5));
   storm = _clamp((weather->load - 55.0) / 45.0, 0.0, 1.0);
   /* Density, fall speed and wind build with CPU load; thunder starts at 75%. */
   rain = _clamp(weather->load / 100.0, 0.0, 1.0);
   wind = (35.0 + 465.0 * rain) *
     (1.0 + 0.15 * sin(weather->time * 1.3));
   /* Overlay objects use canvas coordinates, never the window's screen position. */
   evas_output_viewport_get(evas_object_evas_get(weather->win), &x, &y, &w, &h);
   if (w <= 0 || h <= 0) return ECORE_CALLBACK_RENEW;
   evas_object_move(weather->clip, x, y);
   evas_object_resize(weather->clip, w, h);
   evas_object_move(weather->flash, x, y);
   evas_object_resize(weather->flash, w, h);

   /* Keep a visible baseline in both seasons, even with no CPU activity. */
   count = 32 + (int) (rain * (DROPS - 32));
   for (i = 0; i < DROPS; i++)
     {
        Rain_Drop *drop = &weather->rain[i];
        double length, fall_speed, drift_speed;
        int pixel = (int) _clamp(w / 480.0, 1.0, 3.0);
        int alpha;
        if (i >= count) { evas_object_hide(drop->line); continue; }
        fall_speed = drop->speed *
          (weather->snow ? 0.10 + rain * 0.18 : 0.45 + rain * 1.8);
        drift_speed = wind * (0.6 + drop->depth);
        if (weather->snow)
          drift_speed += sin(weather->time * 1.8 + i) * 28.0;
        drop->y += dt * fall_speed / h;
        drop->x -= dt * drift_speed / w;
        if (drop->y > 1.05) { drop->y = -0.05; drop->x = _random(weather); }
        if (drop->x < -0.05) drop->x += 1.1;
        if (drop->x > 1.05) drop->x -= 1.1;
        length = (14.0 + rain * 42.0) * drop->depth;
        alpha = (int) ((weather->snow ? 130.0 + rain * 70.0 : 95.0 + rain * 80.0) * drop->depth);
        evas_object_move(drop->line,
                         x + (int) (drop->x * w / pixel) * pixel,
                         y + (int) (drop->y * h / pixel) * pixel);
        if (weather->snow)
          {
             int size = pixel * (drop->depth > 0.75 ? 7 : 4);
             evas_object_resize(drop->line, size, size);
          }
        else
          /* Match the streak slope to its horizontal and vertical motion. */
          evas_object_resize(drop->line,
                             pixel * (int) fmax(3.0, length * drift_speed / fall_speed * 1.2 / pixel),
                             pixel * (int) fmax(7.0, length / pixel));
        evas_object_color_set(drop->line, alpha, alpha, alpha, alpha);
        evas_object_show(drop->line);
     }

   /* Little two-pixel spray crowns along the bottom edge. */
   for (i = 0; i < SPLASHES; i++)
     {
        double age = fmod(weather->time * (0.8 + rain) + i * 0.317, 1.0);
        int alpha = !weather->snow && age < 0.28 ? (int) ((1.0 - age / 0.28) * rain * 105.0) : 0;
        int pixel = (int) _clamp(w / 480.0, 1.0, 3.0);
        int sw = pixel * (age < 0.12 ? 7 : 11);
        evas_object_move(weather->splash[i], x + (i * 73 % 101) * w / 101, y + h - pixel * 6);
        evas_object_resize(weather->splash[i], sw, pixel * 5);
        evas_object_color_set(weather->splash[i], alpha, alpha, alpha, alpha);
     }

   weather->lightning += dt;
   if (weather->load >= 75.0 && weather->time >= weather->next_lightning)
     {
        _lightning_make(weather, x, y, w, h);
        weather->next_lightning = weather->time +
          (2.5 + (1.0 - storm) * 12.0) * (0.7 + _random(weather) * 0.6);
     }
   /* Two short strokes of light, followed by a soft afterglow. */
   {
      double age = weather->lightning;
      double light = age < 0.09 ? 1.0 :
        (age < 0.17 ? 0.0 : (age < 0.25 ? 0.8 : exp(-(age - 0.25) * 22.0) * 0.4));
      int alpha = (int) (light * 220.0);
      int flash = (int) (light * 22.0);
      evas_object_color_set(weather->flash, flash * 210 / 255, flash * 225 / 255, flash, flash);
      for (i = 0; i < BOLT_SEGMENTS; i++)
        {
           evas_object_color_set(weather->bolt[i], alpha * 225 / 255, alpha * 240 / 255, alpha, alpha);
           evas_object_color_set(weather->glow[i], alpha * 90 / 255, alpha * 140 / 255, alpha, alpha);
        }
   }
   return ECORE_CALLBACK_RENEW;
}

static void
_weather_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
             void *event_info EINA_UNUSED)
{
   Weather *weather = data;
   int i;

   if (weather->animator) ecore_animator_del(weather->animator);
   for (i = 0; i < DROPS; i++) evas_object_del(weather->rain[i].line);
   for (i = 0; i < SPLASHES; i++) evas_object_del(weather->splash[i]);
   for (i = 0; i < BOLT_SEGMENTS; i++)
     {
        evas_object_del(weather->bolt[i]);
        evas_object_del(weather->glow[i]);
     }
   evas_object_del(weather->flash);
   evas_object_del(weather->clip);
   free(weather);
}

void
evisum_ui_weather_add(Evas_Object *win, Evisum_Ui *ui)
{
   Weather *weather;
   Evas *canvas = evas_object_evas_get(win);
   int i;

   if (!evisum_ui_effects_enabled_get(ui) && !_winter_preview_get()) return;
   weather = calloc(1, sizeof(*weather));
   if (!weather) return;
   weather->win = win;
   weather->random = (uint32_t) (ecore_time_get() * 1000000.0) | 1u;
   weather->last = ecore_loop_time_get();
   weather->target = weather->load = _clamp(ui->cpu_usage, 0.0, 100.0);
   weather->next_lightning = 3.0;
   weather->lightning = 10.0;
   weather->clip = evas_object_rectangle_add(canvas);
   evas_object_color_set(weather->clip, 255, 255, 255, 255);
   evas_object_pass_events_set(weather->clip, EINA_TRUE);
   evas_object_show(weather->clip);
   for (i = 0; i < DROPS; i++)
     {
        Rain_Drop *drop = &weather->rain[i];
        drop->line = evas_object_image_filled_add(canvas);
        evas_object_image_alpha_set(drop->line, EINA_TRUE);
        evas_object_image_smooth_scale_set(drop->line, EINA_FALSE);
        drop->x = _random(weather);
        drop->y = _random(weather);
        drop->depth = 0.4 + _random(weather) * 0.6;
        drop->speed = 280.0 + _random(weather) * 400.0;
        _overlay_setup(weather, drop->line);
     }
   _precipitation_texture_set(weather, _snow_get());
   weather->season_checked = ecore_loop_time_get();
   weather->flash = evas_object_rectangle_add(canvas);
   _overlay_setup(weather, weather->flash);
   evas_object_color_set(weather->flash, 0, 0, 0, 0);
   evas_object_show(weather->flash);
   for (i = 0; i < SPLASHES; i++)
     {
        uint32_t spray[11 * 5] = { 0 };
        spray[0] = spray[10] = spray[13] = spray[19] =
          spray[25] = spray[29] = spray[38] = 0xffb5e5f5;
        weather->splash[i] = evas_object_image_filled_add(canvas);
        evas_object_image_size_set(weather->splash[i], 11, 5);
        evas_object_image_alpha_set(weather->splash[i], EINA_TRUE);
        evas_object_image_smooth_scale_set(weather->splash[i], EINA_FALSE);
        evas_object_image_data_copy_set(weather->splash[i], spray);
        _overlay_setup(weather, weather->splash[i]);
        evas_object_color_set(weather->splash[i], 0, 0, 0, 0);
        evas_object_show(weather->splash[i]);
     }
   for (i = 0; i < BOLT_SEGMENTS; i++)
     {
        weather->glow[i] = evas_object_line_add(canvas);
        weather->bolt[i] = evas_object_line_add(canvas);
        _overlay_setup(weather, weather->glow[i]);
        _overlay_setup(weather, weather->bolt[i]);
        evas_object_color_set(weather->glow[i], 0, 0, 0, 0);
        evas_object_color_set(weather->bolt[i], 0, 0, 0, 0);
        evas_object_show(weather->glow[i]);
        evas_object_show(weather->bolt[i]);
     }
   weather->animator = ecore_animator_add(_weather_animate, weather);
   evas_object_event_callback_add(win, EVAS_CALLBACK_DEL, _weather_del, weather);
}
