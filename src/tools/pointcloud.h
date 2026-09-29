#ifndef POINTCLOUD_H
#define POINTCLOUD_H

#include "types.h"

/* --pointcloud: unproject every pre-rendered view through its camera and
 * write the result as PLY. Batch job; never returns. */
void pointcloud_main(void);

#endif /* POINTCLOUD_H */
