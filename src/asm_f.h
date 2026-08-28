#ifndef ASM_F_H
#define ASM_F_H

#include "types.h"

void matrix_mult(matrix3x3_t *out, matrix3x3_t *in1, matrix3x3_t *in2);
void matrix_vector(vector_t *in, vector_t *out, matrix3x3_t *mat);
void matrix_long_vector(long_vector_t *in, long_vector_t *out, matrix3x3_t *mat);
void unpack_bitmap(char *output, char *input);
void unpack_mask(int16_t *output, char *input);

/* Ellipse column renderers */
void ellipse_line_win95(int mask_idx, int col_height, int z_interp, char *draw_ptr, int shade_idx);
void beam_line_win95(int mask_idx, int col_height, int z_interp, char *draw_ptr, int shade_idx);
void shadow_line_win95(int mask_idx, int col_height, int z_interp, char *draw_ptr, int shade_idx);
void smoke_line_win95(int mask_idx, int col_height, int z_interp, char *draw_ptr, int shade_idx);

/* Random number generator */
int my_rand(void);

/* Case-insensitive file open */
FILE *fopen_ci(const char *path, const char *mode);

/* Read-only asset search roots. enhanced_graphics picks which root wins for
 * the swappable presentation directories (HIRES/VIEWS/VISIB/GRAPHICS/
 * LOWGRAPH/MUSIC). The database unit is never searched across roots. */
extern int32_t enhanced_graphics;
void init_data_roots(void);
int hires_data_available(void);

/* Prefix prepended to every relative asset path. Platforms without a
 * per-process working directory (openfpgaOS) set it to their data mount
 * before any asset load; desktop builds leave it empty. */
void file_set_data_root(const char *root);

/* True when `dir` holds a game database (CODE/ECSTATIC.FAN, case-insensitive).
 * Pass "" for the working directory. Used by the platform layer to find the
 * game folder before anything is loaded. */
int file_dir_has_database(const char *dir);

/* Drop the cached directory listings used by the case-resolving path walk.
 * Asset roots are read-only once mounted, so this is only needed if a root is
 * swapped underneath a running game. */
void file_flush_path_cache(void);

/* VGA plane control (no-op in SDL port) */
void set_read_plane(int plane);

#endif /* ASM_F_H */
