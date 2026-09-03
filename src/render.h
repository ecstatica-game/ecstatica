/**
 * render.h
 *
 * The seam between the engine's display traversal and a hardware renderer.
 *
 * display.c walks actors, parts and triangles exactly as the original did and
 * ends up at four leaf draw calls. Those leaves are routed here: the software
 * renderer keeps them, a hardware backend gets a description of the same
 * geometry instead. Nothing above the leaf knows which is running.
 *
 * Everything backend-independent — instance collection, per-face shading, the
 * projection matrix — lives in render.c. Backends only consume the built lists.
 *
 * This header is included unconditionally by display.c and win.c. With no
 * backend compiled in it collapses to "software", so DOS, Win9x, openfpgaOS and
 * the PSP see an empty seam rather than an #ifdef in their call sites.
 */

#ifndef RENDER_H
#define RENDER_H

#include "types.h"

typedef enum {
    RENDER_SOFTWARE = 0,
    RENDER_HARDWARE = 1
} render_backend_t;

/* RENDER_SOFTWARE unless a backend actually came up. Read it, never write it —
 * render_select() owns the transition because a backend has resources to
 * create and destroy.
 *
 * With no backend compiled in it is a constant rather than a global, so the
 * branches in display.c and win.c fold away entirely instead of costing a load
 * and a test per part on targets that are already short of cycles. */
#ifdef ECS_ENABLE_GL
extern render_backend_t render_backend;
#else
#define render_backend RENDER_SOFTWARE
#endif

/* Port-only preferences, persisted in ecstatica.cfg by file.c. Declared on
 * every target so the settings file round-trips identically whether or not a
 * backend was compiled in — a config written on a desktop must not lose its
 * renderer choice after a trip through a build that cannot honour it.
 *
 * render_hardware_pref is read before the window exists (init.c:106): the GLX
 * backend picks its visual at window creation, so the choice cannot wait for
 * the first frame. */
extern int16_t render_hardware_pref;    /* 0 = software, 1 = hardware */
extern int16_t render_supersample;      /* 3D layer scale, 1..4 */
extern int16_t render_enhanced_light;   /* 0 = original screen-fixed shading */
/* 0 = the pre-rendered backgrounds the game ships, 1 = the map grid drawn as
 * real geometry from the same camera. Hardware only: there is no software path
 * that could draw it. */
extern int16_t render_map3d;

#ifdef ECS_ENABLE_GL

/* True when a hardware backend is available to switch to at all. False after a
 * failed context or shader compile, which pulls the menu row (menu.c). */
bool render_available(void);

/* Bring a backend up or take it down. Safe to call repeatedly with the same
 * value. Returns the backend actually in effect afterwards, which is
 * RENDER_SOFTWARE if hardware was asked for and could not be had. */
render_backend_t render_select(render_backend_t want);

/* Called once from win_main_game after the platform window exists. Reads the
 * persisted preference and the ECSTATICA_RENDERER override. */
void render_init(void);
void render_shutdown(void);

/* Frame boundaries. begin() restores the background colour and depth, end()
 * flushes every pass, composites the 2D plane and presents. */
void render_frame_begin(void);
void render_frame_end(void);

/* Leaf draws. Called only when render_backend == RENDER_HARDWARE. */
void render_ellipsoid(part_t *part, int plane);
void render_triangle(tri_t *tri, int plane, tri_t *shade);

/* The background changed — a camera cut, or a fade rewrote the palette. Both
 * force a re-upload, and both are rare. */
void render_invalidate_background(void);
void render_invalidate_palette(void);

#else /* !ECS_ENABLE_GL */

/* Each stub consumes its arguments, so a caller that computed something only
 * for the hardware path does not warn about it being unused. */
#define render_available()             false
#define render_select(want)            ((void)(want), RENDER_SOFTWARE)
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
