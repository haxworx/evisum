/*
 * Author: Codex (OpenAI), controlled and directed by Alastair Poole.
 */

#ifndef EVISUM_UI_WEATHER_SEASON_H
#define EVISUM_UI_WEATHER_SEASON_H

#include <stdbool.h>
#include <time.h>

/* Meteorological winter, using the local calendar and timezone's hemisphere. */
bool evisum_ui_weather_winter_get(time_t when);

#endif
