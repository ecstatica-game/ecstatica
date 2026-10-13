/**
 * platforms/touch_controls.c
 *
 * See touch_controls.h. The layout puts every control within reach of the
 * thumbs in landscape, where on a phone the 4:3 picture leaves bars wide
 * enough to hold them.
 */

#include "touch_controls.h"

#include <stdlib.h>
#include <string.h>

enum {
    C_STICK, C_A, C_B, C_X, C_Y, C_LB, C_RB, C_LT, C_RT,
    C_SELECT, C_START, C_L3, C_R3, C_COUNT, C_NONE = -1, C_POINTER = -2
};

typedef struct {
    int  x, y;          /* centre */
    int  r;             /* round controls */
    int  hw, hh;        /* half extents of rectangular ones; 0 when round */
    const char *label;
} control_t;

#define MAX_TOUCHES 10

typedef struct {
    long id;
    int  control;       /* C_*, C_POINTER, or C_NONE for a free slot */
    int  x, y;
} touch_t;

static control_t s_ctl[C_COUNT];
static touch_t   s_touch[MAX_TOUCHES];
static int       s_w, s_h, s_unit;
static bool      s_visible;
static platform_sem_t *s_lock;

static void lock(void)   { if (s_lock) platform_sem_wait(s_lock); }
static void unlock(void) { if (s_lock) platform_sem_post(s_lock); }

void touch_init(void) {
    if (!s_lock) s_lock = platform_sem_create(1);
    for (int i = 0; i < MAX_TOUCHES; i++) s_touch[i].control = C_NONE;
}

static void set_round(int c, int x, int y, int r, const char *label) {
    s_ctl[c] = (control_t){ x, y, r, 0, 0, label };
}

static void set_rect(int c, int x, int y, int hw, int hh, const char *label) {
    s_ctl[c] = (control_t){ x, y, 0, hw, hh, label };
}

void touch_layout(int w, int h) {
    lock();
    if (w == s_w && h == s_h) { unlock(); return; }
    s_w = w;
    s_h = h;
    int u = (w < h ? w : h) / 12;
    if (u < 8) u = 8;
    s_unit = u;

    int sx = u * 3, sy = h - u * 3;
    set_round(C_STICK, sx, sy, u * 22 / 10, NULL);
    set_round(C_L3, sx + u * 24 / 10, sy - u * 24 / 10, u * 6 / 10, "L3");

    int fx = w - u * 3, fy = h - u * 3, d = u * 15 / 10, r = u * 3 / 4;
    set_round(C_A, fx, fy + d, r, "A");
    set_round(C_B, fx + d, fy, r, "B");
    set_round(C_X, fx - d, fy, r, "X");
    set_round(C_Y, fx, fy - d, r, "Y");
    set_round(C_R3, fx - u * 26 / 10, fy - u * 26 / 10, u * 6 / 10, "R3");

    int bw = u * 14 / 10, bh = u * 6 / 10;
    int lx = u * 6 / 10 + bw, rx = w - u * 6 / 10 - bw;
    set_rect(C_LT, lx, u * 6 / 10 + bh, bw, bh, "LT");
    set_rect(C_LB, lx, u * 22 / 10 + bh, bw, bh, "LB");
    set_rect(C_RT, rx, u * 6 / 10 + bh, bw, bh, "RT");
    set_rect(C_RB, rx, u * 22 / 10 + bh, bw, bh, "RB");
    set_rect(C_SELECT, lx, u * 38 / 10 + bh, bw, bh, "SELECT");
    set_rect(C_START, rx, u * 38 / 10 + bh, bw, bh, "START");
    unlock();
}

static int hit(int x, int y) {
    for (int c = 0; c < C_COUNT; c++) {
        const control_t *k = &s_ctl[c];
        int dx = x - k->x, dy = y - k->y;
        if (k->r) {
            /* The stick catches a wider ring, so a thumb that lands a little
             * off still steers. */
            int r = c == C_STICK ? k->r * 16 / 10 : k->r * 12 / 10;
            if (dx * dx + dy * dy <= r * r) return c;
        } else if (abs(dx) <= k->hw && abs(dy) <= k->hh) {
            return c;
        }
    }
    return C_NONE;
}

void touch_event(long id, int phase, int x, int y) {
    lock();
    int slot = -1;
    for (int i = 0; i < MAX_TOUCHES; i++)
        if (s_touch[i].control != C_NONE && s_touch[i].id == id) { slot = i; break; }

    if (phase == TOUCH_DOWN) {
        if (slot < 0)
            for (int i = 0; i < MAX_TOUCHES; i++)
                if (s_touch[i].control == C_NONE) { slot = i; break; }
        if (slot >= 0) {
            int c = s_visible ? hit(x, y) : C_NONE;
            if (c == C_NONE) {
                /* One pointer at a time. */
                for (int i = 0; i < MAX_TOUCHES; i++)
                    if (s_touch[i].control == C_POINTER) s_touch[i].control = C_NONE;
                c = C_POINTER;
            }
            s_touch[slot] = (touch_t){ id, c, x, y };
        }
        s_visible = true;
    } else if (slot >= 0) {
        if (phase == TOUCH_UP) {
            s_touch[slot].control = C_NONE;
        } else {
            s_touch[slot].x = x;
            s_touch[slot].y = y;
        }
    }
    unlock();
}

void touch_cancel_all(void) {
    lock();
    for (int i = 0; i < MAX_TOUCHES; i++) s_touch[i].control = C_NONE;
    unlock();
}

void touch_hide(void) {
    lock();
    s_visible = false;
    for (int i = 0; i < MAX_TOUCHES; i++)
        if (s_touch[i].control != C_POINTER) s_touch[i].control = C_NONE;
    unlock();
}

static void stick_value(int x, int y, int16_t *ox, int16_t *oy) {
    const control_t *k = &s_ctl[C_STICK];
    int dx = x - k->x, dy = y - k->y;
    int len2 = dx * dx + dy * dy;
    if (len2 > k->r * k->r) {
        /* Clamp to the rim without floating point: scale by r / len. */
        int len = 1;
        while (len * len < len2) len++;
        dx = dx * k->r / len;
        dy = dy * k->r / len;
    }
    *ox = (int16_t)(dx * 32767 / k->r);
    *oy = (int16_t)(-dy * 32767 / k->r);     /* win.c reads Y as positive-up */
}

void touch_poll(platform_gamepad_state_t *gp, bool *pointer_down,
                int *pointer_x, int *pointer_y) {
    bool down[C_COUNT] = { false };
    *pointer_down = false;

    lock();
    for (int i = 0; i < MAX_TOUCHES; i++) {
        const touch_t *t = &s_touch[i];
        if (t->control == C_POINTER) {
            *pointer_down = true;
            *pointer_x = t->x;
            *pointer_y = t->y;
        } else if (t->control >= 0) {
            down[t->control] = true;
            if (t->control == C_STICK && s_ctl[C_STICK].r > 0)
                stick_value(t->x, t->y, &gp->left_x, &gp->left_y);
        }
    }
    bool visible = s_visible;
    unlock();

    if (!visible) return;
    gp->connected  = true;
    gp->btn_south  |= down[C_A];
    gp->btn_east   |= down[C_B];
    gp->btn_west   |= down[C_X];
    gp->btn_north  |= down[C_Y];
    gp->btn_lb     |= down[C_LB];
    gp->btn_rb     |= down[C_RB];
    gp->btn_lt     |= down[C_LT];
    gp->btn_rt     |= down[C_RT];
    gp->btn_start  |= down[C_START];
    gp->btn_select |= down[C_SELECT];
    gp->btn_lstick |= down[C_L3];
    gp->btn_rstick |= down[C_R3];
}

/* ── Drawing ───────────────────────────────────────────────── */

/* 5x7 glyphs for the labels, one byte per row, bit 4 leftmost. */
static const struct { char c; uint8_t rows[7]; } k_glyphs[] = {
    { 'A', { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 } },
    { 'B', { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E } },
    { 'C', { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E } },
    { 'E', { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F } },
    { 'R', { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E } },
    { 'T', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'X', { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 } },
    { '3', { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E } },
};

static const uint8_t *glyph(char c) {
    for (size_t i = 0; i < sizeof(k_glyphs) / sizeof(k_glyphs[0]); i++)
        if (k_glyphs[i].c == c) return k_glyphs[i].rows;
    return NULL;
}

typedef struct {
    uint32_t *px;
    int w, h, pitch;
} surface_t;

/* Blend toward white (a > 0) or black (a < 0), a in 1/256ths. */
static void blend(const surface_t *s, int x, int y, int a) {
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    uint32_t *p = &s->px[(size_t)y * (size_t)s->pitch + (size_t)x];
    uint32_t c = *p, out = 0xFF000000u;
    for (int sh = 0; sh < 24; sh += 8) {
        int v = (int)((c >> sh) & 0xFF);
        v = a > 0 ? v + ((255 - v) * a >> 8) : v + (v * a >> 8);
        out |= (uint32_t)v << sh;
    }
    *p = out;
}

static void fill_circle(const surface_t *s, int cx, int cy, int r, int a) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) blend(s, cx + x, cy + y, a);
}

static void ring(const surface_t *s, int cx, int cy, int r, int t, int a) {
    int ri = r - t;
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r * r && d > ri * ri) blend(s, cx + x, cy + y, a);
        }
}

static void fill_rect(const surface_t *s, int x0, int y0, int x1, int y1, int a) {
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) blend(s, x, y, a);
}

static void label(const surface_t *s, int cx, int cy, const char *text, int scale, int a) {
    int n = (int)strlen(text);
    int x = cx - (n * 6 - 1) * scale / 2;
    int y = cy - 7 * scale / 2;
    for (int i = 0; i < n; i++, x += 6 * scale) {
        const uint8_t *g = glyph(text[i]);
        if (!g) continue;
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (g[row] & (0x10 >> col))
                    fill_rect(s, x + col * scale, y + row * scale,
                              x + (col + 1) * scale, y + (row + 1) * scale, a);
    }
}

void touch_draw(uint32_t *dst, int surf_w, int surf_h, int pitch) {
    bool down[C_COUNT] = { false };
    int knob_x = s_ctl[C_STICK].x, knob_y = s_ctl[C_STICK].y;

    lock();
    if (!s_visible || surf_w != s_w || surf_h != s_h) { unlock(); return; }
    for (int i = 0; i < MAX_TOUCHES; i++) {
        const touch_t *t = &s_touch[i];
        if (t->control < 0) continue;
        down[t->control] = true;
        if (t->control == C_STICK) {
            int16_t vx, vy;
            stick_value(t->x, t->y, &vx, &vy);
            knob_x = s_ctl[C_STICK].x + vx * s_ctl[C_STICK].r / 32767;
            knob_y = s_ctl[C_STICK].y - vy * s_ctl[C_STICK].r / 32767;
        }
    }
    unlock();

    surface_t s = { dst, surf_w, surf_h, pitch };
    int u = s_unit;
    int line = u / 16 > 1 ? u / 16 : 1;
    const int dim = -90, lit = 110, edge = 150;

    const control_t *st = &s_ctl[C_STICK];
    fill_circle(&s, st->x, st->y, st->r, dim);
    ring(&s, st->x, st->y, st->r, line, edge);
    fill_circle(&s, knob_x, knob_y, u * 9 / 10, down[C_STICK] ? lit : 60);

    for (int c = C_A; c < C_COUNT; c++) {
        const control_t *k = &s_ctl[c];
        int a = down[c] ? lit : dim;
        int text = (int)strlen(k->label);
        if (k->r) {
            fill_circle(&s, k->x, k->y, k->r, a);
            ring(&s, k->x, k->y, k->r, line, edge);
            int scale = k->r * 9 / 10 / (text > 1 ? 6 * text : 7);
            label(&s, k->x, k->y, k->label, scale > 0 ? scale : 1, 230);
        } else {
            fill_rect(&s, k->x - k->hw, k->y - k->hh, k->x + k->hw, k->y + k->hh, a);
            fill_rect(&s, k->x - k->hw, k->y - k->hh, k->x + k->hw, k->y - k->hh + line, edge);
            fill_rect(&s, k->x - k->hw, k->y + k->hh - line, k->x + k->hw, k->y + k->hh, edge);
            /* One size for every bar, fitted to the longest, SELECT. */
            int scale = k->hw * 3 / 2 / (6 * 6);
            label(&s, k->x, k->y, k->label, scale > 0 ? scale : 1, 230);
        }
    }
}
