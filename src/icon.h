#ifndef ICON_H
#define ICON_H

#include "types.h"

extern int16_t rate_box_left;
extern int16_t rate_box_top;
extern int16_t vector_box_left;
extern int16_t vector_box_top;
extern int16_t max_message_len;

void set_vga_constants(void);
void set_svga_constants(void);

#endif /* ICON_H */
