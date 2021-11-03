#include "cpu_default.h"
#include "cpu_basic.h"
#include "cpu_bars.h"

Cpu_Visual visualizations[] = {
   { .name = "default", .func = cpu_visual_default },
   { .name = "basic", .func = cpu_visual_basic },
   { .name = "bars", .func = cpu_visual_bars },
};
