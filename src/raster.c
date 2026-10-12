/**
 * raster.c
 *
 * Banded replay of draw_parts' rasterizer calls; see raster.h. Not in the
 * original, which drew every part in place on its one CPU.
 *
 * A recorded ellipse keeps its part pointer: put_an_ellipse has already
 * transformed the part, and nothing touches it again before the replay. A
 * triangle is copied with its vertices, since cuboids and near-plane clipping
 * build theirs on the stack and draw_polygon edits quads in place.
 */

#include "raster.h"
#include "display.h"
#include "game.h"
#include "platform.h"
#include "prof.h"

#include <stdlib.h>
#include <string.h>

#define MAX_BANDS 4

enum { CMD_ELLIPSE, CMD_TRI, CMD_TEX_TRI };

typedef struct {
    uint8_t  kind;
    int8_t   plane;
    uint8_t  has_shade;
    int      slot;          /* index into areas, -1 for none */
    part_t  *part;
    tri_t    tri;
    vector_t pts[3];
    vector_t shade_pts[3];
} raster_cmd_t;

int raster_recording;

static raster_cmd_t *cmds;
static int cmd_count, cmd_cap;

/* The area_to_clear each command grows; every band keeps its own copy. */
static subarea_t **areas;
static int slot_count, slot_cap;
static subarea_t *band_rects[MAX_BANDS];
static int band_rects_cap;

static int band_count;      /* 0 until the first raster_begin */
static int band_top[MAX_BANDS], band_bottom[MAX_BANDS];
static platform_sem_t *band_go[MAX_BANDS];
static platform_sem_t *band_done;

/* With one band (direct) the calls grow the shared rectangles themselves,
 * exactly as drawing in place would. */
static void replay_band(int b, bool direct) {
    const int top = band_top[b], bottom = band_bottom[b];
    subarea_t *rects = band_rects[b];

    for (int i = 0; i < cmd_count; i++) {
        raster_cmd_t *c = &cmds[i];
        subarea_t *dirty = (!direct && c->slot >= 0) ? &rects[c->slot] : NULL;

        if (c->kind == CMD_ELLIPSE) {
            shade_ellipse_rows(c->part, c->plane, dirty, top, bottom);
            continue;
        }

        point_t p[3], s[3];
        tri_t tri = c->tri, shade;
        for (int k = 0; k < 3; k++) {
            p[k].screen_coord = c->pts[k];
            s[k].screen_coord = c->shade_pts[k];
        }
        tri.point1 = &p[0];
        tri.point2 = &p[1];
        tri.point3 = &p[2];
        /* With a NULL dirty the rasterizer would reach the shared rectangle
         * through parent_actor. */
        if (!direct && !dirty) tri.parent_actor = NULL;
        tri_t *sh = NULL;
        if (c->has_shade) {
            memset(&shade, 0, sizeof(shade));
            shade.point1 = &s[0];
            shade.point2 = &s[1];
            shade.point3 = &s[2];
            sh = &shade;
        }

        if (c->kind == CMD_TEX_TRI)
            draw_new_tex_tri_rows(&tri, c->plane, sh, dirty, top, bottom);
        else
            draw_triangle_ell_rows(&tri, c->plane, sh, dirty, top, bottom);
    }
}

static void band_worker(void *arg) {
    int b = (int)(intptr_t)arg;
    for (;;) {
        platform_sem_wait(band_go[b]);
        uint32_t t0 = PROF_CLOCK();
        replay_band(b, false);
        PROF_WORKER(PROF_W_BAND1 + b - 1, PROF_CLOCK() - t0);
        platform_sem_post(band_done);
    }
}

static void init_bands(void) {
    band_count = 1;
    int n = platform_cpu_count();
    if (n > MAX_BANDS) n = MAX_BANDS;
    if (n < 2) return;

    band_done = platform_sem_create(0);
    if (!band_done) return;
    for (int b = 1; b < n; b++) {
        band_go[b] = platform_sem_create(0);
        if (!band_go[b]) break;
        /* Band b on core b: the game thread is band 0. */
        if (!platform_thread_start("ecs_raster", band_worker, (void *)(intptr_t)b,
                                   PLATFORM_THREAD_WORKER, b))
            break;
        band_count = b + 1;
    }
    DBG_LOG(1, "[RASTER] %d bands\n", band_count);
}

void raster_begin(void) {
    if (!band_count) init_bands();
    if (band_count < 2) return;
    cmd_count = 0;
    slot_count = 0;
    raster_recording = 1;
}

static raster_cmd_t *new_cmd(int kind, int plane, subarea_t *area) {
    if (cmd_count == cmd_cap) {
        int cap = cmd_cap ? cmd_cap * 2 : 1024;
        raster_cmd_t *grown = (raster_cmd_t *)realloc(cmds, (size_t)cap * sizeof(*cmds));
        if (!grown) return NULL;
        cmds = grown;
        cmd_cap = cap;
    }

    /* An actor's parts arrive together, so one slot per run of the same
     * area; a repeat later on just merges twice. */
    int slot = -1;
    if (area) {
        if (slot_count && areas[slot_count - 1] == area) {
            slot = slot_count - 1;
        } else {
            if (slot_count == slot_cap) {
                int cap = slot_cap ? slot_cap * 2 : 256;
                subarea_t **grown = (subarea_t **)realloc(areas, (size_t)cap * sizeof(*areas));
                if (!grown) return NULL;
                areas = grown;
                slot_cap = cap;
            }
            areas[slot_count] = area;
            slot = slot_count++;
        }
    }

    raster_cmd_t *c = &cmds[cmd_count++];
    c->kind = (uint8_t)kind;
    c->plane = (int8_t)plane;
    c->slot = slot;
    c->has_shade = 0;
    c->part = NULL;
    return c;
}

/* Out of memory mid-frame: give up on the bands for this frame and draw the
 * rest in place, after what was recorded so far. */
static void flush_inline(void);

void raster_record_ellipse(part_t *part, int plane) {
    subarea_t *area = part->parent_actor ? part->parent_actor->area_to_clear : NULL;
    raster_cmd_t *c = new_cmd(CMD_ELLIPSE, plane, area);
    if (!c) {
        flush_inline();
        shade_ellipse_rows(part, plane, NULL, top_edge, bottom_edge);
        return;
    }
    c->part = part;
}

void raster_record_tri(int textured, tri_t *tri, int plane, tri_t *shade) {
    subarea_t *area = tri->parent_actor ? tri->parent_actor->area_to_clear : NULL;
    raster_cmd_t *c = new_cmd(textured ? CMD_TEX_TRI : CMD_TRI, plane, area);
    if (!c) {
        flush_inline();
        if (textured)
            draw_new_tex_tri_rows(tri, plane, shade, NULL, top_edge, bottom_edge);
        else
            draw_triangle_ell_rows(tri, plane, shade, NULL, top_edge, bottom_edge);
        return;
    }
    c->tri = *tri;
    c->pts[0] = tri->point1->screen_coord;
    c->pts[1] = tri->point2->screen_coord;
    c->pts[2] = tri->point3->screen_coord;
    if (shade) {
        c->has_shade = 1;
        c->shade_pts[0] = shade->point1->screen_coord;
        c->shade_pts[1] = shade->point2->screen_coord;
        c->shade_pts[2] = shade->point3->screen_coord;
    }
}

static bool prepare_rects(int bands) {
    if (slot_count > band_rects_cap) {
        for (int b = 0; b < MAX_BANDS; b++) {
            free(band_rects[b]);
            band_rects[b] = NULL;
        }
        band_rects_cap = 0;
        for (int b = 0; b < MAX_BANDS; b++) {
            band_rects[b] = (subarea_t *)malloc((size_t)slot_count * sizeof(subarea_t));
            if (!band_rects[b]) return false;
        }
        band_rects_cap = slot_count;
    }
    for (int b = 0; b < bands; b++) {
        for (int s = 0; s < slot_count; s++) {
            band_rects[b][s].left = 0x7FFF;
            band_rects[b][s].right = -0x7FFF;
            band_rects[b][s].top = 0x7FFF;
            band_rects[b][s].bottom = -0x7FFF;
        }
    }
    return true;
}

static void merge_rects(int bands) {
    for (int s = 0; s < slot_count; s++) {
        subarea_t *a = areas[s];
        for (int b = 0; b < bands; b++) {
            const subarea_t *r = &band_rects[b][s];
            if (r->left < a->left) a->left = r->left;
            if (r->right > a->right) a->right = r->right;
            if (r->top < a->top) a->top = r->top;
            if (r->bottom > a->bottom) a->bottom = r->bottom;
        }
    }
}

static void run_bands(int bands) {
    if (bands > 1 && !prepare_rects(bands))
        bands = 1;

    int rows = bottom_edge - top_edge;
    for (int b = 0; b < bands; b++) {
        band_top[b] = top_edge + rows * b / bands;
        band_bottom[b] = top_edge + rows * (b + 1) / bands;
    }

    if (bands == 1) {
        replay_band(0, true);
        return;
    }

    for (int b = 1; b < bands; b++)
        platform_sem_post(band_go[b]);
    replay_band(0, false);
    PROF_BEGIN(PROF_BWAIT);
    for (int b = 1; b < bands; b++)
        platform_sem_wait(band_done);
    PROF_END(PROF_BWAIT);

    merge_rects(bands);
}

static void flush_inline(void) {
    raster_recording = 0;
    if (cmd_count) run_bands(1);
    cmd_count = 0;
    slot_count = 0;
}

void raster_end(void) {
    if (!raster_recording) return;
    raster_recording = 0;
    if (cmd_count) {
        /* Bands thinner than this cost more in set-up than they share. */
        int bands = band_count;
        if (bottom_edge - top_edge < bands * 8) bands = 1;
        run_bands(bands);
    }
    cmd_count = 0;
    slot_count = 0;
}
