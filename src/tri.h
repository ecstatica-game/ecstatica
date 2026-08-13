#ifndef TRI_H
#define TRI_H

#include "types.h"

void draw_polygon(tri_t *tri, int plane, tri_t *shade);
void draw_textured_tri(tri_t *tri, int plane, tri_t *shade);
void draw_new_tex_tri(tri_t *tri, int plane, tri_t *shade);

#endif /* TRI_H */
