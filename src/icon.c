/**
 * icon.c
 *
 * Resolution constants:
 *   set_vga_constants (320×200), set_svga_constants (640×480).
 *
 * 2 functions prefixed icon_ in the original ASM.
 */

#include "icon.h"
#include "display.h"
#include "win.h"

int16_t rate_box_left = 10;
int16_t rate_box_top = 20;
int16_t vector_box_left = 242;
int16_t vector_box_top = 276;
int16_t max_message_len = 52;

/* icon_set_vga_constants  E1: 0x42F050 | E2: 0x4391A0 */
void set_vga_constants(void) {
    screen_width    = 320;
    screen_height   = 200;
    hires_width     = 320;
    hires_height    = 200;
    right_edge      = 320;
    bottom_edge     = 200;
    screen_centre_x = 160;
    screen_centre_y = 100;
    rate_box_left   = 10;
    rate_box_top    = 20;
    vector_box_top  = 114;
    max_message_len = 26;
    vector_box_left = 121;
    win_set_render_size(screen_width, screen_height);
}

/* icon_set_svga_constants  E1: 0x42F238 | E2: 0x439388 */
void set_svga_constants(void) {
    screen_width    = 640;
    screen_height   = 480;
    hires_width     = 640;
    hires_height    = 480;
    right_edge      = 640;
    bottom_edge     = 480;
    screen_centre_x = 320;
    screen_centre_y = 240;
    rate_box_left   = 10;
    rate_box_top    = 20;
    max_message_len = 52;
    vector_box_top  = 276;
    vector_box_left = 242;
    win_set_render_size(screen_width, screen_height);
}
