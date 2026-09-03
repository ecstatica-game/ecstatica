/**
 * render.c
 *
 * Backend-independent half of the hardware renderer: it collects what the
 * engine's display traversal asks to draw, does the parts of the original's
 * shading that are cheaper and more exact on the CPU, and derives the view and
 * projection matrices from the engine's own camera state.
 *
 * Backends (render_gl.c) consume the lists built here and nothing else. That
 * split is what keeps a second backend from becoming a second renderer.
 */

#include "render.h"

/* The three preferences exist on every target so file.c can round-trip the
 * config on a build that has no backend to apply them to. render_backend is a
 * real variable only where there is something for it to switch to; elsewhere
 * render.h makes it a constant. */
#ifdef ECS_ENABLE_GL
render_backend_t render_backend = RENDER_SOFTWARE;
#endif

int16_t render_hardware_pref  = 0;
int16_t render_supersample    = 0;   /* 0 = auto, match the drawable */
int16_t render_enhanced_light = 0;
int16_t render_map3d          = 0;   /* 0 = pre-rendered, 1 = 3D map */

#ifdef ECS_ENABLE_GL

#include "render_priv.h"
#include "display.h"
#include "ellipse.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "topo.h"
#include "win.h"
#include "platform.h"
#include "file.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static bool s_available   = false;   /* a backend came up at least once */
static bool s_frame_begun = false;   /* 3D content was started this frame */

/* ── Draw lists ───────────────────────────────────────────────
 * Sized from the engine's own pools, so they cannot overflow: one ellipsoid
 * per part, and quads split into two triangles hence the doubling.
 */
static render_ellipsoid_t s_ellipsoids[PART_POOL_SIZE];
static int                s_ellipsoid_count;
static render_vertex_t    s_flat_verts[TRI_SIZE * 2 * 3];
static int                s_flat_count;
static render_vertex_t    s_tex_verts[TRI_SIZE * 2 * 3];
static int                s_tex_count;

render_frame_t render_frame;

/* ── Fixed-point helpers ──────────────────────────────────── */

static inline float fx(int v) { return (float)v / (float)FIXED_POINT_ONE; }

/* ── Matrices ─────────────────────────────────────────────────
 * Column-major 4x4, the layout glUniformMatrix4fv wants with transpose=GL_FALSE.
 */

static void mat4_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

/**
 * View matrix: the engine computes  out = view_matrix * (world - view_pos),
 * with matrix entries in 14-bit fixed point (asm_f.c matrix_vector). That is a
 * rotation about the camera position, so it drops straight into a 4x4 with the
 * translation folded into the last column.
 */
static void build_view_matrix(float *m) {
    const matrix3x3_t *v = &view_matrix;
    float r[9];
    r[0] = fx(v->_11); r[1] = fx(v->_12); r[2] = fx(v->_13);
    r[3] = fx(v->_21); r[4] = fx(v->_22); r[5] = fx(v->_23);
    r[6] = fx(v->_31); r[7] = fx(v->_32); r[8] = fx(v->_33);

    float px = (float)view_pos.X, py = (float)view_pos.Y, pz = (float)view_pos.Z;

    mat4_identity(m);
    /* column-major: m[col*4 + row] */
    m[0] = r[0]; m[4] = r[1]; m[8]  = r[2];
    m[1] = r[3]; m[5] = r[4]; m[9]  = r[5];
    m[2] = r[6]; m[6] = r[7]; m[10] = r[8];

    m[12] = -(r[0] * px + r[1] * py + r[2] * pz);
    m[13] = -(r[3] * px + r[4] * py + r[5] * pz);
    m[14] = -(r[6] * px + r[7] * py + r[8] * pz);
    m[15] = 1.0f;
}

/**
 * Projection, derived from perspective_transform (display.c):
 *
 *   coeff_x    = zoom_factor / z,  scaled by screen_width/320
 *   coeff_y    = coeff_x * 7/8,    scaled by screen_height/200
 *   pixel_x    = centre_x + (coeff_x * X >> 10) >> 4
 *
 * so  pixel_x - centre_x = kx * X / z  with  kx = zoom * (sw/320) / 16384,
 * and the NDC form divides by half the viewport.
 *
 * Depth is the ordinary perspective mapping rather than a linear one. A linear
 * z_ndc is impossible to express here — with w = z it would need z² — and it is
 * not needed: the background's linear int16 depth is converted per pixel by the
 * background shader, which has to write gl_FragDepth anyway. Everything else
 * then gets interpolated depth and keeps early-Z.
 *
 * The engine's Y axis points down and its Z points into the screen. Both are
 * baked here so no axis flipping leaks into the rest of the renderer.
 */
static void build_proj_matrix(float *m) {
    const float near_z = (float)RENDER_NEAR_Z;
    const float far_z  = (float)RENDER_FAR_Z;

    float sw = (float)screen_width;
    float sh = (float)screen_height;

    float kx = (float)zoom_factor * (sw / 320.0f) / 16384.0f;
    float ky = (float)zoom_factor * 0.875f * (sh / 200.0f) / 16384.0f;

    memset(m, 0, 16 * sizeof(float));
    m[0]  =  2.0f * kx / sw;
    m[5]  = -2.0f * ky / sh;               /* engine +Y is down */
    m[10] =  (far_z + near_z) / (far_z - near_z);
    m[14] = -2.0f * far_z * near_z / (far_z - near_z);
    m[11] =  1.0f;                         /* engine +Z is forward */
}

/* ── Per-face shading ─────────────────────────────────────────
 * Lifted from draw_triangle_ell (ellipse.c:263-341). Doing it on the CPU costs
 * ~40 operations per triangle and guarantees the hardware path picks the same
 * palette entry the software path would, including the backface colour swap.
 *
 * Returns 0 when the face is culled.
 */
static int face_palette_index(tri_t *tri, tri_t *shade, uint8_t *out_index) {
    point_t *p1 = tri->point1, *p2 = tri->point2, *p3 = tri->point3;
    if (!p1 || !p2 || !p3) return 0;

    int x1 = p1->screen_coord.X, y1 = p1->screen_coord.Y, z1 = p1->screen_coord.Z;
    int x2 = p2->screen_coord.X, y2 = p2->screen_coord.Y, z2 = p2->screen_coord.Z;
    int x3 = p3->screen_coord.X, y3 = p3->screen_coord.Y, z3 = p3->screen_coord.Z;

    /* screen_coord.Z of 0 marks a near-clipped vertex. The software renderer
     * routes those through clip_and_raster; the hardware path lets GL clip the
     * geometry, but the shade still has to come from somewhere, so fall back to
     * the unshaded face colour rather than dividing by a zero Z below. */
    int near_clipped = (!z1 || !z2 || !z3);

    int cross_x = (z2 - z3) * (y1 - y3) - (z1 - z3) * (y2 - y3);
    int cross_z = (y2 - y3) * (x1 - x3) - (y1 - y3) * (x2 - x3);
    int cross_y = (x2 - x3) * (z1 - z3) - (z2 - z3) * (x1 - x3);
    int tri_color;
    int ref_z = z1;

    if (cross_z <= 0) {
        if (!(tri->tri_use_flag & 1)) return 0;      /* single-sided: culled */
        cross_x = -cross_x;
        cross_y = (z2 - z3) * (x1 - x3) - (x2 - x3) * (z1 - z3);
        tri_color = tri->tri_color_4;
        cross_z = -cross_z;
    } else {
        tri_color = tri->tri_color_3;
    }

    if (shade && shade->point1 && shade->point2 && shade->point3) {
        int sx1 = shade->point1->screen_coord.X, sy1 = shade->point1->screen_coord.Y, sz1 = shade->point1->screen_coord.Z;
        int sx2 = shade->point2->screen_coord.X, sy2 = shade->point2->screen_coord.Y, sz2 = shade->point2->screen_coord.Z;
        int sx3 = shade->point3->screen_coord.X, sy3 = shade->point3->screen_coord.Y, sz3 = shade->point3->screen_coord.Z;

        cross_x = (sz2 - sz3) * (sy1 - sy3) - (sz1 - sz3) * (sy2 - sy3);
        cross_z = (sy2 - sy3) * (sx1 - sx3) - (sy1 - sy3) * (sx2 - sx3);
        cross_y = (sx2 - sx3) * (sz1 - sz3) - (sz2 - sz3) * (sx1 - sx3);
        ref_z = sz1;
    }

    if (tri_color < 0) tri_color = 0;
    if (tri_color > 16) tri_color = 16;

    int shade_idx = 64;
    if (!near_clipped) {
        while (cross_x < -0x4000 || cross_x > 0x4000 || cross_y < -0x4000 || cross_y > 0x4000 ||
               cross_z < -0x4000 || cross_z > 0x4000) {
            cross_x >>= 1; cross_y >>= 1; cross_z >>= 1;
        }

        vector_t normal;
        normal.X = (int16_t)cross_x; normal.Y = (int16_t)cross_y; normal.Z = (int16_t)cross_z;

        int16_t rot_y = arctan(normal.X, normal.Z);
        if (rot_y) rotate_vector_about_y(&normal, -rot_y);
        int16_t rot_x = arctan(-normal.Y, normal.Z);
        if (rot_x) rotate_vector_about_x(&normal, -rot_x);

        int shade_col, shade_row;
        if (normal.Z) {
            shade_col = -64 * cross_x / normal.Z + 64;
            if (shade_col < 0) shade_col = 0;
            if (shade_col > 127) shade_col = 127;
            shade_row = -64 * cross_y / normal.Z + 64;
            if (shade_row < 0) shade_row = 0;
            if (shade_row > 127) shade_row = 127;
        } else {
            shade_col = 64; shade_row = 64;
        }
        shade_idx = shade_map[shade_row][shade_col] & 0x7F;
    }

    int depth_shade = moving_camera ? (159 - (ref_z >> 5)) : (191 - (ref_z >> 7));
    if (depth_shade < 0) depth_shade = 0;
    if (depth_shade > 127) depth_shade = 127;

    int band = (tri->tri_use_flag & 0x10)
             ? 127
             : (tri->shade_multiplier * depth_shade >> 14);
    if (band < 0) band = 0;
    if (band > 127) band = 127;

    *out_index = (uint8_t)shade_tab[tri_color][band][shade_idx];
    return 1;
}

/* ── Triangle submission ──────────────────────────────────── */

static void push_vertex(render_vertex_t *dst, const point_t *p,
                        int u, int v, uint8_t pal, int layer) {
    dst->pos[0] = (float)p->world_position.X;
    dst->pos[1] = (float)p->world_position.Y;
    dst->pos[2] = (float)p->world_position.Z;
    dst->uv[0]  = (float)u;
    dst->uv[1]  = (float)v;
    dst->pal    = pal;
    dst->layer  = (int16_t)layer;
}

/* One triangle out of a tri_t whose point1/2/3 are already the three corners
 * wanted. `uv` carries the six texel coordinates in the tri_t field order that
 * draw_new_tex_tri expects (tri.c:370-375). */
static void emit_triangle(tri_t *tri, tri_t *shade, const int *uv) {
    uint8_t pal;
    if (!face_palette_index(tri, shade, &pal)) return;

    int layer = -1;
    if (tri->texture_name_index >= 0)
        layer = render_gl_texture_layer(tri->texture_name_index);

    if (layer >= 0) {
        if (s_tex_count + 3 > (int)(sizeof(s_tex_verts) / sizeof(s_tex_verts[0]))) return;
        render_vertex_t *d = &s_tex_verts[s_tex_count];
        push_vertex(&d[0], tri->point1, uv[0], uv[1], pal, layer);
        push_vertex(&d[1], tri->point2, uv[2], uv[3], pal, layer);
        push_vertex(&d[2], tri->point3, uv[4], uv[5], pal, layer);
        s_tex_count += 3;
    } else {
        if (s_flat_count + 3 > (int)(sizeof(s_flat_verts) / sizeof(s_flat_verts[0]))) return;
        render_vertex_t *d = &s_flat_verts[s_flat_count];
        push_vertex(&d[0], tri->point1, 0, 0, pal, -1);
        push_vertex(&d[1], tri->point2, 0, 0, pal, -1);
        push_vertex(&d[2], tri->point3, 0, 0, pal, -1);
        s_flat_count += 3;
    }
}

/**
 * Mirrors draw_polygon (tri.c:200): a quad becomes two triangles, the second
 * one built from point3 + quad_point4 with the texture coordinates rotated
 * through the same field shuffle the original performs.
 *
 * No near-plane clipping here — GL clips, which is the one place the hardware
 * path is allowed to be simpler than clip_and_raster.
 */
void render_triangle(tri_t *tri, int plane, tri_t *shade) {
    (void)plane;
    if (!tri) return;

    int uv[6];
    uv[0] = tri->tex1_u1; uv[1] = tri->tex1_v1;
    uv[2] = tri->tex1_u2; uv[3] = tri->tex2_u1;
    uv[4] = tri->tex2_v1; uv[5] = tri->tex2_u2;

    if (!tri->quad_point4) {
        emit_triangle(tri, shade, uv);
        return;
    }

    emit_triangle(tri, shade, uv);

    tri_t second = *tri;
    second.point1      = tri->point3;
    second.point3      = tri->quad_point4;
    second.quad_point4 = NULL;
    second.tri_use_flag &= 0xFEBF;
    if (tri->tri_use_flag & 0x0100) second.tri_use_flag |= 0x40;
    if (tri->tri_use_flag & 0x0200) second.tri_use_flag |= 0x0100;

    int uv2[6];
    uv2[0] = tri->tex2_v1; uv2[1] = tri->tex2_u2;   /* old p3 becomes p1 */
    uv2[2] = tri->tex1_u2; uv2[3] = tri->tex2_u1;   /* p2 unchanged      */
    uv2[4] = tri->tex3_u1; uv2[5] = tri->tex3_v1;   /* p4                */
    emit_triangle(&second, shade, uv2);
}

/* ── Ellipsoid submission ─────────────────────────────────── */

/**
 * Everything needed is already in part_t by the time put_an_ellipse reaches its
 * leaf: matrix_2 is view_matrix * matrix_1 (display.c:2038), VECTOR_Squash is
 * the semi-axis triple, and persp_origin holds the view-space centre X/Y from
 * before perspective_transform overwrote vector_persp in place (display.c:1870).
 *
 * So the ellipsoid is  p = centre + R * diag(squash) * u,  |u| = 1,  entirely
 * in view space. The backend intersects a ray against it per pixel.
 */
void render_ellipsoid(part_t *part, int plane) {
    (void)plane;
    if (!part) return;
    if (s_ellipsoid_count >= PART_POOL_SIZE) return;

    int z = part->vector_persp.Z;
    if (z <= 0) return;                     /* near-clipped by the engine */

    int sx = part->VECTOR_Squash.X;
    int sy = part->VECTOR_Squash.Y;
    int sz = part->VECTOR_Squash.Z;
    if (!sx || !sy || !sz) return;          /* degenerate: pivot helper geometry */

    render_ellipsoid_t *e = &s_ellipsoids[s_ellipsoid_count];

    e->centre[0] = (float)part->persp_origin.X;
    e->centre[1] = (float)part->persp_origin.Y;
    e->centre[2] = (float)z;

    const matrix3x3_t *m = &part->matrix_2;
    e->rot[0] = fx(m->_11); e->rot[1] = fx(m->_12); e->rot[2] = fx(m->_13);
    e->rot[3] = fx(m->_21); e->rot[4] = fx(m->_22); e->rot[5] = fx(m->_23);
    e->rot[6] = fx(m->_31); e->rot[7] = fx(m->_32); e->rot[8] = fx(m->_33);

    e->axes[0] = (float)abs(sx);
    e->axes[1] = (float)abs(sy);
    e->axes[2] = (float)abs(sz);

    float r = e->axes[0];
    if (e->axes[1] > r) r = e->axes[1];
    if (e->axes[2] > r) r = e->axes[2];
    e->radius = r;

    int colour = part->color;
    if (colour < 0) colour = 0;
    if (colour > 16) colour = 16;
    e->colour = (uint8_t)colour;

    int cshade = part->color_shade;
    if (cshade < 0) cshade = 0;
    if (cshade > FIXED_POINT_ONE) cshade = FIXED_POINT_ONE;
    e->colour_shade = (int16_t)cshade;

    /* ellipse.c:119 — the 0x42 pair selects the three destination-modulating
     * modes; everything else is an ordinary lit surface. */
    int f = part->flags & 0x42;
    if (f == 0x42)      e->mode = RENDER_ELL_BEAM;
    else if (f == 0x02) e->mode = RENDER_ELL_SHADOW;
    else if (f == 0x40) e->mode = RENDER_ELL_SMOKE;
    else                e->mode = RENDER_ELL_SOLID;

    s_ellipsoid_count++;
}

/* ── Debug map geometry ───────────────────────────────────────
 * The map the game actually collides against, drawn as real geometry from the
 * game's own camera in place of the pre-rendered background.
 *
 * Grid to world is the inverse of find_map_element (topo.c:73):
 *   world X = (col - 64) << 9,  world Z = (row - 64) << 9,  cell = 512 units
 *   world Y = (128 - def_height) << height_shift
 *
 * Built once and kept until the map data changes, which top_of_map_elements
 * tracks well enough — it moves whenever a new area is merged in.
 */
#define MAP_GRID   128
#define MAP_CELL   512

/* Drawn around the camera rather than whole: the grid is 128x128 and most of it
 * is nowhere near the player, and at this scale distant cells pile into a wall
 * of columns that hides the part being looked at. */
#define MAP_VIEW_RADIUS 26
/* A cell whose neighbour is far below would otherwise grow a skirt hundreds of
 * units tall. Enough to read as solid ground, not enough to become a curtain. */
#define MAP_SKIRT_MAX   (12 << 7)

static render_map_vertex_t *s_map_verts;
static int  s_map_count;
static int  s_map_cap;
static int  s_map_built_for = -1;
static int  s_map_centre_row = -9999, s_map_centre_col = -9999;

static void map_push(float x, float y, float z, const uint8_t rgb[3]) {
    if (s_map_count >= s_map_cap) {
        int cap = s_map_cap ? s_map_cap * 2 : 65536;
        render_map_vertex_t *g = (render_map_vertex_t *)realloc(s_map_verts,
                                     (size_t)cap * sizeof(*g));
        if (!g) return;
        s_map_verts = g;
        s_map_cap   = cap;
    }
    render_map_vertex_t *v = &s_map_verts[s_map_count++];
    v->pos[0] = x; v->pos[1] = y; v->pos[2] = z;
    v->rgb[0] = rgb[0]; v->rgb[1] = rgb[1]; v->rgb[2] = rgb[2];
    v->pad = 0;
}

static void map_quad(float x0, float z0, float x1, float z1,
                     float y00, float y10, float y11, float y01,
                     const uint8_t rgb[3]) {
    map_push(x0, y00, z0, rgb); map_push(x1, y10, z0, rgb); map_push(x1, y11, z1, rgb);
    map_push(x0, y00, z0, rgb); map_push(x1, y11, z1, rgb); map_push(x0, y01, z1, rgb);
}

/* Height of a cell's top surface, or INT32_MIN when the cell is empty. */
static int map_cell_height(int row, int col, int *out_flags) {
    if (out_flags) *out_flags = 0;
    if (row < 0 || row >= MAP_GRID || col < 0 || col >= MAP_GRID) return INT32_MIN;
    uint16_t idx = new_map[row][col];
    if (idx == 0xFFFF || idx == 0) return INT32_MIN;

    int flags = 0, y = 0, first = 1;
    for (;;) {
        if ((int)idx >= top_of_map_elements) break;
        map_area_element_t *e = &map_elements[idx];
        if (first) { y = (128 - (int)e->def_height) << height_shift; first = 0; }
        int code_raw = (uint16_t)e->code_index_p1;
        if ((code_raw & 0x3FFF) > 0) flags |= 1;   /* action trigger */
        if (e->camera_index > 0)     flags |= 2;   /* camera zone    */
        if (e->wanderer_spawn)       flags |= 4;   /* wanderer spawn */
        if (e->material == 1)        flags |= 8;   /* impassable     */
        if (code_raw & 0x8000) break;
        idx++;
    }
    if (out_flags) *out_flags = flags;
    return y;
}

/* Colour coding follows the 2D overlay (debug_overlay.c:26-34), with one
 * deliberate departure: camera zones are not tinted. Nearly every cell belongs
 * to some camera, so colouring by that turned the whole map one colour and hid
 * everything worth seeing. Action, spawn and impassable are the rare, useful
 * ones; everything else is plain ground shaded by height. */
static void map_cell_colour(int flags, int y, uint8_t out[3]) {
    uint8_t r = 105, g = 110, b = 100;
    if (flags & 1)             { r = 190; g = 60;  b = 60; }   /* action  */
    else if (flags & 4)        { r = 60;  g = 180; b = 70; }   /* spawn   */
    else if (flags & 8)        { r = 60;  g = 170; b = 180; }  /* blocked */

    /* Higher ground reads lighter. The range is the whole 8-bit height field
     * scaled by height_shift, so normalise against that rather than guessing. */
    float t = 0.55f + 0.45f * (1.0f - (float)(y + (128 << height_shift)) /
                                       (float)(256 << height_shift));
    if (t < 0.35f) t = 0.35f;
    if (t > 1.0f)  t = 1.0f;
    out[0] = (uint8_t)(r * t); out[1] = (uint8_t)(g * t); out[2] = (uint8_t)(b * t);
}

static void build_map_mesh(void) {
    /* Centre on the camera, which is where the player is looking from. */
    int ccol = (view_pos.X >> 9) + 64;
    int crow = (view_pos.Z >> 9) + 64;

    if (s_map_built_for == top_of_map_elements &&
        s_map_centre_row == crow && s_map_centre_col == ccol &&
        s_map_count > 0)
        return;

    s_map_built_for  = top_of_map_elements;
    s_map_centre_row = crow;
    s_map_centre_col = ccol;
    s_map_count = 0;

    int r0 = crow - MAP_VIEW_RADIUS, r1 = crow + MAP_VIEW_RADIUS;
    int c0 = ccol - MAP_VIEW_RADIUS, c1 = ccol + MAP_VIEW_RADIUS;
    if (r0 < 0) r0 = 0; if (r1 >= MAP_GRID) r1 = MAP_GRID - 1;
    if (c0 < 0) c0 = 0; if (c1 >= MAP_GRID) c1 = MAP_GRID - 1;

    for (int row = r0; row <= r1; row++) {
        for (int col = c0; col <= c1; col++) {
            int flags;
            int y = map_cell_height(row, col, &flags);
            if (y == INT32_MIN) continue;

            float x0 = (float)((col - 64) * MAP_CELL);
            float z0 = (float)((row - 64) * MAP_CELL);
            float x1 = x0 + MAP_CELL, z1 = z0 + MAP_CELL;
            float fy = (float)y;

            uint8_t c[3];
            map_cell_colour(flags, y, c);
            map_quad(x0, z0, x1, z1, fy, fy, fy, fy, c);

            /* Skirts toward the lower neighbour on two sides only, so each
             * shared edge is emitted once. Without them the terrain reads as
             * floating tiles rather than solid ground. */
            static const int nb[2][2] = { { 0, -1 }, { -1, 0 } };
            for (int e = 0; e < 2; e++) {
                int ny = map_cell_height(row + nb[e][0], col + nb[e][1], NULL);
                int drop = (ny == INT32_MIN) ? (y + MAP_SKIRT_MAX) : ny;
                if (drop <= y) continue;          /* neighbour is higher or level */
                if (drop - y > MAP_SKIRT_MAX) drop = y + MAP_SKIRT_MAX;
                uint8_t sc[3];
                sc[0] = (uint8_t)(c[0] * 3 / 5);
                sc[1] = (uint8_t)(c[1] * 3 / 5);
                sc[2] = (uint8_t)(c[2] * 3 / 5);
                float d = (float)drop;
                if (e == 0) map_quad(x0, z0, x1, z0, fy, fy, d, d, sc);
                else        map_quad(x0, z0, x0, z1, fy, fy, d, d, sc);
            }
        }
    }
}

/* ── Frame ────────────────────────────────────────────────── */

void render_frame_begin(void) {
    if (render_backend != RENDER_HARDWARE) return;

    s_ellipsoid_count = 0;
    s_flat_count      = 0;
    s_tex_count       = 0;
    s_frame_begun     = true;

    build_view_matrix(render_frame.view);
    build_proj_matrix(render_frame.proj);
    render_frame.moving_camera = moving_camera;

    if (render_map3d) build_map_mesh();
    render_frame.map_verts = render_map3d ? s_map_verts : NULL;
    render_frame.map_count = render_map3d ? s_map_count : 0;

    render_gl_frame_begin();
}

void render_frame_end(void) {
    if (render_backend != RENDER_HARDWARE) return;

    render_frame.ellipsoids      = s_ellipsoids;
    render_frame.ellipsoid_count = s_ellipsoid_count;
    render_frame.flat_verts      = s_flat_verts;
    render_frame.flat_count      = s_flat_count;
    render_frame.tex_verts       = s_tex_verts;
    render_frame.tex_count       = s_tex_count;
    render_frame.have_3d         = s_frame_begun;

    render_gl_frame_end();

    s_frame_begun = false;
}

/* ── Lifecycle ────────────────────────────────────────────── */

bool render_available(void) { return s_available; }

void render_invalidate_background(void) { render_gl_invalidate_background(); }
void render_invalidate_palette(void)    { render_gl_invalidate_palette(); }

render_backend_t render_select(render_backend_t want) {
    if (want == render_backend) return render_backend;

    if (want == RENDER_HARDWARE) {
        if (!s_available) return RENDER_SOFTWARE;
        if (!render_gl_start()) {
            DBG_LOG(1, "[RENDER] hardware start failed, staying on software\n");
            return RENDER_SOFTWARE;
        }
        render_backend = RENDER_HARDWARE;
    } else {
        render_gl_stop();
        render_backend = RENDER_SOFTWARE;
    }

    /* Both directions: the baked background store is a software-renderer
     * artefact, and neither renderer can use the other's. Dropping it is what
     * stops actors that were baked before the switch from staying invisible
     * (hardware) or double-painted (software). */
    unbake_stuck_actors();
    render_gl_invalidate_background();
    return render_backend;
}

void render_init(void) {
    platform_t *p = win_platform();
    if (!p) return;

    s_available = render_gl_init(p);
    if (!s_available) {
        DBG_LOG(1, "[RENDER] no hardware backend; software only\n");
        return;
    }

    /* Env override beats the stored preference, matching ECSTATICA_DEBUG and
     * ECSTATICA_PROBE_XY. Useful for bisecting a rendering difference without
     * going through the menu. */
    bool want_hw = (render_hardware_pref != 0);
    const char *env = getenv("ECSTATICA_RENDERER");
    if (env && *env) {
        if (env[0] == 'g' || env[0] == 'G' || env[0] == 'h' || env[0] == 'H')
            want_hw = true;
        else if (env[0] == 's' || env[0] == 'S')
            want_hw = false;
    }

    if (want_hw) render_select(RENDER_HARDWARE);
}

void render_shutdown(void) {
    if (render_backend == RENDER_HARDWARE) render_gl_stop();
    render_gl_shutdown();
    render_backend = RENDER_SOFTWARE;
    s_available = false;
}

#endif /* ECS_ENABLE_GL */
