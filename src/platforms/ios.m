/**
 * platforms/ios.m
 *
 * iOS and iPadOS backend (UIKit), built by platforms/ios/CMakeLists.txt.
 *
 *   Threads   UIKit owns the main thread, so the engine's main() (compiled as
 *             ecs_main) runs on a thread of its own with its blocking loops.
 *             The main thread shows frames and receives input.
 *   Video     The game thread expands each frame with screen.c into one of
 *             three RGBA buffers sized to the view's aspect at up to 960 lines,
 *             with the on-screen pad drawn on top; a CADisplayLink on the main
 *             thread hands the newest to the view's layer.
 *   Input     Multi-touch (on-screen pad and pointer), hardware keyboards
 *             (UIPress key codes), GameController pads.
 *   Audio     RemoteIO audio unit pulling mixer.c.
 *
 * Games live in the app's Documents folder (Documents/e1, Documents/e2),
 * which the Files app shows under "On My iPhone/iPad"; the library screen can
 * also import a folder from anywhere the Files app reaches.
 */

#import <UIKit/UIKit.h>
#import <GameController/GameController.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "../platform.h"
#include "../asm_f.h"
#include "../types.h"
#include "mixer.h"
#include "screen.h"
#include "touch_controls.h"

#include <mach/mach_time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

int ecs_main(int argc, char *argv[]);

#define PKEY_TABLE_SIZE 256
#define MAX_BUFFER_H 960
#define GAME_STACK (8 * 1024 * 1024)

struct platform_t {
    int render_w, render_h;
    int scale_mode;

    volatile bool keys_now[PKEY_TABLE_SIZE];
    bool          keys_prev[PKEY_TABLE_SIZE];
    volatile bool keys_latch[PKEY_TABLE_SIZE];

    int mouse_x, mouse_y;
    int mouse_buttons;

    uint64_t start;
    mach_timebase_info_data_t timebase;
};

static platform_t g_plat;
static char s_game_dir[1024];
static char s_documents[1024];

/* ── Frames ──────────────────────────────────────────────────
 * The surface size follows the view (set on the main thread); the game thread
 * renders into whichever of three buffers the display link is not showing. */

typedef struct {
    uint32_t *px;
    int w, h, cap;
} frame_t;

static frame_t s_frames[3];
static int s_latest = -1, s_reading = -1;
static unsigned s_seq, s_shown_seq;
static int s_surf_w = 640, s_surf_h = 480;
static pthread_mutex_t s_frame_lock = PTHREAD_MUTEX_INITIALIZER;

/* Background: the game thread waits here instead of drawing. */
static pthread_mutex_t s_active_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  s_active_cond = PTHREAD_COND_INITIALIZER;
static bool s_active = true;

/* ── Keyboard: UIKeyboardHIDUsage to PKEY ── */

static int hid_to_pkey(long hid) {
    if (hid >= 0x04 && hid <= 0x1D) {
        static const int letters[26] = {
            PKEY_A, PKEY_B, PKEY_C, PKEY_D, PKEY_E, PKEY_F, PKEY_G, PKEY_H, PKEY_I, -1,
            -1, PKEY_L, PKEY_M, PKEY_N, PKEY_O, PKEY_P, PKEY_Q, PKEY_R, PKEY_S, PKEY_T,
            -1, PKEY_V, PKEY_W, PKEY_X, -1, PKEY_Z,
        };
        return letters[hid - 0x04];
    }
    if (hid >= 0x3A && hid <= 0x43) return PKEY_F1 + (int)(hid - 0x3A);   /* F1-F10 */
    if (hid >= 0x59 && hid <= 0x61) {
        static const int pad[9] = { PKEY_NUM1, PKEY_NUM2, PKEY_NUM3, PKEY_NUM4, PKEY_NUM5,
                                    PKEY_NUM6, PKEY_NUM7, PKEY_NUM8, PKEY_NUM9 };
        return pad[hid - 0x59];
    }
    switch (hid) {
    case 0x29: return PKEY_ESCAPE;
    case 0x28: case 0x58: return PKEY_RETURN;
    case 0x2C: return PKEY_SPACE;
    case 0x52: return PKEY_UP;
    case 0x51: return PKEY_DOWN;
    case 0x50: return PKEY_LEFT;
    case 0x4F: return PKEY_RIGHT;
    case 0x44: return PKEY_F11;
    case 0x45: return PKEY_F12;
    case 0x1E: return PKEY_1;
    case 0x1F: return PKEY_2;
    case 0x20: return PKEY_3;
    case 0xE0: case 0xE4: return PKEY_LCTRL;
    case 0xE2: case 0xE6: return PKEY_LALT;
    case 0xE1: return PKEY_LSHIFT;
    case 0xE5: return PKEY_RSHIFT;
    case 0xE3: return PKEY_LCMD;
    case 0x2B: return PKEY_TAB;
    case 0x2D: return PKEY_MINUS;
    case 0x2E: return PKEY_EQUALS;
    case 0x2F: return PKEY_LBRACKET;
    case 0x30: return PKEY_RBRACKET;
    case 0x36: return PKEY_COMMA;
    case 0x37: return PKEY_PERIOD;
    case 0x4A: return PKEY_HOME;
    case 0x4B: return PKEY_PGUP;
    case 0x4D: return PKEY_END;
    case 0x4E: return PKEY_PGDN;
    default: return -1;
    }
}

/* ── Game view ───────────────────────────────────────────── */

@interface ECGameView : UIView
@end

@implementation ECGameView

- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.multipleTouchEnabled = YES;
        self.backgroundColor = UIColor.blackColor;
        self.layer.magnificationFilter = kCAFilterNearest;
        self.layer.contentsGravity = kCAGravityResize;
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CGFloat scale = self.window ? self.window.screen.scale : UIScreen.mainScreen.scale;
    int w = (int)(self.bounds.size.width * scale);
    int h = (int)(self.bounds.size.height * scale);
    if (w <= 0 || h <= 0) return;
    int div = (h + MAX_BUFFER_H - 1) / MAX_BUFFER_H;
    if (div < 1) div = 1;
    pthread_mutex_lock(&s_frame_lock);
    s_surf_w = w / div;
    s_surf_h = h / div;
    pthread_mutex_unlock(&s_frame_lock);
    touch_layout(w / div, h / div);
}

- (void)touchesWith:(NSSet<UITouch *> *)touches phase:(int)phase {
    pthread_mutex_lock(&s_frame_lock);
    int sw = s_surf_w, sh = s_surf_h;
    pthread_mutex_unlock(&s_frame_lock);
    CGSize b = self.bounds.size;
    for (UITouch *t in touches) {
        CGPoint pt = [t locationInView:self];
        touch_event((long)(uintptr_t)(__bridge void *)t, phase,
                    (int)(pt.x * sw / b.width), (int)(pt.y * sh / b.height));
    }
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesWith:touches phase:TOUCH_DOWN];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesWith:touches phase:TOUCH_MOVE];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesWith:touches phase:TOUCH_UP];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesWith:touches phase:TOUCH_UP];
}

@end

@interface ECGameViewController : UIViewController
@property (nonatomic, strong) CADisplayLink *link;
@end

@implementation ECGameViewController

- (void)loadView {
    self.view = [[ECGameView alloc] initWithFrame:UIScreen.mainScreen.bounds];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.link = [CADisplayLink displayLinkWithTarget:self selector:@selector(present)];
    [self.link addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
}

/* Core Animation keeps the image it shows, so the frame is copied out and the
 * buffer handed straight back to the game thread. */
- (void)present {
    pthread_mutex_lock(&s_frame_lock);
    int idx = (s_latest >= 0 && s_seq != s_shown_seq) ? s_latest : -1;
    if (idx >= 0) {
        s_reading = idx;
        s_shown_seq = s_seq;
    }
    pthread_mutex_unlock(&s_frame_lock);
    if (idx < 0) return;

    frame_t *f = &s_frames[idx];
    int w = f->w, h = f->h;
    CFDataRef data = CFDataCreate(NULL, (const UInt8 *)f->px, (CFIndex)w * h * 4);

    pthread_mutex_lock(&s_frame_lock);
    s_reading = -1;
    pthread_mutex_unlock(&s_frame_lock);
    if (!data) return;

    CGDataProviderRef dp = CGDataProviderCreateWithCFData(data);
    CFRelease(data);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGImageRef img = CGImageCreate(w, h, 8, 32, (size_t)w * 4, cs,
                                   kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big,
                                   dp, NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease(cs);
    CGDataProviderRelease(dp);
    if (!img) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    self.view.layer.contents = (__bridge id)img;
    [CATransaction commit];
    CGImageRelease(img);
}

- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)canBecomeFirstResponder { return YES; }

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self becomeFirstResponder];
}

- (void)keys:(NSSet<UIPress *> *)presses down:(bool)down {
    for (UIPress *p in presses) {
        if (@available(iOS 13.4, *)) {
            if (!p.key) continue;
            int k = hid_to_pkey((long)p.key.keyCode);
            if (k < 0) continue;
            if (down && !g_plat.keys_now[k]) g_plat.keys_latch[k] = true;
            g_plat.keys_now[k] = down;
            touch_hide();
        }
    }
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    [self keys:presses down:true];
}
- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    [self keys:presses down:false];
}
- (void)pressesCancelled:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event {
    [self keys:presses down:false];
}

@end

/* ── Game thread ─────────────────────────────────────────── */

static void *game_thread(void *arg) {
    (void)arg;
    char *argv[] = { "ecstatica", NULL };
    ecs_main(1, argv);
    exit(0);
    return NULL;
}

static void start_game(const char *dir) {
    snprintf(s_game_dir, sizeof(s_game_dir), "%s", dir);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, GAME_STACK);
    pthread_t th;
    pthread_create(&th, &attr, game_thread, NULL);
    pthread_attr_destroy(&attr);
}

/* ── Library ─────────────────────────────────────────────── */

static NSString *documents(void) {
    return NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
}

static BOOL game_installed(NSString *dir) {
    return file_dir_has_database(dir.fileSystemRepresentation) != 0;
}

@interface ECLibraryViewController : UIViewController <UIDocumentPickerDelegate>
@property (nonatomic, strong) UIStackView *stack;
@property (nonatomic, strong) UILabel *status;
@property (nonatomic, strong) NSMutableArray<UIButton *> *buttons;
@property (nonatomic, copy) NSString *importGame;
- (void)play:(UIButton *)sender;
@end

@implementation ECLibraryViewController

- (UILabel *)label:(NSString *)text size:(CGFloat)size bold:(BOOL)bold {
    UILabel *l = [UILabel new];
    l.text = text;
    l.numberOfLines = 0;
    l.textColor = [UIColor colorWithRed:0.91 green:0.89 blue:0.84 alpha:1];
    l.font = bold ? [UIFont boldSystemFontOfSize:size] : [UIFont systemFontOfSize:size];
    return l;
}

- (UIButton *)button:(NSString *)title action:(SEL)action tag:(NSInteger)tag {
    UIButton *b = [UIButton buttonWithType:UIButtonTypeSystem];
    [b setTitle:title forState:UIControlStateNormal];
    b.titleLabel.font = [UIFont systemFontOfSize:17];
    b.tag = tag;
    [b addTarget:self action:action forControlEvents:UIControlEventTouchUpInside];
    [self.buttons addObject:b];
    return b;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;
    self.buttons = [NSMutableArray array];
    [self rebuild];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self rebuild];
}

- (void)rebuild {
    [self.stack.superview removeFromSuperview];
    [self.buttons removeAllObjects];

    UIScrollView *scroll = [UIScrollView new];
    scroll.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:scroll];
    UILayoutGuide *g = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [scroll.leadingAnchor constraintEqualToAnchor:g.leadingAnchor constant:24],
        [scroll.trailingAnchor constraintEqualToAnchor:g.trailingAnchor constant:-24],
        [scroll.topAnchor constraintEqualToAnchor:g.topAnchor constant:16],
        [scroll.bottomAnchor constraintEqualToAnchor:g.bottomAnchor],
    ]];

    UIStackView *s = [UIStackView new];
    s.axis = UILayoutConstraintAxisVertical;
    s.spacing = 8;
    s.translatesAutoresizingMaskIntoConstraints = NO;
    [scroll addSubview:s];
    [NSLayoutConstraint activateConstraints:@[
        [s.leadingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.leadingAnchor],
        [s.trailingAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.trailingAnchor],
        [s.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor],
        [s.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor],
        [s.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor],
    ]];
    self.stack = s;

    [s addArrangedSubview:[self label:@"Ecstatica" size:30 bold:YES]];
    [s addArrangedSubview:[self label:@"You need your own copy of the game. Import the folder that "
                                      "holds CODE/ECSTATIC.FAN, or copy it into this app's folder "
                                      "in the Files app."
                                 size:15 bold:NO]];

    NSArray *titles = @[ @"Ecstatica", @"Ecstatica II" ];
    NSArray *games = @[ @"e1", @"e2" ];
    for (NSInteger i = 0; i < 2; i++) {
        NSString *dir = [documents() stringByAppendingPathComponent:games[i]];
        BOOL ok = game_installed(dir);
        UILabel *t = [self label:titles[i] size:21 bold:YES];
        [s setCustomSpacing:4 afterView:s.arrangedSubviews.lastObject];
        [s addArrangedSubview:t];
        [s addArrangedSubview:[self label:ok ? @"Installed." : @"Not imported yet." size:15 bold:NO]];
        UIStackView *row = [UIStackView new];
        row.spacing = 20;
        UIButton *play = [self button:@"Play" action:@selector(play:) tag:i];
        play.enabled = ok;
        [row addArrangedSubview:play];
        [row addArrangedSubview:[self button:@"Import folder…" action:@selector(import:) tag:i]];
        UIButton *rm = [self button:@"Remove" action:@selector(remove:) tag:i];
        rm.enabled = ok;
        [row addArrangedSubview:rm];
        [row addArrangedSubview:[UIView new]];
        [s addArrangedSubview:row];
    }

    self.status = [self label:@"" size:15 bold:NO];
    [s addArrangedSubview:self.status];

    UILabel *notes = [self label:
        @"In the Files app this app's folder is On My iPhone (or iPad) › Ecstatica. Put the games "
         "in its e1 and e2 folders. Ecstatica 1 needs the DOS release root with its W folder "
         "inside, for the 640×480 graphics. Saved games are kept in each game's folder.\n\n"
         "Music needs a General MIDI soundfont (.sf2), such as GeneralUser GS, in the Ecstatica "
         "folder. Without one, sound effects and speech still play."
                            size:13 bold:NO];
    notes.textColor = [UIColor colorWithRed:0.6 green:0.57 blue:0.52 alpha:1];
    [s addArrangedSubview:notes];
}

- (void)play:(UIButton *)sender {
    NSString *game = sender.tag == 0 ? @"e1" : @"e2";
    NSString *dir = [documents() stringByAppendingPathComponent:game];
    ECGameViewController *vc = [ECGameViewController new];
    vc.modalPresentationStyle = UIModalPresentationFullScreen;
    [self presentViewController:vc animated:NO completion:^{
        start_game(dir.fileSystemRepresentation);
    }];
}

- (void)remove:(UIButton *)sender {
    NSString *dir = [documents() stringByAppendingPathComponent:sender.tag == 0 ? @"e1" : @"e2"];
    [NSFileManager.defaultManager removeItemAtPath:dir error:nil];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES
                                             attributes:nil error:nil];
    [self rebuild];
}

- (void)import:(UIButton *)sender {
    self.importGame = sender.tag == 0 ? @"e1" : @"e2";
    UIDocumentPickerViewController *p =
        [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[ UTTypeFolder ]];
    p.delegate = self;
    [self presentViewController:p animated:YES completion:nil];
}

/* The game root holds CODE/ECSTATIC.FAN; look up to two levels down. */
static NSURL *find_root(NSURL *url, int depth) {
    if (file_dir_has_database(url.fileSystemRepresentation)) return url;
    if (depth == 0) return nil;
    NSArray *kids = [NSFileManager.defaultManager contentsOfDirectoryAtURL:url
                        includingPropertiesForKeys:@[ NSURLIsDirectoryKey ] options:0 error:nil];
    for (NSURL *k in kids) {
        NSNumber *isDir = nil;
        [k getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
        if (!isDir.boolValue) continue;
        NSURL *r = find_root(k, depth - 1);
        if (r) return r;
    }
    return nil;
}

static BOOL wanted(NSString *rel) {
    for (NSString *part in rel.pathComponents) {
        NSString *low = part.lowercaseString;
        if ([low hasPrefix:@"."] || [low isEqualToString:@"directx"] ||
            [low isEqualToString:@"ishield"] || [low isEqualToString:@"__macosx"])
            return NO;
    }
    NSString *ext = rel.pathExtension.lowercaseString;
    return !([ext isEqualToString:@"exe"] || [ext isEqualToString:@"dll"] || [ext isEqualToString:@"ply"]);
}

- (void)documentPicker:(UIDocumentPickerViewController *)picker didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *src = urls.firstObject;
    NSString *game = self.importGame;
    if (!src || !game) return;
    for (UIButton *b in self.buttons) b.enabled = NO;
    self.status.text = @"Importing…";

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString *err = nil;
        BOOL scoped = [src startAccessingSecurityScopedResource];
        NSURL *root = find_root(src, 2);
        if (!root) {
            err = @"No CODE/ECSTATIC.FAN in that folder.";
        } else {
            NSFileManager *fm = NSFileManager.defaultManager;
            NSString *dst = [documents() stringByAppendingPathComponent:game];
            [fm removeItemAtPath:dst error:nil];
            NSDirectoryEnumerator *en = [fm enumeratorAtURL:root
                                 includingPropertiesForKeys:@[ NSURLIsDirectoryKey ] options:0 errorHandler:nil];
            NSUInteger count = 0;
            NSUInteger base = root.path.length + 1;
            for (NSURL *u in en) {
                NSString *rel = [u.path substringFromIndex:MIN(base, u.path.length)];
                NSNumber *isDir = nil;
                [u getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
                if (!wanted(rel)) {
                    if (isDir.boolValue) [en skipDescendants];
                    continue;
                }
                NSString *to = [dst stringByAppendingPathComponent:rel];
                if (isDir.boolValue) {
                    [fm createDirectoryAtPath:to withIntermediateDirectories:YES attributes:nil error:nil];
                    continue;
                }
                [fm createDirectoryAtPath:to.stringByDeletingLastPathComponent
                    withIntermediateDirectories:YES attributes:nil error:nil];
                NSError *e = nil;
                if (![fm copyItemAtURL:u toURL:[NSURL fileURLWithPath:to] error:&e]) {
                    err = e.localizedDescription;
                    break;
                }
                if (++count % 25 == 0) {
                    NSUInteger n = count;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        self.status.text = [NSString stringWithFormat:@"Importing… %lu files", (unsigned long)n];
                    });
                }
            }
        }
        if (scoped) [src stopAccessingSecurityScopedResource];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self rebuild];
            self.status.text = err ? [@"Import failed: " stringByAppendingString:err] : @"Import finished.";
        });
    });
}

- (BOOL)prefersStatusBarHidden { return YES; }

@end

/* ── App ─────────────────────────────────────────────────── */

@interface ECAppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic, strong) UIWindow *window;
@end

@implementation ECAppDelegate

- (BOOL)application:(UIApplication *)app didFinishLaunchingWithOptions:(NSDictionary *)opts {
    /* Empty game folders, so the Files app shows where the games go. */
    NSFileManager *fm = NSFileManager.defaultManager;
    for (NSString *g in @[ @"e1", @"e2" ])
        [fm createDirectoryAtPath:[documents() stringByAppendingPathComponent:g]
            withIntermediateDirectories:YES attributes:nil error:nil];
    snprintf(s_documents, sizeof(s_documents), "%s", documents().fileSystemRepresentation);

    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    ECLibraryViewController *lib = [ECLibraryViewController new];
    self.window.rootViewController = lib;
    [self.window makeKeyAndVisible];

    /* `-game e1|e2` on the command line (simctl launch, Xcode scheme) skips
     * the library. */
    NSString *game = [NSUserDefaults.standardUserDefaults stringForKey:@"game"];
    if ([game isEqualToString:@"e1"] || [game isEqualToString:@"e2"]) {
        UIButton *b = [UIButton new];
        b.tag = [game isEqualToString:@"e1"] ? 0 : 1;
        dispatch_async(dispatch_get_main_queue(), ^{ [lib play:b]; });
    }
    return YES;
}

static void set_active(bool active) {
    pthread_mutex_lock(&s_active_lock);
    s_active = active;
    pthread_cond_broadcast(&s_active_cond);
    pthread_mutex_unlock(&s_active_lock);
}

- (void)applicationWillResignActive:(UIApplication *)app {
    set_active(false);
    touch_cancel_all();
}

- (void)applicationDidBecomeActive:(UIApplication *)app {
    set_active(true);
}

@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        touch_init();
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([ECAppDelegate class]));
    }
}

/* ── platform.h ──────────────────────────────────────────── */

platform_t *platform_init(const char *title, int fb_width, int fb_height, int scale) {
    (void)title;
    (void)scale;
    platform_t *p = &g_plat;
    p->render_w = fb_width;
    p->render_h = fb_height;
    p->mouse_x = fb_width / 2;
    p->mouse_y = fb_height / 2;
    mach_timebase_info(&p->timebase);
    p->start = mach_absolute_time();
    return p;
}

bool platform_hires_supported(platform_t *p) {
    (void)p;
    return true;
}

bool platform_boot_low_res(platform_t *p) {
    (void)p;
    return false;
}

bool platform_scale_mode_supported(platform_t *p) {
    (void)p;
    return true;
}

void platform_set_scale_mode(platform_t *p, int mode) {
    if (p && mode >= SCALE_PILLARBOX && mode <= SCALE_STRETCH) p->scale_mode = mode;
}

int platform_crop_inset_y(platform_t *p, int render_h) {
    pthread_mutex_lock(&s_frame_lock);
    int w = s_surf_w, h = s_surf_h;
    pthread_mutex_unlock(&s_frame_lock);
    return p ? screen_crop_inset_y(w, h, render_h, p->scale_mode) : 0;
}

void platform_set_render_size(platform_t *p, int w, int h) {
    if (!p || w <= 0 || h <= 0) return;
    p->render_w = w;
    p->render_h = h;
    if (p->mouse_x >= w) p->mouse_x = w - 1;
    if (p->mouse_y >= h) p->mouse_y = h - 1;
}

void platform_blit(platform_t *p, const uint8_t *framebuffer, const uint8_t *palette) {
    if (!p || !framebuffer || !palette) return;

    pthread_mutex_lock(&s_frame_lock);
    int idx = 0;
    while (idx == s_latest || idx == s_reading) idx++;
    int w = s_surf_w, h = s_surf_h;
    pthread_mutex_unlock(&s_frame_lock);

    frame_t *f = &s_frames[idx];
    if (f->cap < w * h) {
        free(f->px);
        f->px = (uint32_t *)malloc((size_t)w * (size_t)h * 4);
        f->cap = f->px ? w * h : 0;
        if (!f->px) return;
    }
    screen_present(f->px, w, h, w, framebuffer, p->render_w, p->render_h, palette, p->scale_mode);
    touch_draw(f->px, w, h, w);
    f->w = w;
    f->h = h;

    pthread_mutex_lock(&s_frame_lock);
    s_latest = idx;
    s_seq++;
    pthread_mutex_unlock(&s_frame_lock);
}

void platform_blit_rgba(platform_t *p, const uint8_t *framebuffer) {
    /* Only the debug overlay uses this, and it is off here. */
    (void)p;
    (void)framebuffer;
}

void platform_set_title(platform_t *p, const char *title) {
    (void)p;
    (void)title;
}

bool platform_pump_events(platform_t *p) {
    if (!p) return false;

    pthread_mutex_lock(&s_active_lock);
    while (!s_active) pthread_cond_wait(&s_active_cond, &s_active_lock);
    pthread_mutex_unlock(&s_active_lock);

    memcpy(p->keys_prev, (const void *)p->keys_now, sizeof(p->keys_prev));

    platform_gamepad_state_t unused;
    memset(&unused, 0, sizeof(unused));
    bool down = false;
    int x = 0, y = 0;
    touch_poll(&unused, &down, &x, &y);
    if (down) {
        pthread_mutex_lock(&s_frame_lock);
        screen_rect_t pic = screen_picture_rect(s_surf_w, s_surf_h, p->scale_mode);
        pthread_mutex_unlock(&s_frame_lock);
        screen_to_render(&pic, p->render_w, p->render_h, x, y, &p->mouse_x, &p->mouse_y);
    }
    p->mouse_buttons = down ? PMOUSE_LEFT : 0;
    return true;
}

bool platform_key_down(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    return p->keys_now[keycode];
}

bool platform_key_pressed(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    return p->keys_now[keycode] && !p->keys_prev[keycode];
}

bool platform_key_hit(platform_t *p, int keycode) {
    if (!p || keycode < 0 || keycode >= PKEY_TABLE_SIZE) return false;
    bool hit = p->keys_latch[keycode];
    p->keys_latch[keycode] = false;
    return hit;
}

int platform_mouse_state(platform_t *p, int *out_x, int *out_y) {
    if (!p) return 0;
    if (out_x) *out_x = p->mouse_x;
    if (out_y) *out_y = p->mouse_y;
    return p->mouse_buttons;
}

void platform_gamepad_poll(platform_t *p, platform_gamepad_state_t *state) {
    memset(state, 0, sizeof(*state));
    if (!p) return;

    GCController *gc = GCController.controllers.firstObject;
    GCExtendedGamepad *gp = gc.extendedGamepad;
    if (gp) {
        state->connected = true;
        state->dpad_up    = gp.dpad.up.isPressed;
        state->dpad_down  = gp.dpad.down.isPressed;
        state->dpad_left  = gp.dpad.left.isPressed;
        state->dpad_right = gp.dpad.right.isPressed;
        state->left_x  = (int16_t)(gp.leftThumbstick.xAxis.value * 32767);
        state->left_y  = (int16_t)(gp.leftThumbstick.yAxis.value * 32767);
        state->right_x = (int16_t)(gp.rightThumbstick.xAxis.value * 32767);
        state->right_y = (int16_t)(gp.rightThumbstick.yAxis.value * 32767);
        state->btn_south = gp.buttonA.isPressed;
        state->btn_east  = gp.buttonB.isPressed;
        state->btn_west  = gp.buttonX.isPressed;
        state->btn_north = gp.buttonY.isPressed;
        state->btn_lb = gp.leftShoulder.isPressed;
        state->btn_rb = gp.rightShoulder.isPressed;
        state->btn_lt = gp.leftTrigger.value > 0.3f;
        state->btn_rt = gp.rightTrigger.value > 0.3f;
        state->btn_start  = gp.buttonMenu.isPressed;
        state->btn_select = gp.buttonOptions.isPressed;
        state->btn_lstick = gp.leftThumbstickButton.isPressed;
        state->btn_rstick = gp.rightThumbstickButton.isPressed;

        bool used = state->btn_south || state->btn_east || state->btn_west || state->btn_north ||
                    state->btn_start || state->left_x > 8000 || state->left_x < -8000 ||
                    state->left_y > 8000 || state->left_y < -8000;
        if (used) touch_hide();
    }

    bool down;
    int x, y;
    touch_poll(state, &down, &x, &y);
}

uint32_t platform_ticks(platform_t *p) {
    platform_t *q = p ? p : &g_plat;
    if (!q->timebase.denom) {
        mach_timebase_info(&q->timebase);
        q->start = mach_absolute_time();
    }
    uint64_t elapsed = mach_absolute_time() - q->start;
    return (uint32_t)(elapsed * q->timebase.numer / q->timebase.denom / 1000000ull);
}

void platform_delay(uint32_t ms) {
    if (ms) usleep(ms * 1000u);
}

/* ── Audio ───────────────────────────────────────────────── */

static AudioUnit s_unit;
static bool s_audio_ready;

static OSStatus render_cb(void *ref, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *ts,
                          UInt32 bus, UInt32 frames, AudioBufferList *io) {
    (void)ref;
    (void)flags;
    (void)ts;
    (void)bus;
    mixer_render((int16_t *)io->mBuffers[0].mData, (int)frames);
    return noErr;
}

void platform_audio_init(void) {
    if (s_audio_ready) return;
    AVAudioSession *session = AVAudioSession.sharedInstance;
    [session setCategory:AVAudioSessionCategorySoloAmbient error:nil];
    [session setActive:YES error:nil];
    double rate = session.sampleRate > 0 ? session.sampleRate : 44100;

    AudioComponentDescription desc = {
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_RemoteIO,
        .componentManufacturer = kAudioUnitManufacturer_Apple,
    };
    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (!comp || AudioComponentInstanceNew(comp, &s_unit) != noErr) return;

    AudioStreamBasicDescription fmt = {
        .mSampleRate = rate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mFramesPerPacket = 1,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel = 16,
        .mBytesPerPacket = 4,
        .mBytesPerFrame = 4,
    };
    AudioUnitSetProperty(s_unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                         &fmt, sizeof(fmt));
    AURenderCallbackStruct cb = { .inputProc = render_cb };
    AudioUnitSetProperty(s_unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                         &cb, sizeof(cb));

    mixer_init((int)rate, s_documents);
    if (AudioUnitInitialize(s_unit) != noErr || AudioOutputUnitStart(s_unit) != noErr) {
        AudioComponentInstanceDispose(s_unit);
        mixer_shutdown();
        return;
    }
    s_audio_ready = true;
}

void platform_audio_shutdown(void) {
    if (!s_audio_ready) return;
    AudioOutputUnitStop(s_unit);
    AudioUnitUninitialize(s_unit);
    AudioComponentInstanceDispose(s_unit);
    mixer_shutdown();
    s_audio_ready = false;
}

/* ── Files ───────────────────────────────────────────────── */

void platform_early_init(void) {
    if (file_dir_has_database(s_game_dir) && chdir(s_game_dir) == 0) {
        file_flush_path_cache();
        return;
    }
    fprintf(stderr, "ecstatica: no game data in %s\n", s_game_dir);
    exit(2);
}

bool platform_slow_writes(void) {
    return false;
}

int platform_save_slot_count(void) {
    return 11;
}

void platform_save_path(char *buf, int bufsz, int slot, int game_version) {
    (void)game_version;   /* one folder per game */
    snprintf(buf, bufsz, "saved/%04d.ecs", slot);
}

void platform_save_prepare(void) {
    mkdir("saved", 0777);
}

void platform_shutdown(platform_t *p) {
    (void)p;
    platform_audio_shutdown();
}
