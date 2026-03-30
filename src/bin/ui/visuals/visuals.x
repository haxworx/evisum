#include "cpu_default.h"
#include "cpu_basic.h"
#include "cpu_bars.h"

Evisum_Ui_Cpu_Visual visualizations[] = {
   { .name = "default", .func = evisum_ui_cpu_visual_default },
   { .name = "basic", .func = evisum_ui_cpu_visual_basic },
   { .name = "bars", .func = evisum_ui_cpu_visual_bars },
};
