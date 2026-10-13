/**
 * platforms/screen.c
 *
 * See screen.h.
 */

#include "screen.h"
#include "../platform.h"

#include <string.h>

static uint32_t s_lut[256];
static uint8_t  s_lut_pal[768];
static int      s_lut_valid;

screen_rect_t screen_picture_rect(int dst_w, int dst_h, int scale_mode) {
    screen_rect_t r = { 0, 0, dst_w, dst_h };
    if (scale_mode == SCALE_STRETCH)
        return r;

    bool wide = dst_w * 3 > dst_h * 4;
    if (scale_mode == SCALE_CROP && wide) {
        r.h = dst_w * 3 / 4;
        r.y = (dst_h - r.h) / 2;
    } else if (wide) {
        r.w = dst_h * 4 / 3;
        r.x = (dst_w - r.w) / 2;
    } else {
        /* Portrait or squarer than 4:3: bars above and below. */
        r.h = dst_w * 3 / 4;
        r.y = (dst_h - r.h) / 2;
    }
    return r;
}

int screen_crop_inset_y(int dst_w, int dst_h, int render_h, int scale_mode) {
    if (scale_mode != SCALE_CROP) return 0;
    screen_rect_t r = screen_picture_rect(dst_w, dst_h, scale_mode);
    if (r.y >= 0 || r.h <= 0) return 0;
    return -r.y * render_h / r.h;
}

void screen_to_render(const screen_rect_t *pic, int src_w, int src_h,
                      int x, int y, int *out_x, int *out_y) {
    int rx = pic->w > 0 ? (x - pic->x) * src_w / pic->w : 0;
    int ry = pic->h > 0 ? (y - pic->y) * src_h / pic->h : 0;
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (rx >= src_w) rx = src_w - 1;
    if (ry >= src_h) ry = src_h - 1;
    *out_x = rx;
    *out_y = ry;
}

static void update_lut(const uint8_t *pal) {
    if (s_lut_valid && memcmp(s_lut_pal, pal, 768) == 0)
        return;
    memcpy(s_lut_pal, pal, 768);
    s_lut_valid = 1;
    for (int i = 0; i < 256; i++) {
        uint32_t r = (uint32_t)(pal[i * 3 + 0] & 0x3F) << 2;
        uint32_t g = (uint32_t)(pal[i * 3 + 1] & 0x3F) << 2;
        uint32_t b = (uint32_t)(pal[i * 3 + 2] & 0x3F) << 2;
        /* Little-endian word for the bytes R, G, B, A. */
        s_lut[i] = 0xFF000000u | (b << 16) | (g << 8) | r;
    }
}

void screen_present(uint32_t *dst, int dst_w, int dst_h, int dst_pitch,
                    const uint8_t *fb, int src_w, int src_h,
                    const uint8_t *pal, int scale_mode) {
    enum { MAX_W = 4096 };
    static uint16_t cols[MAX_W];

    if (!dst || !fb || !pal || dst_w <= 0 || dst_h <= 0 || dst_w > MAX_W)
        return;
    update_lut(pal);

    screen_rect_t pic = screen_picture_rect(dst_w, dst_h, scale_mode);
    int x0 = pic.x < 0 ? 0 : pic.x;
    int x1 = pic.x + pic.w > dst_w ? dst_w : pic.x + pic.w;

    for (int x = x0; x < x1; x++)
        cols[x] = (uint16_t)((x - pic.x) * src_w / pic.w);

    const uint8_t *prev_src = NULL;
    uint32_t *prev_row = NULL;
    for (int y = 0; y < dst_h; y++) {
        uint32_t *row = dst + (size_t)y * (size_t)dst_pitch;
        if (y < pic.y || y >= pic.y + pic.h) {
            memset(row, 0, (size_t)dst_w * 4);
            continue;
        }
        const uint8_t *src = fb + (size_t)((y - pic.y) * src_h / pic.h) * (size_t)src_w;
        if (src == prev_src) {
            /* Upscaling repeats source rows; copy the expanded one. */
            memcpy(row, prev_row, (size_t)dst_w * 4);
            continue;
        }
        if (x0 > 0) memset(row, 0, (size_t)x0 * 4);
        for (int x = x0; x < x1; x++)
            row[x] = s_lut[src[cols[x]]];
        if (x1 < dst_w) memset(row + x1, 0, (size_t)(dst_w - x1) * 4);
        prev_src = src;
        prev_row = row;
    }
}
