#ifndef ARM9
#define ARM9
#endif

#include <nds.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "dslvgl.h"

static const size_t    dslvgl_disp_area    = 256*192;
static const size_t    dslvgl_fb_size      = dslvgl_disp_area*sizeof(uint16_t);

/*
 * Stores private global parameters for DSLVGL. Has some defaults for the
 * moment, until init functions are written.
 */
static dslvgl_global_attr_t params = {
    .isr_tick_en = true,
    .isr_vblank_en = false,
    .fbmain_en = false,
    .fbsub_en = false,
};

inline void swap_rgb565_bgr555(uint16_t * src, uint16_t * dst) {
    *dst = ((*src&0xF800)>>11) | ((*src&0x07C0)>>1) | ((*src&0x001F)<<10) | 0x8000;
}

void dslvgl_print(lv_log_level_t level, const char * buf) 
{
    printf("lvgl%1o: %s",level,buf);
}


void dslvgl_tick_isr () {
    lv_tick_inc(1);
}

void dslvgl_vblank_isr() {
    if(!params.isr_vblank_en) return;
    if(params.fbmain_en && comutex_try_acquire(&params.fbmain.mutex)) {
        assert(dmaBusy(DMA_CH_MAIN) == false);
        dmaCopyHalfWords(
            DMA_CH_MAIN, 
            params.fbmain.buffer, 
            bgGetGfxPtr(params.fbmain.bgid), 
            dslvgl_fb_size);
        comutex_release(&params.fbmain.mutex);
    }
    if(params.fbsub_en && comutex_try_acquire(&params.fbsub.mutex)) {
        assert(dmaBusy(DMA_CH_SUB) == false);
        dmaCopyHalfWords(
            DMA_CH_SUB, 
            params.fbsub.buffer, 
            bgGetGfxPtr(params.fbsub.bgid), 
            dslvgl_fb_size);
        comutex_release(&params.fbsub.mutex);
    }
    // If a mutex can't be acquired, it's because the middle buffer is being 
    // written to. In that case we skip rendering a frame.
    // The lowest DMA channel number gets priority until the transfer 
    // is complete.
}

void dslvgl_main_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {

    while(dmaBusy(DMA_CH_MAIN)) {LV_LOG_TRACE("dma");}  // just in case DMA isn't done yet
    comutex_acquire(&params.fbmain.mutex);
    uint16_t * wrkbuf = params.fbmain.buffer;

    const int32_t hres   = lv_display_get_horizontal_resolution(display);
    uint16_t     *srcbuf = (uint16_t *)px_map;
    // for every line in the area
    for(uint16_t *l = wrkbuf+hres*area->y1; l <= wrkbuf+area->y2*hres; l+=hres) {
        // for every pixel in the line
        for(int32_t p = area->x1; p <= area->x2; p++) {
            swap_rgb565_bgr555(srcbuf, l+p);
            srcbuf++;
        }
    }

    
    if(lv_display_flush_is_last(display)) {
        // for dual displays, put both dmaCopy on the VBlank interrupt.
        // finished
        // needs a mutex
        //swiWaitForVBlank();
        //dmaCopyHalfWords(3, wrkbuf, bgGetGfxPtr(bg), dispsize*sizeof(uint16_t));

    }
    comutex_release(&params.fbmain.mutex);
    lv_display_flush_ready(display);
}


static int dslvgl_main_init(void) {
    videoSetMode(MODE_5_2D);
    vramSetBankD(VRAM_D_MAIN_BG);
    params.fbmain.bgid = 
        bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    if(!(params.fbmain.buffer = malloc(dslvgl_fb_size))) return -1;
    params.bbmain_size = dslvgl_fb_size>>3;
    if(!(params.bbmain = malloc(params.bbmain_size))) return -1;
    if(!comutex_init(&params.fbmain.mutex)) return -1;
    params.fbmain_en = true;
    return 0;
}

static int dslvgl_sub_init(void) {
    videoSetMode(MODE_5_2D);
    vramSetBankD(VRAM_C_MAIN_BG);
    params.fbsub.bgid = 
        bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    params.fbsub.buffer = malloc(dslvgl_fb_size);
    if(!(params.fbsub.buffer = malloc(dslvgl_fb_size))) return -1;
    params.bbsub_size = dslvgl_fb_size>>3;
    if(!(params.bbsub = malloc(params.bbsub_size))) return -1;
    if(!comutex_init(&params.fbsub.mutex)) return -1;
    params.fbsub_en = true;
    return 0;
}

/*
 * Initializes DSLVGL driver with default parameters.
 */

int dslvgl_driver_init(enum dslvgl_init_mode mode) {
    int r;
    params.driver_mode = mode;
    switch(mode) {
        case DSLVGL_INIT_BOTH:
        case DSLVGL_INIT_MAIN:
            r = dslvgl_main_init();
            assert(r>=0);
            if(mode != DSLVGL_INIT_BOTH)
                break;
            __attribute__ ((fallthrough));
        case DSLVGL_INIT_SUB:
            r = dslvgl_sub_init();
            assert(r>=0);
            break;
        case DSLVGL_INIT_TILED:
            errno = ENOSYS;
            return -1;
        default:
            errno = EINVAL;
            return -1;
            
    }
    params.isr_vblank_en = true;
    params.isr_tick_en = true;
    return 0;
}

uint16_t * dslvgl_driver_get_buf(enum dslvgl_disp display) {
    switch(display) {
        case DSLVGL_DISP_TILED:
            if(params.driver_mode == DSLVGL_INIT_TILED)
                return params.bbmain;
            else return NULL;
        case DSLVGL_DISP_MAIN:
            if(params.driver_mode == DSLVGL_INIT_MAIN ||
               params.driver_mode == DSLVGL_INIT_BOTH)
                return params.bbmain;
            else return NULL;
        case DSLVGL_DISP_SUB:
            if(params.driver_mode == DSLVGL_INIT_SUB  ||
               params.driver_mode == DSLVGL_INIT_BOTH)
                return params.bbsub;
            else return NULL;
    }
    return NULL;
}

uint16_t * dslvgl_driver_get_buf1(enum dslvgl_disp display) {
    return dslvgl_driver_get_buf(display);
}
uint16_t * dslvgl_driver_get_buf2(enum dslvgl_disp display) {
    (void) display; // supress unused argument warnings
    return NULL;
}

uint32_t dslvgl_driver_get_buf_size(enum dslvgl_disp display) {
    switch(display) {
        case DSLVGL_DISP_TILED:
            if(params.driver_mode == DSLVGL_INIT_TILED)
                return params.bbmain_size;
            else return 0;
        case DSLVGL_DISP_MAIN:
            if(params.driver_mode == DSLVGL_INIT_MAIN ||
               params.driver_mode == DSLVGL_INIT_BOTH)
                return params.bbmain_size;
            else return 0;
        case DSLVGL_DISP_SUB:
            if(params.driver_mode == DSLVGL_INIT_SUB  ||
               params.driver_mode == DSLVGL_INIT_BOTH)
                return params.bbsub_size;
            else return 0;
    }
    return 0;
}

lv_display_render_mode_t dslvgl_driver_get_render_mode(enum dslvgl_disp disp) {
    (void)disp; // supress unused argument warnings
    return LV_DISPLAY_RENDER_MODE_PARTIAL;
}



