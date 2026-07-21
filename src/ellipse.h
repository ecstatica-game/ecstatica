#ifndef ELLIPSE_H
#define ELLIPSE_H

#include "types.h"

void shade_ellipse(part_t *part, int plane);
void shade_ellipse_win95(part_t *part, int plane);
void tri_line_win95(int draw_height, int draw_data, int draw_height_bias,
                    int16_t *mask_ptr, char *fb_ptr, int pitch);
void tex_tri_line_win95(int draw_height, int draw_data, int draw_height_bias,
                        int16_t *mask_ptr, char *fb_ptr, int pitch,
                        int32_t tex_u, int32_t tex_v,
                        int32_t tex_du, int32_t tex_dv,
                        const char *texture_data, int tex_width);
void draw_triangle_ell(tri_t *tri, int plane, tri_t *shade);
int16_t arctan(int16_t x, int16_t y);
int16_t arcsin(int16_t val);
void calculate_squash(part_t *part);
void find_ellipse(part_t *part);
int  ellipse_half_size(void);
void view_trans_long_tri(tri_t *tri);
int16_t arctan_slow(int16_t x, int16_t y);

#endif /* ELLIPSE_H */
