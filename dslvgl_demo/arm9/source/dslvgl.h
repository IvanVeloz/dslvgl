#include "lvgl.h"

/*
 * Log printing function that is known-good. Can be registered callback with 
 * `lv_log_register_print_cb()` 
 */
void dslvgl_print(lv_log_level_t level, const char * buf);