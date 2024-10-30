// SPDX-License-Identifier: CC0-1.0
//
// SPDX-FileContributor: Antonio Niño Díaz, 2023
// SPDX-FileContributor: Ivan Veloz, 2024

#ifndef ARM9
#define ARM9
#endif

#include <stdio.h>
#include <time.h>

#include <nds.h>
#include <maxmod9.h>

#include "common.h"

// Headers autogenerated when files are find inside AUDIODIRS in the Makefile
#include "soundbank.h"
#include "soundbank_bin.h"

// Header autogenerated for each PNG file inside GFXDIRS in the Makefile
#include "neon.h"

// Header autogenerated for each BIN file inside BINDIRS in the Makefile
#include "data_string_bin.h"

#include "lvgl.h"
#include <time.h>
#include "examples/anim/lv_example_anim_2.c"
#include "examples/get_started/lv_example_get_started.h"

const size_t dispsize = 256*192;
int bg;

void lvgl_tick_isr () {
    lv_tick_inc(1);
}

static void swap_framebuffer(int bg) {
    if (bgGetMapBase(bg) == 8)
        bgSetMapBase(bg, 0);
    else
        bgSetMapBase(bg, 8);
}

inline void swap_rgb565_bgr555(uint16_t * src, uint16_t * dst) {
    *dst = ((*src&0xF800)>>11) | ((*src&0x07C0)>>1) | ((*src&0x001F)<<10) | 0x8000;
}

void lvgl_flush_cb(lv_display_t * display, const lv_area_t * area, uint8_t * px_map) {
    static bool firstrun = true;
    static uint16_t * wrkbuf;
    static uint16_t * actbuf;

    if(firstrun) {
         wrkbuf = bgGetGfxPtr(bg);
         swap_framebuffer(bg);
         actbuf = bgGetGfxPtr(bg);
         firstrun = false;
    }

    while(dmaBusy(0)) {LV_LOG_TRACE("dma");}  // just in case DMA isn't done yet

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
        swiWaitForVBlank();
        wrkbuf = bgGetGfxPtr(bg);
        swap_framebuffer(bg);
        actbuf = bgGetGfxPtr(bg);
        dmaCopyHalfWords(3, actbuf, wrkbuf, dispsize*sizeof(uint16_t));
    }

    lv_display_flush_ready(display);
}

void lv_example_get_started_1(void)
{
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

// Callback called whenver the keyboard is pressed so that a character is
// printed on the screen.
void on_key_pressed(int key)
{
   if (key > 0)
      printf("%c", key);
}

int main(int argc, char **argv)
{

    lcdSwap();

    videoSetMode(MODE_5_2D);

    vramSetPrimaryBanks(VRAM_A_MAIN_BG_0x06000000, VRAM_B_MAIN_BG_0x06020000,
                        VRAM_C_LCD, VRAM_D_LCD);


    bg = bgInit(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

    const size_t bufsize = dispsize*sizeof(uint16_t)>>3;
    uint16_t * buf1 = malloc(bufsize);

    // Setup sub screen for the text console
    consoleDemoInit();

    /* LVGL STUFF */
    // Initialize LVGL
    lv_init();
    
    // Setup tick timer, 1ms period
    timerStart(0,ClockDivider_1,timerFreqToTicks_1(1000),lvgl_tick_isr);

    lv_display_t * displaymain = lv_display_create(256, 192);
    lv_display_set_flush_cb(displaymain, lvgl_flush_cb);
    lv_display_set_buffers(displaymain, buf1, NULL, bufsize, LV_DISPLAY_RENDER_MODE_PARTIAL);

    //lv_example_get_started_1();
    lv_example_anim_2();

    /* END OF LVGL STUFF */

    // Load demo keyboard
    Keyboard *kbd = keyboardDemoInit();
    kbd->OnKeyPressed = on_key_pressed;

    // Setup sound bank
    mmInitDefaultMem((mm_addr)soundbank_bin);

    // load the module
    mmLoad(MOD_JOINT_PEOPLE);

    // load sound effects
    mmLoadEffect(SFX_FIRE_EXPLOSION);

    // Start playing module
    mmStart(MOD_JOINT_PEOPLE, MM_PLAY_LOOP);

    int angle_x = 0;
    int angle_z = 0;
    char name[256] = { 0 };

    while (1)
    {
        // Synchronize game loop to the screen refresh
        //swiWaitForVBlank();

        // Call LVGL periodic task (timer handler), on a while(1), by the book.
        lv_timer_handler();

        // Print some text in the demo console
        // -----------------------------------
        /*
        consoleClear();

        // Print current time
        char str[100];
        time_t t = time(NULL);
        struct tm *tmp = localtime(&t);
        if (strftime(str, sizeof(str), "%Y-%m-%dT%H:%M:%S%z", tmp) == 0)
            snprintf(str, sizeof(str), "Failed to get time");
        printf("%s\n\n", str);

        // Print contents of the BIN file
        for (int i = 0; i < data_string_bin_size; i++)
            printf("%c", data_string_bin[i]);
        printf("\n");

        // Print some controls
        printf("PAD:    Rotate triangle\n");
        printf("SELECT: Keyboard input test\n");
        printf("START:  Exit to loader\n");
        printf("A:      Play SFX\n");
        printf("\n");

        // Test code from a different folder
        printf("Name: [%s]\n", name);
        printf("Name length: %d\n", my_strlen(name));
        */
        // Handle user input
        // -----------------

        scanKeys();

        uint16_t keys = keysHeld();
        uint16_t keys_down = keysDown();

        if (keys & KEY_LEFT)
            angle_z += 3;
        if (keys & KEY_RIGHT)
            angle_z -= 3;

        if (keys & KEY_UP)
            angle_x += 3;
        if (keys & KEY_DOWN)
            angle_x -= 3;

        if (keys_down & KEY_A)
            mmEffect(SFX_FIRE_EXPLOSION);

        if (keys & KEY_SELECT)
        {
            consoleSetCursor(NULL, 1, 12);
            printf("Type your name: ");
            scanf("%255s", name);
        }

        if (keys & KEY_START)
            break;

    }

    free(buf1);

    return 0;
}
