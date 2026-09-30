/*
 * --pointcloud: reconstruct the world from the pre-rendered views.
 *
 * Every view ships a linear view-space depth per pixel (mask_map[2]) next to
 * its palette image (bitmap[3]), and the map archive holds the camera each one
 * was rendered from. Inverting perspective_transform (display.c) and the view
 * rotation puts every background pixel back at its world position, so walking
 * all cameras gives a coloured reconstruction of the whole game.
 *
 * Output, in the working directory:
 *   pointcloud.ply       one point per occupied voxel, at the voxel centre
 *   pointcloud_map.ply   the collision map as solid blocks, tops coloured from
 *                        the voxels resting on them
 *
 * PLY axes are the engine's with Y and Z negated — a 180 degree turn about X,
 * so the world stands upright in ordinary viewers without being mirrored.
 *
 * Environment:
 *   ECSTATICA_PC_VOXEL  voxel edge in world units (default 32)
 *   ECSTATICA_PC_STEP   pixel stride (default 1)
 *   ECSTATICA_PC_MAXZ   drop pixels deeper than this (default 12000, 0 = keep
 *                       all; the far painted backdrops land well outside the map)
 *   ECSTATICA_PC_FILL   1 = fill between neighbouring pixels (default), 0 = off
 *   ECSTATICA_PC_BLEND  0 = sharpest sample wins (default), 1 = weighted mean
 *   ECSTATICA_PC_FIRST / ECSTATICA_PC_LAST   camera range
 *   ECSTATICA_PC_TINT   1 = colour points by camera instead of by texel
 */
#include "pointcloud.h"
#include "asm_f.h"
#include "display.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "topo.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One occupied voxel: `best` is the sample with the smallest footprint (the
 * nearest, most head-on view); all samples feed a footprint-weighted mean. */
typedef struct {
    uint64_t key;
    float    best_fp;
    uint8_t  best[3];
    float    sum[3];
    float    weight;
} voxel_t;

static voxel_t  *s_vox;
static size_t    s_vox_count, s_vox_cap;
/* Open-addressed index into s_vox; 0 = empty, otherwise index + 1. */
static uint32_t *s_slots;
static size_t    s_slot_cap;

static float s_voxel;

static int env_int(const char *name, int dflt) {
    const char *v = getenv(name);
    return (v && *v) ? atoi(v) : dflt;
}

static uint64_t hash64(uint64_t k) {
    k ^= k >> 33; k *= 0xFF51AFD7ED558CCDull;
    k ^= k >> 33; k *= 0xC4CEB9FE1A85EC53ull;
    k ^= k >> 33;
    return k;
}

#define KEY_BIAS (1 << 20)
#define KEY_MASK 0x1FFFFFull

static uint64_t voxel_key(int vx, int vy, int vz) {
    return (1ull << 63)
         | (((uint64_t)(vx + KEY_BIAS) & KEY_MASK) << 42)
         | (((uint64_t)(vy + KEY_BIAS) & KEY_MASK) << 21)
         |  ((uint64_t)(vz + KEY_BIAS) & KEY_MASK);
}

static void voxel_centre(uint64_t key, float out[3]) {
    out[0] = ((float)((int)((key >> 42) & KEY_MASK) - KEY_BIAS) + 0.5f) * s_voxel;
    out[1] = ((float)((int)((key >> 21) & KEY_MASK) - KEY_BIAS) + 0.5f) * s_voxel;
    out[2] = ((float)((int)( key        & KEY_MASK) - KEY_BIAS) + 0.5f) * s_voxel;
}

static void slots_rebuild(size_t cap) {
    free(s_slots);
    s_slot_cap = cap;
    s_slots = (uint32_t *)calloc(cap, sizeof(uint32_t));
    if (!s_slots) quit("pointcloud: out of memory");
    size_t mask = cap - 1;
    for (size_t v = 0; v < s_vox_count; v++) {
        size_t i = (size_t)hash64(s_vox[v].key) & mask;
        while (s_slots[i]) i = (i + 1) & mask;
        s_slots[i] = (uint32_t)(v + 1);
    }
}

static voxel_t *voxel_find(uint64_t key) {
    if (!s_slot_cap) return NULL;
    size_t mask = s_slot_cap - 1;
    size_t i = (size_t)hash64(key) & mask;
    while (s_slots[i]) {
        voxel_t *v = &s_vox[s_slots[i] - 1];
        if (v->key == key) return v;
        i = (i + 1) & mask;
    }
    return NULL;
}

static voxel_t *voxel_get(uint64_t key) {
    if ((s_vox_count + 1) * 10 > s_slot_cap * 7)
        slots_rebuild(s_slot_cap ? s_slot_cap * 2 : (1u << 22));

    size_t mask = s_slot_cap - 1;
    size_t i = (size_t)hash64(key) & mask;
    while (s_slots[i]) {
        voxel_t *v = &s_vox[s_slots[i] - 1];
        if (v->key == key) return v;
        i = (i + 1) & mask;
    }

    if (s_vox_count >= s_vox_cap) {
        size_t cap = s_vox_cap ? s_vox_cap * 2 : (1u << 21);
        voxel_t *g = (voxel_t *)realloc(s_vox, cap * sizeof(*g));
        if (!g) quit("pointcloud: out of memory");
        s_vox = g;
        s_vox_cap = cap;
    }
    voxel_t *v = &s_vox[s_vox_count++];
    memset(v, 0, sizeof(*v));
    v->key = key;
    v->best_fp = 1e30f;
    s_slots[i] = (uint32_t)s_vox_count;
    return v;
}

/* fp is the world size of the pixel the sample came from. */
static void splat(const float p[3], const uint8_t rgb[3], float fp) {
    uint64_t key = voxel_key((int)(float)floor((double)(p[0] / s_voxel)),
                             (int)(float)floor((double)(p[1] / s_voxel)),
                             (int)(float)floor((double)(p[2] / s_voxel)));
    voxel_t *v = voxel_get(key);
    if (fp < v->best_fp) {
        v->best_fp = fp;
        memcpy(v->best, rgb, 3);
    }
    float w = 1.0f / (fp * fp + 1.0f);
    v->sum[0] += rgb[0] * w;
    v->sum[1] += rgb[1] * w;
    v->sum[2] += rgb[2] * w;
    v->weight += w;
}

static void voxel_colour(const voxel_t *v, bool blend, uint8_t out[3]) {
    if (!blend || v->weight <= 0.0f) { memcpy(out, v->best, 3); return; }
    for (int c = 0; c < 3; c++) {
        float f = v->sum[c] / v->weight + 0.5f;
        out[c] = (uint8_t)(f > 255.0f ? 255.0f : f);
    }
}

/* ── PLY output ─────────────────────────────────────────────── */

static void put_vertex(FILE *f, float x, float y, float z, const uint8_t rgb[3]) {
    float p[3];
    p[0] = x; p[1] = -y; p[2] = -z;
    fwrite(p, sizeof(float), 3, f);
    fwrite(rgb, 1, 3, f);
}

static const char PLY_VERTEX_PROPS[] =
    "property float x\nproperty float y\nproperty float z\n"
    "property uchar red\nproperty uchar green\nproperty uchar blue\n";

static bool write_voxels(const char *path, bool blend) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "ply\nformat binary_little_endian 1.0\nelement vertex %lu\n%s"
               "end_header\n", (unsigned long)s_vox_count, PLY_VERTEX_PROPS);
    for (size_t i = 0; i < s_vox_count; i++) {
        float c[3];
        uint8_t rgb[3];
        voxel_centre(s_vox[i].key, c);
        voxel_colour(&s_vox[i], blend, rgb);
        put_vertex(f, c[0], c[1], c[2], rgb);
    }
    fclose(f);
    return true;
}

/* ── Views ──────────────────────────────────────────────────── */

static void camera_tint(int cam, uint8_t out[3]) {
    uint32_t h = (uint32_t)hash64((uint64_t)cam + 1);
    out[0] = (uint8_t)(64 + (h & 0xBF));
    out[1] = (uint8_t)(64 + ((h >> 8) & 0xBF));
    out[2] = (uint8_t)(64 + ((h >> 16) & 0xBF));
}

/* load_raw pops a requester when a VGA view is missing; probing first keeps
 * the batch run non-interactive over the sparse camera table. */
static bool view_exists(int cam) {
    char path[32];
    snprintf(path, sizeof(path), "%s/%04d.RAW", mode_svga ? "HIRES" : "VIEWS", cam);
    FILE *f = fopen_ci(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static float dist3(const float a[3], const float b[3]) {
    float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return (float)sqrt((double)(dx * dx + dy * dy + dz * dz));
}

/* Same surface unless the depth jumps; relative, since grazing floors change
 * depth fast, with a floor for the near field. */
static bool continuous(int za, int zb) {
    if (!zb) return false;
    int d = za > zb ? za - zb : zb - za;
    int lim = (za < zb ? za : zb) / 16;
    return d <= (lim > 64 ? lim : 64);
}

/* Inverse of view_transform + perspective_transform:
 *   px - centre_x = kx * X / z,  kx = zoom * (sw/320) / 16384
 *   py - centre_y = ky * Y / z,  ky = kx * 7/8 * (sh/200) / (sw/320)
 *   world = view_matrix^T * view + view_pos
 *
 * With fill on, each continuous 2x2 pixel block is sampled as a bilinear patch
 * finely enough that no voxel is skipped; far away a pixel spans several. */
static void unproject_view(int cam, int step, int max_z, bool fill, bool tint) {
    int w = screen_width, h = screen_height;
    float kx = (float)zoom_factor * ((float)w / 320.0f) / 16384.0f;
    float ky = (float)zoom_factor * 0.875f * ((float)h / 200.0f) / 16384.0f;
    if (kx <= 0.0f || ky <= 0.0f) return;

    const float s = 1.0f / 16384.0f;
    float m[3][3];
    m[0][0] = view_matrix._11 * s; m[0][1] = view_matrix._12 * s; m[0][2] = view_matrix._13 * s;
    m[1][0] = view_matrix._21 * s; m[1][1] = view_matrix._22 * s; m[1][2] = view_matrix._23 * s;
    m[2][0] = view_matrix._31 * s; m[2][1] = view_matrix._32 * s; m[2][2] = view_matrix._33 * s;
    float ox = view_pos.X, oy = view_pos.Y, oz = view_pos.Z;

    uint8_t pal[256][3];
    for (int i = 0; i < 256; i++) {
        pal[i][0] = (uint8_t)(view_cmap[i].R << 2);
        pal[i][1] = (uint8_t)(view_cmap[i].G << 2);
        pal[i][2] = (uint8_t)(view_cmap[i].B << 2);
    }
    uint8_t tint_rgb[3];
    camera_tint(cam, tint_rgb);

    /* Two rows of world positions; z = 0 marks an unusable pixel. */
    int cols = (w + step - 1) / step;
    float (*pos)[3] = (float (*)[3])malloc((size_t)cols * 2 * sizeof(*pos));
    int    *zs  = (int *)malloc((size_t)cols * 2 * sizeof(int));
    if (!pos || !zs) quit("pointcloud: out of memory");

    #define ROW_POS(r) (pos + (size_t)((r) & 1) * cols)
    #define ROW_Z(r)   (zs  + (size_t)((r) & 1) * cols)
    #define TEXEL(px, py) (tint ? tint_rgb : \
        pal[((const uint8_t *)bitmap[3])[(size_t)(py) * hires_width + (px)]])

    int rows = (h + step - 1) / step;
    for (int r = 0; r < rows; r++) {
        int py = r * step;
        const int16_t *drow = mask_map[2] + (size_t)py * hires_width;
        float vy_k = ((float)py - screen_centre_y) / ky;
        float (*pr)[3] = ROW_POS(r);
        int *zr = ROW_Z(r);
        for (int c = 0; c < cols; c++) {
            int px = c * step;
            int z = drow[px];
            if (z < 128 || z >= 0x7FFF || (max_z && z > max_z)) { zr[c] = 0; continue; }
            float vz = (float)z;
            float vx = ((float)px - screen_centre_x) / kx * vz;
            float vy = vy_k * vz;
            pr[c][0] = m[0][0] * vx + m[1][0] * vy + m[2][0] * vz + ox;
            pr[c][1] = m[0][1] * vx + m[1][1] * vy + m[2][1] * vz + oy;
            pr[c][2] = m[0][2] * vx + m[1][2] * vy + m[2][2] * vz + oz;
            zr[c] = z;
        }
        if (r == 0) continue;

        /* Patches between row r-1 (top) and row r (bottom). */
        int ty = (r - 1) * step;
        float (*pt)[3] = ROW_POS(r - 1);
        int *zt = ROW_Z(r - 1);
        for (int c = 0; c < cols; c++) {
            int za = zt[c];
            if (!za) continue;
            int tx = c * step;
            const uint8_t *ca = TEXEL(tx, ty);
            float frontal = (float)za / kx * (float)step;

            bool have_r = c + 1 < cols;
            int zb = have_r ? zt[c + 1] : 0;
            int zc = zr[c];
            int zd = have_r ? zr[c + 1] : 0;
            bool quad = fill && continuous(za, zb) && continuous(za, zc) &&
                        continuous(za, zd);
            if (!quad) {
                splat(pt[c], ca, frontal);
                continue;
            }

            const float *a = pt[c], *b = pt[c + 1], *cc = pr[c], *d = pr[c + 1];
            float edge = dist3(a, b);
            float e2 = dist3(a, cc); if (e2 > edge) edge = e2;
            e2 = dist3(b, d);        if (e2 > edge) edge = e2;
            e2 = dist3(cc, d);       if (e2 > edge) edge = e2;

            int n = (int)(float)ceil((double)(edge / (s_voxel * 0.5f)));
            if (n < 1) n = 1;
            if (n > 32) n = 32;
            const uint8_t *cb = TEXEL(tx + step < w ? tx + step : tx, ty);
            const uint8_t *cl = TEXEL(tx, py);
            const uint8_t *cd = TEXEL(tx + step < w ? tx + step : tx, py);

            for (int j = 0; j < n; j++) {
                float v = (float)j / (float)n;
                for (int i = 0; i < n; i++) {
                    float u = (float)i / (float)n;
                    float p[3];
                    for (int k = 0; k < 3; k++) {
                        float top = a[k] + (b[k] - a[k]) * u;
                        float bot = cc[k] + (d[k] - cc[k]) * u;
                        p[k] = top + (bot - top) * v;
                    }
                    const uint8_t *col = v < 0.5f ? (u < 0.5f ? ca : cb)
                                                  : (u < 0.5f ? cl : cd);
                    splat(p, col, edge);
                }
            }
        }
    }
    /* The last row never becomes the top of a patch. */
    {
        int r = rows - 1;
        float (*pr)[3] = ROW_POS(r);
        int *zr = ROW_Z(r);
        for (int c = 0; c < cols; c++)
            if (zr[c])
                splat(pr[c], TEXEL(c * step, r * step), (float)zr[c] / kx * step);
    }
    #undef ROW_POS
    #undef ROW_Z
    #undef TEXEL

    free(pos);
    free(zs);
}

/* ── Collision map as blocks ────────────────────────────────── */

/* Each map cell holds a run of elements ended by bit 15 of code_index_p1. An
 * element is a slab from height2 up to def_height (below height2 the next
 * element applies: bridges, overhangs), clipped by block_config to the cell,
 * a triangle or quadrants. */

#define MAP_MAX_DROP 12

static int cell_top(int row, int col) {
    if (row < 0 || row >= 128 || col < 0 || col >= 128) return 0;
    uint16_t idx = new_map[row][col];
    if (idx == 0 || idx == 0xFFFF) return 0;
    int top = 0;
    for (;;) {
        if ((int)idx >= top_of_map_elements) break;
        const map_area_element_t *e = &map_elements[idx];
        if (e->def_height > top) top = e->def_height;
        if (e->code_index_p1 & 0x8000) break;
        idx++;
    }
    return top;
}

/* Footprint of one element as up to four polygons in cell-local (x, z),
 * 0..512. Returns the polygon count. */
static int element_polys(int bc, float poly[4][4][2], int nv[4]) {
    static const float C[4][2] = { { 0, 0 }, { 512, 0 }, { 512, 512 }, { 0, 512 } };
    static const int T[4][3] = { { 0, 1, 2 }, { 0, 1, 3 }, { 1, 2, 3 }, { 0, 3, 2 } };
    if (bc == 1) {
        memcpy(poly[0], C, sizeof(C));
        nv[0] = 4;
        return 1;
    }
    if (bc >= 2 && bc <= 5) {
        for (int i = 0; i < 3; i++) {
            poly[0][i][0] = C[T[bc - 2][i]][0];
            poly[0][i][1] = C[T[bc - 2][i]][1];
        }
        nv[0] = 3;
        return 1;
    }
    if (bc < 6) return 0;
    static const struct { int bit; float x0, z0; } Q[4] = {
        { 1, 256, 0 }, { 2, 0, 0 }, { 4, 256, 256 }, { 8, 0, 256 },
    };
    int n = 0;
    for (int q = 0; q < 4; q++) {
        if (!(bc & Q[q].bit)) continue;
        float x0 = Q[q].x0, z0 = Q[q].z0;
        float sq[4][2];
        sq[0][0] = x0;       sq[0][1] = z0;
        sq[1][0] = x0 + 256; sq[1][1] = z0;
        sq[2][0] = x0 + 256; sq[2][1] = z0 + 256;
        sq[3][0] = x0;       sq[3][1] = z0 + 256;
        memcpy(poly[n], sq, sizeof(sq));
        nv[n++] = 4;
    }
    return n;
}

static bool point_in_poly(float x, float z, const float p[][2], int n) {
    bool in = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if ((p[i][1] > z) != (p[j][1] > z) &&
            x < (p[j][0] - p[i][0]) * (z - p[i][1]) / (p[j][1] - p[i][1]) + p[i][0])
            in = !in;
    }
    return in;
}

/* Mean colour of the reconstructed surface just above this footprint (engine
 * Y is down). */
static bool top_colour(float cx, float cz, const float poly[][2], int nv,
                       float y_top, bool blend, uint8_t out[3]) {
    float sum[3] = { 0, 0, 0 };
    int hits = 0;
    int vy0 = (int)(float)floor((double)(y_top / s_voxel));
    for (float lz = s_voxel * 0.5f; lz < 512.0f; lz += s_voxel) {
        for (float lx = s_voxel * 0.5f; lx < 512.0f; lx += s_voxel) {
            if (!point_in_poly(lx, lz, poly, nv)) continue;
            int vx = (int)(float)floor((double)((cx + lx) / s_voxel));
            int vz = (int)(float)floor((double)((cz + lz) / s_voxel));
            for (int dy = -2; dy <= 1; dy++) {
                voxel_t *v = voxel_find(voxel_key(vx, vy0 + dy, vz));
                if (!v) continue;
                uint8_t c[3];
                voxel_colour(v, blend, c);
                sum[0] += c[0]; sum[1] += c[1]; sum[2] += c[2];
                hits++;
                break;
            }
        }
    }
    if (!hits) return false;
    for (int k = 0; k < 3; k++) out[k] = (uint8_t)(sum[k] / hits + 0.5f);
    return true;
}

typedef struct {
    FILE *f;
    unsigned long verts, faces;
} mesh_out_t;

static void shade(const uint8_t in[3], int num, int den, uint8_t out[3]) {
    for (int k = 0; k < 3; k++) out[k] = (uint8_t)(in[k] * num / den);
}

/* Vertices stream to m->f; faces are fan-triangulated into a second stream
 * appended once the vertex count is known. Faces own their vertices so each
 * can carry its own colour. */
static void emit_face(mesh_out_t *m, FILE *faces, const float (*v)[3], int n,
                      const uint8_t rgb[3]) {
    unsigned long base = m->verts;
    for (int i = 0; i < n; i++) put_vertex(m->f, v[i][0], v[i][1], v[i][2], rgb);
    m->verts += (unsigned long)n;
    for (int i = 1; i + 1 < n; i++) {
        uint8_t cnt = 3;
        int32_t idx[3];
        idx[0] = (int32_t)base; idx[1] = (int32_t)(base + i); idx[2] = (int32_t)(base + i + 1);
        fwrite(&cnt, 1, 1, faces);
        fwrite(idx, sizeof(int32_t), 3, faces);
        m->faces++;
    }
}

static void dump_map_blocks(bool blend) {
    FILE *vf = tmpfile(), *ff = tmpfile();
    if (!vf || !ff) { DBG_LOG(1, "[PC] map: no temp file\n"); return; }
    mesh_out_t m;
    m.f = vf; m.verts = 0; m.faces = 0;
    unsigned long elems = 0, coloured = 0;

    for (int row = 0; row < 128; row++) {
        for (int col = 0; col < 128; col++) {
            uint16_t idx = new_map[row][col];
            if (idx == 0 || idx == 0xFFFF) continue;
            float cx = (float)((col - 64) << 9), cz = (float)((row - 64) << 9);

            /* Ignore empty neighbours, or borders drop to height 0. */
            int ground = 256;
            static const int nb[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
            for (int k = 0; k < 4; k++) {
                int t = cell_top(row + nb[k][0], col + nb[k][1]);
                if (t && t < ground) ground = t;
            }

            for (;;) {
                if ((int)idx >= top_of_map_elements) break;
                const map_area_element_t *e = &map_elements[idx];
                int top = e->def_height;
                /* height2 == 0 is solid ground: run it down to the lowest
                 * neighbour, capped like render.c's skirts. */
                int bot = e->height2;
                if (!bot) {
                    bot = ground;
                    if (bot < top - MAP_MAX_DROP) bot = top - MAP_MAX_DROP;
                }
                if (bot >= top) bot = top - 1;
                if (bot < 0) bot = 0;
                float yt = (float)((128 - top) << height_shift);
                float yb = (float)((128 - bot) << height_shift);

                float poly[4][4][2];
                int nv[4];
                int np = element_polys(e->block_config, poly, nv);
                for (int p = 0; p < np; p++) {
                    uint8_t c[3] = { 255, 0, 255 };
                    if (e->material == 1) { c[0] = 0; c[1] = 255; c[2] = 255; }
                    if (top_colour(cx, cz, (const float (*)[2])poly[p], nv[p], yt, blend, c))
                        coloured++;

                    float fv[4][3];
                    for (int i = 0; i < nv[p]; i++) {
                        fv[i][0] = cx + poly[p][i][0]; fv[i][1] = yt; fv[i][2] = cz + poly[p][i][1];
                    }
                    emit_face(&m, ff, (const float (*)[3])fv, nv[p], c);

                    uint8_t sc[3], bc[3];
                    shade(c, 3, 5, sc);
                    shade(c, 2, 5, bc);
                    for (int i = 0; i < nv[p]; i++) {
                        fv[nv[p] - 1 - i][0] = cx + poly[p][i][0];
                        fv[nv[p] - 1 - i][1] = yb;
                        fv[nv[p] - 1 - i][2] = cz + poly[p][i][1];
                    }
                    emit_face(&m, ff, (const float (*)[3])fv, nv[p], bc);

                    for (int i = 0; i < nv[p]; i++) {
                        int j = (i + 1) % nv[p];
                        float sv[4][3];
                        sv[0][0] = cx + poly[p][i][0]; sv[0][1] = yt; sv[0][2] = cz + poly[p][i][1];
                        sv[1][0] = cx + poly[p][i][0]; sv[1][1] = yb; sv[1][2] = cz + poly[p][i][1];
                        sv[2][0] = cx + poly[p][j][0]; sv[2][1] = yb; sv[2][2] = cz + poly[p][j][1];
                        sv[3][0] = cx + poly[p][j][0]; sv[3][1] = yt; sv[3][2] = cz + poly[p][j][1];
                        emit_face(&m, ff, (const float (*)[3])sv, 4, sc);
                    }
                }
                elems++;
                if (e->code_index_p1 & 0x8000) break;
                idx++;
            }
        }
    }

    FILE *f = fopen("pointcloud_map.ply", "wb");
    if (f) {
        fprintf(f, "ply\nformat binary_little_endian 1.0\nelement vertex %lu\n%s"
                   "element face %lu\nproperty list uchar int vertex_indices\n"
                   "end_header\n", m.verts, PLY_VERTEX_PROPS, m.faces);
        char buf[65536];
        size_t n;
        rewind(vf);
        while ((n = fread(buf, 1, sizeof(buf), vf)) > 0) fwrite(buf, 1, n, f);
        rewind(ff);
        while ((n = fread(buf, 1, sizeof(buf), ff)) > 0) fwrite(buf, 1, n, f);
        fclose(f);
        fprintf(stderr, "pointcloud: wrote pointcloud_map.ply (%lu elements, %lu faces, "
                        "%lu tops coloured from views)\n", elems, m.faces, coloured);
    }
    fclose(vf);
    fclose(ff);
}

void pointcloud_main(void) {
    int voxel = env_int("ECSTATICA_PC_VOXEL", 32);
    int step  = env_int("ECSTATICA_PC_STEP", 1);
    int max_z = env_int("ECSTATICA_PC_MAXZ", 12000);
    int first = env_int("ECSTATICA_PC_FIRST", 1);
    int last  = env_int("ECSTATICA_PC_LAST", num_cameras - 1);
    bool fill  = env_int("ECSTATICA_PC_FILL", 1) != 0;
    bool blend = env_int("ECSTATICA_PC_BLEND", 0) != 0;
    bool tint  = env_int("ECSTATICA_PC_TINT", 0) != 0;
    if (voxel < 1) voxel = 1;
    if (step < 1) step = 1;
    if (first < 1) first = 1;
    if (last >= num_cameras) last = num_cameras - 1;
    if (last >= 1200) last = 1199;
    s_voxel = (float)voxel;

    fprintf(stderr, "pointcloud: %d cameras, %dx%d, cams %d..%d, voxel %d, step %d, "
                    "fill %d, blend %d\n", num_cameras, screen_width, screen_height,
            first, last, voxel, step, fill, blend);
    if (num_cameras <= 1)
        quit("pointcloud: no cameras loaded — is the map in the archive?");

    int views = 0, skipped = 0;
    for (int cam = first; cam <= last; cam++) {
        if (!camera[cam].zoom_factor || !view_exists(cam)) { skipped++; continue; }

        selected_camera = (int16_t)cam;
        switch_camera(&camera[cam]);
        if (load_raw()) { skipped++; continue; }

        size_t before = s_vox_count;
        unproject_view(cam, step, max_z, fill, tint);
        views++;
        DBG_LOG(1, "[PC] cam=%d pos=(%d,%d,%d) rot=(%d,%d,%d) zoom=%d +%lu voxels\n",
                cam, camera[cam].view_pos.X, camera[cam].view_pos.Y,
                camera[cam].view_pos.Z, camera[cam].view_rot.X,
                camera[cam].view_rot.Y, camera[cam].view_rot.Z,
                camera[cam].zoom_factor, (unsigned long)(s_vox_count - before));
        if ((views & 31) == 0)
            fprintf(stderr, "pointcloud: %d views, %lu voxels\n",
                    views, (unsigned long)s_vox_count);
    }

    if (!write_voxels("pointcloud.ply", blend))
        quit("pointcloud: can't write pointcloud.ply");
    fprintf(stderr, "pointcloud: wrote pointcloud.ply (%lu voxels from %d views, %d skipped)\n",
            (unsigned long)s_vox_count, views, skipped);

    dump_map_blocks(blend);

    free(s_vox);
    free(s_slots);
    quit("");
}
