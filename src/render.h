/**
 * render.h
 *
 * The seam between the display traversal and a hardware renderer. display.c
 * ends at four leaf draw calls; the software renderer keeps them, a hardware
 * backend gets a description of the same geometry instead. With no backend
 * compiled in this collapses to "software" with no #ifdef at the call sites.
 */

#ifndef RENDER_H
#define RENDER_H

#include "types.h"

typedef enum {
    RENDER_SOFTWARE = 0,
    RENDER_HARDWARE = 1
} render_backend_t;

/* Read-only; render_select() owns transitions. A constant when no backend is
 * compiled in, so the branches fold away on slow targets. */
#ifdef ECS_ENABLE_GL
extern render_backend_t render_backend;
#else
#define render_backend RENDER_SOFTWARE
#endif

/* Port-only preferences in ecstatica.cfg, declared on every target so the file
 * round-trips through builds that cannot honour them. render_hardware_pref is
 * read before the window exists: GLX picks its visual at window creation. */
extern int16_t render_hardware_pref;    /* 0 = software, 1 = hardware */
extern int16_t render_supersample;      /* 3D layer scale, 1..4 */
extern int16_t render_enhanced_light;   /* 0 = original screen-fixed shading */
/* 0 = the shipped pre-rendered backgrounds, 1 = the map grid as geometry. */
extern int16_t render_map3d;

#ifdef ECS_ENABLE_GL

/* False after a failed context or shader compile; hides the menu row. */
bool render_available(void);

/* Idempotent. Returns the backend in effect, RENDER_SOFTWARE if hardware
 * could not be had. */
render_backend_t render_select(render_backend_t want);

/* After the window exists; reads the preference and ECSTATICA_RENDERER. */
void render_init(void);
void render_shutdown(void);

/* Frame boundaries. begin() restores the background colour and depth, end()
 * flushes every pass, composites the 2D plane and presents. */
void render_frame_begin(void);
void render_frame_end(void);

/* Leaf draws. Called only when render_backend == RENDER_HARDWARE. */
void render_ellipsoid(part_t *part, int plane);
void render_triangle(tri_t *tri, int plane, tri_t *shade);

/* Camera cut or palette change: forces a re-upload. */
void render_invalidate_background(void);
void render_invalidate_palette(void);

#else /* !ECS_ENABLE_GL */

/* Stubs consume their arguments to avoid unused warnings at call sites. */
#define render_available()             false
#define render_select(want)            ((void)(want), (void)RENDER_SOFTWARE)
#define render_init()                  ((void)0)
#define render_shutdown()              ((void)0)
#define render_frame_begin()           ((void)0)
#define render_frame_end()             ((void)0)
#define render_ellipsoid(p, pl)        ((void)(p), (void)(pl))
#define render_triangle(t, pl, s)      ((void)(t), (void)(pl), (void)(s))
#define render_invalidate_background() ((void)0)
#define render_invalidate_palette()    ((void)0)

#endif /* ECS_ENABLE_GL */

#endif /* RENDER_H */
