/**
 * platforms/dos.c
 *
 * DOS backend (Open Watcom + DOS/4GW), for the hardware the games shipped on.
 *
 *   Video     VGA mode 13h at 320x200; VESA 2.0 linear framebuffer above that,
 *             mapped into the flat address space through DPMI.
 *   Keyboard  INT 9 handler. The engine's PKEY_* values are already DOS
 *             scancodes, so the ISR indexes the key table directly.
 *   Mouse     INT 33h.
 *   Timing    BIOS tick plus a live PIT channel-0 read for the fraction
 *             within it, rather than the bare 18.2 Hz tick.
 *   Audio     not implemented; see the note above platform_audio_init().
 *
 * The 8-bit indexed framebuffer the engine hands to platform_blit() is exactly
 * what the hardware wants, so the blit is a copy with no palette expansion and
 * no scaling — unlike every other backend here.
 */

#ifdef __WATCOMC__

#include "../platform.h"
#include "../types.h"

#include <conio.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── State ──────────────────────────────────────────────────── */

struct platform_t {
    int      fb_width;
    int      fb_height;
    uint8_t *vram;          /* mode 13h: 0xA0000. VESA: mapped LFB. */
    int      vesa_mode;     /* 0 when in mode 13h */
    uint32_t lfb_phys;
    uint16_t lfb_sel;       /* DPMI selector for the mapped LFB, 0 if none */
};

static platform_t  s_plat;
static bool        s_keys[256];
static bool        s_keys_prev[256];
static bool        s_keys_latched[256];   /* sticky until read */
static bool        s_mouse_ok;
static uint64_t    s_start_ticks;

static uint64_t pit_now(void);   /* defined with the timing code below */

/* ── Keyboard ───────────────────────────────────────────────── */

static void (__interrupt __far *s_old_int9)(void);
static volatile int s_extended;

/* Reading port 0x60 does not consume the byte, so the BIOS handler we chain to
 * still sees it. Chaining keeps BIOS keyboard state and Ctrl-Alt-Del working. */
static void __interrupt __far kbd_isr(void)
{
    unsigned char code = (unsigned char)inp(0x60);

    if (code == 0xE0) {
        s_extended = 1;
    } else {
        int released = (code & 0x80) != 0;
        int sc       = code & 0x7F;

        /* Extended keys are stored at sc|0x80 — the same encoding PKEY_UP
         * (0xC8 = 0x48|0x80) and friends already use in platform.h. */
        if (s_extended) sc |= 0x80;
        s_extended = 0;

        s_keys[sc] = !released;
        if (!released) s_keys_latched[sc] = true;
    }

    _chain_intr(s_old_int9);
}

static void kbd_install(void)
{
    memset((void *)s_keys, 0, sizeof(s_keys));
    memset((void *)s_keys_prev, 0, sizeof(s_keys_prev));
    memset((void *)s_keys_latched, 0, sizeof(s_keys_latched));
    s_old_int9 = _dos_getvect(9);
    _dos_setvect(9, kbd_isr);
}

static void kbd_remove(void)
{
    if (s_old_int9) {
        _dos_setvect(9, s_old_int9);
        s_old_int9 = NULL;
    }
}

/* ── Video ──────────────────────────────────────────────────── */

/* DPMI real-mode call structure (function 0300h). */
#pragma pack(push, 1)
typedef struct {
    uint32_t edi, esi, ebp, reserved, ebx, edx, ecx, eax;
    uint16_t flags, es, ds, fs, gs, ip, cs, sp, ss;
} rminfo_t;
#pragma pack(pop)

/* Issue a real-mode interrupt through the DPMI host.
 *
 * int386x() cannot be used for the BIOS calls below: in the flat model its
 * SREGS carry protected-mode selectors, which real-mode BIOS code cannot
 * dereference — that is a general protection fault, not a wrong answer. DPMI
 * 0300h is the supported route, and takes a real-mode register image. INT 31h
 * itself is fine through int386x, because the DPMI host is not real-mode code
 * and ES:EDI there is an ordinary protected-mode pointer to the image. */
static int rm_int(int intno, rminfo_t *rmi)
{
    union REGS   r;
    struct SREGS sr;

    segread(&sr);
    memset(&r, 0, sizeof(r));
    r.w.ax  = 0x0300;
    r.h.bl  = (unsigned char)intno;
    r.h.bh  = 0;
    r.w.cx  = 0;
    r.x.edi = (uint32_t)rmi;
    sr.es   = sr.ds;
    int386x(0x31, &r, &r, &sr);
    return !r.w.cflag;
}

static void set_bios_video_mode(int mode)
{
    rminfo_t rmi;
    memset(&rmi, 0, sizeof(rmi));
    rmi.eax = (uint32_t)mode;
    rm_int(0x10, &rmi);
}

static void set_mode13(void)    { set_bios_video_mode(0x0013); }
static void set_text_mode(void) { set_bios_video_mode(0x0003); }

/* VBE mode info, only the fields used here. */
#pragma pack(push, 1)
typedef struct {
    uint16_t attributes;
    uint8_t  win_a, win_b;
    uint16_t granularity, win_size;
    uint16_t seg_a, seg_b;
    uint32_t win_func;
    uint16_t pitch;
    uint16_t width, height;
    uint8_t  w_char, y_char, planes, bpp, banks;
    uint8_t  memory_model, bank_size, image_pages, reserved0;
    uint8_t  red_mask, red_pos, green_mask, green_pos;
    uint8_t  blue_mask, blue_pos, rsv_mask, rsv_pos, dcm_info;
    uint32_t phys_base;         /* linear framebuffer */
    uint32_t reserved1;
    uint16_t reserved2;
} vbe_mode_info_t;
#pragma pack(pop)

/* Allocate a block of conventional memory through DPMI (int 31h AX=0100h),
 * because real-mode BIOS calls cannot write into our protected-mode heap. */
static void *dos_alloc_real(int paras, uint16_t *sel, uint16_t *seg)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0100;
    r.w.bx = (uint16_t)paras;
    int386(0x31, &r, &r);
    if (r.w.cflag) return NULL;
    *seg = r.w.ax;
    *sel = r.w.dx;
    return (void *)((uint32_t)r.w.ax << 4);
}

static void dos_free_real(uint16_t sel)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0101;
    r.w.dx = sel;
    int386(0x31, &r, &r);
}

/* Map a physical region into the flat address space (DPMI AX=0800h). Under
 * DOS/4GW the returned linear address is directly usable. */
static uint8_t *dpmi_map_physical(uint32_t phys, uint32_t len)
{
    union REGS r;
    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0800;
    r.w.bx = (uint16_t)(phys >> 16);
    r.w.cx = (uint16_t)(phys & 0xFFFF);
    r.w.si = (uint16_t)(len >> 16);
    r.w.di = (uint16_t)(len & 0xFFFF);
    int386(0x31, &r, &r);
    if (r.w.cflag) return NULL;
    return (uint8_t *)(((uint32_t)r.w.bx << 16) | r.w.cx);
}

/* Find a VBE mode matching w*h*8bpp with a linear framebuffer. Returns the
 * mode number, or 0 if the card cannot do it. */
static int vesa_find_mode(int w, int h, uint32_t *phys_out, uint16_t *pitch_out)
{
    rminfo_t    rmi;
    uint16_t    sel, seg;
    uint8_t    *buf;
    uint16_t   *modes;
    uint16_t    mode_list[256];
    int         count = 0;
    int         found = 0;
    int         i;

    buf = (uint8_t *)dos_alloc_real(512 / 16 + 1, &sel, &seg);
    if (!buf) return 0;

    /* VBE 2.0 controller info — ask for the VBE2 block so mode list is valid. */
    memset(buf, 0, 512);
    memcpy(buf, "VBE2", 4);
    memset(&rmi, 0, sizeof(rmi));
    rmi.eax = 0x4F00;
    rmi.es  = seg;
    rmi.edi = 0;
    if (!rm_int(0x10, &rmi) || (rmi.eax & 0xFFFF) != 0x004F ||
        memcmp(buf, "VESA", 4) != 0) {
        dos_free_real(sel);
        return 0;
    }

    /* The mode list lives in the card's own memory and 4F01h overwrites our
     * buffer, so copy it out before probing anything. */
    {
        uint32_t mp = *(uint32_t *)(buf + 14);   /* VbeFarPtr to mode list */
        modes = (uint16_t *)((((mp >> 16) & 0xFFFF) << 4) + (mp & 0xFFFF));
        while (count < 256 && modes[count] != 0xFFFF) {
            mode_list[count] = modes[count];
            count++;
        }
    }

    for (i = 0; i < count; i++) {
        vbe_mode_info_t *mi = (vbe_mode_info_t *)buf;
        memset(buf, 0, 256);
        memset(&rmi, 0, sizeof(rmi));
        rmi.eax = 0x4F01;
        rmi.ecx = mode_list[i];
        rmi.es  = seg;
        rmi.edi = 0;
        if (!rm_int(0x10, &rmi) || (rmi.eax & 0xFFFF) != 0x004F) continue;

        /* bit 0 = supported, bit 7 = linear framebuffer available */
        if (!(mi->attributes & 0x01)) continue;
        if (!(mi->attributes & 0x80)) continue;
        if (mi->bpp != 8) continue;
        if (mi->width != w || mi->height != h) continue;

        *phys_out  = mi->phys_base;
        *pitch_out = mi->pitch ? mi->pitch : (uint16_t)w;
        found = mode_list[i];
        break;
    }

    dos_free_real(sel);
    return found;
}

static int vesa_set_mode(int mode)
{
    rminfo_t rmi;
    memset(&rmi, 0, sizeof(rmi));
    rmi.eax = 0x4F02;
    rmi.ebx = (uint32_t)(mode | 0x4000);   /* bit 14: use linear framebuffer */
    if (!rm_int(0x10, &rmi)) return 0;
    return (rmi.eax & 0xFFFF) == 0x004F;
}

/* Pick the hardware mode for the engine's current render size. 320x200 is
 * plain VGA; anything larger needs VESA. */
static bool apply_video_mode(platform_t *p, int w, int h)
{
    if (w == 320 && h == 200) {
        set_mode13();
        p->vram      = (uint8_t *)0xA0000;
        p->vesa_mode = 0;
        p->fb_width  = w;
        p->fb_height = h;
        return true;
    }

    {
        uint32_t phys  = 0;
        uint16_t pitch = 0;
        int mode = vesa_find_mode(w, h, &phys, &pitch);
        if (!mode) return false;
        if (!vesa_set_mode(mode)) return false;

        p->vram = dpmi_map_physical(phys, (uint32_t)pitch * (uint32_t)h);
        if (!p->vram) return false;

        p->vesa_mode = mode;
        p->lfb_phys  = phys;
        p->fb_width  = w;
        p->fb_height = h;
        return true;
    }
}

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale)
{
    (void)title; (void)scale;

    memset(&s_plat, 0, sizeof(s_plat));

    if (!apply_video_mode(&s_plat, fb_width, fb_height)) {
        set_text_mode();
        fprintf(stderr, "No %dx%d 8-bit video mode with a linear framebuffer.\n",
                fb_width, fb_height);
        return NULL;
    }

    kbd_install();

    /* INT 33h reset: AX=0 returns AX=FFFFh when a driver is present. */
    {
        union REGS r;
        memset(&r, 0, sizeof(r));
        r.w.ax = 0x0000;
        int386(0x33, &r, &r);
        s_mouse_ok = (r.w.ax == 0xFFFF);
    }

    s_start_ticks = pit_now();
    return &s_plat;
}

void platform_set_render_size(platform_t *p, int w, int h)
{
    if (!p) return;
    if (p->fb_width == w && p->fb_height == h) return;
    apply_video_mode(p, w, h);
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette)
{
    int n;

    if (!p || !p->vram || !framebuffer) return;

    if (palette) {
        /* VGA DAC, 6 bits per channel — which is the engine's own format, so
         * the values go out untouched. */
        int i;
        outp(0x3C8, 0);
        for (i = 0; i < 256 * 3; i++)
            outp(0x3C9, palette[i]);
    }

    n = p->fb_width * p->fb_height;
    memcpy(p->vram, framebuffer, (size_t)n);
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer)
{
    /* Truecolour path — the engine only uses this for tooling, and there is no
     * truecolour mode set up here. */
    (void)p; (void)framebuffer;
}

void platform_shutdown(platform_t *p)
{
    (void)p;
    kbd_remove();
    set_text_mode();
}

void platform_set_title(platform_t *p, const char *title)
{
    (void)p; (void)title;   /* no window manager */
}

/* ── Input ──────────────────────────────────────────────────── */

bool platform_pump_events(platform_t *p)
{
    (void)p;
    memcpy((void *)s_keys_prev, (const void *)s_keys, sizeof(s_keys));
    return true;
}

bool platform_key_down(platform_t *p, int keycode)
{
    (void)p;
    if (keycode < 0 || keycode > 255) return false;
    return s_keys[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode)
{
    (void)p;
    if (keycode < 0 || keycode > 255) return false;
    return s_keys[keycode] && !s_keys_prev[keycode];
}

bool platform_key_hit(platform_t *p, int keycode)
{
    (void)p;
    if (keycode < 0 || keycode > 255) return false;
    if (!s_keys_latched[keycode]) return false;
    s_keys_latched[keycode] = false;
    return true;
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y)
{
    union REGS r;

    if (out_x) *out_x = 0;
    if (out_y) *out_y = 0;
    if (!s_mouse_ok || !p) return 0;

    memset(&r, 0, sizeof(r));
    r.w.ax = 0x0003;
    int386(0x33, &r, &r);

    /* Mode 13h reports X in a 0..639 space regardless of the 320-pixel mode. */
    if (out_x) *out_x = (p->fb_width == 320) ? (r.w.cx >> 1) : r.w.cx;
    if (out_y) *out_y = r.w.dx;
    return r.w.bx & 0x07;
}

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state)
{
    (void)p;
    if (state) memset(state, 0, sizeof(*state));
}

/* ── Timing ─────────────────────────────────────────────────────
 * The 18.2 Hz BIOS tick alone is 55 ms of granularity, far too coarse to drive
 * a frame loop. Rather than reprogram the PIT — which would upset the DOS
 * time-of-day count — this reads the BIOS tick and then latches channel 0's
 * live countdown for the fraction within that tick, giving sub-microsecond
 * resolution while leaving the timer rate alone.
 */

#define PIT_HZ 1193182UL

static uint64_t pit_now(void)
{
    uint32_t t0, t1;
    uint16_t count;
    unsigned lo, hi;

    /* The tick counter can roll between the two reads; retry if it moved. */
    do {
        t0 = *(volatile uint32_t *)0x46C;

        _disable();
        outp(0x43, 0x00);               /* latch channel 0 */
        lo = (unsigned)inp(0x40);
        hi = (unsigned)inp(0x40);
        _enable();
        count = (uint16_t)((hi << 8) | lo);

        t1 = *(volatile uint32_t *)0x46C;
    } while (t0 != t1);

    /* Channel 0 counts down from 65536, so elapsed within the tick is the
     * complement. */
    return ((uint64_t)t0 << 16) + (uint64_t)(65535u - count);
}

uint32_t platform_ticks(platform_t *p)
{
    (void)p;
    return (uint32_t)(((pit_now() - s_start_ticks) * 1000ULL) / PIT_HZ);
}

void platform_delay(uint32_t ms)
{
    uint64_t target = pit_now() + ((uint64_t)ms * PIT_HZ) / 1000ULL;
    while (pit_now() < target)
        ;
}

/* ── Audio ──────────────────────────────────────────────────────
 * Not implemented. Sound Blaster DMA output and MPU-401/OPL3 music are the
 * remaining piece of this backend; the engine runs silent until they land.
 * Every entry point is a no-op rather than a stub that pretends to succeed,
 * so nothing upstream waits on a voice that will never play.
 */

void platform_audio_init(void) { }

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop)
{
    (void)data; (void)length; (void)rate; (void)volume; (void)pan; (void)loop;
    return -1;
}

void platform_audio_stop_voice(int slot) { (void)slot; }
void platform_audio_stop_all(void) { }
void platform_audio_shutdown(void) { }

int platform_midi_play(const void *smf_data, int length, bool loop)
{
    (void)smf_data; (void)length; (void)loop;
    return -1;
}

void platform_midi_stop(void) { }

void platform_set_sfx_volume(int vol) { (void)vol; }
void platform_set_music_volume(int vol) { (void)vol; }

/* ── Capabilities ───────────────────────────────────────────── */

void platform_early_init(void)
{
    /* Data directory is the working directory, as on the other desktops. */
}

int platform_save_slot_count(void)
{
    return 10;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version)
{
    /* 8.3 names: SAVE0.E1 / SAVE0.E2 */
    snprintf(buf, bufsz, "SAVE%d.E%d", slot, game_version);
}

void platform_save_prepare(void)
{
}

#endif /* __WATCOMC__ */
