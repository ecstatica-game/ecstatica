/**
 * platforms/touch_controls.h
 *
 * On-screen gamepad for touch screens (Android, iOS): a stick on the left,
 * the four face buttons on the right, shoulders, triggers, Start/Select and
 * the stick clicks. It reports a platform_gamepad_state_t, so win.c maps it
 * exactly as it maps a real pad. A touch that starts on none of the controls
 * is the pointer, for the menus.
 *
 * Coordinates are surface pixels: the buffer the backend presents, which
 * draw() paints into. Events may come from another thread than the game's.
 */

#ifndef PLATFORMS_TOUCH_CONTROLS_H
#define PLATFORMS_TOUCH_CONTROLS_H

#include "screen.h"
#include "../platform.h"

enum { TOUCH_DOWN, TOUCH_MOVE, TOUCH_UP };

void touch_init(void);

/* Surface size; the controls scale with its shorter side. */
void touch_layout(int surf_w, int surf_h);

void touch_event(long id, int phase, int x, int y);
void touch_cancel_all(void);

/* Controls start hidden and appear with the first touch; the backend hides
 * them again when a real gamepad or keyboard is used. */
void touch_hide(void);

/* Adds the on-screen pad to *gp (sets connected while any control is
 * visible). The pointer is returned in surface pixels. */
void touch_poll(platform_gamepad_state_t *gp, bool *pointer_down,
                int *pointer_x, int *pointer_y);

void touch_draw(uint32_t *dst, int surf_w, int surf_h, int pitch);

#endif /* PLATFORMS_TOUCH_CONTROLS_H */
