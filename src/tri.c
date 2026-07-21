/**
 * tri.c
 *
 * Triangle / polygon rendering:
 *   draw_polygon handles quads (splits to 2 tris), calls draw_triangle.
 *
 * 4 functions prefixed tri_ in the original ASM.
 */

#include "tri.h"
#include "display.h"
#include "ellipse.h"
#include "init.h"
#include "game.h"
#include "topo.h"
#include <string.h>

/* ══════════════════════════════════════════════════════════════
 *  Polygon Rendering
 * ══════════════════════════════════════════════════════════════ */

/* tri_draw_polygon_433D30
 * Renders a triangle or quad. Quads are split into two triangles:
 *   tri 1: point1-point2-point3 (original)
 *   tri 2: point3-quad_point4 with adjusted texture coords.
 *
 * Dispatch: if texture_name_index >= 0, calls draw_new_tex_tri;
 * otherwise calls draw_triangle_ell (flat shaded).
 */
void draw_polygon(tri_t *triangle, int plane, tri_t *shade) {
    tri_t saved_tri;

    if (triangle->quad_point4) {
        /* First triangle: p1-p2-p3 */
        if (triangle->texture_name_index >= 0)
            draw_new_tex_tri(triangle, plane, shade);
        else
            draw_triangle_ell(triangle, plane, shade);

        /* Save original state */
        memcpy(&saved_tri, triangle, sizeof(tri_t));

        /* Set up second triangle: p3-p4 with shifted texture coords */
        triangle->tex1_u1 = triangle->tex2_v1;
        triangle->tex1_v1 = triangle->tex2_u2;
        triangle->tex2_v1 = triangle->tex3_u1;
        triangle->tex2_u2 = triangle->tex3_v1;
        triangle->point1 = triangle->point3;
        triangle->point3  = triangle->quad_point4;
        triangle->tri_use_flag &= 0xFEBF;

        if (saved_tri.tri_use_flag & 0x0100)
            triangle->tri_use_flag |= 0x40;
        if (saved_tri.tri_use_flag & 0x0200)
            triangle->tri_use_flag |= 0x0100;

        /* Second triangle */
        if (triangle->texture_name_index >= 0)
            draw_new_tex_tri(triangle, plane, shade);
        else
            draw_triangle_ell(triangle, plane, shade);

        /* Restore original state */
        memcpy(triangle, &saved_tri, sizeof(tri_t));
    } else {
        /* Simple triangle */
        if (triangle->texture_name_index >= 0)
            draw_new_tex_tri(triangle, plane, shade);
        else
            draw_triangle_ell(triangle, plane, shade);
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Textured Triangle Rasterizer
 *
 *  tri_draw_new_tex_tri_437A64 — E2: 0x437A64
 *
 *  Column-by-column textured triangle renderer. Same structure as
 *  draw_triangle_ell (flat shaded) but interpolates texture UV
 *  coordinates across edges and calls tex_tri_line_win95 per column.
 *
 *  Texture UV layout in tri_t (sequential int16_t fields):
 *    Point 1: U = tex1_u1, V = tex1_v1
 *    Point 2: U = tex1_u2, V = tex2_u1
 *    Point 3: U = tex2_v1, V = tex2_u2
 * ══════════════════════════════════════════════════════════════ */

void draw_new_tex_tri(tri_t *tri, int plane, tri_t *shade) {
    if (!tri->point1 || !tri->point2 || !tri->point3) return;

    check_fade();

    int x1 = tri->point1->screen_coord.X;
    int y1 = tri->point1->screen_coord.Y;
    int z1 = tri->point1->screen_coord.Z;
    int x2 = tri->point2->screen_coord.X;
    int y2 = tri->point2->screen_coord.Y;
    int z2 = tri->point2->screen_coord.Z;
    int x3 = tri->point3->screen_coord.X;
    int y3 = tri->point3->screen_coord.Y;
    int z3 = tri->point3->screen_coord.Z;

    if (!z1 || !z2 || !z3) return;

    int ref_z = z1;
    int mask_idx = (plane == 2) ? 1 : 0;

    /* Screen bounds check */
    int x_left  = (((x1 < x2 ? x1 : x2) < x3 ? (x1 < x2 ? x1 : x2) : x3) >> 4) + screen_centre_x;
    int x_right = (((x1 > x2 ? x1 : x2) > x3 ? (x1 > x2 ? x1 : x2) : x3) >> 4) + screen_centre_x;
    int y_top_b = (((y1 < y2 ? y1 : y2) < y3 ? (y1 < y2 ? y1 : y2) : y3) >> 4) + screen_centre_y;
    int y_bot_b = (((y1 > y2 ? y1 : y2) > y3 ? (y1 > y2 ? y1 : y2) : y3) >> 4) + screen_centre_y;

    if (x_left >= screen_width || x_right < 0 || y_top_b >= screen_height || y_bot_b < 0)
        return;

    /* Dirty rectangle update */
    if (tri->parent_actor) {
        subarea_t *area = tri->parent_actor->area_to_clear;
        if (x_left   < area->left)   area->left   = x_left;
        if (x_right  > area->right)  area->right  = x_right;
        if (y_top_b  < area->top)    area->top    = y_top_b;
        if (y_bot_b  > area->bottom) area->bottom = y_bot_b;
    }

    /* Cross product for face normal */
    int cross_x = (z2 - z3) * (y1 - y3) - (z1 - z3) * (y2 - y3);
    int cross_z = (y2 - y3) * (x1 - x3) - (y1 - y3) * (x2 - x3);
    int cross_y = (x2 - x3) * (z1 - z3) - (z2 - z3) * (x1 - x3);
    int tri_color;

    /* Backface cull / flip */
    if (cross_z <= 0) {
        if (!(tri->tri_use_flag & 1)) return;
        cross_x = -cross_x;
        cross_y = (z2 - z3) * (x1 - x3) - (x2 - x3) * (z1 - z3);
        tri_color = tri->tri_color_4;
        cross_z = -cross_z;
    } else {
        tri_color = tri->tri_color_3;
    }

    if (shade) {
        int sx1 = shade->point1->screen_coord.X;
        int sy1 = shade->point1->screen_coord.Y;
        int sz1 = shade->point1->screen_coord.Z;
        int sx2 = shade->point2->screen_coord.X;
        int sy2 = shade->point2->screen_coord.Y;
        int sz2 = shade->point2->screen_coord.Z;
        int sx3 = shade->point3->screen_coord.X;
        int sy3 = shade->point3->screen_coord.Y;
        int sz3 = shade->point3->screen_coord.Z;

        cross_x = (sz2 - sz3) * (sy1 - sy3) - (sz1 - sz3) * (sy2 - sy3);
        cross_z = (sy2 - sy3) * (sx1 - sx3) - (sy1 - sy3) * (sx2 - sx3);
        cross_y = (sx2 - sx3) * (sz1 - sz3) - (sz2 - sz3) * (sx1 - sx3);
        ref_z = sz1;
    }

    while (cross_x < -0x4000 || cross_x > 0x4000 || cross_y < -0x4000 || cross_y > 0x4000 ||
           cross_z < -0x4000 || cross_z > 0x4000) {
        cross_x >>= 1; cross_y >>= 1; cross_z >>= 1;
    }

    /* Compute shade via shade_map lookup */
    vector_t normal;
    normal.X = cross_x; normal.Y = cross_y; normal.Z = cross_z;

    int16_t rot_y = arctan(normal.X, normal.Z);
    if (rot_y) rotate_vector_about_y(&normal, -rot_y);

    int16_t rot_x = arctan(-normal.Y, normal.Z);
    if (rot_x) rotate_vector_about_x(&normal, -rot_x);

    int shade_col_v, shade_row_v;
    if (normal.Z) {
        shade_col_v = -64 * cross_x / normal.Z + 64;
        if (shade_col_v < 0) shade_col_v = 0;
        if (shade_col_v > 127) shade_col_v = 127;
        shade_row_v = -64 * cross_y / normal.Z + 64;
        if (shade_row_v < 0) shade_row_v = 0;
        if (shade_row_v > 127) shade_row_v = 127;
    } else {
        shade_col_v = 64; shade_row_v = 64;
    }

    int shade_val = shade_map[shade_row_v][shade_col_v];
    int depth_shade;
    if (moving_camera)
        depth_shade = 159 - (ref_z >> 5);
    else
        depth_shade = 191 - (ref_z >> 7);
    if (depth_shade < 0) depth_shade = 0;
    if (depth_shade > 127) depth_shade = 127;

    int shade_idx = shade_val & 0x7F;
    int shade_band = tri->shade_multiplier * depth_shade >> 14;
    int pixel_color;
    if (tri->tri_use_flag & 0x10)
        pixel_color = (unsigned char)shade_tab[tri_color][127][shade_idx];
    else
        pixel_color = (unsigned char)shade_tab[tri_color][shade_band][shade_idx];

    /* Read texture UV coords from tri_t:
     *   Point 1: U = tex1_u1, V = tex1_v1
     *   Point 2: U = tex1_u2, V = tex2_u1
     *   Point 3: U = tex2_v1, V = tex2_u2
     */
    int tu1 = tri->tex1_u1;
    int tv1 = tri->tex1_v1;
    int tu2 = tri->tex1_u2;
    int tv2 = tri->tex2_u1;
    int tu3 = tri->tex2_v1;
    int tv3 = tri->tex2_u2;

    /* Load texture data */
    int tex_name_idx = tri->texture_name_index;
    texture_t *tex = NULL;
    if (tex_name_idx >= 0 && tex_name_idx < TEXTURE_TAB_SIZE)
        tex = texture_tab[tex_name_idx];
    if (!tex || !tex->texture_data) return;

    const char *texture_data = tex->texture_data;
    int tex_width = tex->x_size ? tex->x_size : 128;

    /* Sort vertices by X for left-to-right rasterization.
     * Also rearrange UV coords to match sorted order. */
    point_t *left_pt, *mid_pt, *right_pt;
    int lu, lv, mu, mv, ru, rv;

    if (x1 <= x2) {
        if (x2 >= x3) {
            if (x1 >= x3) {
                left_pt = tri->point3; mid_pt = tri->point1; right_pt = tri->point2;
                lu = tu3; lv = tv3; mu = tu1; mv = tv1; ru = tu2; rv = tv2;
            } else {
                left_pt = tri->point1; mid_pt = tri->point3; right_pt = tri->point2;
                lu = tu1; lv = tv1; mu = tu3; mv = tv3; ru = tu2; rv = tv2;
            }
        } else {
            left_pt = tri->point1; mid_pt = tri->point2; right_pt = tri->point3;
            lu = tu1; lv = tv1; mu = tu2; mv = tv2; ru = tu3; rv = tv3;
        }
    } else {
        if (x2 <= x3) {
            if (x1 <= x3) {
                left_pt = tri->point2; mid_pt = tri->point1; right_pt = tri->point3;
                lu = tu2; lv = tv2; mu = tu1; mv = tv1; ru = tu3; rv = tv3;
            } else {
                left_pt = tri->point2; mid_pt = tri->point3; right_pt = tri->point1;
                lu = tu2; lv = tv2; mu = tu3; mv = tv3; ru = tu1; rv = tv1;
            }
        } else {
            left_pt = tri->point3; mid_pt = tri->point2; right_pt = tri->point1;
            lu = tu3; lv = tv3; mu = tu2; mv = tv2; ru = tu1; rv = tv1;
        }
    }

    /* Re-read sorted vertex coordinates */
    int px1 = left_pt->screen_coord.X;
    int py1 = left_pt->screen_coord.Y;
    int pz1 = left_pt->screen_coord.Z;
    int px2 = mid_pt->screen_coord.X;
    int py2 = mid_pt->screen_coord.Y;
    int pz2 = mid_pt->screen_coord.Z;
    int px3 = right_pt->screen_coord.X;
    int py3 = right_pt->screen_coord.Y;
    int pz3 = right_pt->screen_coord.Z;

    int cols_left_mid   = (px2 >> 4) - (px1 >> 4);
    int cols_left_right = (px3 >> 4) - (px1 >> 4);

    /* Edge slopes: dY/dX in 16.16, dZ/dX in <<20 */
    int dy12 = 0, dz12 = 0;
    int dy13 = 0, dz13 = 0;
    int dy23 = 0, dz23 = 0;

    /* Texture UV edge slopes in <<20 */
    int du12 = 0, dv12 = 0;
    int du13 = 0, dv13 = 0;
    int du23 = 0, dv23 = 0;

    if (px2 == px1) {
        dy12 = (py1 >= py2) ? (int)0x80000001 : 0x7FFFFFFF;
    } else {
        int dx = px2 - px1;
        dy12 = ((py2 - py1) << 16) / dx;
        dz12 = ((pz2 - pz1) << 20) / dx;
        du12 = ((mu - lu) << 20) / dx;
        dv12 = ((mv - lv) << 20) / dx;
    }
    if (px3 != px1) {
        int dx = px3 - px1;
        dy13 = ((py3 - py1) << 16) / dx;
        dz13 = ((pz3 - pz1) << 20) / dx;
        du13 = ((ru - lu) << 20) / dx;
        dv13 = ((rv - lv) << 20) / dx;
    }
    if (px3 != px2) {
        int dx = px3 - px2;
        dy23 = ((py3 - py2) << 16) / dx;
        dz23 = ((pz3 - pz2) << 20) / dx;
        du23 = ((ru - mu) << 20) / dx;
        dv23 = ((rv - mv) << 20) / dx;
    }

    /* Per-row steps (dZ/dY, dU/dY, dV/dY) for within-column interpolation */
    int dz_per_row = 0, du_per_row = 0, dv_per_row = 0;
    {
        int y_span = ((py1 + ((px2 - px1) * dy13 >> 16)) - py2) >> 4;
        if (y_span) {
            int z_span = pz1 + ((dz13 * (px2 - px1)) >> 20) - pz2;
            dz_per_row = (z_span << 16) / y_span;

            int u_span = lu + ((du13 * (px2 - px1)) >> 20) - mu;
            du_per_row = (u_span << 16) / y_span;

            int v_span = lv + ((dv13 * (px2 - px1)) >> 20) - mv;
            dv_per_row = (v_span << 16) / y_span;
        }
    }

    int pitch;
    char *fb_data = (char *)dd_lock(plane, &pitch);

    int running_z = pz1 << 16;
    int col = screen_centre_x + (px1 >> 4);
    int subpix = 15 - (px1 & 0xF);

    /* Y intercepts in 16.16 fixed-point */
    int y_top_fp  = (screen_centre_y << 16) + (py1 << 12);
    int y_bot_fp  = y_top_fp;

    /* Running UV in <<16 */
    int running_u = lu << 16;
    int running_v = lv << 16;

    int y_corr_13 = (subpix * dy13) >> 4;
    int y_corr_12 = (subpix * dy12) >> 4;

    int col_count = 0;

    if (dy12 > dy13) {
        /* Case 1: edge 1→2 is below edge 1→3 */
        y_top_fp  += y_corr_13;
        running_z += (dz13 * subpix) >> 4;
        running_u += (du13 * subpix) >> 4;
        running_v += (dv13 * subpix) >> 4;
        y_bot_fp  += y_corr_12;

        while (col_count < cols_left_mid) {
            if (col >= right_edge) break;
            if (col >= left_edge) {
                int ty = y_top_fp >> 16;
                int16_t *mptr = mask_map[mask_idx] + ty * screen_width + col;
                int z_at_row = ((dz_per_row * (15 - (ty & 0xF))) >> 4) + running_z;
                int u_at_row = ((du_per_row * (15 - (ty & 0xF))) >> 4) + running_u;
                int v_at_row = ((dv_per_row * (15 - (ty & 0xF))) >> 4) + running_v;
                char *fbptr = fb_data + ty * pitch + col;
                int clip = top_edge - ty;
                if (clip > 0) {
                    fbptr    += clip * pitch;
                    mptr     += clip * screen_width;
                    z_at_row += dz_per_row * clip;
                    u_at_row += du_per_row * clip;
                    v_at_row += dv_per_row * clip;
                    ty = top_edge;
                }
                int by = y_bot_fp >> 16;
                if (by > bottom_edge) by = bottom_edge;
                if (by > ty) {
                    int height_color = (((by - ty) << 8) - 256) | pixel_color;
                    tex_tri_line_win95(z_at_row, height_color, dz_per_row,
                                       mptr, fbptr, pitch,
                                       u_at_row, v_at_row,
                                       du_per_row, dv_per_row,
                                       texture_data, tex_width);
                }
            }
            col++;
            y_top_fp  += dy13;
            running_z += dz13;
            running_u += du13;
            running_v += dv13;
            y_bot_fp  += dy12;
            col_count++;
        }

        /* Right half: switch bottom edge to 2→3 */
        y_bot_fp = (((15 - (px2 & 0xF)) * dy23) >> 4)
                 + (py2 << 12) + (screen_centre_y << 16);

        while (col_count < cols_left_right) {
            if (col >= right_edge) break;
            if (col >= left_edge) {
                int ty = y_top_fp >> 16;
                int16_t *mptr = mask_map[mask_idx] + ty * screen_width + col;
                int z_at_row = ((dz_per_row * (15 - (ty & 0xF))) >> 4) + running_z;
                int u_at_row = ((du_per_row * (15 - (ty & 0xF))) >> 4) + running_u;
                int v_at_row = ((dv_per_row * (15 - (ty & 0xF))) >> 4) + running_v;
                char *fbptr = fb_data + ty * pitch + col;
                int clip = top_edge - ty;
                if (clip > 0) {
                    fbptr    += clip * pitch;
                    mptr     += clip * screen_width;
                    z_at_row += dz_per_row * clip;
                    u_at_row += du_per_row * clip;
                    v_at_row += dv_per_row * clip;
                    ty = top_edge;
                }
                int by = y_bot_fp >> 16;
                if (by > bottom_edge) by = bottom_edge;
                if (by > ty) {
                    int height_color = (((by - ty) << 8) - 256) | pixel_color;
                    tex_tri_line_win95(z_at_row, height_color, dz_per_row,
                                       mptr, fbptr, pitch,
                                       u_at_row, v_at_row,
                                       du_per_row, dv_per_row,
                                       texture_data, tex_width);
                }
            }
            col++;
            y_top_fp  += dy13;
            running_z += dz13;
            running_u += du13;
            running_v += dv13;
            y_bot_fp  += dy23;
            col_count++;
        }
    } else {
        /* Case 2: edge 1→2 is above or equal to edge 1→3 */
        y_top_fp  += y_corr_12;
        running_z += (dz12 * subpix) >> 4;
        running_u += (du12 * subpix) >> 4;
        running_v += (dv12 * subpix) >> 4;
        y_bot_fp  += y_corr_13;

        while (col_count < cols_left_mid) {
            if (col >= right_edge) break;
            if (col >= left_edge) {
                int ty = y_top_fp >> 16;
                int16_t *mptr = mask_map[mask_idx] + ty * screen_width + col;
                int z_at_row = ((dz_per_row * (15 - (ty & 0xF))) >> 4) + running_z;
                int u_at_row = ((du_per_row * (15 - (ty & 0xF))) >> 4) + running_u;
                int v_at_row = ((dv_per_row * (15 - (ty & 0xF))) >> 4) + running_v;
                char *fbptr = fb_data + ty * pitch + col;
                int clip = top_edge - ty;
                if (clip > 0) {
                    fbptr    += clip * pitch;
                    mptr     += clip * screen_width;
                    z_at_row += dz_per_row * clip;
                    u_at_row += du_per_row * clip;
                    v_at_row += dv_per_row * clip;
                    ty = top_edge;
                }
                int by = y_bot_fp >> 16;
                if (by > bottom_edge) by = bottom_edge;
                if (by > ty) {
                    int height_color = (((by - ty) << 8) - 256) | pixel_color;
                    tex_tri_line_win95(z_at_row, height_color, dz_per_row,
                                       mptr, fbptr, pitch,
                                       u_at_row, v_at_row,
                                       du_per_row, dv_per_row,
                                       texture_data, tex_width);
                }
            }
            col++;
            y_top_fp  += dy12;
            running_z += dz12;
            running_u += du12;
            running_v += dv12;
            y_bot_fp  += dy13;
            col_count++;
        }

        /* Right half: switch top edge to 2→3 */
        y_top_fp = (((15 - (px2 & 0xF)) * dy23) >> 4)
                 + (screen_centre_y << 16) + (py2 << 12);
        int running_z2 = ((dz23 * (15 - (px2 & 0xF))) >> 4) + (pz2 << 16);
        int running_u2 = ((du23 * (15 - (px2 & 0xF))) >> 4) + (mu << 16);
        int running_v2 = ((dv23 * (15 - (px2 & 0xF))) >> 4) + (mv << 16);

        while (col_count < cols_left_right) {
            if (col >= right_edge) break;
            if (col >= left_edge) {
                int ty = y_top_fp >> 16;
                int16_t *mptr = mask_map[mask_idx] + ty * screen_width + col;
                int z_at_row = ((dz_per_row * (15 - (ty & 0xF))) >> 4) + running_z2;
                int u_at_row = ((du_per_row * (15 - (ty & 0xF))) >> 4) + running_u2;
                int v_at_row = ((dv_per_row * (15 - (ty & 0xF))) >> 4) + running_v2;
                char *fbptr = fb_data + ty * pitch + col;
                int clip = top_edge - ty;
                if (clip > 0) {
                    fbptr    += clip * pitch;
                    mptr     += clip * screen_width;
                    z_at_row += dz_per_row * clip;
                    u_at_row += du_per_row * clip;
                    v_at_row += dv_per_row * clip;
                    ty = top_edge;
                }
                int by = y_bot_fp >> 16;
                if (by > bottom_edge) by = bottom_edge;
                if (by > ty) {
                    int height_color = (((by - ty) << 8) - 256) | pixel_color;
                    tex_tri_line_win95(z_at_row, height_color, dz_per_row,
                                       mptr, fbptr, pitch,
                                       u_at_row, v_at_row,
                                       du_per_row, dv_per_row,
                                       texture_data, tex_width);
                }
            }
            col++;
            y_top_fp   += dy23;
            running_z2 += dz23;
            running_u2 += du23;
            running_v2 += dv23;
            y_bot_fp   += dy13;
            col_count++;
        }
    }

    dd_unlock(plane, fb_data);
}

/* ══════════════════════════════════════════════════════════════
 *  Textured Triangle (cuboid variant)
 *
 *  tri_draw_textured_tri_433E1C — E2: 0x433E1C
 *
 *  Called from put_a_cuboid. Identical structure to draw_new_tex_tri
 *  for Win95 mode — both use the same column rasterization with
 *  tex_tri_line_win95.
 * ══════════════════════════════════════════════════════════════ */

void draw_textured_tri(tri_t *tri, int plane, tri_t *shade) {
    draw_new_tex_tri(tri, plane, shade);
}

/* ══════════════════════════════════════════════════════════════
 *  Triangle Clipper
 *
 *  tri_clip_tri_4356CC — E2: 0x4356CC
 *
 *  Recursive Sutherland-Hodgman triangle clipper using floats.
 *  Clips triangle against view frustum planes with recursion
 *  depth limit of 5. No external callers found in binary —
 *  kept as stub pending discovery of call site.
 * ══════════════════════════════════════════════════════════════ */

void clip_tri(void) {
}
