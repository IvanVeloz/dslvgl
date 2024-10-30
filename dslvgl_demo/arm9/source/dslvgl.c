#include <stdio.h>
#include "dslvgl.h"

void dslvgl_print(lv_log_level_t level, const char * buf) 
{
    printf("lvgl%1o: %s",level,buf);
}
