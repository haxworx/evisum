#include "cpu_default.h"
#include "cpu_basic.h"

Cpu_Visual visualizations[] = {
   { .name = "default", .func = cpu_visual_default },
   { .name = "basic", .func = cpu_visual_basic },
};
