/**
 * Ecstatica Platform Implementation — macOS (Cocoa / AppKit)
 *
 * Uses an NSWindow + NSView to blit the game's software-rendered
 * framebuffer to screen. No OpenGL, no Metal — just
 * CGBitmapContext → NSBitmapImageRep → drawInRect.
 *
 * Compile with:  clang -ObjC -framework Cocoa -framework QuartzCore
 */

#import <Cocoa/Cocoa.h>
#import <mach/mach_time.h>
#include <string.h>
#include <stdlib.h>
#include "platform.h"

/* ── Internal structures ── */

#define MAX_KEYS 256

@class ECView;

struct platform_t {
    int fb_width;
    int fb_height;
    int render_width;
    int render_height;
    int scale;
    uint32_t *rgba_buffer;     /* fb_width * fb_height * 4 bytes (RGBA8888) */
    bool      quit_requested;

    /* input */
    bool key_state[MAX_KEYS];
    bool key_prev[MAX_KEYS];
    int  mouse_x;
    int  mouse_y;
    int  mouse_buttons;

    /* timing */
    uint64_t start_mach;
    mach_timebase_info_data_t timebase;

    /* Cocoa objects */
    NSWindow *window;
    ECView   *view;
    NSApplication *app;
    id app_delegate;  /* strong ref — NSApp/NSWindow hold delegate weakly */
};

/* ── Cocoa view that displays the RGBA buffer ── */

@interface ECView : NSView
@property (nonatomic, assign) platform_t *platform;
@end

@implementation ECView

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)canBecomeKeyView      { return YES; }

- (void)drawRect:(NSRect)dirtyRect {
    platform_t *p = self.platform;
    if (!p || !p->rgba_buffer) return;

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(
        p->rgba_buffer,
        p->fb_width, p->fb_height,
        8, p->fb_width * 4,
        cs,
        kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host
    );
    CGColorSpaceRelease(cs);
    if (!ctx) return;

    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    if (!img) return;

    NSGraphicsContext *gc = [NSGraphicsContext currentContext];
    CGContextRef drawCtx = [gc CGContext];
    CGContextSetInterpolationQuality(drawCtx, kCGInterpolationNone);
    CGContextDrawImage(drawCtx, NSRectToCGRect(self.bounds), img);
    CGImageRelease(img);
}

/* ── Keyboard mapping (macOS virtual keycode → platform keycode) ── */
static int macos_vk_to_pkey(unsigned short vk) {
    switch (vk) {
        case 53:  return PKEY_ESCAPE;
        case 36:  return PKEY_RETURN;
        case 49:  return PKEY_SPACE;
        case 126: return PKEY_UP;
        case 125: return PKEY_DOWN;
        case 123: return PKEY_LEFT;
        case 124: return PKEY_RIGHT;
        case 122: return PKEY_F1;
        case 120: return PKEY_F2;
        case 99:  return PKEY_F3;
        case 118: return PKEY_F4;
        case 96:  return PKEY_F5;
        case 97:  return PKEY_F6;
        case 98:  return PKEY_F7;
        case 100: return PKEY_F8;
        case 101: return PKEY_F9;
        case 109: return PKEY_F10;
        case 103: return PKEY_F11;
        case 111: return PKEY_F12;
        case 0:   return PKEY_A;
        case 2:   return PKEY_D;
        case 1:   return PKEY_S;
        case 13:  return PKEY_W;
        case 12:  return PKEY_Q;
        case 14:  return PKEY_E;
        case 34:  return PKEY_I;
        case 35:  return PKEY_P;
        case 8:   return PKEY_C;
        case 46:  return PKEY_M;
        case 6:   return PKEY_Z;
        case 59:  return PKEY_LCTRL;
        case 58:  return PKEY_LALT;
        case 18:  return PKEY_1;
        case 19:  return PKEY_2;
        case 20:  return PKEY_3;
        case 83:  return PKEY_NUM1;
        case 84:  return PKEY_NUM2;
        case 85:  return PKEY_NUM3;
        case 86:  return PKEY_NUM4;
        case 87:  return PKEY_NUM5;
        case 88:  return PKEY_NUM6;
        case 89:  return PKEY_NUM7;
        case 91:  return PKEY_NUM8;
        case 92:  return PKEY_NUM9;
        default:  return -1;
    }
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
    int k = macos_vk_to_pkey(event.keyCode);
    if (k >= 0 && k < MAX_KEYS) {
        if (event.type == NSEventTypeKeyDown)
            self.platform->key_state[k] = true;
        else if (event.type == NSEventTypeKeyUp)
            self.platform->key_state[k] = false;
        return YES;
    }
    return NO;
}

- (void)keyDown:(NSEvent *)event {
    int k = macos_vk_to_pkey(event.keyCode);
    if (k >= 0 && k < MAX_KEYS) self.platform->key_state[k] = true;
}

- (void)keyUp:(NSEvent *)event {
    int k = macos_vk_to_pkey(event.keyCode);
    if (k >= 0 && k < MAX_KEYS) self.platform->key_state[k] = false;
}

- (void)flagsChanged:(NSEvent *)event {
    NSEventModifierFlags flags = event.modifierFlags;
    self.platform->key_state[PKEY_LCTRL]  = (flags & NSEventModifierFlagControl) != 0;
    self.platform->key_state[PKEY_LALT]   = (flags & NSEventModifierFlagOption)  != 0;
    self.platform->key_state[PKEY_LSHIFT] = (flags & NSEventModifierFlagShift)   != 0;
    self.platform->key_state[PKEY_LCMD]   = (flags & NSEventModifierFlagCommand) != 0;
}

- (void)updateMouseWithEvent:(NSEvent *)event {
    platform_t *p = self.platform;
    NSPoint loc = [self convertPoint:event.locationInWindow fromView:nil];
    int rw = p->render_width;
    int rh = p->render_height;
    p->mouse_x = (int)(loc.x * rw / (p->fb_width * p->scale));
    p->mouse_y = rh - 1 - (int)(loc.y * rh / (p->fb_height * p->scale));
    if (p->mouse_x < 0) p->mouse_x = 0;
    if (p->mouse_y < 0) p->mouse_y = 0;
    if (p->mouse_x >= rw) p->mouse_x = rw - 1;
    if (p->mouse_y >= rh) p->mouse_y = rh - 1;
}

- (void)mouseDown:(NSEvent *)event {
    [self updateMouseWithEvent:event];
    self.platform->mouse_buttons |= PMOUSE_LEFT;
}
- (void)mouseUp:(NSEvent *)event {
    [self updateMouseWithEvent:event];
    self.platform->mouse_buttons &= ~PMOUSE_LEFT;
}
- (void)rightMouseDown:(NSEvent *)event {
    [self updateMouseWithEvent:event];
    self.platform->mouse_buttons |= PMOUSE_RIGHT;
}
- (void)rightMouseUp:(NSEvent *)event {
    [self updateMouseWithEvent:event];
    self.platform->mouse_buttons &= ~PMOUSE_RIGHT;
}
- (void)mouseMoved:(NSEvent *)event {
    [self updateMouseWithEvent:event];
}
- (void)mouseDragged:(NSEvent *)event {
    [self updateMouseWithEvent:event];
}
- (void)rightMouseDragged:(NSEvent *)event {
    [self updateMouseWithEvent:event];
}

@end

/* ── App delegate to handle window close ── */

@interface ECAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (nonatomic, assign) platform_t *platform;
@end

@implementation ECAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if (self.platform) self.platform->quit_requested = true;
    return NSTerminateCancel;  /* let the game loop handle shutdown */
}

- (void)windowWillClose:(NSNotification *)notification {
    if (self.platform) self.platform->quit_requested = true;
}

@end

/* ── Platform API implementation ── */

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    platform_t *p = (platform_t *)calloc(1, sizeof(platform_t));
    if (!p) return NULL;

    p->fb_width      = fb_width;
    p->fb_height     = fb_height;
    p->render_width  = fb_width;
    p->render_height = fb_height;
    p->scale         = scale;
    p->rgba_buffer = (uint32_t *)calloc(fb_width * fb_height, sizeof(uint32_t));

    mach_timebase_info(&p->timebase);
    p->start_mach = mach_absolute_time();

    /* Initialize Cocoa application */
    @autoreleasepool {
        p->app = [NSApplication sharedApplication];
        [p->app setActivationPolicy:NSApplicationActivationPolicyRegular];

        ECAppDelegate *delegate = [[ECAppDelegate alloc] init];
        delegate.platform = p;
        p->app_delegate = delegate;
        [p->app setDelegate:delegate];

        /* Create window */
        NSRect frame = NSMakeRect(100, 100, fb_width * scale, fb_height * scale);
        NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable;
        p->window = [[NSWindow alloc] initWithContentRect:frame
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
        [p->window setTitle:[NSString stringWithUTF8String:title]];
        [p->window setAcceptsMouseMovedEvents:YES];
        [p->window setDelegate:delegate];

        /* Create custom view */
        ECView *view = [[ECView alloc] initWithFrame:frame];
        view.platform = p;
        p->view = view;

        [p->window setContentView:view];
        [p->window makeFirstResponder:view];
        [p->window makeKeyAndOrderFront:nil];

        [p->app activateIgnoringOtherApps:YES];

        /* Run just enough to show the window */
        NSEvent *event;
        while ((event = [p->app nextEventMatchingMask:NSEventMaskAny
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES])) {
            [p->app sendEvent:event];
        }
    }

    return p;
}

void platform_set_render_size(platform_t *p, int w, int h) {
    if (!p) return;
    p->render_width  = w;
    p->render_height = h;
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || !palette) return;

    int rw = p->render_width;
    int rh = p->render_height;
    int fw = p->fb_width;
    int fh = p->fb_height;

    if (rw == fw && rh == fh) {
        int total = fw * fh;
        for (int i = 0; i < total; i++) {
            uint8_t idx = framebuffer[i];
            uint8_t r = (palette[idx * 3 + 0] & 0x3F) << 2;
            uint8_t g = (palette[idx * 3 + 1] & 0x3F) << 2;
            uint8_t b = (palette[idx * 3 + 2] & 0x3F) << 2;
            p->rgba_buffer[i] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
    } else {
        for (int y = 0; y < fh; y++) {
            int sy = y * rh / fh;
            for (int x = 0; x < fw; x++) {
                int sx = x * rw / fw;
                uint8_t idx = framebuffer[sy * rw + sx];
                uint8_t r = (palette[idx * 3 + 0] & 0x3F) << 2;
                uint8_t g = (palette[idx * 3 + 1] & 0x3F) << 2;
                uint8_t b = (palette[idx * 3 + 2] & 0x3F) << 2;
                p->rgba_buffer[y * fw + x] = (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
        }
    }

    @autoreleasepool {
        [p->view setNeedsDisplay:YES];
    }
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    if (!p || !framebuffer) return;

    memcpy(p->rgba_buffer, framebuffer, p->fb_width * p->fb_height * 4);

    @autoreleasepool {
        [p->view setNeedsDisplay:YES];
    }
}

bool platform_pump_events(platform_t *p) {
    if (!p) return false;

    /* Save previous key state for edge detection */
    memcpy(p->key_prev, p->key_state, sizeof(p->key_prev));

    @autoreleasepool {
        NSEvent *event;
        while ((event = [p->app nextEventMatchingMask:NSEventMaskAny
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES])) {
            [p->app sendEvent:event];
        }

        /* Force display update */
        [p->view displayIfNeeded];
    }

    return !p->quit_requested;
}

bool platform_key_down(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= MAX_KEYS) return false;
    return p->key_state[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= MAX_KEYS) return false;
    return p->key_state[keycode] && !p->key_prev[keycode];
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y) {
    if (!p) return 0;
    if (out_x) *out_x = p->mouse_x;
    if (out_y) *out_y = p->mouse_y;
    return p->mouse_buttons;
}

uint32_t platform_ticks(platform_t *p) {
    static uint64_t s_start_mach = 0;
    static mach_timebase_info_data_t s_timebase = {0, 0};
    uint64_t ref_start;
    mach_timebase_info_data_t *tb;

    if (p) {
        ref_start = p->start_mach;
        tb = &p->timebase;
    } else {
        /* Fallback for NULL context: use static global time base */
        if (!s_timebase.denom) {
            mach_timebase_info(&s_timebase);
            s_start_mach = mach_absolute_time();
        }
        ref_start = s_start_mach;
        tb = &s_timebase;
    }
    uint64_t elapsed = mach_absolute_time() - ref_start;
    /* Convert to milliseconds */
    return (uint32_t)((elapsed * tb->numer) / (tb->denom * 1000000ULL));
}

void platform_delay(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void platform_shutdown(platform_t *p) {
    if (!p) return;
    @autoreleasepool {
        if (p->window) {
            [p->window close];
            p->window = nil;
        }
    }
    if (p->rgba_buffer) {
        free(p->rgba_buffer);
        p->rgba_buffer = NULL;
    }
    free(p);
}

void platform_set_title(platform_t *p, const char *title) {
    if (!p || !title) return;
    @autoreleasepool {
        [p->window setTitle:[NSString stringWithUTF8String:title]];
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Audio backend — AudioUnit output + software mixer
 * ══════════════════════════════════════════════════════════════ */

#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>
#include <pthread.h>

#define AUDIO_MAX_VOICES 16
#define AUDIO_OUT_RATE   44100
#define AUDIO_OUT_CH     2

typedef struct {
    const uint8_t *data;   /* unsigned 8-bit PCM, mono */
    int   length;          /* bytes */
    int   rate;            /* source sample rate, Hz */
    int   volume;          /* 0..127 */
    int   pan;             /* -128..127 */
    double pos;            /* current playback position in source samples (float) */
    bool  active;
    bool  loop;
} audio_voice_t;

static audio_voice_t s_voices[AUDIO_MAX_VOICES];
static pthread_mutex_t s_audio_mutex = PTHREAD_MUTEX_INITIALIZER;
static AudioUnit s_audio_unit;
static bool s_audio_ready = false;
static float s_master_sfx_volume = 0.9f;
static float s_master_music_volume = 0.8f;

static OSStatus audio_render_cb(void *inRefCon,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList *ioData) {
    (void)inRefCon; (void)ioActionFlags; (void)inTimeStamp; (void)inBusNumber;

    float *out = (float *)ioData->mBuffers[0].mData;
    memset(out, 0, sizeof(float) * inNumberFrames * AUDIO_OUT_CH);

    pthread_mutex_lock(&s_audio_mutex);
    for (int v = 0; v < AUDIO_MAX_VOICES; ++v) {
        audio_voice_t *voice = &s_voices[v];
        if (!voice->active) continue;

        double step = (double)voice->rate / (double)AUDIO_OUT_RATE;
        float db    = (float)(voice->volume - 127) * 0.25f;
        float vol   = powf(10.0f, db / 20.0f) * s_master_sfx_volume;
        float pan_norm = (float)voice->pan / 127.0f;   /* -1..+1 */
        float l_gain = vol * (pan_norm > 0 ? (1.0f - pan_norm) : 1.0f);
        float r_gain = vol * (pan_norm < 0 ? (1.0f + pan_norm) : 1.0f);

        for (UInt32 f = 0; f < inNumberFrames; ++f) {
            int idx = (int)voice->pos;
            if (idx >= voice->length) {
                if (voice->loop) {
                    voice->pos = 0.0;
                    idx = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }
            float s = ((float)voice->data[idx] - 128.0f) / 128.0f;
            out[f * AUDIO_OUT_CH + 0] += s * l_gain;
            out[f * AUDIO_OUT_CH + 1] += s * r_gain;
            voice->pos += step;
        }
    }
    pthread_mutex_unlock(&s_audio_mutex);

    /* Simple clip */
    for (UInt32 i = 0; i < inNumberFrames * AUDIO_OUT_CH; ++i) {
        if (out[i] >  1.0f) out[i] =  1.0f;
        if (out[i] < -1.0f) out[i] = -1.0f;
    }
    return noErr;
}

void platform_audio_init(void) {
    if (s_audio_ready) return;

    AudioComponentDescription desc = {
        .componentType         = kAudioUnitType_Output,
        .componentSubType      = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
    };
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (!comp) return;
    if (AudioComponentInstanceNew(comp, &s_audio_unit) != noErr) return;

    AudioStreamBasicDescription fmt = {
        .mSampleRate       = AUDIO_OUT_RATE,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
        .mFramesPerPacket  = 1,
        .mChannelsPerFrame = AUDIO_OUT_CH,
        .mBitsPerChannel   = 32,
        .mBytesPerPacket   = 4 * AUDIO_OUT_CH,
        .mBytesPerFrame    = 4 * AUDIO_OUT_CH,
    };
    AudioUnitSetProperty(s_audio_unit, kAudioUnitProperty_StreamFormat,
                         kAudioUnitScope_Input, 0, &fmt, sizeof(fmt));

    AURenderCallbackStruct cb = { .inputProc = audio_render_cb };
    AudioUnitSetProperty(s_audio_unit, kAudioUnitProperty_SetRenderCallback,
                         kAudioUnitScope_Input, 0, &cb, sizeof(cb));

    if (AudioUnitInitialize(s_audio_unit) != noErr) return;
    if (AudioOutputUnitStart(s_audio_unit) != noErr)  return;
    s_audio_ready = true;
}

int platform_audio_play_pcm(const void *data, int length, int rate,
                            int volume, int pan, bool loop) {
    if (!s_audio_ready || !data || length <= 0) return -1;
    if (rate <= 0) rate = 22050;

    pthread_mutex_lock(&s_audio_mutex);
    int slot = -1;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        if (!s_voices[i].active) { slot = i; break; }
    }
    if (slot < 0) slot = 0;   /* steal voice 0 */
    s_voices[slot].data   = (const uint8_t *)data;
    s_voices[slot].length = length;
    s_voices[slot].rate   = rate;
    s_voices[slot].volume = volume;
    s_voices[slot].pan    = pan;
    s_voices[slot].pos    = 0.0;
    s_voices[slot].loop   = loop;
    s_voices[slot].active = true;
    pthread_mutex_unlock(&s_audio_mutex);
    return slot;
}

void platform_audio_stop_voice(int slot) {
    if (slot < 0 || slot >= AUDIO_MAX_VOICES) return;
    pthread_mutex_lock(&s_audio_mutex);
    s_voices[slot].active = false;
    pthread_mutex_unlock(&s_audio_mutex);
}

void platform_audio_stop_all(void) {
    pthread_mutex_lock(&s_audio_mutex);
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) s_voices[i].active = false;
    pthread_mutex_unlock(&s_audio_mutex);
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready) return;
    AudioOutputUnitStop(s_audio_unit);
    AudioUnitUninitialize(s_audio_unit);
    AudioComponentInstanceDispose(s_audio_unit);
    s_audio_ready = false;
}

/* ── MIDI music playback via AVMIDIPlayer + built-in GM synth ── */
#import <AVFoundation/AVFoundation.h>

static AVMIDIPlayer *s_midi_player = nil;
static NSData *s_midi_data = nil;
static bool s_midi_loop = false;

int platform_midi_play(const void *smf_data, int length, bool loop) {
    if (!smf_data || length <= 0) return -1;

    /* Stop any previous tune first. */
    platform_midi_stop();

    NSData *data = [NSData dataWithBytes:smf_data length:length];
    NSError *err = nil;
    AVMIDIPlayer *player = [[AVMIDIPlayer alloc] initWithData:data
                                                soundBankURL:nil
                                                       error:&err];
    if (!player || err) {
        NSLog(@"[MIDI] init failed: %@", err);
        return -1;
    }
    [player prepareToPlay];
    s_midi_player = player;
    s_midi_data = data;
    s_midi_loop = loop;
    [player play:^{
        /* Called on completion. Loop if requested. */
        if (s_midi_loop && s_midi_player == player) {
            player.currentPosition = 0.0;
            [player play:nil];
        }
    }];
    return 0;
}

void platform_midi_stop(void) {
    if (s_midi_player) {
        [s_midi_player stop];
        s_midi_player = nil;
        s_midi_data = nil;
    }
}

void platform_set_sfx_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_master_sfx_volume = vol;
}

void platform_set_music_volume(float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_master_music_volume = vol;
}
