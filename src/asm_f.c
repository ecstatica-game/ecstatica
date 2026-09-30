/**
 * asm_f.c
 *
 * Core math and rendering routines originally in x86 assembly.
 * Matrix/vector operations use 14-bit fixed-point (>>14).
 * bitmap/mask unpacking uses RLE with 2-bit type + 6-bit length spans.
 * Triangle/ellipse rasterizers use column-based z-buffered rendering.
 */

#include "asm_f.h"
#include "display.h"
#include "init.h"
#include "ellipse.h"
#include <string.h>

/* asm_matrix_mult_45DD04 — 3x3 fixed-point matrix multiply: out = in1 * in2 */
void matrix_mult(matrix3x3_t *out, matrix3x3_t *in1, matrix3x3_t *in2) {
    matrix3x3_t tmp;

    tmp._11 = (int16_t)(((int32_t)in1->_11 * in2->_11 +
                          (int32_t)in1->_12 * in2->_21 +
                          (int32_t)in1->_13 * in2->_31) >> 14);
    tmp._12 = (int16_t)(((int32_t)in1->_11 * in2->_12 +
                          (int32_t)in1->_12 * in2->_22 +
                          (int32_t)in1->_13 * in2->_32) >> 14);
    tmp._13 = (int16_t)(((int32_t)in1->_11 * in2->_13 +
                          (int32_t)in1->_12 * in2->_23 +
                          (int32_t)in1->_13 * in2->_33) >> 14);

    tmp._21 = (int16_t)(((int32_t)in1->_21 * in2->_11 +
                          (int32_t)in1->_22 * in2->_21 +
                          (int32_t)in1->_23 * in2->_31) >> 14);
    tmp._22 = (int16_t)(((int32_t)in1->_21 * in2->_12 +
                          (int32_t)in1->_22 * in2->_22 +
                          (int32_t)in1->_23 * in2->_32) >> 14);
    tmp._23 = (int16_t)(((int32_t)in1->_21 * in2->_13 +
                          (int32_t)in1->_22 * in2->_23 +
                          (int32_t)in1->_23 * in2->_33) >> 14);

    tmp._31 = (int16_t)(((int32_t)in1->_31 * in2->_11 +
                          (int32_t)in1->_32 * in2->_21 +
                          (int32_t)in1->_33 * in2->_31) >> 14);
    tmp._32 = (int16_t)(((int32_t)in1->_31 * in2->_12 +
                          (int32_t)in1->_32 * in2->_22 +
                          (int32_t)in1->_33 * in2->_32) >> 14);
    tmp._33 = (int16_t)(((int32_t)in1->_31 * in2->_13 +
                          (int32_t)in1->_32 * in2->_23 +
                          (int32_t)in1->_33 * in2->_33) >> 14);

    *out = tmp;
}

/* asm_matrix_vector_45DE8B — transform vector by matrix: out = M * v (>>14) */
void matrix_vector(vector_t *in, vector_t *out, matrix3x3_t *mat) {
    int32_t x = in->X, y = in->Y, z = in->Z;

    out->X = (int16_t)(((int32_t)mat->_11 * x +
                         (int32_t)mat->_12 * y +
                         (int32_t)mat->_13 * z) >> 14);
    out->Y = (int16_t)(((int32_t)mat->_21 * x +
                         (int32_t)mat->_22 * y +
                         (int32_t)mat->_23 * z) >> 14);
    out->Z = (int16_t)(((int32_t)mat->_31 * x +
                         (int32_t)mat->_32 * y +
                         (int32_t)mat->_33 * z) >> 14);
}

/* asm_matrix_long_vector_45DF0E — 64-bit products, as the original's
 * imul/shrd; int32 products overflow before the sum. */
void matrix_long_vector(long_vector_t *in, long_vector_t *out, matrix3x3_t *mat) {
    int64_t x = in->l_X, y = in->l_Y, z = in->l_Z;

    out->l_X = (int32_t)(((int64_t)mat->_11 * x +
                           (int64_t)mat->_12 * y +
                           (int64_t)mat->_13 * z) >> 14);
    out->l_Y = (int32_t)(((int64_t)mat->_21 * x +
                           (int64_t)mat->_22 * y +
                           (int64_t)mat->_23 * z) >> 14);
    out->l_Z = (int32_t)(((int64_t)mat->_31 * x +
                           (int64_t)mat->_32 * y +
                           (int64_t)mat->_33 * z) >> 14);
}

/* asm_unpack_bitmap_45DF96
 * Header byte: low 2 bits = type, high 6 bits = length (in output bytes).
 * length == 0 → end of stream.
 *   type 0: packed deltas. Each src byte = two signed 4-bit deltas added
 *           to running accumulator. Low nibble emitted first, then high.
 *           length counts OUTPUTS, so length src bytes = ceil(length/2).
 *           If length is odd the final src byte's high nibble is unused.
 *   type 1/3: fill — one src byte repeated length times.
 *   type 2: copy — length raw src bytes.
 * Accumulator persists across spans.
 */
void unpack_bitmap(char *dst, char *src) {
    unsigned char cur_val = 0;

    for (;;) {
        unsigned char header = *src++;
        int length = header >> 2;
        int type   = header & 0x03;

        if (length == 0)
            break;

        switch (type) {
        case 0: {
            int cl = length;
            while (cl > 0) {
                unsigned char packed = *src;
                int delta_lo = packed & 0x0F;
                if (delta_lo & 0x08) delta_lo |= ~0x0F;  /* sign extend */
                cur_val += (unsigned char)delta_lo;
                *dst++ = (char)cur_val;
                src++;
                if (--cl == 0) break;
                int delta_hi = ((signed char)packed) >> 4;
                cur_val += (unsigned char)delta_hi;
                *dst++ = (char)cur_val;
                --cl;
            }
            break;
        }

        case 1:
        case 3:
            cur_val = *src++;
            for (int i = 0; i < length; i++)
                *dst++ = (char)cur_val;
            break;

        case 2:
            for (int i = 0; i < length; i++) {
                cur_val = *src++;
                *dst++ = (char)cur_val;
            }
            break;
        }
    }
}

/* asm_unpack_mask_45DFF3 — 16-bit mask decompression with <<2 scaling
 * Header byte: low 2 bits = type, high 6 bits = length (in output int16s).
 * length == 0 → end of stream.
 *   type 0: packed 4-bit deltas, each scaled <<2. Low nibble first, then high.
 *           length counts OUTPUTS, src bytes = ceil(length/2).
 *   type 1: 8-bit signed deltas, scaled <<2.
 *   type 2: raw 16-bit values, each scaled <<2. cur_val left as last value.
 *   type 3: fill — one 16-bit value (<<2) repeated length times.
 * Accumulator persists across spans.
 */
void unpack_mask(int16_t *dst, char *src) {
    int16_t cur_val = 0;

    for (;;) {
        unsigned char header = (unsigned char)*src++;
        int length = header >> 2;
        int type   = header & 0x03;

        if (length == 0)
            break;

        switch (type) {
        case 0: {
            int cl = length;
            while (cl > 0) {
                unsigned char packed = (unsigned char)*src;
                int delta_lo = packed & 0x0F;
                if (delta_lo & 0x08) delta_lo |= ~0x0F;
                cur_val += (int16_t)(delta_lo << 2);
                *dst++ = cur_val;
                src++;
                if (--cl == 0) break;
                int delta_hi = ((signed char)packed) >> 4;
                cur_val += (int16_t)(delta_hi << 2);
                *dst++ = cur_val;
                --cl;
            }
            break;
        }

        case 1:
            for (int i = 0; i < length; i++) {
                signed char delta = (signed char)*src++;
                cur_val += (int16_t)(delta << 2);
                *dst++ = cur_val;
            }
            break;

        case 2:
            for (int i = 0; i < length; i++) {
                int16_t lo = (unsigned char)*src++;
                int16_t hi = (unsigned char)*src++;
                cur_val = (int16_t)(((hi << 8) | lo) << 2);
                *dst++ = cur_val;
            }
            break;

        case 3: {
            int16_t lo = (unsigned char)*src++;
            int16_t hi = (unsigned char)*src++;
            cur_val = (int16_t)(((hi << 8) | lo) << 2);
            for (int i = 0; i < length; i++)
                *dst++ = cur_val;
            break;
        }
        }
    }
}

/* asm_tri_line_win95_45D92A — flat-shaded triangle column with z-buffer.
 *   draw_height      — Z, 16.16 fixed-point
 *   draw_data        — (pixel_count << 8) | colour, minus 256 per pixel
 *   draw_height_bias — Z step per row, 16.16
 *   mask_ptr / fb_ptr, pitch — depth buffer and framebuffer column
 */
void tri_line_win95(int draw_height, int draw_data, int draw_height_bias,
                    int16_t *mask_ptr, char *fb_ptr, int pitch) {
    /* Stores through char* may alias any global, so hoist the reads. */
    const int mask_stride = screen_width;

    do {
        int16_t z_val = (int16_t)(draw_height >> 16);
        if (z_val <= *mask_ptr) {
            *mask_ptr = z_val;
            *fb_ptr = (char)draw_data;
        }
        fb_ptr += pitch;
        mask_ptr += mask_stride;
        draw_height += draw_height_bias;
        draw_data -= 256;
        /* `sub ecx,100h / jge`: the low byte is the colour, so `> 0` would
         * drop the last pixel of a column whenever the colour is 0. */
    } while (draw_data >= 0);
}

/* asm_ellipse_line_win95_45D10E — ellipsoid column with z-buffer; shade_map
 * decides the visibility and shade of each pixel. Uses depth_mask, shade_lut,
 * z_scale, fb_pitch, shade_dy and z_dy. */
void ellipse_line_win95(int mask_idx, int col_height,
                        int z_interp, char *draw_ptr,
                        int shade_idx) {
    /* Stores through char* may alias any global, so hoist the reads. Same in
     * the three variants below. */
    const int             pitch       = fb_pitch;
    const int             mask_stride = screen_width;
    const int32_t         zs          = z_scale;
    const int32_t         sdy         = shade_dy;
    const int32_t         zdy         = z_dy;
    int16_t *const        dm          = depth_mask;
    const char *const     lut         = shade_lut;
    const char *const     smap        = &shade_map[0][0];
    const int16_t *const  prof        = &profile[0][0];

    int shade_offset;
    int16_t z_depth;
    do {
        shade_offset = shade_idx >> 16;
        /* One load: the visibility test and the palette index are the
         * same byte. */
        unsigned char shade = (unsigned char)smap[shade_offset];
        if (!(shade & 0x80)) {
            z_depth = (int16_t)(((int32_t)prof[shade_offset] * zs + z_interp) >> 16);
            if (z_depth<= dm[mask_idx]) {
                dm[mask_idx] = z_depth;
                *draw_ptr = lut[shade];
            }
        }
        draw_ptr += pitch;
        mask_idx += mask_stride;
        shade_idx += sdy;
        z_interp += zdy;
        col_height -= 256;
    } while (col_height >= 0);
}

/* asm_beam_line_win95_45D2CC — beam/highlight column; beam_tab1/2. */
void beam_line_win95(int mask_idx, int col_height,
                        int z_interp, char *draw_ptr,
                        int shade_idx) {
    const int             pitch       = fb_pitch;
    const int             mask_stride = screen_width;
    const int32_t         zs          = z_scale;
    const int32_t         sdy         = shade_dy;
    const int32_t         zdy         = z_dy;
    int16_t *const        dm          = depth_mask;
    const char *const     tab1        = beam_tab1;
    const char *const     tab2        = beam_tab2;
    const char *const     smap        = &shade_map[0][0];
    const int16_t *const  prof        = &profile[0][0];

    int shade_offset;
    int16_t z_depth;
    do {
        shade_offset = shade_idx >> 16;
        if (!((unsigned char)smap[shade_offset] & 0x80)) {
            int32_t profile_z = (int32_t)prof[shade_offset] * zs;
            z_depth = (int16_t)((profile_z + z_interp) >> 16);
            if (z_depth<= dm[mask_idx]) {
                z_depth = (int16_t)((z_interp - profile_z) >> 16);
                if (z_depth>= dm[mask_idx])
                    *draw_ptr = tab2[(unsigned char)*draw_ptr];
                else
                    *draw_ptr = tab1[(unsigned char)*draw_ptr];
            }
        }
        draw_ptr += pitch;
        mask_idx += mask_stride;
        shade_idx += sdy;
        z_interp += zdy;
        col_height -= 256;
    } while (col_height >= 0);
}

/* asm_shadow_line_win95_45D482 — shadow column; shadow_lut. */
void shadow_line_win95(int mask_idx, int col_height,
                          int z_interp, char *draw_ptr,
                          int shade_idx) {
    const int             pitch       = fb_pitch;
    const int             mask_stride = screen_width;
    const int32_t         zs          = z_scale;
    const int32_t         sdy         = shade_dy;
    const int32_t         zdy         = z_dy;
    int16_t *const        dm          = depth_mask;
    const char *const     lut         = shadow_lut;
    const char *const     smap        = &shade_map[0][0];
    const int16_t *const  prof        = &profile[0][0];

    int shade_offset;
    int16_t z_depth;
    do {
        shade_offset = shade_idx >> 16;
        if (!((unsigned char)smap[shade_offset] & 0x80)) {
            int32_t profile_z = (int32_t)prof[shade_offset] * zs;
            z_depth = (int16_t)((profile_z + z_interp) >> 16);
            if (z_depth<= dm[mask_idx]) {
                z_depth = (int16_t)((z_interp - profile_z) >> 16);
                if (z_depth>= dm[mask_idx]) {
                    *draw_ptr = lut[(unsigned char)*draw_ptr];
                }
            }
        }
        draw_ptr += pitch;
        mask_idx += mask_stride;
        shade_idx += sdy;
        z_interp += zdy;
        col_height -= 256;
    } while (col_height >= 0);
}

/* asm_smoke_line_win95_45D5F8 — smoke/transparent column; beam_tab1. */
void smoke_line_win95(int mask_idx, int col_height,
                         int z_interp, char *draw_ptr,
                         int shade_idx) {
    const int             pitch       = fb_pitch;
    const int             mask_stride = screen_width;
    const int32_t         zs          = z_scale;
    const int32_t         sdy         = shade_dy;
    const int32_t         zdy         = z_dy;
    int16_t *const        dm          = depth_mask;
    const char *const     tab1        = beam_tab1;
    const char *const     smap        = &shade_map[0][0];
    const int16_t *const  prof        = &profile[0][0];

    int shade_offset;
    int16_t z_depth;
    do {
        shade_offset = shade_idx >> 16;
        if (!((unsigned char)smap[shade_offset] & 0x80)) {
            z_depth = (int16_t)(((int32_t)prof[shade_offset] * zs + z_interp) >> 16);
            if (z_depth<= dm[mask_idx]) {
                *draw_ptr = tab1[(unsigned char)*draw_ptr];
            }
        }
        draw_ptr += pitch;
        mask_idx += mask_stride;
        shade_idx += sdy;
        z_interp += zdy;
        col_height -= 256;
    } while (col_height >= 0);
}

/* asm_tex_tri_line_win95_45D99A — textured tri_line_win95; texel index is
 * (v & 0x7F) * tex_width + (u & 0x7F). */
void tex_tri_line_win95(int draw_height, int draw_data, int draw_height_bias,
                        int16_t *mask_ptr, char *fb_ptr, int pitch,
                        int32_t tex_u, int32_t tex_v,
                        int32_t tex_du, int32_t tex_dv,
                        const char *texture_data, int tex_width) {
    const int mask_stride = screen_width;

    do {
        int16_t z_val = (int16_t)(draw_height >> 16);
        if (z_val <= *mask_ptr) {
            *mask_ptr = z_val;
            int u = (tex_u >> 16) & 0x7F;
            int v = (tex_v >> 16) & 0x7F;
            *fb_ptr = texture_data[v * tex_width + u];
        }
        fb_ptr += pitch;
        mask_ptr += mask_stride;
        draw_height += draw_height_bias;
        tex_u += tex_du;
        tex_v += tex_dv;
        draw_data -= 256;
        /* 0x44FA4A: same `jge`; the original even overwrites the low byte with
         * the texel, which is harmless for the same reason. */
    } while (draw_data >= 0);
}

/* VGA register access — no-ops here. */

/* asm_set_plane_mask  E1: 0x44ED24 | E2: 0x45CCD4 */
void set_plane_mask(int mask) { (void)mask; }

/* asm_set_read_plane  E1: 0x44ED32 | E2: 0x45CCE2 */
void set_read_plane(int plane) { (void)plane; }

/* asm_set_latches  E1: 0x44ED40 | E2: 0x45CCF0 */
void set_latches(void) { }

/* asm_clear_latches  E1: 0x44ED53 | E2: 0x45CD03 */
void clear_latches(void) { }

/* asm_show_page  E1: 0x44ED5E | E2: 0x45CD0E */
void show_page(int page) { (void)page; }

/* asm_set_svga_bank  E1: 0x44EDDA | E2: 0x45CD8A */
void set_svga_bank(int bank) { (void)bank; }

/* asm_set320_x240_mode  E1: 0x44EC01 | E2: 0x45CBB1 */
void set_320x240_mode(void) { }

/* asm_write_pixel_mode_x  E1: 0x44ECC4 | E2: 0x45CC74 */
void write_pixel_mode_x(int x, int y, int colour) {
    (void)x; (void)y; (void)colour;
}

/* asm_xor_pixel_mode_x  E1: 0x44ECEF | E2: 0x45CC9F */
void xor_pixel_mode_x(int x, int y) {
    (void)x; (void)y;
}

/* asm_add_timer_int_handler  E1: 0x44E9EE | E2: 0x45C99E */
void add_timer_int_handler(void) { }

/* asm_remove_timer_handler  E1: 0x44EBC5 | E2: 0x45CB75 */
void remove_timer_handler(void) { }

/* asm_dosmemalloc  E1: 0x45012E | E2: 0x45E0DE */
void *dos_mem_alloc(int size) {
    (void)size;
    return NULL;
}

/* asm_dosmemfree  E1: 0x45016B | E2: 0x45E11B */
void dos_mem_free(void *ptr) {
    (void)ptr;
}
