/**
 * viewer.c
 *
 * Model and animation browser, built on the game's own pipeline.
 *
 * Not part of the original binary. Everything it draws goes through
 * prepare_parts / draw_parts / show_parts exactly as gameplay does, so a
 * model looks here the way it looks in the game — the viewer only replaces
 * what the world normally supplies: the camera, the background, and the
 * decision about which thing and which action are live.
 *
 * Entered with --viewer, from setup(), after the archives are open.
 */

#include "viewer.h"

#include "anim.h"
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
 * Same convention as debug_overlay.c: the top of the scene palette is
 * black/unused in both games' view palettes, so the HUD claims a few
 * entries rather than hunting for readable colours in scene art. Injected
 * into view_cmap AND colour_map because E1's show_parts re-applies
 * colour_map on every flip while E2 uses view_cmap directly. */
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
enum { TOOL_MODELS = 0, TOOL_SCENES = 1 };

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
 * A scene is a set of scripts, one per actor, each an action with
 * action_flags & 2 — so update_act() runs them in absolute time to
 * act->duration and stops, rather than looping a fraction like a
 * repertoire action. Playback goes through the original editor's own
 * preview path, advance_selected_scene_or_action(), which walks
 * selected_scene's scripts when script_mode is set. */
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
/* Scenes are framed by an authored camera with a painted background.
 * Detaching swaps in the model viewer's orbit camera over a flat backdrop,
 * which is the only way to see staging the scene's own shot hides. */
static bool     cam_attached = true;

static actor_t *cur_actor;
static int16_t  cur_thing = -1;
static action_t *cur_action;
static int16_t   cur_action_idx = -1;

/* Phase through the running action. Units follow the action's own
 * convention, which position_act() switches on: scene actions (flags & 2)
 * are positioned in absolute time up to act->duration, everything else in
 * a 0..0xFFFF fraction of it. */
static int      phase;
static int      phase_max = 0xFFFF;
static bool     playing = true;
static bool     looping = true;
static int      speed_pct = 100;
/* Shortest a looping action is allowed to take on screen. See
 * effective_duration(). F toggles it off for true game rate. */
static bool     stretch_short = true;
static int      min_loop_ms = 600;

static int      cam_mode = CAM_ORBIT;
static vector_t focus;
/* Centre of the model measured relative to its own origin, so the orbit
 * target tracks an actor that an action walks across the world. */
static vector_t focus_offset;
/* Actions carry root motion — a walk cycle travels, a jump arcs. Pinned
 * (the default) holds the actor over its origin so a cycle plays in place;
 * unpinned lets it move and the camera follows. */
static bool     pin_root = true;
static int32_t  orbit_yaw, orbit_pitch, orbit_dist = 1200;
static int      bg_choice;
static int      pal_cam;
static bool     show_help;
static bool     auto_spin;
static bool     panels_hidden;

/* Archive scan.
 *
 * action_t carries a thing_name_index, but E2's data leaves it at -1 on every
 * record, so it is no use as a model→action map. What an action really binds
 * to is the set of parts its events name: modify_part() looks each one up in
 * the actor's _PartTab by event_index, and an action whose events name a part
 * the model does not have cannot have been authored for it. So the scan
 * records that set per action and the pane matches it against the model.
 *
 * SCAN_UNKNOWN marks "not looked at yet" so a resumed scan skips what it
 * already resolved, including entries filled in as a side effect of another
 * action's record being merged. */
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

/* An action naming only one part is usually a one-off effect rather than a
 * pose, and matches almost any rig. Two is the floor for a useful hit. */
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
    e[0] = (palette_entry_t){ 63, 63, 63 };            /* COL_TEXT   */
    e[1] = (palette_entry_t){ 63, 56,  0 };            /* COL_HILITE */
    e[2] = (palette_entry_t){ 34, 34, 38 };            /* COL_DIM    */
    e[3] = (palette_entry_t){  4,  6, 14 };            /* COL_PANEL  */
    e[4] = bg_choices[bg_choice];                      /* COL_BG     */

    static const int idx[5] = { COL_TEXT, COL_HILITE, COL_DIM, COL_PANEL, COL_BG };
    for (int i = 0; i < 5; i++) {
        view_cmap[idx[i]]   = e[i];
        colour_map[idx[i]]  = e[i];
        fade_cmap[idx[i]]   = e[i];
    }
    set_palette_flag = 1;
}

/* Rebuild the background store the dirty-rect machinery restores from.
 * bitmap[3] is the pristine view, bitmap[2] the working copy; the mask
 * planes go fully open so nothing in the scene occludes the model. */
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

/* E2 keeps a palette per camera in the archive; E1 in this port has one
 * global palette (show_parts re-applies colour_map every frame), so the
 * cycle is a no-op there and the HUD says so.
 *
 * E2 model colours are indices into a scene palette, so without one loaded
 * the viewer renders them through whatever happened to be in view_cmap at
 * boot — the title screen's palette — and every model comes out washed out.
 * A real camera palette has to be picked before the first frame. */
static void apply_scene_palette(int cam) {
    if (game_version != GAME_VERSION_E2) return;
    if (cam < 0) cam = 0;
    pal_cam = cam;
    selected_camera = (int16_t)cam;
    load_palette(NULL);
    inject_palette();
}

/* Cameras are sparse — most indices have no palette. Walking palette_offset
 * rather than find_highest_camera_num() also avoids the map, which the
 * viewer never loads (it would report no cameras at all). */
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

    /* The repertoire is what the game itself plays for this thing, so it
     * leads the list even after a scan has found more. */
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
    /* view_matrix maps world → view, so the world vector that lands on a
     * view-space axis is the matching ROW of the matrix. */
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
        /* Deliberately not clamped. view_transform() subtracts view_pos from
         * the world point in int16, so what has to fit is the difference, not
         * the camera position itself — and two's complement wrap gives the
         * right difference either way. E1's map reaches ±32000, where a clamp
         * here silently drags the camera off its own aim point. */
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

/* Frame the model: centre on the part bounding box, back off far enough
 * that the whole of it fits. Runs after a prepare, so AbsPosition is live. */
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
        /* part->AbsPosition is relative to the actor's own origin, which is
         * exactly what focus_offset wants — recompute_focus() adds
         * position_vector back each frame. */
        focus_offset.X = (int16_t)((minx + maxx) / 2);
        focus_offset.Y = (int16_t)((miny + maxy) / 2);
        focus_offset.Z = (int16_t)((minz + maxz) / 2);

        int ex = maxx - minx, ey = maxy - miny, ez = maxz - minz;

        /* Aim above the model's centre so it sits low in frame rather than
         * dead centre. World up is -Y here, so a smaller Y is higher. The
         * clear space this buys at the top is where the status lines run,
         * and it is the tall models — whose heads land right in them — that
         * need it. Scaled by height so short props are barely moved. */
        focus_offset.Y = (int16_t)(focus_offset.Y - ey / 5);

        int extent = ex > ey ? ex : ey;
        if (ez > extent) extent = ez;
        if (extent < 40) extent = 40;
        orbit_dist = clampi(extent * 5 / 2, 120, 24000);
    }

    /* Angles are 16-bit turns, so 0x8000 is half a revolution. A model's
     * rest facing puts its front away from the camera at yaw 0, hence the
     * half turn; the extra 0x2000 is a three-quarter view rather than a
     * dead-on one, which reads better for limbs. */
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

/* Browsing loads a new action per selection and never plays a scene, so
 * nothing retires them the way gameplay does. Left alone the 400-entry pool
 * fills and find_free_action() falls into try_to_remove_scene_or_action(),
 * which picks its victim without knowing the viewer still points at it. The
 * viewer therefore retires its own, oldest first. */
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
    /* Same scaling behaviour() applies. E2-only, and only ever non-100 for an
     * actor whose script has run CT_SPEED_FACTOR — which the viewer does not
     * do, so in practice this is a no-op that keeps the path faithful. */
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

    /* Actor init code can pull scenes and other things in behind us. The
     * viewer shows one thing at a time, so the display list is rebuilt to
     * hold exactly this actor and the scene list dropped. */
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

/* nth playable script, skipping the 0x20 "not part of this run" ones that
 * start_scene() also passes over. */
static script_t *scene_cast_at(const scene_t *s, int n) {
    if (!s) return NULL;
    for (script_t *sc = s->scene_script_list; sc; sc = sc->next_script) {
        if (sc->script_action.action_flags & 0x20) continue;
        if (n-- == 0) return sc;
    }
    return NULL;
}

/* Scripts run in parallel and finish independently, so the scene is over
 * when the longest one is. */
static int scene_length(const scene_t *s) {
    int longest = 1;
    for (script_t *sc = s ? s->scene_script_list : NULL; sc; sc = sc->next_script) {
        if (sc->script_action.action_flags & 0x20) continue;
        if (sc->script_action.act_duration > longest)
            longest = sc->script_action.act_duration;
    }
    return longest;
}

/* Point the camera the way the scene was shot: check_view() pulls the
 * painted background into the plane the dirty-rect restore reads from, the
 * matching depth mask, the scene palette, and the camera position itself. */
static void detach_scene_camera(void);

/* Camera 0 is check_view's "no view" case: it blanks the plate and opens the
 * mask instead of loading a shot, and its camera_data is never authored. A
 * scene on it has no framing to be faithful to, so the orbit camera is the
 * only thing that can show it. */
static bool scene_has_shot(const scene_t *s) {
    return s && s->camera_index > 0;
}

static void attach_scene_camera(void) {
    if (!cur_scene) return;
    if (!scene_has_shot(cur_scene)) { detach_scene_camera(); return; }
    /* check_view() early-outs when the camera is already active, which would
     * skip the background reload after a detour through the flat backdrop. */
    active_camera = NULL;
    check_view(cur_scene->camera_index);
    pal_cam = cur_scene->camera_index;
    inject_palette();
}

/* Frame the cast member the CAST pane has selected, in world space.
 *
 * Not the whole cast: a scene's cast routinely mixes a character with a piece
 * of set dressing parked hundreds of units away — E1's Start_sc pairs the hero
 * with `bridge` — and a box around both leaves everyone a few pixels tall.
 * Selecting in the CAST pane re-frames, so each one can be looked at in turn
 * and E dollies out to take in the rest. */
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

    /* Making the subject cur_actor is what puts scene mode on the same
     * footing as model mode: recompute_focus() then tracks it every frame,
     * so the camera follows an actor the scene walks across the set. */
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
        /* Centred, unlike the model fit: that one drops the subject low to
         * clear the status lines, which only reads well on a standing
         * humanoid. A scene subject can be a horse or a bridge, and the bias
         * pushes those out of frame. Extra distance for the same reason. */
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

    /* Actors carry state from whatever was shown before — a half-finished
     * act, a repertoire, a world position. start_scene() sets up the act but
     * not the rest, and in game these come up fresh from the world. */
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

/* Playback. advance_selected_scene_or_action() steps every script by the same
 * delta, which is what do_movement() does for a scene in game. Seeking back
 * has to replay from the start: a script's progress lives in its act's
 * key_progress, and the only way to unwind it is to run it again. */
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

    /* Same reason as the model path: 0x400 parks an actor into the
     * background store, which in a viewer freezes it there. */
    for (actor_t *a = root_thing; a; a = a->next_in_display_list)
        a->flags &= ~0x0400u;
}

/* ── Archive scan ────────────────────────────────────────────── */

/* The part indices an action's events name. Part-targeted event types are
 * flagged in event_type_flags[]; 0x200 marks the ones modify_part redirects
 * to a held object rather than the actor's own rig, so they say nothing
 * about which model the action belongs to. */
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

/* Record every action the table now holds and drop the ones this scan
 * brought in — a single archive record can carry several actions, so the
 * sweep is over the whole table rather than just the index we asked for.
 * Dropping them keeps the 400-entry action pool from filling up halfway
 * through a 2000-entry archive. */
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

static void draw_help(void) {
    static const char *lines[] = {
        "  MODEL / ANIMATION / SCENE VIEWER",
        "",
        "  V            switch MODELS <-> SCENES",
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

static void draw_hud(void) {
    inject_palette();

    int cols = screen_width / tx_w;
    int rows_total = screen_height / tx_h;

    /* Status line, always on — it is the only thing that fits at 320x200
     * once the panels are hidden. */
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

    /* Bottom line: camera and render state. */
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
            /* The detached camera follows whoever is selected, so moving
             * through the cast has to re-aim it as you go — waiting for
             * RETURN made the pane look inert. */
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
            /* Picking a cast member means "show me this one", which the
             * authored shot cannot do — it is a fixed frame. So RETURN here
             * detaches onto that actor; O puts the shot back. */
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

static void handle_input(int dt) {
    if (platform_key_hit(plat, PKEY_H)) show_help = !show_help;
    if (platform_key_hit(plat, PKEY_X)) panels_hidden = !panels_hidden;

    if (platform_key_hit(plat, PKEY_LEFT))  pane = PANE_MODELS;
    if (platform_key_hit(plat, PKEY_RIGHT)) pane = PANE_ANIMS;

    if (platform_key_hit(plat, PKEY_UP))   move_selection(-1);
    if (platform_key_hit(plat, PKEY_DOWN)) move_selection(+1);
    if (platform_key_hit(plat, PKEY_PGUP)) move_selection(-16);
    if (platform_key_hit(plat, PKEY_PGDN)) move_selection(+16);
    if (platform_key_hit(plat, PKEY_HOME)) move_selection(-1000000);
    if (platform_key_hit(plat, PKEY_END))  move_selection(+1000000);

    if (platform_key_hit(plat, PKEY_RETURN)) activate_selection();

    if (platform_key_hit(plat, PKEY_V)) {
        tool_mode ^= 1;
        pane = PANE_MODELS;
        if (tool_mode == TOOL_SCENES) {
            script_mode = 1;
            if (!cur_scene && scene_count) { load_scene(scene_sel); playing = true; }
            else if (cam_attached) attach_scene_camera();
        } else {
            /* Scene actors and the scene camera both have to go, or the
             * model shows up in someone else's shot with the cast in it. */
            script_mode = 0;
            unload_scene();
            rebuild_background();
            if (game_version == GAME_VERSION_E2) apply_scene_palette(pal_cam);
            load_model(model_sel);
        }
    }

    if (tool_mode == TOOL_MODELS &&
        platform_key_hit(plat, PKEY_TAB) && scan_pos < 0) scan_begin();

    /* Headless check: no key events reach a window that never gets focus,
     * so the scan needs a way in for a scripted run. */
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

    /* Continuous camera controls, scaled by frame time so a fast machine
     * does not spin the model twice as quickly. Skipped while a scene's own
     * camera is attached: it is fixed by the shot, and the painted
     * background would not move with it anyway. */
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

    /* Mouse: left drag orbits, right drag dollies.
     *
     * Yaw is inverted so the drag grabs the MODEL rather than the camera —
     * pull left and the model turns left. Pitch is not: dragging down to
     * look down at the model is what the hand expects, and matches every
     * other orbit control. */
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

/* my_time() ticks at 60Hz under E1 and 70Hz under E2 — the unit an action's
 * duration is counted in. */
static int tick_rate(void) {
    return (game_version == GAME_VERSION_E1) ? 60 : 70;
}

/* How long a loop actually takes on screen, in ticks.
 *
 * Playback itself is at the engine's rate: advance_act() steps a non-scene
 * action by (elapsed << 16) / duration, which is what this does. But plenty
 * of actions are far too short to read on repeat — E2's herojump is 16 ticks,
 * under a quarter second — and in play they are seen once, in context, not
 * looped. So anything under min_loop_ms is stretched to it. The HUD says SLOW
 * whenever that is in effect, and F turns it off for true game rate. */
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
            /* Scene actions are positioned in absolute time, so the stretch
             * has to scale the step rather than the divisor. */
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
        /* Horizontal travel only. Y carries jumps and crouches, which are
         * part of the pose and worth seeing. */
        cur_actor->position_vector.X = 0;
        cur_actor->position_vector.Z = 0;
        set_vector(&cur_actor->actor_velocity, 0, 0, 0);
    }
}

/* ── Frame ───────────────────────────────────────────────────── */

static void render_frame(void) {
    /* 0x400 marks a thing as "stuck" — drawn once into the background store
     * instead of the frame. update_act sets it whenever an act runs dry,
     * which in a viewer would freeze the actor into the backdrop. 0x800 is
     * the per-frame "already drawn" gate. Scene mode has a whole cast, so
     * the whole display list gets the same treatment. */
    for (actor_t *a = root_thing; a; a = a->next_in_display_list) {
        a->flags &= ~(uint16_t)(0x0400 | 0x0800);
        a->flags |= ACTOR_FLAG_VISIBLE;
        a->state_flags &= ~0x80;
    }

    /* Full-screen restore from the background store, rather than the
     * per-actor dirty rectangles: the HUD covers areas no actor claims. */
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

    /* Keeps prepare_parts out of the camera-maintenance block (check_camera,
     * check_hot_spots, check_hero_rep) and draw_stuck_parts out of the frame
     * entirely — all of which need a world the viewer does not load. */
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

    build_model_list();
    build_scene_list();
    apply_scene_palette(step_scene_palette(0, +1));
    inject_palette();
    rebuild_background();

    tool_mode = (viewer_mode == VIEWER_SCENES) ? TOOL_SCENES : TOOL_MODELS;
    if (tool_mode == TOOL_SCENES) {
        script_mode = 1;
        if (scene_count) load_scene(0);
    } else if (model_count) {
        load_model(0);
    }

    last_tick = my_time();
    int last_bg = bg_choice;

    for (;;) {
        if (!platform_pump_events(plat)) break;

        /* One read: platform_key_hit consumes the latch, so ESC has to be
         * dispatched here rather than tested again inside handle_input. */
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
