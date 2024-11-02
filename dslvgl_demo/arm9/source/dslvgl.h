#ifndef DSLVGL_H_
#define DSLVGL_H_

#include <nds.h>
#include <stdbool.h>
#include <stddef.h>
#include "lvgl/lvgl.h"

#ifndef DMA_CH_MAIN
#   define  DMA_CH_MAIN 3
#endif

#ifndef DMA_CH_SUB
#   define DMA_CH_SUB  2
#endif

enum dslvgl_init_mode {
    DSLVGL_INIT_MAIN,           // use main display only
    DSLVGL_INIT_SUB,            // use sub display only
    DSLVGL_INIT_BOTH,           // use both displays (independently)
    DSLVGL_INIT_TILED           // use both displays (tiled)
};

enum dslvgl_disp {
    DSLVGL_DISP_MAIN,           // refers to the main display
    DSLVGL_DISP_SUB,            // refers to the sub display
    DSLVGL_DISP_TILED           // refers to both displays in a tile
};

typedef struct {
    uint16_t * buffer;  // middle framebuffer raw pointer
    int bgid;           // front framebuffer's background ID (returned from bgInit or bgInitSub)
    comutex_t mutex;    // mutex to lock the structure's data
} dslvgl_framebuffer_t;

typedef struct {
    dslvgl_framebuffer_t fbmain;// mid and front frame buffers, main display
    dslvgl_framebuffer_t fbsub; // mid and front frame buffers, sub display
    uint16_t * bbmain;          // back buffer, main display
    uint16_t * bbsub;           // back buffer, sub display
    uint32_t bbmain_size;
    uint32_t bbsub_size;
    enum dslvgl_init_mode driver_mode;  // mode the driver was initialized with
    bool isr_tick_en;
    bool isr_vblank_en;
    bool fbmain_en;             // enable rendering to main display
    bool fbsub_en;              // enable rendering to sub display
} dslvgl_global_attr_t;

/*
 * Log printing function that is known-good. Can be registered callback with 
 * `lv_log_register_print_cb()` 
 */
void dslvgl_print(lv_log_level_t level, const char * buf);

/*
 * This function must be called every millisecond.
 */
void dslvgl_tick_isr ();

/*
 *  This function must be called at the start of every v-blank interval.
 */
void dslvgl_vblank_isr();

void dslvgl_main_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map);

/*
 * Initializes DSLVGL driver with default parameters.
 */
int dslvgl_driver_init(enum dslvgl_init_mode mode);

uint16_t * dslvgl_driver_get_buf(enum dslvgl_disp display);
uint16_t * dslvgl_driver_get_buf1(enum dslvgl_disp display);
uint16_t * dslvgl_driver_get_buf2(enum dslvgl_disp display);
uint32_t dslvgl_driver_get_buf_size(enum dslvgl_disp display);
lv_display_render_mode_t dslvgl_driver_get_render_mode(
    enum dslvgl_disp display);

#endif
