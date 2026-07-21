#ifndef WIN_H
#define WIN_H

#include "types.h"

/* ══════════════════════════════════════════════════════════════
 *  Window / Platform Globals (extern)
 * ══════════════════════════════════════════════════════════════ */

extern void *hwnd;
extern bool app_active;

/* ══════════════════════════════════════════════════════════════
 *  Function Declarations
 * ══════════════════════════════════════════════════════════════ */

void make_code_writable(void);
void flip_win95(void);
void present_delay(int ms);
void finish_win95(void);
void window_proc(void);
void doInit(void);
void change_screen_mode_win95(void);
void win_set_render_size(int w, int h);
void win_main_game(void);
void get_windows_directory_win95(void);

#endif /* WIN_H */
