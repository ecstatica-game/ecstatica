#ifndef PREFETCH_H
#define PREFETCH_H

#include <stddef.h>
#include <stdint.h>
#include "types.h"

/* Not in the original. Reads the backgrounds of the cameras around a position
 * on a background thread, so a camera cut finds its .RAW already in memory.
 * Without threads every call is a no-op and load_raw reads as before. */

/* Main thread, every frame; only does work when pos enters a new map cell. */
void prefetch_views_near(const vector_t *pos);

/* The whole file at `path` if it was prefetched, else NULL. Waits if it is
 * still being read. Valid until the next prefetch_views_near; not freed by
 * the caller. */
const uint8_t *prefetch_get(const char *path, size_t *len);

/* Stops queueing and waits out a read in flight. Before exit: newlib's exit
 * frees the heap, and a read still running would allocate from it. */
void prefetch_shutdown(void);

#endif
