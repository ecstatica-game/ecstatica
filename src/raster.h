#ifndef RASTER_H
#define RASTER_H

#include "types.h"

/* Not in the original. Between raster_begin and raster_end, draw_parts' calls
 * to the software rasterizers are recorded instead of run; raster_end replays
 * the list once per horizontal band, a band per core. Every rasterizer clips
 * its rows to the band and steps its values linearly from row to row, and each
 * band replays the calls in order, so the pixels and dirty rectangles are the
 * same as drawing them in order on one core. */

extern int raster_recording;

/* A no-op where there is only one core. */
void raster_begin(void);
void raster_end(void);

void raster_record_ellipse(part_t *part, int plane);
void raster_record_tri(int textured, tri_t *tri, int plane, tri_t *shade);

/* The rasterizers proper, drawing rows [top, bottom) only. A NULL dirty grows
 * the owning actor's area_to_clear, as the original did; the bands pass their
 * own rectangle instead, merged afterwards. */
void shade_ellipse_rows(part_t *part, int plane, subarea_t *dirty, int top, int bottom);
void draw_triangle_ell_rows(tri_t *tri, int plane, tri_t *shade, subarea_t *dirty,
                            int top, int bottom);
void draw_new_tex_tri_rows(tri_t *tri, int plane, tri_t *shade, subarea_t *dirty,
                           int top, int bottom);

#endif
