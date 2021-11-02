#include "cpu_default.h"
#include "cpu_basic.h"

Visualization visualizations[] = {
   { .name = "default", .func = cpu_visual_default },
   { .name = "basic", .func = cpu_visual_basic },
};
