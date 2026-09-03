/**
 * render_priv.h
 *
 * Shared between render.c (which builds the draw lists) and the backend
 * (which consumes them). Not part of the engine-facing interface — nothing
 * outside those two files includes this.
 */

#ifndef RENDER_PRIV_H
#define RENDER_PRIV_H

#ifdef ECS_ENABLE_GL

#include "types.h"
#include "platform.h"

/* Matches view_transform's near-clip test (display.c:1048) and the int16 range
 * the engine's depth mask is stored in (mask_map cleared to 0x7FFF, map.c:62). */
#define RENDER_NEAR_Z   128
#define RENDER_FAR_Z    32767

enum {
    RENDER_ELL_SOLID = 0,
    RENDER_ELL_SHADOW,
    RENDER_ELL_SMOKE,
    RENDER_ELL_BEAM
};

/**
 * One ellipsoid, entirely in view space:  p = centre + rot * diag(axes) * u.
 *
 * `rot` is part->matrix_2 converted out of 14-bit fixed point — it already
 * contains the view rotation, so no further transform is applied to it.
 */
typedef struct {
    float   centre[3];
    float   rot[9];          /* row-major 3x3 */
    float   axes[3];         /* semi-axes, world units */
    float   radius;          /* max(axes), for the bounding quad */
    uint8_t colour;          /* shade_tab first index, 0..16 */
    uint8_t mode;            /* RENDER_ELL_*  */
    int16_t colour_shade;    /* 14-bit fixed multiplier for the fog band */
} render_ellipsoid_t;

/**
 * One triangle corner. Position is world space — the backend applies view and
 * projection itself. `pal` is the final palette index from the CPU-side face
 * shading; `layer` is the texture array slot, or -1 for flat.
 */
typedef struct {
    float   pos[3];
    float   uv[2];           /* texel units; wrapped at 0x7F like asm_f.c:456 */
    uint8_t pal;
    int16_t layer;
} render_vertex_t;

/**
 * One corner of the debug map mesh. Colour is carried as RGB rather than a
 * palette index: the map view is a diagnostic, not part of the game's look, and
 * its colour coding has to stay readable whatever palette the current view
 * happens to be using.
 */
typedef struct {
    float   pos[3];
    uint8_t rgb[3];
    uint8_t pad;
} render_map_vertex_t;

typedef struct {
    float view[16];
    float proj[16];
    int   moving_camera;

    const render_ellipsoid_t *ellipsoids;
    int                       ellipsoid_count;
    const render_vertex_t    *flat_verts;
    int                       flat_count;
    const render_vertex_t    *tex_verts;
    int                       tex_count;
    bool                      have_3d;

    /* Debug map geometry, built once and reused until the map data changes. */
    const render_map_vertex_t *map_verts;
    int                        map_count;
} render_frame_t;

extern render_frame_t render_frame;

/* ── Backend interface ────────────────────────────────────── */

bool render_gl_init(platform_t *p);      /* create context, load GL, compile */
void render_gl_shutdown(void);
bool render_gl_start(void);              /* become the active renderer */
void render_gl_stop(void);

void render_gl_frame_begin(void);
void render_gl_frame_end(void);

void render_gl_invalidate_background(void);
void render_gl_invalidate_palette(void);

/**
 * Texture array slot for a texture_tab index, uploading it on first use.
 * Returns -1 when the texture is not loaded, which makes the caller fall back
 * to a flat triangle rather than dropping the face.
 */
int  render_gl_texture_layer(int16_t texture_name_index);

#endif /* ECS_ENABLE_GL */
#endif /* RENDER_PRIV_H */
