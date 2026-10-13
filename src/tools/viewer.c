/**
 * viewer.c
 *
 * Model and animation browser (--viewer), not in the original. Everything is
 * drawn through prepare_parts / draw_parts / show_parts as in gameplay; the
 * viewer only supplies the camera, the background and which thing and action
 * are live. The image browser (--images) shows the raw view and interface
 * art instead, decoded by the engine's own unpackers.
 */

#include "viewer.h"

#include "anim.h"
#include "asm_f.h"
#include "compat.h"
#include "display.h"
#include "edit.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "menu.h"
#include "move.h"
#include "music.h"
#include "platform.h"
#include "topo.h"
#include "win.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int viewer_mode = 0;

/* ── Reserved palette slots ──────────────────────────────────────
 * As debug_overlay.c: the top palette entries are unused by the view palettes.
 * Written into both view_cmap and colour_map, because E1's show_parts
 * re-applies colour_map on every flip. */
#define COL_TEXT     255  /* white       */
#define COL_HILITE   254  /* yellow      */
#define COL_DIM      253  /* grey        */
#define COL_PANEL    252  /* panel fill  */
#define COL_BG       251  /* scene background — cycled by B */

static const palette_entry_t bg_choices[] = {
    {  0,  0,  0 },   /* black */
    { 12, 12, 14 },   /* near-black, shows dark silhouettes */
    { 26, 26, 30 },   /* slate */
    { 42, 24, 42 },   /* magenta-grey, exposes stray fills */
};
#define BG_CHOICE_COUNT ((int)(sizeof(bg_choices) / sizeof(bg_choices[0])))

/* ── State ───────────────────────────────────────────────────── */

enum { PANE_MODELS = 0, PANE_ANIMS = 1 };
enum { CAM_ORBIT = 0, CAM_FREE = 1 };
enum { TOOL_MODELS = 0, TOOL_SCENES = 1, TOOL_IMAGES = 2, TOOL_COUNT = 3 };

/* An entry in the animation pane. `slot` is the repertoire slot the action
 * sits in, or -1 when the archive scan is what turned it up. */
typedef struct {
    int16_t action;
    int16_t slot;
} anim_entry_t;

static platform_t *plat;

static int16_t *model_ids;
static int      model_count;
static int      model_sel;
static int      model_top;

static anim_entry_t *anim_list;
static int      anim_count;
static int      anim_sel;
static int      anim_top;

static int      pane = PANE_MODELS;
static int      tool_mode = TOOL_MODELS;

/* ── Scene mode ──────────────────────────────────────────────────
 * One script per actor, each an action with action_flags & 2, run in absolute
 * time to act->duration. Playback uses the original editor's preview path,
 * advance_selected_scene_or_action(), with script_mode set. */
static int16_t *scene_ids;
static int      scene_count;
static int      scene_sel;
static int      scene_top;
static int      cast_sel;
static int      cast_top;

static scene_t *cur_scene;
static int16_t  cur_scene_idx = -1;
static int      scene_phase;      /* ticks into the scene */
static int      scene_len = 1;    /* longest script action, in ticks */
/* Detached: the orbit camera over a flat backdrop, to see what the scene's
 * own shot hides. */
static bool     cam_attached = true;

static actor_t *cur_actor;
static int16_t  cur_thing = -1;
static action_t *cur_action;
static int16_t   cur_action_idx = -1;

/* Scene actions (flags & 2) are positioned in absolute time up to
 * act->duration, others by a 0..0xFFFF fraction (see position_act()). */
static int      phase;
static int      phase_max = 0xFFFF;
static bool     playing = true;
static bool     looping = true;
static int      speed_pct = 100;
/* See effective_duration(). F toggles it. */
static bool     stretch_short = true;
static int      min_loop_ms = 600;

static int      cam_mode = CAM_ORBIT;
static vector_t focus;
/* Relative to the actor's origin, so the camera tracks a walking actor. */
static vector_t focus_offset;
/* Holds root motion over the origin so a walk cycle plays in place. */
static bool     pin_root = true;
static int32_t  orbit_yaw, orbit_pitch, orbit_dist = 1200;
static int      bg_choice;
static int      pal_cam;
static bool     show_help;
static bool     auto_spin;
static bool     panels_hidden;

/* Archive scan. E2 leaves action_t.thing_name_index at -1, so an action is
 * matched to a model by the parts its events name: modify_part() looks each
 * one up in _PartTab, so an action naming a part the model lacks was not
 * authored for it. SCAN_UNKNOWN marks entries not resolved yet. */
#define SCAN_UNKNOWN (-2)
#define SCAN_SKIP    (-1)
#define PART_BITS    ((PART_TAB_SIZE + 7) / 8)
#define BIT_SET(buf, i)  ((buf)[(i) >> 3] |= (uint8_t)(1u << ((i) & 7)))
#define BIT_GET(buf, i)  (((buf)[(i) >> 3] >> ((i) & 7)) & 1u)

/* Distinct part references per action, or SCAN_UNKNOWN / SCAN_SKIP. */
static int16_t *action_nrefs;
static uint8_t *action_parts;       /* ACTION_TAB_SIZE * PART_BITS */
static bool    *action_preexisting;
static uint8_t  model_parts[PART_BITS];
static int      scan_pos = -1;      /* -1 = not scanning */
static bool     scan_done;

/* One-part actions are usually effects that match almost any rig. */
#define SCAN_MIN_REFS 2

static int      last_tick;

/* ── Small helpers ───────────────────────────────────────────── */

static int thing_limit(void) {
    return (game_version == GAME_VERSION_E1) ? E1_THING_TAB_SIZE : THING_TAB_SIZE;
}

static int action_limit(void) {
    return (game_version == GAME_VERSION_E1) ? E1_ACTION_TAB_SIZE : ACTION_TAB_SIZE;
}

static int scene_limit(void) {
    return (game_version == GAME_VERSION_E1) ? E1_SCENE_TAB_SIZE : SCENE_TAB_SIZE;
}

static const char *scene_name_of(int16_t i) {
    if (i < 0 || i >= SCENE_TAB_SIZE || !scene_names) return "?";
    return scene_names[i].field_0;
}

static const char *thing_name_of(int16_t i) {
    if (i < 0 || i >= THING_TAB_SIZE || !thing_names) return "?";
    return thing_names[i].field_0;
}

static const char *action_name_of(int16_t i) {
    if (i < 0 || i >= ACTION_TAB_SIZE || !action_names) return "?";
    return action_names[i].field_0;
}

static int effective_duration(void);

static int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ── Palette / background ────────────────────────────────────── */

static void inject_palette(void) {
    palette_entry_t e[5];
    e[0] = pal_rgb(63, 63, 63);            /* COL_TEXT   */
    e[1] = pal_rgb(63, 56, 0);            /* COL_HILITE */
    e[2] = pal_rgb(34, 34, 38);            /* COL_DIM    */
    e[3] = pal_rgb(4, 6, 14);            /* COL_PANEL  */
    e[4] = bg_choices[bg_choice];                      /* COL_BG     */

    static const int idx[5] = { COL_TEXT, COL_HILITE, COL_DIM, COL_PANEL, COL_BG };
    for (int i = 0; i < 5; i++) {
        view_cmap[idx[i]]   = e[i];
        colour_map[idx[i]]  = e[i];
        fade_cmap[idx[i]]   = e[i];
    }
    set_palette_flag = 1;
}

/* bitmap[3] is the pristine view, bitmap[2] the working copy; the mask planes
 * are opened so nothing occludes the model. */
static void rebuild_background(void) {
    int16_t save_pen = a_pen_colour;
    int16_t save_mode = draw_mode[3];

    draw_mode[3] = 1;
    a_pen_colour = COL_BG;
    rect_fill(3, 0, 0, screen_width, screen_height);
    a_pen_colour = save_pen;
    draw_mode[3] = save_mode;

    int16_t *m = mask_map[2];
    for (int i = 0; i < screen_height * screen_width; i++)
        *m++ = 0x7FFF;

    clip_blit(3, 0, 0, 2, 0, 0, screen_width, screen_height, 192);
    clip_mask(2, 1, 0, 0, screen_width, screen_height);
    clip_mask(2, 0, 0, 0, screen_width, screen_height);
}

/* E2 keeps a palette per camera; E1 here has one global palette, so the cycle
 * is a no-op there. E2 model colours index a scene palette, so one must be
 * loaded before the first frame or models render through the title palette. */
static void apply_scene_palette(int cam) {
    if (game_version != GAME_VERSION_E2) return;
    if (cam < 0) cam = 0;
    pal_cam = cam;
    selected_camera = (int16_t)cam;
    load_palette(NULL);
    inject_palette();
}

/* Walks palette_offset: most cameras have no palette, and the map (which
 * find_highest_camera_num() needs) is never loaded here. */
static int step_scene_palette(int from, int dir) {
    for (int n = 0; n < PALETTES_MAX; n++) {
        from += dir;
        if (from >= PALETTES_MAX) from = 0;
        if (from < 0) from = PALETTES_MAX - 1;
        if (palette_offset[from] >= 0) return from;
    }
    return from;
}

/* ── Model list ──────────────────────────────────────────────── */

static void build_model_list(void) {
    int limit = thing_limit();
    model_ids = (int16_t *)calloc(limit, sizeof(int16_t));
    model_count = 0;
    if (!model_ids) return;

    for (int i = 0; i < limit; i++) {
        if (!thing_names || !thing_names[i].field_0[0]) continue;
        if (load_by_offset && actor_offset[i] < 0) continue;
        model_ids[model_count++] = (int16_t)i;
    }
}

/* ── Animation list ──────────────────────────────────────────── */

static void anim_add(int16_t action, int16_t slot) {
    for (int i = 0; i < anim_count; i++) {
        if (anim_list[i].action == action) {
            if (anim_list[i].slot < 0) anim_list[i].slot = slot;
            return;
        }
    }
    anim_list[anim_count].action = action;
    anim_list[anim_count].slot = slot;
    anim_count++;
}

static void build_anim_list(void) {
    anim_count = 0;
    anim_sel = 0;
    anim_top = 0;
    if (!anim_list || !cur_actor) return;

    /* The repertoire leads the list, scanned or not. */
    int16_t ri = cur_actor->actor_rep_index;
    if (ri < 0) ri = cur_actor->default_repert;
    if (ri >= 0 && ri < REPERTOIRE_TAB_SIZE) {
        check_rep_loaded(ri);
        rephead_t *rep = repertoire_tab[ri];
        if (rep) {
            for (int s = 0; s < 208; s++) {
                int16_t a = rep->action_slots[s];
                if (a >= 0 && a < action_limit() && action_name_of(a)[0])
                    anim_add(a, (int16_t)s);
            }
        }
    }

    if (!action_nrefs || !action_parts) return;

    /* Scanned actions: every part the action names must exist on this model. */
    memset(model_parts, 0, sizeof(model_parts));
    for (part_t *p = cur_actor->actor_parts_list; p; p = p->next_in_display_list)
        if (p->name_index >= 0 && p->name_index < PART_TAB_SIZE)
            BIT_SET(model_parts, p->name_index);

    int limit = action_limit();
    for (int a = 0; a < limit; a++) {
        int refs = action_nrefs[a];
        if (refs < SCAN_MIN_REFS) continue;
        const uint8_t *want = action_parts + (size_t)a * PART_BITS;
        int ok = 1;
        for (int b = 0; b < PART_BITS; b++) {
            if (want[b] & ~model_parts[b]) { ok = 0; break; }
        }
        if (ok) anim_add((int16_t)a, -1);
    }
}

/* ── Camera ──────────────────────────────────────────────────── */

static void camera_basis(vector_t *right, vector_t *up, vector_t *fwd) {
    /* view_matrix maps world → view, so its rows are the view axes in world space. */
    if (right) set_vector(right, view_matrix._11, view_matrix._12, view_matrix._13);
    if (up)    set_vector(up,    view_matrix._21, view_matrix._22, view_matrix._23);
    if (fwd)   set_vector(fwd,   view_matrix._31, view_matrix._32, view_matrix._33);
}

static void recompute_focus(void) {
    if (!cur_actor) return;
    focus.X = (int16_t)(cur_actor->position_vector.X + focus_offset.X);
    focus.Y = (int16_t)(cur_actor->position_vector.Y + focus_offset.Y);
    focus.Z = (int16_t)(cur_actor->position_vector.Z + focus_offset.Z);
}

static void update_camera(void) {
    recompute_focus();
    if (cam_mode == CAM_ORBIT) {
        view_rot.X = (int16_t)orbit_pitch;
        view_rot.Y = (int16_t)orbit_yaw;
        view_rot.Z = 0;
        calculate_view_matrices();

        vector_t fwd;
        camera_basis(NULL, NULL, &fwd);
        /* Not clamped: view_transform() takes the int16 difference, which wraps
         * correctly. E1's map reaches ±32000. */
        view_pos.X = (int16_t)(focus.X - (int)((int32_t)fwd.X * orbit_dist >> 14));
        view_pos.Y = (int16_t)(focus.Y - (int)((int32_t)fwd.Y * orbit_dist >> 14));
        view_pos.Z = (int16_t)(focus.Z - (int)((int32_t)fwd.Z * orbit_dist >> 14));
    } else {
        view_rot.X = (int16_t)orbit_pitch;
        view_rot.Y = (int16_t)orbit_yaw;
        view_rot.Z = 0;
        calculate_view_matrices();
    }
    zoom_factor = 0x400 << 12;
}

/* Centre on the parts' bounding box and back off until it fits. Needs a prior
 * prepare so AbsPosition is live. */
static void fit_camera(void) {
    int minx = 32767, miny = 32767, minz = 32767;
    int maxx = -32768, maxy = -32768, maxz = -32768;
    int found = 0;

    if (cur_actor) {
        for (part_t *p = cur_actor->actor_parts_list; p; p = p->next_in_display_list) {
            int rx = abs(p->VECTOR_Squash.X);
            int ry = abs(p->VECTOR_Squash.Y);
            int rz = abs(p->VECTOR_Squash.Z);
            if (p->AbsPosition.X - rx < minx) minx = p->AbsPosition.X - rx;
            if (p->AbsPosition.Y - ry < miny) miny = p->AbsPosition.Y - ry;
            if (p->AbsPosition.Z - rz < minz) minz = p->AbsPosition.Z - rz;
            if (p->AbsPosition.X + rx > maxx) maxx = p->AbsPosition.X + rx;
            if (p->AbsPosition.Y + ry > maxy) maxy = p->AbsPosition.Y + ry;
            if (p->AbsPosition.Z + rz > maxz) maxz = p->AbsPosition.Z + rz;
            found = 1;
        }
    }

    if (!found) {
        set_vector(&focus_offset, 0, 0, 0);
        orbit_dist = 1200;
    } else {
        /* AbsPosition is origin-relative, as focus_offset wants. */
        focus_offset.X = (int16_t)((minx + maxx) / 2);
        focus_offset.Y = (int16_t)((miny + maxy) / 2);
        focus_offset.Z = (int16_t)((minz + maxz) / 2);

        int ex = maxx - minx, ey = maxy - miny, ez = maxz - minz;

        /* Aim above centre (world up is -Y) so tall models clear the status
         * lines at the top. */
        focus_offset.Y = (int16_t)(focus_offset.Y - ey / 5);

        int extent = ex > ey ? ex : ey;
        if (ez > extent) extent = ez;
        if (extent < 40) extent = 40;
        orbit_dist = clampi(extent * 5 / 2, 120, 24000);
    }

    /* Half a turn (0x8000) to face the camera, plus 0x2000 for a
     * three-quarter view. */
    orbit_yaw = 0x8000 + 0x2000;
    orbit_pitch = 0x0C00;    /* ~17 degrees above, looking down */
    recompute_focus();
    update_camera();
}

/* ── Model loading ───────────────────────────────────────────── */

static void stop_animation(void) {
    if (cur_actor) {
        cur_actor->actor_act.act_action = NULL;
        cur_actor->actor_act.actor_keys_list = NULL;
        cur_actor->actor_act.key_progress = 0;
        cur_actor->actor_act_list = NULL;
    }
    cur_action = NULL;
    cur_action_idx = -1;
    phase = 0;
    phase_max = 0xFFFF;
}

/* The viewer never plays scenes, so nothing retires the actions it loads.
 * Once the 400-entry pool fills, try_to_remove_scene_or_action() could evict
 * one the viewer still points at, so it retires its own, oldest first. */
#define PLAYED_CACHE 64
static int16_t played_ring[PLAYED_CACHE];
static int     played_n;

static void remember_played(int16_t index) {
    for (int i = 0; i < played_n; i++)
        if (played_ring[i] == index) return;

    if (played_n == PLAYED_CACHE) {
        int16_t old = played_ring[0];
        memmove(played_ring, played_ring + 1, (PLAYED_CACHE - 1) * sizeof(int16_t));
        played_n--;
        action_t *a = (old >= 0 && old < ACTION_TAB_SIZE) ? action_tab[old] : NULL;
        if (a && a != cur_action) {
            do_delete_action(a);
            set_action_load_tried(old, false);
        }
    }
    played_ring[played_n++] = index;
}

static void play_action(int16_t index) {
    if (!cur_actor || index < 0 || index >= action_limit()) return;

    remember_played(index);
    check_action_loaded(index);
    action_t *a = action_tab[index];
    if (!a) return;

    cur_action = a;
    cur_action_idx = index;

    act_t *act = &cur_actor->actor_act;
    act->act_action = a;
    act->duration = a->act_duration > 0 ? a->act_duration : 1;
    /* As behaviour() does; only differs from 100 after CT_SPEED_FACTOR. */
    if (game_version != GAME_VERSION_E1 && cur_actor->actor_Speed_factor > 0 &&
        cur_actor->actor_Speed_factor != 100)
        act->duration = (int16_t)(100 * act->duration / cur_actor->actor_Speed_factor);
    act->actor_keys_list = a->key_list;
    act->key_progress = 0;
    act->flags = (uint16_t)(a->action_flags & 0xFE);
    act->loop_count = 0;
    act->anim_param = 0;
    act->next = NULL;

    phase = 0;
    phase_max = (a->action_flags & 2) ? act->duration : 0xFFFF;

    copy_defaults_to_actual(cur_actor);
    update_thing(cur_actor);
}

static void load_model(int list_index) {
    if (list_index < 0 || list_index >= model_count) return;
    int16_t idx = model_ids[list_index];

    stop_animation();
    if (cur_actor) {
        remove_from_display_list(cur_actor);
        cur_actor = NULL;
    }
    root_thing = NULL;

    check_actor_loaded_by_index(idx);
    actor_t *a = thing_tab[idx];
    if (!a) {
        cur_thing = -1;
        anim_count = 0;
        return;
    }

    /* Init code can pull in scenes; keep only this actor on display. */
    initialise_actor(a);
    root_scene = NULL;
    root_thing = a;
    a->next_in_display_list = NULL;
    a->parent_actor = NULL;
    a->part_heap_link = NULL;
    a->holding_actor = NULL;
    a->actor_scene = NULL;
    a->actor_behavior = BH_EXTERNAL;
    a->flags = (a->flags | ACTOR_FLAG_ACTIVE | ACTOR_FLAG_VISIBLE) & ~0x0400u;
    a->state_flags = 0;
    set_vector(&a->position_vector, 0, 0, 0);
    set_vector(&a->rotate_vector, 0, 0, 0);
    set_vector(&a->actor_velocity, 0, 0, 0);

    cur_actor = a;
    cur_thing = idx;
    selected_thing = NULL;

    copy_defaults_to_actual(a);
    update_thing(a);
    prepare_an_actor(a);
    fit_camera();

    build_anim_list();
    if (anim_count > 0) play_action(anim_list[0].action);
}

/* ── Scene loading and playback ──────────────────────────────── */

static void build_scene_list(void) {
    int limit = scene_limit();
    scene_ids = (int16_t *)calloc(limit, sizeof(int16_t));
    scene_count = 0;
    if (!scene_ids) return;

    for (int i = 0; i < limit; i++) {
        if (!scene_names || !scene_names[i].field_0[0]) continue;
        if (load_by_offset && scene_offset[i] < 0) continue;
        scene_ids[scene_count++] = (int16_t)i;
    }
}

static int scene_cast_count(const scene_t *s) {
    int n = 0;
    if (!s) return 0;
    for (script_t *sc = s->scene_script_list; sc; sc = sc->next_script)
        if (!(sc->script_action.action_flags & 0x20)) n++;
    return n;
}

/* nth playable script, skipping 0x20 ones as start_scene() does. */
static script_t *scene_cast_at(const scene_t *s, int n) {
    if (!s) return NULL;
    for (script_t *sc = s->scene_script_list; sc; sc = sc->next_script) {
        if (sc->script_action.action_flags & 0x20) continue;
        if (n-- == 0) return sc;
    }
    return NULL;
}

static int scene_length(const scene_t *s) {
    int longest = 1;
    for (script_t *sc = s ? s->scene_script_list : NULL; sc; sc = sc->next_script) {
        if (sc->script_action.action_flags & 0x20) continue;
        if (sc->script_action.act_duration > longest)
            longest = sc->script_action.act_duration;
    }
    return longest;
}

/* check_view() loads the scene's painted background, depth mask, palette and
 * camera position. */
static void detach_scene_camera(void);

/* Camera 0 is check_view's "no view" case with no authored framing, so only
 * the orbit camera can show such a scene. */
static bool scene_has_shot(const scene_t *s) {
    return s && s->camera_index > 0;
}

static void attach_scene_camera(void) {
    if (!cur_scene) return;
    if (!scene_has_shot(cur_scene)) { detach_scene_camera(); return; }
    /* check_view() early-outs when the camera is already active. */
    active_camera = NULL;
    check_view(cur_scene->camera_index);
    pal_cam = cur_scene->camera_index;
    inject_palette();
}

/* Frame the cast member selected in the CAST pane. Not the whole cast: set
 * dressing can sit hundreds of units away (E1's Start_sc pairs the hero with
 * `bridge`). */
static void fit_camera_cast(void) {
    int minx = 32767, miny = 32767, minz = 32767;
    int maxx = -32768, maxy = -32768, maxz = -32768;
    int found = 0;

    script_t *sel = scene_cast_at(cur_scene, cast_sel);
    actor_t *subject = (sel && sel->script_actor_index >= 0 &&
                        sel->script_actor_index < THING_TAB_SIZE)
                     ? thing_tab[sel->script_actor_index] : NULL;
    if (!subject) subject = root_thing;
    if (!subject) return;

    /* As cur_actor, recompute_focus() follows it across the set. */
    cur_actor = subject;

    for (actor_t *a = subject; a; a = NULL) {
        prepare_an_actor(a);
        for (part_t *p = a->actor_parts_list; p; p = p->next_in_display_list) {
            int rx = abs(p->VECTOR_Squash.X);
            int ry = abs(p->VECTOR_Squash.Y);
            int rz = abs(p->VECTOR_Squash.Z);
            if (p->AbsPosition.X - rx < minx) minx = p->AbsPosition.X - rx;
            if (p->AbsPosition.Y - ry < miny) miny = p->AbsPosition.Y - ry;
            if (p->AbsPosition.Z - rz < minz) minz = p->AbsPosition.Z - rz;
            if (p->AbsPosition.X + rx > maxx) maxx = p->AbsPosition.X + rx;
            if (p->AbsPosition.Y + ry > maxy) maxy = p->AbsPosition.Y + ry;
            if (p->AbsPosition.Z + rz > maxz) maxz = p->AbsPosition.Z + rz;
            found = 1;
        }
    }

    if (!found) {
        set_vector(&focus_offset, 0, 0, 0);
        orbit_dist = 1200;
    } else {
        /* Centred, unlike the model fit: a scene subject can be a horse or a
         * bridge, which the low bias pushes out of frame. */
        int ex = maxx - minx, ey = maxy - miny, ez = maxz - minz;
        focus_offset.X = (int16_t)((minx + maxx) / 2);
        focus_offset.Y = (int16_t)((miny + maxy) / 2);
        focus_offset.Z = (int16_t)((minz + maxz) / 2);

        int extent = ex > ey ? ex : ey;
        if (ez > extent) extent = ez;
        if (extent < 40) extent = 40;
        orbit_dist = clampi(extent * 3, 120, 24000);
    }

    orbit_yaw = 0x8000 + 0x2000;
    orbit_pitch = 0x0C00;
    recompute_focus();
    update_camera();
}

static void detach_scene_camera(void) {
    active_camera = NULL;
    rebuild_background();
    inject_palette();
    fit_camera_cast();
}

static void unload_scene(void) {
    if (cur_scene) {
        for (script_t *sc = cur_scene->scene_script_list; sc; sc = sc->next_script) {
            int16_t ai = sc->script_actor_index;
            if (ai < 0 || ai >= THING_TAB_SIZE) continue;
            actor_t *a = thing_tab[ai];
            if (!a) continue;
            a->actor_scene = NULL;
            a->actor_act.act_action = NULL;
            a->actor_act_list = NULL;
            remove_from_display_list(a);
        }
        cur_scene->scene_use_flag &= (uint16_t)~1u;
        /* Drop the started/finished bits so the same scene can be replayed. */
        scene_name_flags[cur_scene->scene_index] &= (int16_t)~6;
    }
    cur_scene = NULL;
    cur_scene_idx = -1;
    selected_scene = NULL;
    root_scene = NULL;
    root_thing = NULL;
    scene_phase = 0;
}

static void restart_scene(void) {
    if (!cur_scene) return;
    scene_name_flags[cur_scene->scene_index] &= (int16_t)~6;
    for (script_t *sc = cur_scene->scene_script_list; sc; sc = sc->next_script) {
        int16_t ai = sc->script_actor_index;
        if (ai < 0 || ai >= THING_TAB_SIZE) continue;
        actor_t *a = thing_tab[ai];
        if (a) a->flags &= ~0x0400u;
    }
    start_scene(cur_scene);
    selected_scene = cur_scene;
    scene_phase = 0;
}

static void load_scene(int list_index) {
    if (list_index < 0 || list_index >= scene_count) return;
    int16_t idx = scene_ids[list_index];

    stop_animation();
    unload_scene();
    if (cur_actor) { remove_from_display_list(cur_actor); cur_actor = NULL; }
    cur_thing = -1;
    root_thing = NULL;

    check_scene_loaded(idx);
    scene_t *s = scene_tab[idx];
    if (!s) return;

    check_actors_in_scene_loaded(s);

    /* start_scene() sets up the act but not the leftover repertoire and
     * position from whatever was shown before. */
    for (script_t *sc = s->scene_script_list; sc; sc = sc->next_script) {
        int16_t ai = sc->script_actor_index;
        if (ai < 0 || ai >= THING_TAB_SIZE) continue;
        actor_t *a = thing_tab[ai];
        if (!a) continue;
        initialise_actor(a);
        a->actor_behavior = BH_EXTERNAL;
        a->flags = (a->flags | ACTOR_FLAG_ACTIVE | ACTOR_FLAG_VISIBLE) & ~0x0400u;
        a->state_flags = 0;
        set_vector(&a->actor_velocity, 0, 0, 0);
        copy_defaults_to_actual(a);
        update_thing(a);
    }

    cur_scene = s;
    cur_scene_idx = idx;
    scene_len = scene_length(s);
    cast_sel = 0;
    cast_top = 0;

    restart_scene();

    if (cam_attached && scene_has_shot(s)) attach_scene_camera();
    else                                   detach_scene_camera();
}

/* Every script steps by the same delta, as do_movement() does. Seeking back
 * replays from the start: progress lives in each act's key_progress. */
static void scene_seek(int to) {
    if (!cur_scene) return;
    if (to < 0) to = 0;
    if (to > scene_len) to = scene_len;

    if (to < scene_phase) {
        restart_scene();
        if (to == 0) return;
    }
    int delta = to - scene_phase;
    while (delta > 0) {
        int step = delta > 0x7000 ? 0x7000 : delta;   /* the arg is int16 */
        advance_selected_scene_or_action((int16_t)step);
        delta -= step;
    }
    scene_phase = to;
}

static void advance_scene(int dt) {
    if (!cur_scene) return;
    if (playing && dt > 0) {
        int step = dt * speed_pct / 100;
        if (step < 1) step = 1;
        if (scene_phase >= scene_len) {
            if (looping) restart_scene();
            else         playing = false;
        }
        if (playing) scene_seek(scene_phase + step);
    }

    /* 0x400 would park actors in the background store. */
    for (actor_t *a = root_thing; a; a = a->next_in_display_list)
        a->flags &= ~0x0400u;
}

/* ── Archive scan ────────────────────────────────────────────── */

/* The part indices an action's events name. 0x200 events go to a held object,
 * not the rig, so they are ignored. */
static void record_action_parts(int index, const action_t *a) {
    uint8_t *bits = action_parts + (size_t)index * PART_BITS;
    memset(bits, 0, PART_BITS);
    int refs = 0;

    for (key_state_t *k = a->key_list; k; k = k->next) {
        for (event_t *ev = k->key_event_list; ev; ev = ev->next) {
            if (ev->event_type >= 80) continue;
            int16_t ef = event_type_flags[ev->event_type];
            if (!(ef & EVENT_FLAGS_PART) || (ef & 0x200)) continue;
            int16_t pi = ev->event_index;
            if (pi < 0 || pi >= PART_TAB_SIZE) continue;
            if (BIT_GET(bits, pi)) continue;
            BIT_SET(bits, pi);
            refs++;
        }
    }
    action_nrefs[index] = (int16_t)refs;
}

static void scan_begin(void) {
    int limit = action_limit();
    if (!action_nrefs || !action_parts) return;

    for (int i = 0; i < limit; i++) {
        action_preexisting[i] = (action_tab[i] != NULL);
        if (action_preexisting[i])
            record_action_parts(i, action_tab[i]);
        else if (load_by_offset && action_offset[i] < 0)
            action_nrefs[i] = SCAN_SKIP;
        else if (!action_name_of(i)[0])
            action_nrefs[i] = SCAN_SKIP;
        else
            action_nrefs[i] = SCAN_UNKNOWN;
    }
    scan_pos = 0;
    scan_done = false;
}

/* One archive record can carry several actions, so sweep the whole table, and
 * drop what the scan loaded so the 400-entry pool does not fill. */
static void scan_sweep(void) {
    int limit = action_limit();
    for (int j = 0; j < limit; j++) {
        action_t *a = action_tab[j];
        if (!a || action_preexisting[j]) continue;
        record_action_parts(j, a);
        if (a == cur_action) {
            action_preexisting[j] = true;   /* in use — leave it alone */
            continue;
        }
        do_delete_action(a);
        set_action_load_tried((int16_t)j, false);
    }
}

static void scan_step(void) {
    if (scan_pos < 0) return;

    int limit = action_limit();
    int32_t deadline = my_time() + 2;   /* my_time ticks at 60-70Hz: ~30ms */

    while (scan_pos < limit) {
        int i = scan_pos++;
        if (action_nrefs[i] != SCAN_UNKNOWN) continue;

        check_action_loaded_no_msg((int16_t)i);
        scan_sweep();
        if (action_nrefs[i] == SCAN_UNKNOWN)
            action_nrefs[i] = SCAN_SKIP;   /* offset present, nothing came back */

        if (my_time() >= deadline) break;
    }

    if (scan_pos >= limit) {
        scan_pos = -1;
        scan_done = true;
        build_anim_list();
        if (anim_count > 0 && !cur_action) play_action(anim_list[0].action);
    }
}

/* ── HUD ─────────────────────────────────────────────────────── */

static int hud_plane(void) { return 1 - db; }

static void vtext(int x, int y, int fg, int bg, const char *fmt, ...) {
    char buf[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    int plane = hud_plane();
    int16_t save_a = a_pen_colour, save_b = b_pen_colour;
    int16_t save_mode = draw_mode[plane];

    a_pen_colour = (int16_t)fg;
    b_pen_colour = (int16_t)bg;
    draw_mode[plane] = (bg >= 0) ? 2 : 1;
    move_pen(plane, (int16_t)x, (int16_t)y);
    text(plane, buf, 0);

    draw_mode[plane] = save_mode;
    a_pen_colour = save_a;
    b_pen_colour = save_b;
}

static void panel(int x, int y, int w, int h) {
    int plane = hud_plane();
    int16_t save = a_pen_colour;
    a_pen_colour = COL_PANEL;
    rect_fill(plane, x, y, w, h);
    a_pen_colour = save;
}

static void draw_list(int x, int y, int cols, int rows,
                      int count, int sel, int top, bool active,
                      const char *title,
                      void (*label)(int i, char *out, int n)) {
    int w = cols * tx_w + 4;
    int h = (rows + 2) * tx_h + 4;
    panel(x, y, w, h);
    vtext(x + 2, y + 2, active ? COL_HILITE : COL_DIM, COL_PANEL, "%s", title);

    char buf[160];
    for (int r = 0; r < rows; r++) {
        int i = top + r;
        if (i >= count) break;
        label(i, buf, cols + 1);
        int fg = (i == sel) ? (active ? COL_HILITE : COL_TEXT) : COL_TEXT;
        int bg = (i == sel) ? COL_DIM : COL_PANEL;
        vtext(x + 2, y + 2 + (r + 1) * tx_h, fg, bg, "%-*.*s", cols, cols, buf);
    }
}

/* draw_list() deliberately passes a column-width n smaller than a name or
 * slot number could ever need — the label is meant to be clipped to the
 * list column, same as the "%-*.*s" it then draws with. GCC can't see that
 * intent and flags every one of these as a possible truncation. */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static void model_label(int i, char *out, int n) {
    int16_t id = model_ids[i];
    snprintf(out, n, "%4d %s", id, thing_name_of(id));
}

static void scene_label(int i, char *out, int n) {
    int16_t id = scene_ids[i];
    snprintf(out, n, "%4d %s", id, scene_name_of(id));
}

static void cast_label(int i, char *out, int n) {
    script_t *sc = scene_cast_at(cur_scene, i);
    if (!sc) { snprintf(out, n, "-"); return; }
    snprintf(out, n, "%-13.13s %4d", thing_name_of(sc->script_actor_index),
             sc->script_action.act_duration);
}

static void anim_label(int i, char *out, int n) {
    anim_entry_t *e = &anim_list[i];
    if (e->slot >= 0)
        snprintf(out, n, "%c%-16.16s s%d", (e->action == cur_action_idx) ? '>' : ' ',
                 action_name_of(e->action), e->slot);
    else
        snprintf(out, n, "%c%-16.16s  -", (e->action == cur_action_idx) ? '>' : ' ',
                 action_name_of(e->action));
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

static void draw_help_lines(const char *const *lines) {
    int rows = 0;
    while (lines[rows]) rows++;
    int w = 62 * tx_w;
    int h = (rows + 2) * tx_h;
    int x = (screen_width - w) / 2;
    int y = (screen_height - h) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    panel(x, y, w, h);
    for (int i = 0; i < rows; i++)
        vtext(x + 2, y + tx_h + i * tx_h, i == 0 ? COL_HILITE : COL_TEXT, COL_PANEL,
              "%s", lines[i]);
}

static void draw_help(void) {
    static const char *const lines[] = {
        "  MODEL / ANIMATION / SCENE VIEWER",
        "",
        "  V            switch MODELS -> SCENES -> IMAGES",
        "  LEFT/RIGHT   switch pane (list <-> anims or cast)",
        "  UP/DOWN      move selection    PGUP/PGDN page",
        "  HOME/END     first / last      RETURN load / play / follow",
        "  TAB          scan archive for every action of this model",
        "",
        "  SPACE        play / pause      L  loop on / off",
        "  , .          step one frame    - =  slower / faster",
        "  T            restart action or scene",
        "  F            true game rate (off: short loops are slowed)",
        "",
        "  O            models: orbit / free camera",
        "               scenes: authored shot / detached orbit",
        "  W A S D      orbit yaw+pitch, or fly in free mode",
        "  Q E          dolly in / out, or rise / fall in free mode",
        "  drag LMB     look    drag RMB  dolly",
        "  R            reframe model or cast member   Z  auto-spin",
        "  P            pin root motion (walk cycles play in place)",
        "",
        "  N M          previous / next scene palette (E2 only)",
        "  B            background colour",
        "  1 2 3        level of detail",
        "  X            hide / show panels",
        "  H            this help         ESC  quit",
        NULL,
    };
    draw_help_lines(lines);
}

static void draw_hud(void) {
    inject_palette();

    int cols = screen_width / tx_w;
    int rows_total = screen_height / tx_h;

    /* Always on: the only thing that fits at 320x200 with panels hidden. */
    const char *gv = (game_version == GAME_VERSION_E1) ? "E1" : "E2";

    if (tool_mode == TOOL_SCENES) {
        vtext(2, 2, COL_HILITE, COL_PANEL, "%s SCENE %-14.14s", gv,
              cur_scene_idx >= 0 ? scene_name_of(cur_scene_idx) : "(none)");
        if (cur_scene) {
            int pct = scene_len > 0 ? scene_phase * 100 / scene_len : 0;
            vtext(2, 2 + tx_h, COL_TEXT, COL_PANEL,
                  "cam%-4d %3d%% %s x%d%s %d/%dt cast%d",
                  cur_scene->camera_index, pct, playing ? "PLAY" : "STOP",
                  speed_pct, looping ? " LOOP" : "",
                  scene_phase, scene_len, scene_cast_count(cur_scene));
        } else {
            vtext(2, 2 + tx_h, COL_DIM, COL_PANEL, "%-24.24s", "(no scene loaded)");
        }
    } else {
    vtext(2, 2, COL_HILITE, COL_PANEL, "%s %-14.14s", gv,
          cur_thing >= 0 ? thing_name_of(cur_thing) : "(no model)");

    if (cur_action) {
        int pct = phase_max > 0 ? (int)((int64_t)phase * 100 / phase_max) : 0;
        int real = cur_actor ? cur_actor->actor_act.duration : 1;
        bool slowed = cur_actor && effective_duration() != real;
        vtext(2, 2 + tx_h, COL_TEXT, COL_PANEL, "%-14.14s %3d%% %s x%d%s %dt%s",
              action_name_of(cur_action_idx), pct,
              playing ? "PLAY" : "STOP", speed_pct, looping ? " LOOP" : "",
              real, slowed ? " SLOW" : "");
    } else {
        vtext(2, 2 + tx_h, COL_DIM, COL_PANEL, "%-14.14s", "(no action)");
    }
    }

    if (scan_pos >= 0) {
        int total = action_limit();
        vtext(2, 2 + 2 * tx_h, COL_HILITE, COL_PANEL,
              "scanning %d/%d", scan_pos, total);
    }

    if (show_help) {
        draw_help();
        return;
    }

    if (!panels_hidden && cols >= 60) {
        int list_rows = rows_total - 8;
        if (list_rows > 44) list_rows = 44;
        if (list_rows < 4) list_rows = 4;

        int lcols = 22;
        int rcols = 22;
        int top_y = 4 * tx_h;

        model_top = clampi(model_top, 0, model_count > list_rows ? model_count - list_rows : 0);
        if (model_sel < model_top) model_top = model_sel;
        if (model_sel >= model_top + list_rows) model_top = model_sel - list_rows + 1;

        anim_top = clampi(anim_top, 0, anim_count > list_rows ? anim_count - list_rows : 0);
        if (anim_sel < anim_top) anim_top = anim_sel;
        if (anim_sel >= anim_top + list_rows) anim_top = anim_sel - list_rows + 1;

        char title[64];
        if (tool_mode == TOOL_SCENES) {
            int cast = scene_cast_count(cur_scene);

            scene_top = clampi(scene_top, 0, scene_count > list_rows ? scene_count - list_rows : 0);
            if (scene_sel < scene_top) scene_top = scene_sel;
            if (scene_sel >= scene_top + list_rows) scene_top = scene_sel - list_rows + 1;

            cast_top = clampi(cast_top, 0, cast > list_rows ? cast - list_rows : 0);
            if (cast_sel < cast_top) cast_top = cast_sel;
            if (cast_sel >= cast_top + list_rows) cast_top = cast_sel - list_rows + 1;

            snprintf(title, sizeof(title), "SCENES %d/%d",
                     scene_count ? scene_sel + 1 : 0, scene_count);
            draw_list(2, top_y, lcols, list_rows, scene_count, scene_sel, scene_top,
                      pane == PANE_MODELS, title, scene_label);

            snprintf(title, sizeof(title), "CAST %d/%d", cast ? cast_sel + 1 : 0, cast);
            draw_list(screen_width - (rcols * tx_w + 6), top_y, rcols, list_rows,
                      cast, cast_sel, cast_top, pane == PANE_ANIMS, title, cast_label);
        } else {
        snprintf(title, sizeof(title), "MODELS %d/%d", model_count ? model_sel + 1 : 0, model_count);
        draw_list(2, top_y, lcols, list_rows, model_count, model_sel, model_top,
                  pane == PANE_MODELS, title, model_label);

        snprintf(title, sizeof(title), "ANIMS %d/%d%s",
                 anim_count ? anim_sel + 1 : 0, anim_count, scan_done ? " *" : "");
        draw_list(screen_width - (rcols * tx_w + 6), top_y, rcols, list_rows,
                  anim_count, anim_sel, anim_top, pane == PANE_ANIMS, title, anim_label);
        }
    }

    int by = screen_height - tx_h - 2;
    int parts = 0, tris = 0;
    for (actor_t *a = root_thing; a; a = a->next_in_display_list) {
        for (part_t *p = a->actor_parts_list; p; p = p->next_in_display_list) parts++;
        for (tri_t *t = a->polygone_tri_list; t; t = t->next) tris++;
    }
    vtext(2, by, COL_DIM, COL_PANEL,
          "%s d%-5d yaw%5d pit%5d  parts%3d tri%3d lod%d pal%d %s  H=help",
          (tool_mode == TOOL_SCENES && cam_attached && scene_has_shot(cur_scene)) ? "shot "
              : (cam_mode == CAM_ORBIT ? "orbit" : "free "),
          (int)orbit_dist, (int)(int16_t)orbit_yaw, (int)(int16_t)orbit_pitch,
          parts, tris, level_of_detail,
          game_version == GAME_VERSION_E2 ? pal_cam : 0,
          pin_root ? "pin" : "   ");
}

/* ── Image mode ──────────────────────────────────────────────────
 * The game's raw art, read from the asset directories of every data root:
 * views (VIEWS 320x200, HIRES 640x480; a picture plus a depth plane), the
 * interface and title art in GRAPHICS / LOWGRAPH / the root, and the camera
 * palettes beside the views. */

enum { IMG_PAL_AUTO, IMG_PAL_FILE, IMG_PAL_DEFAULT, IMG_PAL_CAMERA, IMG_PAL_GREY,
       IMG_PAL_COUNT };
static const char *const img_pal_names[IMG_PAL_COUNT] = {
    "auto", "file", "default", "camera", "grey"
};

#define IMG_DIRS_MAX 16
#define IMG_ZOOM_MAX 4

typedef struct {
    char label[40];
    char path[512];
    int  first, count;
    int  sel;          /* remembered when switching back */
    bool views;        /* VIEWS / HIRES: files numbered by camera */
    bool root;
} img_dir_t;

typedef struct {
    int16_t dir;
    char    name[14];
} img_file_t;

typedef struct {
    int      w, h;
    uint8_t *pix;
    uint8_t *depth;           /* depth plane as a grey ramp, NULL if none */
    bool     view;            /* a background, drawn in its camera's palette */
    bool     has_pal;
    palette_entry_t pal[256];
    const char *format;
    char     error[64];
} img_t;

static img_dir_t   img_dirs[IMG_DIRS_MAX];
static int         img_dir_count;
static int         img_dir_sel;
static img_file_t *img_files;
static int         img_file_count;
static int         img_file_cap;
static int         img_file_top;
static int         img_dir_top;
static bool        img_listed;

static img_t       img;
static int         img_frame;          /* 0 picture, 1 depth */
static int         img_pal_src = IMG_PAL_AUTO;
static int         img_pal_cam = -1;
static char        img_pal_label[48];
static int         img_zoom;           /* 0 fits the screen */
static int         img_pan_x, img_pan_y;
static int         img_scale_fx = 0x10000;
static bool        img_show_grid;
static bool        cam_has_pal[PALETTES_MAX];

static palette_entry_t default_cmap[256];   /* colour_map as init loaded it */
static palette_entry_t img_cmap[256];

static bool has_ext(const char *name, const char *ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n > e && strcasecmp(name + n - e, ext) == 0;
}

static bool is_palette_file(const char *name) {
    return has_ext(name, ".PA2") || has_ext(name, ".PAL");
}

static bool is_gws(const uint8_t *p, size_t len) {
    return len >= 32 + 768 && memcmp(p, "mhwanh", 6) == 0;
}

static void img_collect(void *ctx, const char *name) {
    img_dir_t *d = (img_dir_t *)ctx;
    if (strlen(name) >= sizeof(img_files[0].name)) return;
    if (!has_ext(name, ".RAW") && !(d->views && is_palette_file(name))) return;

    /* The root holds other .RAW-named data; only Graphic Workshop art counts. */
    if (d->root) {
        char path[600];
        uint8_t head[32 + 768];
        snprintf(path, sizeof(path), "%s/%s", d->path, name);
        FILE *f = fopen(path, "rb");
        if (!f) return;
        size_t got = fread(head, 1, sizeof(head), f);
        fclose(f);
        if (!is_gws(head, got)) return;
    }

    if (img_file_count == img_file_cap) {
        int cap = img_file_cap ? img_file_cap * 2 : 1024;
        img_file_t *grown = (img_file_t *)realloc(img_files, cap * sizeof(img_file_t));
        if (!grown) return;
        img_files = grown;
        img_file_cap = cap;
    }
    img_file_t *e = &img_files[img_file_count++];
    e->dir = (int16_t)(d - img_dirs);
    snprintf(e->name, sizeof(e->name), "%s", name);
}

static int img_file_cmp(const void *a, const void *b) {
    return strcasecmp(((const img_file_t *)a)->name, ((const img_file_t *)b)->name);
}

static int img_camera_of(const img_file_t *f) {
    if (!f || !img_dirs[f->dir].views) return -1;
    for (int i = 0; i < 4; i++)
        if (f->name[i] < '0' || f->name[i] > '9') return -1;
    int cam = atoi(f->name);
    return cam < PALETTES_MAX ? cam : -1;
}

static void build_image_list(void) {
    static const struct { const char *dir; bool views; } kinds[] = {
        { "VIEWS", true }, { "HIRES", true }, { "GRAPHICS", false },
        { "LOWGRAPH", false }, { "", false },
    };
    const char *roots[2];
    int nroots = file_asset_roots(roots);

    img_listed = true;
    for (int r = 0; r < nroots; r++) {
        for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
            if (img_dir_count == IMG_DIRS_MAX) break;
            img_dir_t *d = &img_dirs[img_dir_count];
            memset(d, 0, sizeof(*d));
            d->views = kinds[k].views;
            d->root = !kinds[k].dir[0];
            if (r == 0)
                snprintf(d->label, sizeof(d->label), "%s", d->root ? "/" : kinds[k].dir);
            else
                snprintf(d->label, sizeof(d->label), "%s/%s", roots[r], kinds[k].dir);
            d->first = img_file_count;
            if (!file_list_dir(roots[r], kinds[k].dir, d->path, sizeof(d->path),
                               img_collect, d))
                continue;
            d->count = img_file_count - d->first;
            if (!d->count) continue;
            qsort(img_files + d->first, d->count, sizeof(img_file_t), img_file_cmp);
            img_dir_count++;
        }
    }

    /* E2 keeps the camera palettes in FILES/ECSTATIC, the intro's beside the
     * views as .PA2. E1 has neither and draws every view in colour_map. */
    if (game_version != GAME_VERSION_E2) return;
    for (int c = 0; c < PALETTES_MAX; c++)
        cam_has_pal[c] = palette_offset[c] >= 0;
    for (int i = 0; i < img_file_count; i++) {
        int cam = img_camera_of(&img_files[i]);
        if (cam >= 0 && has_ext(img_files[i].name, ".PA2")) cam_has_pal[cam] = true;
    }
}

static img_file_t *img_current(void) {
    if (img_dir_sel < 0 || img_dir_sel >= img_dir_count) return NULL;
    img_dir_t *d = &img_dirs[img_dir_sel];
    if (d->sel < 0 || d->sel >= d->count) return NULL;
    return &img_files[d->first + d->sel];
}

static uint8_t *read_whole(const char *path, size_t *len) {
    *len = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = n > 0 ? (uint8_t *)malloc((size_t)n) : NULL;
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf) *len = (size_t)n;
    return buf;
}

/* Pixels an unpack_bitmap (mask 0) or unpack_mask (mask 1) stream produces, or
 * -1 if it runs off the end: both unpackers trust their input. */
static long packed_length(const uint8_t *p, size_t len, int mask) {
    size_t o = 0;
    long n = 0;
    for (;;) {
        if (o >= len) return -1;
        int hdr = p[o++];
        int count = hdr >> 2, type = hdr & 3;
        if (!count) return n;
        size_t in;
        if (type == 0)  in = (size_t)(count + 1) / 2;
        else if (!mask) in = (type == 2) ? (size_t)count : 1;
        else            in = (type == 1) ? (size_t)count : (type == 2) ? 2u * count : 2;
        o += in;
        if (o > len) return -1;
        n += count;
    }
}

/* A packed view stores no size; the pixel count gives it. */
static bool view_dims(long n, int *w, int *h) {
    if (n == 320L * 200)        { *w = 320; *h = 200; return true; }
    if (n > 0 && n % 640 == 0)  { *w = 640; *h = (int)(n / 640); return true; }
    if (n > 0 && n % 320 == 0)  { *w = 320; *h = (int)(n / 320); return true; }
    return false;
}

/* Near is bright. 0x7FFF is what rebuild_background() writes for "nothing
 * here", so the top of the range is left black. */
static void depth_to_grey(const int16_t *z, int n) {
    int lo = 32767, hi = -32768;
    for (int i = 0; i < n; i++) {
        if (z[i] >= 0x7FF0) continue;
        if (z[i] < lo) lo = z[i];
        if (z[i] > hi) hi = z[i];
    }
    int span = hi > lo ? hi - lo : 1;
    for (int i = 0; i < n; i++)
        img.depth[i] = (z[i] >= 0x7FF0) ? 0 : (uint8_t)(250 - (z[i] - lo) * 249 / span);
}

static void attach_depth(const int16_t *z, int n) {
    img.depth = (uint8_t *)malloc((size_t)n);
    if (img.depth) depth_to_grey(z, n);
}

static void img_decode(const uint8_t *buf, size_t len, const img_file_t *f) {
    bool in_views = img_dirs[f->dir].views;

    if (is_palette_file(f->name)) {
        /* load_palette()'s layout: signature, 24 bytes of cycling, 6-bit RGB. */
        if (len < 2 + 24 + 768) {
            snprintf(img.error, sizeof(img.error), "short palette (%u bytes)", (unsigned)len);
            return;
        }
        img.w = img.h = 16;
        img.pix = (uint8_t *)malloc(256);
        if (!img.pix) return;
        for (int i = 0; i < 256; i++) {
            const uint8_t *c = buf + 26 + i * 3;
            img.pix[i] = (uint8_t)i;
            img.pal[i] = pal_rgb(c[0] & 63, c[1] & 63, c[2] & 63);
        }
        img.has_pal = true;
        img.format = "camera palette";
        return;
    }

    if (is_gws(buf, len)) {
        /* load_raw_graphic()'s Graphic Workshop RAW: big-endian header, 8-bit
         * RGB, pixels. E1's unpacked views (load_raw()'s "mh") add a 16-bit
         * depth plane. */
        const bitmap_hdr_t *hdr = (const bitmap_hdr_t *)buf;
        int w = reverse_char_word_val(hdr->size_x);
        int h = reverse_char_word_val(hdr->size_y);
        size_t n = (size_t)w * (size_t)h;
        if (w <= 0 || h <= 0 || len < 800 + n) {
            snprintf(img.error, sizeof(img.error), "bad GWS header %dx%d", w, h);
            return;
        }
        img.w = w;
        img.h = h;
        img.pix = (uint8_t *)malloc(n);
        if (!img.pix) return;
        memcpy(img.pix, buf + 800, n);
        for (int i = 0; i < 256; i++) {
            const uint8_t *c = buf + 32 + i * 3;
            img.pal[i] = pal_rgb(c[0] >> 2, c[1] >> 2, c[2] >> 2);
        }
        img.has_pal = true;
        img.view = in_views;
        img.format = "GWS raw";
        if (in_views && len >= 800 + 3 * n) {
            int16_t *z = (int16_t *)malloc(n * 2);
            if (z) {
                memcpy(z, buf + 800 + n, n * 2);
                attach_depth(z, (int)n);
                free(z);
            }
            img.format = "GWS raw view";
        }
        return;
    }

    /* load_raw()'s packed view: signature, the two stream sizes, the picture
     * stream, the depth stream. E1 appends 3 KB of camera data, unused here. */
    int32_t bg_size = 0, hmap_size = 0;
    if (len >= 10) {
        memcpy(&bg_size, buf + 2, 4);
        memcpy(&hmap_size, buf + 6, 4);
    }
    if (bg_size <= 0 || hmap_size <= 0 || (size_t)bg_size + (size_t)hmap_size + 10 > len) {
        snprintf(img.error, sizeof(img.error), "unknown format");
        return;
    }
    long n = packed_length(buf + 10, (size_t)bg_size, 0);
    long zn = packed_length(buf + 10 + bg_size, (size_t)hmap_size, 1);
    int w, h;
    if (n < 0 || !view_dims(n, &w, &h)) {
        snprintf(img.error, sizeof(img.error), "bad packed stream (%ld px)", n);
        return;
    }
    img.w = w;
    img.h = h;
    img.view = true;
    img.format = "packed view";
    img.pix = (uint8_t *)malloc((size_t)n);
    if (!img.pix) return;
    unpack_bitmap((char *)img.pix, (char *)buf + 10);
    if (zn == n) {
        int16_t *z = (int16_t *)malloc((size_t)n * 2);
        if (z) {
            unpack_mask(z, (char *)buf + 10 + bg_size);
            attach_depth(z, (int)n);
            free(z);
        }
    }
}

static void grey_ramp(palette_entry_t *out) {
    for (int i = 0; i < 256; i++) {
        uint8_t v = (uint8_t)(i > 250 ? 63 : i * 63 / 250);
        out[i] = pal_rgb(v, v, v);
    }
}

/* What load_palette() gives the camera: its .PA2 (read while intro_flag is
 * set) or its slot in FILES/ECSTATIC, else colour_map. */
static const char *camera_palette(int cam, palette_entry_t *out) {
    if (game_version != GAME_VERSION_E2 || cam < 0 || cam >= PALETTES_MAX ||
        !cam_has_pal[cam]) {
        memcpy(out, default_cmap, sizeof(default_cmap));
        return NULL;
    }
    char pa2[24];
    snprintf(pa2, sizeof(pa2), "VIEWS/%04d.PA2", cam);
    FILE *f = fopen_ci(pa2, "rb");
    bool intro = f != NULL;
    if (f) fclose(f);

    int16_t save_cam = selected_camera;
    bool save_intro = intro_flag;
    /* load_palette() keeps entries 8-15 of colour_map. */
    memcpy(colour_map, default_cmap, sizeof(default_cmap));
    selected_camera = (int16_t)cam;
    intro_flag = intro;
    load_palette(NULL);
    selected_camera = save_cam;
    intro_flag = save_intro;
    memcpy(out, view_cmap, sizeof(default_cmap));
    return intro ? "(.PA2)" : "(FILES/ECSTATIC)";
}

static void img_resolve_palette(void) {
    const char *src_name = img_pal_names[img_pal_src];
    if (img_frame == 1) {
        grey_ramp(img_cmap);
        snprintf(img_pal_label, sizeof(img_pal_label), "depth ramp");
        return;
    }

    int src = img_pal_src;
    if (src == IMG_PAL_AUTO) {
        /* As the game draws it: a view in its camera's palette (E1: always
         * colour_map, it ignores the copy in an "mh" view), any other art in
         * the palette stored with it. */
        if (img.view)
            src = (game_version == GAME_VERSION_E2) ? IMG_PAL_CAMERA : IMG_PAL_DEFAULT;
        else
            src = img.has_pal ? IMG_PAL_FILE : IMG_PAL_DEFAULT;
    }

    int cam = (img_pal_src == IMG_PAL_CAMERA && img_pal_cam >= 0)
            ? img_pal_cam : img_camera_of(img_current());
    const char *from;
    switch (src) {
    case IMG_PAL_FILE:
        if (img.has_pal) {
            memcpy(img_cmap, img.pal, sizeof(img_cmap));
            snprintf(img_pal_label, sizeof(img_pal_label), "%s: own palette", src_name);
            break;
        }
        /* fall through */
    case IMG_PAL_DEFAULT:
        memcpy(img_cmap, default_cmap, sizeof(img_cmap));
        snprintf(img_pal_label, sizeof(img_pal_label), "%s: PALLETTE.RAW", src_name);
        break;
    case IMG_PAL_CAMERA:
        from = camera_palette(cam, img_cmap);
        if (from)
            snprintf(img_pal_label, sizeof(img_pal_label), "%s: camera %d %s",
                     src_name, cam, from);
        else
            snprintf(img_pal_label, sizeof(img_pal_label), "%s: camera %d none, PALLETTE.RAW",
                     src_name, cam);
        break;
    default:
        grey_ramp(img_cmap);
        snprintf(img_pal_label, sizeof(img_pal_label), "%s ramp", src_name);
        break;
    }
}

static void img_load_current(void) {
    free(img.pix);
    free(img.depth);
    memset(&img, 0, sizeof(img));
    img_frame = 0;
    img_pan_x = img_pan_y = 0;

    img_file_t *f = img_current();
    if (f) {
        char path[600];
        size_t len;
        snprintf(path, sizeof(path), "%s/%s", img_dirs[f->dir].path, f->name);
        uint8_t *buf = read_whole(path, &len);
        if (buf) img_decode(buf, len, f);
        else     snprintf(img.error, sizeof(img.error), "cannot read");
        free(buf);
        if (!img.pix && !img.error[0])
            snprintf(img.error, sizeof(img.error), "out of memory");
    }
    img_resolve_palette();
}

/* Camera palettes in order, for N / M. */
static int img_step_camera(int from, int dir) {
    for (int n = 0; n < PALETTES_MAX; n++) {
        from += dir;
        if (from >= PALETTES_MAX) from = 0;
        if (from < 0) from = PALETTES_MAX - 1;
        if (cam_has_pal[from]) return from;
    }
    return from;
}

static void images_enter(void) {
    if (!img_listed) {
        build_image_list();
        img_load_current();
    } else {
        img_resolve_palette();
    }
}

/* colour_map is the base every other mode draws through. */
static void images_leave(void) {
    memcpy(colour_map, default_cmap, sizeof(default_cmap));
    memcpy(view_cmap, default_cmap, sizeof(default_cmap));
    memcpy(fade_cmap, default_cmap, sizeof(default_cmap));
    inject_palette();
}

/* ── Image drawing ── */

static void img_blit(int plane) {
    char *dst = bitmap[plane];
    if (!dst) return;
    int sw = screen_width, sh = screen_height;

    /* Outside the image: the background slot, or the darkest colour when the
     * panels are hidden and every palette entry is the image's own. */
    int fill = COL_BG;
    if (panels_hidden) {
        int best = 1 << 30;
        for (int i = 0; i < 256; i++) {
            int l = img_cmap[i].R * 3 + img_cmap[i].G * 6 + img_cmap[i].B;
            if (l < best) { best = l; fill = i; }
        }
    }
    memset(dst, fill, (size_t)sw * sh);

    const uint8_t *src = (img_frame == 1 && img.depth) ? img.depth : img.pix;
    if (!src || img.w <= 0 || img.h <= 0) return;

    int64_t s;
    if (img_zoom == 0) {
        int64_t sx = ((int64_t)sw << 16) / img.w;
        int64_t sy = ((int64_t)sh << 16) / img.h;
        s = sx < sy ? sx : sy;
        if (s >= 0x10000) s &= ~(int64_t)0xFFFF;   /* whole steps stay crisp */
    } else {
        s = (int64_t)img_zoom << 16;
    }
    if (s < 1) s = 1;
    img_scale_fx = (int)s;

    int dw = (int)((img.w * s) >> 16), dh = (int)((img.h * s) >> 16);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;

    int ox, oy;
    if (dw > sw) {
        img_pan_x = clampi(img_pan_x, 0, dw - sw);
        ox = -img_pan_x;
    } else {
        img_pan_x = 0;
        ox = (sw - dw) / 2;
    }
    if (dh > sh) {
        img_pan_y = clampi(img_pan_y, 0, dh - sh);
        oy = -img_pan_y;
    } else {
        img_pan_y = 0;
        oy = (sh - dh) / 2;
    }

    int x0 = ox > 0 ? ox : 0, x1 = ox + dw < sw ? ox + dw : sw;
    int y0 = oy > 0 ? oy : 0, y1 = oy + dh < sh ? oy + dh : sh;
    for (int y = y0; y < y1; y++) {
        const uint8_t *row = src + (size_t)((int64_t)(y - oy) * img.h / dh) * img.w;
        char *out = dst + (size_t)y * sw;
        for (int x = x0; x < x1; x++)
            out[x] = (char)row[(int64_t)(x - ox) * img.w / dw];
    }
}

static void draw_palette_grid(int plane) {
    int cell = tx_h;
    int size = cell * 16;
    int gx = 4;
    int gy = screen_height - size - tx_h - 6;
    if (gx < 0 || gy < 0) return;
    int16_t save = a_pen_colour;
    for (int i = 0; i < 256; i++) {
        a_pen_colour = (int16_t)i;
        rect_fill(plane, gx + (i & 15) * cell, gy + (i >> 4) * cell, cell, cell);
    }
    a_pen_colour = save;
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static void img_dir_label(int i, char *out, int n) {
    snprintf(out, n, "%-15.15s %5d", img_dirs[i].label, img_dirs[i].count);
}

static void img_file_label(int i, char *out, int n) {
    const img_dir_t *d = &img_dirs[img_dir_sel];
    snprintf(out, n, "%s", img_files[d->first + i].name);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

static void draw_images_hud(void) {
    /* Hidden panels hand the reserved slots back to the image. */
    if (panels_hidden) return;
    inject_palette();

    const char *gv = (game_version == GAME_VERSION_E1) ? "E1" : "E2";
    img_file_t *f = img_current();
    const img_dir_t *d = f ? &img_dirs[f->dir] : NULL;

    vtext(2, 2, COL_HILITE, COL_PANEL, "%s IMAGE %s%s%s", gv,
          d ? d->label : "(no images)",
          d && d->label[strlen(d->label) - 1] != '/' ? "/" : "", f ? f->name : "");
    if (img.error[0]) {
        vtext(2, 2 + tx_h, COL_HILITE, COL_PANEL, "%s", img.error);
    } else if (img.pix) {
        vtext(2, 2 + tx_h, COL_TEXT, COL_PANEL, "%dx%d %s  %s %d/%d",
              img.w, img.h, img.format,
              img_frame ? "depth" : "picture", img_frame + 1, img.depth ? 2 : 1);
    }
    vtext(2, 2 + 2 * tx_h, COL_TEXT, COL_PANEL, "pal %s", img_pal_label);

    if (show_help) {
        static const char *const lines[] = {
            "  IMAGE VIEWER",
            "",
            "  V            switch MODELS -> SCENES -> IMAGES",
            "  LEFT/RIGHT   switch pane (directories <-> files)",
            "  UP/DOWN      move selection    PGUP/PGDN page",
            "  HOME/END     first / last",
            "  [ ]          previous / next directory",
            "  , .          previous / next image",
            "  TAB          picture <-> depth plane (views)",
            "",
            "  P            palette: auto / file / default /",
            "               camera / grey",
            "  N M          previous / next camera palette (E2)",
            "  G            show the palette",
            "  Z            zoom: fit, 1x, 2x, 3x, 4x",
            "  W A S D      pan a zoomed image",
            "  B            background colour",
            "  X            hide everything (exact palette)",
            "  H            this help         ESC  quit",
            NULL,
        };
        draw_help_lines(lines);
        return;
    }

    int cols = screen_width / tx_w;
    int rows_total = screen_height / tx_h;
    if (cols >= 60) {
        int list_rows = rows_total - 9;
        if (list_rows > 44) list_rows = 44;
        if (list_rows < 4) list_rows = 4;
        int lcols = 22, rcols = 14;
        int top_y = 5 * tx_h;
        int files = img_dir_count ? img_dirs[img_dir_sel].count : 0;
        int fsel = img_dir_count ? img_dirs[img_dir_sel].sel : 0;

        img_dir_top = clampi(img_dir_top, 0,
                             img_dir_count > list_rows ? img_dir_count - list_rows : 0);
        if (img_dir_sel < img_dir_top) img_dir_top = img_dir_sel;
        if (img_dir_sel >= img_dir_top + list_rows) img_dir_top = img_dir_sel - list_rows + 1;

        img_file_top = clampi(img_file_top, 0, files > list_rows ? files - list_rows : 0);
        if (fsel < img_file_top) img_file_top = fsel;
        if (fsel >= img_file_top + list_rows) img_file_top = fsel - list_rows + 1;

        char title[64];
        snprintf(title, sizeof(title), "DIRS %d/%d",
                 img_dir_count ? img_dir_sel + 1 : 0, img_dir_count);
        draw_list(2, top_y, lcols, list_rows, img_dir_count, img_dir_sel, img_dir_top,
                  pane == PANE_MODELS, title, img_dir_label);

        snprintf(title, sizeof(title), "FILES %d/%d", files ? fsel + 1 : 0, files);
        draw_list(screen_width - (rcols * tx_w + 6), top_y, rcols, list_rows,
                  files, fsel, img_file_top, pane == PANE_ANIMS, title, img_file_label);
    }

    int pct = (int)((int64_t)img_scale_fx * 100 >> 16);
    vtext(2, screen_height - tx_h - 2, COL_DIM, COL_PANEL,
          "%s %d%% pan %d,%d  P pal  TAB frame  X hide  H=help",
          img_zoom ? "zoom" : "fit ", pct, img_pan_x, img_pan_y);

    if (img_show_grid) draw_palette_grid(hud_plane());
}

static void render_images_frame(void) {
    /* Every frame: draw_images_hud() overwrites the reserved slots, and E1's
     * show_parts() presents colour_map. */
    memcpy(view_cmap, img_cmap, sizeof(img_cmap));
    memcpy(colour_map, img_cmap, sizeof(img_cmap));
    memcpy(fade_cmap, img_cmap, sizeof(img_cmap));
    set_palette_flag = 1;

    img_blit(hud_plane());
    draw_images_hud();
    show_parts();
}

/* ── Image input ── */

static void img_select_dir(int to) {
    if (!img_dir_count) return;
    to = clampi(to, 0, img_dir_count - 1);
    if (to == img_dir_sel) return;
    img_dir_sel = to;
    img_file_top = 0;
    img_load_current();
}

static void img_select_file(int delta) {
    if (!img_dir_count) return;
    img_dir_t *d = &img_dirs[img_dir_sel];
    if (!d->count) return;
    int to = clampi(d->sel + delta, 0, d->count - 1);
    if (to == d->sel) return;
    d->sel = to;
    img_load_current();
}

static void images_input(int dt) {
    if (platform_key_hit(plat, PKEY_LEFT))  pane = PANE_MODELS;
    if (platform_key_hit(plat, PKEY_RIGHT)) pane = PANE_ANIMS;

    static const struct { int key, delta; } moves[] = {
        { PKEY_UP, -1 }, { PKEY_DOWN, +1 }, { PKEY_PGUP, -16 }, { PKEY_PGDN, +16 },
        { PKEY_HOME, -1000000 }, { PKEY_END, +1000000 },
    };
    for (size_t i = 0; i < sizeof(moves) / sizeof(moves[0]); i++) {
        if (!platform_key_hit(plat, moves[i].key)) continue;
        if (pane == PANE_MODELS) img_select_dir(img_dir_sel + moves[i].delta);
        else                     img_select_file(moves[i].delta);
    }

    if (platform_key_hit(plat, PKEY_LBRACKET)) img_select_dir(img_dir_sel - 1);
    if (platform_key_hit(plat, PKEY_RBRACKET)) img_select_dir(img_dir_sel + 1);
    if (platform_key_hit(plat, PKEY_COMMA))    img_select_file(-1);
    if (platform_key_hit(plat, PKEY_PERIOD))   img_select_file(+1);

    bool repal = false;
    if (platform_key_hit(plat, PKEY_TAB) && img.depth) {
        img_frame ^= 1;
        repal = true;
    }
    if (platform_key_hit(plat, PKEY_P)) {
        do {
            img_pal_src = (img_pal_src + 1) % IMG_PAL_COUNT;
        } while ((img_pal_src == IMG_PAL_FILE && !img.has_pal) ||
                 (img_pal_src == IMG_PAL_CAMERA && game_version != GAME_VERSION_E2));
        if (img_pal_src == IMG_PAL_CAMERA && img_pal_cam < 0) {
            int cam = img_camera_of(img_current());
            img_pal_cam = (cam >= 0 && cam_has_pal[cam]) ? cam : img_step_camera(-1, +1);
        }
        repal = true;
    }
    if (game_version == GAME_VERSION_E2) {
        int dir = platform_key_hit(plat, PKEY_N) ? -1 : platform_key_hit(plat, PKEY_M) ? 1 : 0;
        if (dir) {
            int from = img_pal_cam >= 0 ? img_pal_cam : img_camera_of(img_current());
            img_pal_cam = img_step_camera(from, dir);
            img_pal_src = IMG_PAL_CAMERA;
            repal = true;
        }
    }
    if (repal) img_resolve_palette();

    if (platform_key_hit(plat, PKEY_Z)) img_zoom = (img_zoom + 1) % (IMG_ZOOM_MAX + 1);
    if (platform_key_hit(plat, PKEY_G)) img_show_grid = !img_show_grid;
    if (platform_key_hit(plat, PKEY_B)) bg_choice = (bg_choice + 1) % BG_CHOICE_COUNT;

    int step = 6 * (dt > 0 ? dt : 1);
    if (platform_key_down(plat, PKEY_A)) img_pan_x -= step;
    if (platform_key_down(plat, PKEY_D)) img_pan_x += step;
    if (platform_key_down(plat, PKEY_W)) img_pan_y -= step;
    if (platform_key_down(plat, PKEY_S)) img_pan_y += step;
    if (img_pan_x < 0) img_pan_x = 0;
    if (img_pan_y < 0) img_pan_y = 0;
}

/* ── Input ───────────────────────────────────────────────────── */

static void move_selection(int delta) {
    if (tool_mode == TOOL_SCENES) {
        if (pane == PANE_MODELS) {
            if (!scene_count) return;
            scene_sel = clampi(scene_sel + delta, 0, scene_count - 1);
        } else {
            int cast = scene_cast_count(cur_scene);
            if (!cast) return;
            int was = cast_sel;
            cast_sel = clampi(cast_sel + delta, 0, cast - 1);
            /* The detached camera follows the selection. */
            if (cast_sel != was && !cam_attached) fit_camera_cast();
        }
        return;
    }
    if (pane == PANE_MODELS) {
        if (!model_count) return;
        model_sel = clampi(model_sel + delta, 0, model_count - 1);
    } else {
        if (!anim_count) return;
        anim_sel = clampi(anim_sel + delta, 0, anim_count - 1);
    }
}

static void activate_selection(void) {
    if (tool_mode == TOOL_SCENES) {
        if (pane == PANE_MODELS) {
            load_scene(scene_sel);
            playing = true;
        } else {
            /* RETURN detaches onto this actor; O restores the shot. */
            cam_attached = false;
            detach_scene_camera();
        }
        return;
    }
    if (pane == PANE_MODELS)
        load_model(model_sel);
    else if (anim_count)
        play_action(anim_list[anim_sel].action);
}

static void switch_tool(int to) {
    if (tool_mode == TOOL_IMAGES) images_leave();
    tool_mode = to;
    pane = PANE_MODELS;
    if (tool_mode == TOOL_IMAGES) {
        /* Nothing on the display list: the image is the whole frame. */
        script_mode = 0;
        stop_animation();
        unload_scene();
        if (cur_actor) { remove_from_display_list(cur_actor); cur_actor = NULL; }
        cur_thing = -1;
        root_thing = NULL;
        images_enter();
    } else if (tool_mode == TOOL_SCENES) {
        script_mode = 1;
        if (!cur_scene && scene_count) { load_scene(scene_sel); playing = true; }
        else if (cam_attached) attach_scene_camera();
    } else {
        /* Or the model shows up in the scene's shot with its cast. */
        script_mode = 0;
        unload_scene();
        rebuild_background();
        if (game_version == GAME_VERSION_E2) apply_scene_palette(pal_cam);
        load_model(model_sel);
    }
}

static void handle_input(int dt) {
    if (platform_key_hit(plat, PKEY_H)) show_help = !show_help;
    if (platform_key_hit(plat, PKEY_X)) panels_hidden = !panels_hidden;
    if (platform_key_hit(plat, PKEY_V)) switch_tool((tool_mode + 1) % TOOL_COUNT);

    if (tool_mode == TOOL_IMAGES) {
        images_input(dt);
        return;
    }

    if (platform_key_hit(plat, PKEY_LEFT))  pane = PANE_MODELS;
    if (platform_key_hit(plat, PKEY_RIGHT)) pane = PANE_ANIMS;

    if (platform_key_hit(plat, PKEY_UP))   move_selection(-1);
    if (platform_key_hit(plat, PKEY_DOWN)) move_selection(+1);
    if (platform_key_hit(plat, PKEY_PGUP)) move_selection(-16);
    if (platform_key_hit(plat, PKEY_PGDN)) move_selection(+16);
    if (platform_key_hit(plat, PKEY_HOME)) move_selection(-1000000);
    if (platform_key_hit(plat, PKEY_END))  move_selection(+1000000);

    if (platform_key_hit(plat, PKEY_RETURN)) activate_selection();

    if (tool_mode == TOOL_MODELS &&
        platform_key_hit(plat, PKEY_TAB) && scan_pos < 0) scan_begin();

    /* Lets a headless scripted run start the scan without key events. */
    { static int nf = 0;
      if (getenv("ECSTATICA_VIEWER_AUTOSCAN") && ++nf == 60 && scan_pos < 0)
          scan_begin(); }

    if (platform_key_hit(plat, PKEY_SPACE)) playing = !playing;
    if (platform_key_hit(plat, PKEY_L))     looping = !looping;
    if (platform_key_hit(plat, PKEY_T)) {
        if (tool_mode == TOOL_SCENES) {
            restart_scene();
            playing = true;
        } else if (cur_action_idx >= 0) {
            play_action(cur_action_idx);
            playing = true;
        }
    }
    if (platform_key_hit(plat, PKEY_MINUS))  speed_pct = clampi(speed_pct - 10, 10, 400);
    if (platform_key_hit(plat, PKEY_EQUALS)) speed_pct = clampi(speed_pct + 10, 10, 400);
    if (platform_key_hit(plat, PKEY_F))      stretch_short = !stretch_short;

    if (platform_key_hit(plat, PKEY_COMMA)) {
        playing = false;
        if (tool_mode == TOOL_SCENES) scene_seek(scene_phase - scene_len / 32 - 1);
        else phase = clampi(phase - phase_max / 32 - 1, 0, phase_max);
    }
    if (platform_key_hit(plat, PKEY_PERIOD)) {
        playing = false;
        if (tool_mode == TOOL_SCENES) scene_seek(scene_phase + scene_len / 32 + 1);
        else phase = clampi(phase + phase_max / 32 + 1, 0, phase_max);
    }

    if (platform_key_hit(plat, PKEY_O)) {
        if (tool_mode == TOOL_SCENES) {
            cam_attached = !cam_attached && scene_has_shot(cur_scene);
            if (cam_attached) attach_scene_camera();
            else              detach_scene_camera();
        } else {
            cam_mode ^= 1;
        }
    }
    if (platform_key_hit(plat, PKEY_R)) fit_camera();
    if (platform_key_hit(plat, PKEY_Z)) auto_spin = !auto_spin;
    if (platform_key_hit(plat, PKEY_P)) {
        pin_root = !pin_root;
        if (pin_root && cur_actor) {
            cur_actor->position_vector.X = 0;
            cur_actor->position_vector.Z = 0;
        }
    }
    if (platform_key_hit(plat, PKEY_B)) bg_choice = (bg_choice + 1) % BG_CHOICE_COUNT;

    if (platform_key_hit(plat, PKEY_1)) level_of_detail = 0;
    if (platform_key_hit(plat, PKEY_2)) level_of_detail = 1;
    if (platform_key_hit(plat, PKEY_3)) level_of_detail = 2;

    if (game_version == GAME_VERSION_E2) {
        if (platform_key_hit(plat, PKEY_N)) apply_scene_palette(step_scene_palette(pal_cam, -1));
        if (platform_key_hit(plat, PKEY_M)) apply_scene_palette(step_scene_palette(pal_cam, +1));
    }

    /* Scaled by frame time. Not while a scene's own camera is attached. */
    if (tool_mode == TOOL_SCENES && cam_attached) return;

    int step = dt > 0 ? dt : 1;
    int32_t rot = 240 * step;
    int32_t dolly = (orbit_dist / 24 + 4) * step;

    bool w = platform_key_down(plat, PKEY_W);
    bool s = platform_key_down(plat, PKEY_S);
    bool a = platform_key_down(plat, PKEY_A);
    bool d = platform_key_down(plat, PKEY_D);
    bool q = platform_key_down(plat, PKEY_Q);
    bool e = platform_key_down(plat, PKEY_E);

    if (cam_mode == CAM_ORBIT) {
        if (a) orbit_yaw -= rot;
        if (d) orbit_yaw += rot;
        if (w) orbit_pitch -= rot;
        if (s) orbit_pitch += rot;
        if (q) orbit_dist = clampi((int)(orbit_dist - dolly), 40, 30000);
        if (e) orbit_dist = clampi((int)(orbit_dist + dolly), 40, 30000);
    } else {
        vector_t right, up, fwd;
        camera_basis(&right, &up, &fwd);
        int32_t mv = dolly;
        int32_t dx = 0, dy = 0, dz = 0;
        if (w) { dx += (int32_t)fwd.X * mv >> 14; dy += (int32_t)fwd.Y * mv >> 14; dz += (int32_t)fwd.Z * mv >> 14; }
        if (s) { dx -= (int32_t)fwd.X * mv >> 14; dy -= (int32_t)fwd.Y * mv >> 14; dz -= (int32_t)fwd.Z * mv >> 14; }
        if (d) { dx += (int32_t)right.X * mv >> 14; dy += (int32_t)right.Y * mv >> 14; dz += (int32_t)right.Z * mv >> 14; }
        if (a) { dx -= (int32_t)right.X * mv >> 14; dy -= (int32_t)right.Y * mv >> 14; dz -= (int32_t)right.Z * mv >> 14; }
        if (e) { dx += (int32_t)up.X * mv >> 14; dy += (int32_t)up.Y * mv >> 14; dz += (int32_t)up.Z * mv >> 14; }
        if (q) { dx -= (int32_t)up.X * mv >> 14; dy -= (int32_t)up.Y * mv >> 14; dz -= (int32_t)up.Z * mv >> 14; }
        view_pos.X = (int16_t)clampi(view_pos.X + dx, -32000, 32000);
        view_pos.Y = (int16_t)clampi(view_pos.Y + dy, -32000, 32000);
        view_pos.Z = (int16_t)clampi(view_pos.Z + dz, -32000, 32000);
    }

    /* Left drag orbits, right drag dollies. Yaw is inverted so the drag grabs
     * the model; pitch is not. */
    static int prev_mx, prev_my, prev_mb;
    int mx, my;
    int mb = platform_mouse_state(plat, &mx, &my);
    if (mb && prev_mb) {
        int ddx = mx - prev_mx, ddy = my - prev_my;
        if (mb & PMOUSE_LEFT) {
            orbit_yaw -= ddx * 180;
            orbit_pitch += ddy * 180;
        }
        if (mb & PMOUSE_RIGHT)
            orbit_dist = clampi((int)(orbit_dist + ddy * 8), 40, 30000);
    }
    prev_mx = mx; prev_my = my; prev_mb = mb;

    if (auto_spin) orbit_yaw += 90 * step;

    orbit_pitch = clampi((int)orbit_pitch, -0x3F00, 0x3F00);
    orbit_yaw &= 0xFFFF;
}

/* ── Animation stepping ──────────────────────────────────────── */

/* my_time() rate: 60Hz for E1, 70Hz for E2, the unit of action durations. */
static int tick_rate(void) {
    return (game_version == GAME_VERSION_E1) ? 60 : 70;
}

/* Ticks one loop takes on screen. Actions shorter than min_loop_ms (E2's
 * herojump is 16 ticks) are stretched to it; the HUD shows SLOW, and F
 * restores game rate. */
static int effective_duration(void) {
    int d = cur_actor->actor_act.duration;
    if (d < 1) d = 1;
    if (!stretch_short) return d;
    int floor_ticks = tick_rate() * min_loop_ms / 1000;
    return d < floor_ticks ? floor_ticks : d;
}

static void advance_animation(int dt) {
    if (!cur_actor || !cur_action) return;

    act_t *act = &cur_actor->actor_act;
    int scaled = dt * speed_pct / 100;
    int real = act->duration > 0 ? act->duration : 1;
    int eff = effective_duration();

    if (playing && scaled > 0) {
        int adv;
        if (cur_action->action_flags & 2) {
            /* Scene actions are absolute time, so scale the step instead. */
            adv = (int)(((int64_t)scaled * real) / eff);
        } else {
            adv = (int)(((int64_t)scaled << 16) / eff);
        }
        if (adv < 1) adv = 1;
        phase += adv;
    }

    while (phase > phase_max) {
        if (!looping) {
            phase = phase_max;
            playing = false;
            break;
        }
        complete_act(act, cur_actor);
        phase -= phase_max + 1;
        if (phase < 0) phase = 0;
    }

    position_act(act, (uint16_t)phase, cur_actor);

    if (pin_root) {
        /* Horizontal only: Y carries jumps and crouches. */
        cur_actor->position_vector.X = 0;
        cur_actor->position_vector.Z = 0;
        set_vector(&cur_actor->actor_velocity, 0, 0, 0);
    }
}

/* ── Frame ───────────────────────────────────────────────────── */

static void render_frame(void) {
    /* 0x400 ("stuck") would bake the actor into the background store when an
     * act runs dry; 0x800 is the already-drawn gate. */
    for (actor_t *a = root_thing; a; a = a->next_in_display_list) {
        a->flags &= ~(uint16_t)(0x0400 | 0x0800);
        a->flags |= ACTOR_FLAG_VISIBLE;
        a->state_flags &= ~0x80;
    }

    /* Full restore: the HUD covers areas no actor claims. */
    background_status = 1;
    prepare_parts();
    clip_mask(1, 0, 0, 0, screen_width, screen_height);

    draw_parts();
    draw_hud();
    show_parts();
}

/* ── Entry point ─────────────────────────────────────────────── */

void viewer_main(void) {
    plat = win_platform();
    if (!plat) quit("Viewer: no platform");

    /* Keeps prepare_parts out of the camera maintenance and draw_stuck_parts
     * out entirely; both need a world. */
    editor_mode = 1;
    topography = 0;
    script_mode = 0;
    make_backgrounds = 0;
    moving_camera = 0;
    intro_flag = false;
    fade_in = false;
    fade_to_black = false;
    fade_to_white = false;
    joystick_control = false;
    music_on = false;
    sound_fx_on = false;
    active_camera = NULL;
    selected_thing = NULL;
    root_thing = NULL;
    root_scene = NULL;
    level_of_detail = 0;
    program_up_and_running = true;
    game_up_and_running = true;

    platform_set_title(plat, game_version == GAME_VERSION_E1
                             ? "Ecstatica — Viewer" : "Ecstatica II — Viewer");

    action_nrefs = (int16_t *)calloc(ACTION_TAB_SIZE, sizeof(int16_t));
    action_parts = (uint8_t *)calloc(ACTION_TAB_SIZE, PART_BITS);
    action_preexisting = (bool *)calloc(ACTION_TAB_SIZE, sizeof(bool));
    anim_list = (anim_entry_t *)calloc(ACTION_TAB_SIZE + 208, sizeof(anim_entry_t));
    if (!action_nrefs || !action_parts || !action_preexisting || !anim_list)
        quit("Viewer: out of memory");
    for (int i = 0; i < ACTION_TAB_SIZE; i++) action_nrefs[i] = SCAN_UNKNOWN;

    memcpy(default_cmap, colour_map, sizeof(default_cmap));

    build_model_list();
    build_scene_list();
    apply_scene_palette(step_scene_palette(0, +1));
    inject_palette();
    rebuild_background();

    tool_mode = (viewer_mode == VIEWER_SCENES) ? TOOL_SCENES
              : (viewer_mode == VIEWER_IMAGES) ? TOOL_IMAGES : TOOL_MODELS;
    if (tool_mode == TOOL_IMAGES) {
        images_enter();
    } else if (tool_mode == TOOL_SCENES) {
        script_mode = 1;
        if (scene_count) load_scene(0);
    } else if (model_count) {
        load_model(0);
    }

    last_tick = my_time();
    int last_bg = bg_choice;

    for (;;) {
        if (!platform_pump_events(plat)) break;

        /* platform_key_hit consumes the latch, so ESC is handled here only. */
        if (platform_key_hit(plat, PKEY_ESCAPE)) {
            if (show_help) show_help = false;
            else break;
        }

        int now = my_time();
        int dt = now - last_tick;
        if (dt < 0) dt = 0;
        if (dt > 12) dt = 12;      /* a stall must not jump the animation */
        last_tick = now;

        handle_input(dt);
        if (bg_choice != last_bg) {
            last_bg = bg_choice;
            rebuild_background();
        }

        if (tool_mode == TOOL_IMAGES) {
            render_images_frame();
            platform_delay(10);
            continue;
        }

        scan_step();
        if (tool_mode == TOOL_SCENES) {
            advance_scene(dt);
            if (!cam_attached) update_camera();
        } else {
            advance_animation(dt);
            update_camera();
        }

        if (tool_mode == TOOL_MODELS && cur_actor) prepare_an_actor(cur_actor);
        render_frame();

        platform_delay(10);
    }

    quit("");
}
