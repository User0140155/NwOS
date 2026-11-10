#include "nwos_internal.h"

void dateCr()
{
    set_color(TERM_COLOR_WHITE);
    print(OS_MADE_DATE "\n");
    print("v." OS_VERSION_STR "\n");
    set_color(base_color);
}