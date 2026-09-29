#ifndef WIN_H
#define WIN_H

#include "types.h"

extern void *hwnd;
extern bool app_active;

/* SCALE_PILLARBOX / SCALE_CROP / SCALE_STRETCH (platform.h). Loaded from
 * ecstatica.cfg before do_init() creates the platform, so the saved
 * preference reaches platform_set_scale_mode() on first apply — see init.c. */
extern int16_t display_scale_mode;

struct platform_t;

/* The platform handle do_init() created. NULL before do_init(). The viewer
 * reads input straight from it — window_proc() maps keys onto the game's
 * movement globals, which the viewer has no use for. */
struct platform_t *win_platform(void);

void make_code_writable(void);
void flip_win95(void);
void present_delay(int ms);
void window_proc(void);
void doInit(void);
void change_screen_mode_win95(void);
void win_set_render_size(int w, int h);
void win_set_scale_mode(int mode);

/* E1 speed mode (sneak/walk/run), shared between window_proc()'s L3 handler
 * and the Settings menu's Speed Mode row — see win.c. dir > 0 advances,
 * dir <= 0 retreats; e1_speed_mode_step() reads the current step back for
 * display (0 = sneak, 1 = walk, 2 = run). */
void e1_cycle_speed_mode(int dir);
int e1_speed_mode_step(void);
void win_main_game(void);
void get_windows_directory_win95(void);

#endif /* WIN_H */
