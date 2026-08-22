#ifndef WIN_H
#define WIN_H

#include "types.h"

extern void *hwnd;
extern bool app_active;

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
void win_main_game(void);
void get_windows_directory_win95(void);

#endif /* WIN_H */
