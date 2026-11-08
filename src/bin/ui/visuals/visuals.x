#include "cpu_default.h"
#include "cpu_heatmap.h"
#include "cpu_basic.h"
#include "cpu_bars.h"

Evisum_Ui_Cpu_Visual visualizations[] = {
   { .name = "default", .func = evisum_ui_cpu_visual_default },
   { .name = "heatmap", .func = evisum_ui_cpu_visual_heatmap,
     .description = N_("This view shows CPU activity over time. "
                       "It can also show frequency and temperature when available."),
     .author = "Carsten Haitzler (raster)" },
   { .name = "basic", .func = evisum_ui_cpu_visual_basic,
     .description = N_("Size shows activity level (frequency when available, otherwise load). "
                       "Color and the 0%-100% legend show CPU usage percentage."),
     .author = "Al Poole (netstar)" },
   { .name = "bars", .func = evisum_ui_cpu_visual_bars,
     .description = N_("This view shows CPU usage only."),
     .author = "Al Poole (netstar)" },
};
