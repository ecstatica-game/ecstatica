/**
 * platforms/screen.h
 *
 * Palette expansion and 4:3 scaling into a 32-bit surface whose bytes are
 * R, G, B, A, the layout the web canvas, Android's RGBX_8888 window buffers
 * and iOS's CGImage all take. Shared by those three backends.
 */

#ifndef PLATFORMS_SCREEN_H
#define PLATFORMS_SCREEN_H

#include <stdint.h>

/* Where the picture lands on the surface. Under SCALE_CROP it is larger than
 * the surface and starts above it (y < 0). */
typedef struct {
    int x, y, w, h;
} screen_rect_t;

screen_rect_t screen_picture_rect(int dst_w, int dst_h, int scale_mode);

/* Black bars, picture scaled nearest-neighbour. `pal` is the game's 6-bit
 * VGA palette. */
void screen_present(uint32_t *dst, int dst_w, int dst_h, int dst_pitch,
                    const uint8_t *fb, int src_w, int src_h,
                    const uint8_t *pal, int scale_mode);

/* platform_crop_inset_y for a surface of this size. */
int screen_crop_inset_y(int dst_w, int dst_h, int render_h, int scale_mode);

/* Surface pixel to render pixel, clamped to the picture. */
void screen_to_render(const screen_rect_t *pic, int src_w, int src_h,
                      int x, int y, int *out_x, int *out_y);

#endif /* PLATFORMS_SCREEN_H */
