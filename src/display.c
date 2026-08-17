/**
 * display.c
 *
 * Skeletal hierarchy, matrix/vector math, view pipeline,
 * actor preparation, part/ellipse/triangle rendering.
 * 61 functions prefixed with display_ in the original ASM.
 */

#include "display.h"
#include "asm_f.h"
#include "debug_overlay.h"
#include "edit.h"
#include "ellipse.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "move.h"
#include "music.h"
#include "topo.h"
#include "tri.h"
#include "win.h"
#include <string.h>
#include <stdlib.h>

int16_t screen_width = 640;
int16_t screen_height = 480;
int16_t hires_width = 640;
int16_t hires_height = 480;
int16_t screen_centre_x = 320;
int16_t screen_centre_y = 240;
int16_t left_edge = 0;
int16_t right_edge = 640;
int16_t top_edge = 0;
int16_t bottom_edge = 480;
int32_t zoom_factor = 0;
int32_t near_clip = 0;
int16_t top_clip = 0;

char *bitmap[6] = {0};
char *hires_bitmap[6] = {0};
int16_t *mask_map[3] = {0};
int16_t draw_mode[6] = {0};

int16_t db = 0;

int16_t a_pen_colour = 15;
int16_t b_pen_colour = 0;
int16_t pen_position_x[6] = {0};
int16_t pen_position_y[6] = {0};
int16_t tx_w = 6;
int16_t tx_h = 8;
char character_set[256][48] = {{0}};

int16_t display_mode = 0;

matrix3x3_t view_matrix = {0};
vector_t view_rot = {0};
vector_t view_pos = {0};
camera_data_t camera[1200] = {{0}};
camera_data_t *active_camera = NULL;
int16_t cameras_viewed[150] = {0};

subarea_t clear_tab[2][100] = {{{0}}};
int16_t number_to_clear[2] = {0};

/* Null bbox used when ClearTab overflows so ellipse renderer has a
 * valid write target (writes discarded). */
subarea_t null_area_to_clear = {0};

int32_t subtitles_time = 0;
int16_t clear_subtitles = 0;
subarea_t sub_area_to_clear[20] = {{0}};
char *subtitle_text[20] = {0};
int16_t subtitle_length[20] = {0};
int16_t subtitle_offset[20] = {0};
int16_t subtitle_status[20] = {0};
int16_t subtitle_colour[20] = {0};

int16_t graphic_flag[GRAPHICS_MAX] = {0};
char *graphic_data[GRAPHICS_MAX] = {0};
int16_t graphic_x[GRAPHICS_MAX] = {0};
int16_t graphic_y[GRAPHICS_MAX] = {0};
int16_t graphic_size_x[GRAPHICS_MAX] = {0};
int16_t graphic_size_y[GRAPHICS_MAX] = {0};
graphic_name_t graphic_name_arr[GRAPHICS_MAX] = {{0}};
int16_t background_status = 0;

int32_t z_scale = 0;
int16_t *depth_mask = NULL;
char *shade_lut = NULL;
int32_t fb_pitch = 0;
int32_t shade_dy = 0;
int32_t z_dy = 0;
char *beam_tab1 = NULL;
char *beam_tab2 = NULL;
char *shadow_lut = NULL;

int16_t max_fps = 30;
int16_t fps = 0;

bool making_background = false;
/* Max detail: draw all triangles including detail LOD tiers (0x2/0x4).
 * The original sets this per-actor by distance in DrawDetail (0x47A6E8);
 * pending that, default to max so triangle geometry (e.g. dress meshes)
 * is never culled. Values >2 would drop ALL triangles — 2 is the ceiling. */
int16_t level_of_detail = 2;
int16_t set_palette_flag = 0;

part_t *blocked_parts_list[2] = {0};
int db_bpl = 0;

/* Alias shortened names to the actual global variable names in vars.h */
#define HeldByPart actor_held_by_part
#define HeldByActor actor_held_by_actor

/* display_initialise_parts  E1: 0x41CFD0 | E2: 0x4209E0 */
void initialise_parts(void) {
    root_thing = NULL;
    thing_list = NULL;
    action_list = NULL;
    scene_list = NULL;
    code_list = NULL;
    sound_list = NULL;
    map_area_list = NULL;
    texture_list = NULL;
    point_list = NULL;
    triangle_list = NULL;
    script_list = NULL;
    repertoire_list = NULL;

    clear_ptr_tabs();

    for (int i = 0; i < ACTOR_POOL_SIZE; i++) {
        restore_actor_entries(i);
    }

    memset(scene_name_flags, 0, sizeof(scene_name_flags));
    memset(thing_name_flags, 0, sizeof(thing_name_flags));

    /* Default camera position */
    view_pos.X = 0;
    view_pos.Y = 0;
    view_pos.Z = 0x1000;
    view_rot.X = 0;
    view_rot.Y = 0;
    view_rot.Z = 0;

    calculate_view_matrices();
}

/* display_restore_actor_entries  E1: 0x41D140 | E2: 0x420B5C */
void restore_actor_entries(int index) {
    if (index < 0 || index >= ACTOR_POOL_SIZE) return;

    actor_rep_name[index] = -2;
    actor_magic[index] = 0;
    actor_flags[index] &= ~0x8;
    actor_position[index].X = 0;
    actor_position[index].Y = 0;
    actor_position[index].Z = 0;
    actor_orientation[index].X = 0;
    actor_orientation[index].Y = 0;
    actor_orientation[index].Z = 0;
    actor_hit_points[index] = 0;
    HeldByPart[index] = -1;
    HeldByActor[index] = -1;
}

/* display_restore_actor  E1: 0x41D1BC | E2: 0x420BE0 */
void restore_actor(int index) {
    restore_actor_entries(index);

    if (actor_flags[index] & ACTOR_FLAG_ACTIVE) {
        initialise_actor(&actor_heap_arr[index]);
        copy_defaults_to_actual(&actor_heap_arr[index]);
    }
}

/* display_new_game  E1: 0x41D1E0 | E2: 0x420C04 */
void new_game(void) {
    root_thing = NULL;
    thing_list = NULL;
    action_list = NULL;
    scene_list = NULL;
    sound_list = NULL;
    map_area_list = NULL;
    texture_list = NULL;
    point_list = NULL;
    triangle_list = NULL;
    script_list = NULL;
    repertoire_list = NULL;

    /* Do NOT clear code_list/map_area_list — they persist across games */
    memset(thing_tab, 0, sizeof(thing_tab));
    memset(action_tab, 0, sizeof(action_tab));
    memset(scene_tab, 0, sizeof(scene_tab));
    memset(repertoire_tab, 0, sizeof(repertoire_tab));
    memset(sound_tab, 0, sizeof(sound_tab));
    memset(texture_tab, 0, sizeof(texture_tab));

    for (int i = 0; i < ACTOR_POOL_SIZE; i++) {
        restore_actor_entries(i);
    }

    view_pos.X = 0;
    view_pos.Y = 0;
    view_pos.Z = 0x1000;
    view_rot.X = 0;
    view_rot.Y = 0;
    view_rot.Z = 0;

    calculate_view_matrices();
}

/* display_initialise_actor  E1: 0x41D360 | E2: 0x420D9C */
void initialise_actor(actor_t *actor) {
    if (!actor) return;

    actor->flags &= 0xF3F6;
    actor->actor_behavior = 0;
    actor->range_threshold = 0x7FFF;
    actor->actor_hitpoints = 0;
    actor->move_type = -1;
    actor->action_state = 0;
    actor->part_heap_link = NULL;
    actor->state_flags &= ~0x10;
    actor->actor_reperture = NULL;
    actor->actor_scene = NULL;
    actor->actor_act.act_action = NULL;
    actor->actor_act_list = NULL;
    actor->tactions_list = NULL;
    actor->time_actor = game_time - 10000;
    actor->actor_rep_index = actor->default_repert;

    for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list)
        p->actor_2_held = NULL;

    actor->full_actor_hp = 100;
    actor->actor_hitpoints = actor->full_actor_hp;

    /* Execute actor init code if present */
    if (actor->actor_init_code >= 0 && code_tab[actor->actor_init_code]) {
        execute_code(code_tab[actor->actor_init_code], actor);
    }
}

/* display_hold_thing_with_part  E1: 0x41D458 | E2: 0x420E94 */
/* Env-gated: dump the geometry of a held actor once, so a malformed held item
 * can be read off without guessing. ECSTATICA_TRACE_HELD=1 */
static void trace_held_actor(actor_t *actor) {
    static int on = -1;
    static int16_t seen[64];
    static int n_seen = 0;
    if (on < 0) {
        const char *e = getenv("ECSTATICA_TRACE_HELD");
        on = (e && *e && *e != '0') ? 1 : 0;
    }
    if (!on || !actor) return;
    for (int i = 0; i < n_seen; i++)
        if (seen[i] == actor->name_index) return;
    if (n_seen < 64) seen[n_seen++] = actor->name_index;

    fprintf(stderr, "[HELD] actor=%d '%s' flags=%04x sflags=%04x type=%d "
                    "act=%d parts:\n",
            actor->name_index,
            (actor->name_index >= 0 && actor->name_index < THING_TAB_SIZE)
                ? thing_names[actor->name_index].field_0 : "?",
            actor->flags, actor->state_flags, actor->type,
            actor->actor_act.act_action ? actor->actor_act.act_action->action_index : -1);
    for (part_t *pp = actor->actor_parts_list; pp; pp = pp->next_in_display_list) {
        fprintf(stderr, "  part=%3d type=%2d col=%4d dcol=%4d flags=%04x dflags=%04x "
                        "sq=(%d,%d,%d) dsq=(%d,%d,%d) rc=(%d,%d,%d) off=(%d,%d,%d) "
                        "p2p=%d kids=%d\n",
                pp->name_index, pp->type, pp->color, pp->default_color,
                pp->flags, pp->default_flags,
                pp->VECTOR_Squash.X, pp->VECTOR_Squash.Y, pp->VECTOR_Squash.Z,
                pp->def_Squash.X, pp->def_Squash.Y, pp->def_Squash.Z,
                pp->VECTOR_RelCentre.X, pp->VECTOR_RelCentre.Y, pp->VECTOR_RelCentre.Z,
                pp->Offset.X, pp->Offset.Y, pp->Offset.Z,
                pp->field_12E_point_to_point
                    ? pp->field_12E_point_to_point->point_index : -1,
                pp->actor_parts_list ? 1 : 0);
    }
    int ntri = 0;
    for (tri_t *t = actor->polygone_tri_list; t; t = t->next) ntri++;
    fprintf(stderr, "  triangles=%d\n", ntri);
    fflush(stderr);
}

void hold_thing_with_part(actor_t *actor, part_t *part) {
    if (!actor || !part) return;
    trace_held_actor(actor);

    copy_vector(&actor->position_vector, &part->ellipse_center);
    copy_matrix(&actor->matrix_1, &part->matrix_1);

    vector_t held_rot;
    vector_t held_off;
    if (part->name_index == 7) {
        held_rot = actor->held_rot_left;
        held_off = actor->held_off_left;
    } else {
        held_rot = actor->held_rotate;
        held_off = actor->held_offset;
    }

    if (held_rot.Y) rotate_about_y(&actor->matrix_1, held_rot.Y);
    if (held_rot.X) rotate_about_x(&actor->matrix_1, held_rot.X);
    if (held_rot.Z) rotate_about_z(&actor->matrix_1, held_rot.Z);

    vector_t transformed;
    matrix_vector(&held_off, &transformed, &actor->matrix_1);
    add_vector(&actor->position_vector, &transformed);

    find_relative_rot_vector(&actor->rotate_vector, &actor->matrix_1);

    if (actor->flags & 0x0400) {
        while (part) {
            actor_t *pa = part->parent_actor;
            if (!(pa->flags & 0x0400)) {
                clear_a_stuck_thing(actor);
                actor->flags &= ~0x0400;
                return;
            }
            part = pa->part_heap_link;
        }
    } else {
        if (part->parent_actor->flags & 0x0400)
            clear_a_stuck_thing(part->parent_actor);
        part->parent_actor->flags &= ~0x0400;
    }
}

/* display_prepare_an_actor  E1: 0x41D5F0 | E2: 0x421044 */
void prepare_an_actor(actor_t *actor) {
    if (!actor) return;

    check_fade();

    part_t *part = actor->part_heap_link;
    if (part) {
        actor_t *parent_actor = part->parent_actor;
        if (!(parent_actor->state_flags & 0x80))
            prepare_an_actor(parent_actor);
        hold_thing_with_part(actor, part);
        actor_last_act[actor->name_index] = 0;
    } else {
        make_identity(&actor->matrix_1);
        if (actor->rotate_vector.Y)
            rotate_about_y(&actor->matrix_1, actor->rotate_vector.Y);
        if (actor->rotate_vector.X)
            rotate_about_x(&actor->matrix_1, actor->rotate_vector.X);
        if (actor->rotate_vector.Z)
            rotate_about_z(&actor->matrix_1, actor->rotate_vector.Z);
    }

    if (actor->flags & 0x8) {
        copy_vector(&actor->joint_position, &actor->position_vector);

        for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list)
            p->flags &= 0x7FFF;

        blocked_parts_list[0] = NULL;
        db_bpl = 0;

        find_positions(actor, 0);

        for (int i = 0; i < 5; i++) {
            if (!blocked_parts_list[db_bpl]) break;
            blocked_parts_list[1 - db_bpl] = NULL;
            db_bpl = 1 - db_bpl;
            for (part_t *bp = blocked_parts_list[db_bpl]; bp; bp = bp->blocked_part)
                find_positions((actor_t *)bp->holding_actor, 0);
        }
    }

    actor->state_flags |= 0x80;
}

/* display_prepare_parts_4211C8 — main per-frame display prep.
 * Mirror of asm camera-maintenance block:
 *   if (!edit && !topography && !script_mode && !make_backgrounds):
 *     if (sel && sel->actor_act.act_action && act_action->action_flags & 2):
 *       if (sel->actor_scene): check_view(actor_scene->camera_index)
 *     else:
 *       if (!moving_camera) check_camera()
 *       check_hot_spots()
 *     check_hero_rep(); play_ambients();
 */
void prepare_parts(void) {
    /* Drive per-frame palette interpolation independently of actor prepare.
     * bug 15 tied check_fade to prepare_an_actor, but during intro (before
     * any actor is visible) that call chain never fires and fade_in stalls
     * — leaving view_cmap at all-zero from the FADE_TO_BLACK 0 that runs
     * before intro. Call unconditionally at prepare_parts entry. */
    check_fade();

    /* Camera-maintenance block — asm 4211C8 prologue.
     * Skipped in editor/topography/script/make_backgrounds. */
    if (!editor_mode && !topography && !script_mode && !make_backgrounds) {
        actor_t *sel = selected_thing;
        action_t *act = sel ? sel->actor_act.act_action : NULL;
        if (sel && act && (act->action_flags & 2)) {
            if (sel->actor_scene) {
                //     sel->actor_scene->scene_index, sel->actor_scene->camera_index);
                check_view(sel->actor_scene->camera_index);
            }
        } else {
            if (!moving_camera) check_camera();
            check_hot_spots();
        }
        /* check_hero_rep + play_ambients absent in E1 prepare_parts (0x41D740).
         * check_hero_rep writes rep_index values 9..0x10 keyed to E2 magic/held
         * item slots; E1 repertoire_tab has different indexes so hero->
         * actor_reperture ends up NULL → hero has no visual → disappears. */
        if (game_version == GAME_VERSION_E2) {
            check_hero_rep();
            play_ambients();
        }
    }

    if (active_camera)
        active_camera->time = my_time();

    /* Clear per-frame bits for all things in display list:
     * state_flags 0x80 — "already prepared" gate for prepare_an_actor
     * flags 0x0800     — "already drawn" gate between draw_stuck_parts
     *                    and draw_parts; must reset each frame or actors
     *                    that transition out of stuck state become
     *                    permanently invisible to both render paths. */
    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        t->state_flags &= ~0x80;
        t->flags &= ~0x0800;
    }

    /* Prepare all actors not yet marked (prepare_an_actor sets 0x80) */
    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        if (!(t->state_flags & 0x80)) {
            prepare_an_actor(t);
        }
    }

    /* asm display_prepare_parts_421198+116: expired subtitles are retired here,
     * before the background restore, so their clear_tab rects are honoured by
     * clear_parts/clear_masking in this same frame. */
    clear_expired_subtitles();

    /* Clear previous frame's graphics */
    clear_db_mouse_cursor_win95();

    if (background_status) {
        /* New background loaded: trigger palette apply on first of the two frames,
         * then do a full blit of bitmap[2] (background store) → draw buffer. */
        if (background_status-- == 2) {
            set_palette_flag = 1;
            if (intro_flag)
                remove_all_graphics();
            else
                invalidate_drawn_graphics();
        }
        clip_blit(2, 0, 0, 1 - db, 0, 0, screen_width, screen_height, 192);
    } else {
        clear_parts();
    }

    clear_masking();

    /* CT_SUBTITLE renderer. Delegates to req.c draw_subtitles
     * which writes text to bitmap 2 at fixed offsets. */
    draw_subtitles();


    number_to_clear[db] = 0;
}

/* display_clear_a_stuck_thing_421684 — Bug 55 pt.4: was strict `>` and
 * size w/o +1. Correct behavior: always assign area_to_clear = &bounding_box first,
 * use inclusive bounds, +1 sizes, and append slot to BOTH clear_tab[db] and
 * clear_tab[1-db] so both buffer sides get restored on next flip. */
void clear_a_stuck_thing(actor_t *actor) {
    if (!actor) return;

    actor->area_to_clear = &actor->bounding_box;
    subarea_t *area = actor->area_to_clear;
    int w = area->right - area->left + 1;
    int h = area->bottom - area->top + 1;

    clip_blit(3, area->left, area->top, 2, area->left, area->top, w, h, 0);
    clip_mask(2, 1, area->left, area->top, w, h);

    for (int side = 0; side < 2; side++) {
        int idx = (side == 0) ? db : (1 - db);
        if (number_to_clear[idx] >= 150) {
            beep_message("ClearTab overflow!");
        } else {
            clear_tab[idx][number_to_clear[idx]++] = *area;
        }
    }
}

/* display_clear_a_subtitle  E1: 0x41DD00 | E2: 0x421820 */
void clear_a_subtitle(int index) {
    subarea_t area = sub_area_to_clear[index];
    int w = area.right + 1 - area.left;
    int h = area.bottom + 1 - area.top;
    if (w <= 0 || h <= 0) return;

    clip_blit(3, area.left, area.top, 2, area.left, area.top, w, h, 0);
    clip_mask(2, 1, area.left, area.top, w, h);

    for (int side = 0; side < 2; side++) {
        int idx = (side == 0) ? db : (1 - db);
        if (number_to_clear[idx] >= 150) {
            beep_message("ClearTab overflow!");
        } else {
            clear_tab[idx][number_to_clear[idx]++] = area;
        }
    }
}

/* display_clear_masking_424B48 — Bug 55 pt.2: was reading clear_tab[db]
 * (current frame's slots, unpopulated), using strict `>`, and size = R-L
 * with no +1. Correct: read clear_tab[1-db] (prior frame slots — those are
 * the ones to erase), inclusive `<=`, size R-L+1. Old semantics never
 * cleared the actual dirty rects → mask stale. */
void clear_masking(void) {
    for (int i = 0; i < number_to_clear[1 - db]; i++) {
        subarea_t *area = &clear_tab[1 - db][i];
        if (area->left <= area->right && area->top <= area->bottom) {
            clip_mask(1, 0,
                     area->left, area->top,
                     area->right - area->left + 1,
                     area->bottom - area->top + 1);
        }
    }
}

/* display_clear_parts_424BD8 — Bug 55 pt.3: strict `>` + size w/o +1
 * meant last row/col of dirty rect never cleared, leaving 1-px trails. */
void clear_parts(void) {
    for (int i = 0; i < number_to_clear[db]; i++) {
        subarea_t *area = &clear_tab[db][i];
        if (area->left <= area->right && area->top <= area->bottom) {
            clear_background(1 - db,
                           area->left, area->top,
                           area->right - area->left + 1,
                           area->bottom - area->top + 1);
        }
    }
}

/* display_draw_stuck_parts_421A14 — Bug 55: assign area_to_clear pointer.
 * Was leaving actor->area_to_clear NULL, so ellipse renderer read garbage
 * and never wrote the dirty bbox back → clear_masking didn't clear old
 * ellipse pixels → static-actor render trails on screen. Also missing
 * clear_background + clip_mask + clear_tab append. */
void draw_stuck_parts(void) {
    if (editor_mode) return;

    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x0400)) continue;    /* Not stuck */
        if (actor->flags & 0x0800) continue;       /* Already drawn this frame */
        if (!(actor->flags & 0x08)) continue;      /* Not visible */
        if (make_backgrounds && actor == selected_thing) continue;

        /* Point area_to_clear at struct-embedded bounding_box so ellipse
         * renderer's per-part bbox extension writes to a real target. */
        actor->area_to_clear = &actor->bounding_box;
        actor->flags |= 0x0800;
        actor->bounding_box.left   = 0x7FFF;
        actor->bounding_box.top    = 0x7FFF;
        actor->bounding_box.right  = -0x7FFF;
        actor->bounding_box.bottom = -0x7FFF;

        find_view_positions(actor);
        put_shadows(actor);
        put_parts_not_shadows(actor);
        put_triangles(actor);
        put_smoke(actor);

        subarea_t *a = actor->area_to_clear;
        if (a->left   < left_edge)     a->left   = left_edge;
        if (a->top    < top_edge)      a->top    = top_edge;
        if (a->right  > right_edge - 1)  a->right  = right_edge - 1;
        if (a->bottom > bottom_edge - 1) a->bottom = bottom_edge - 1;

        if (a->left <= a->right && a->top <= a->bottom) {
            int w = a->right - a->left + 1;
            int h = a->bottom - a->top + 1;
            clear_background(1 - db, a->left, a->top, w, h);
            clip_mask(1, 0, a->left, a->top, w, h);

            if (number_to_clear[1 - db] < 150) {
                subarea_t *ct = &clear_tab[1 - db][number_to_clear[1 - db]++];
                *ct = *a;
            } else {
                beep_message("ClearTab overflow!");
            }
        }
    }

    /* DrawStuckParts fires DrawGraphics at end. Was missing —
     * put_a_graphic set flag=NeedToDraw and need_draw_graphics=1 but nothing
     * ever consumed them → intro title graphic, HUD icons, life/magic
     * bars never appeared. */
    if (need_clear_graphics) clear_graphics();
    if (need_draw_graphics) draw_graphics();
}

/* display_add_polygons  E1: 0x41E3A0 | E2: 0x421EE0 */
void add_polygons(void) {
    /* No-op: empty loop iterating actor polygon lists but performing no
       action.  Zero callers. */
}

/* display_add_a_triangle  E1: ? | E2: 0x421F20 */
void add_a_triangle(void) {
    /* E2 stub — just returns */
}

/* display_add_actor_polys  E1: 0x41E3CC | E2: 0x421F0C */
void add_actor_polys(actor_t *actor) {
    /* No-op: empty loop iterating actor->polygone_tri_list but performing
       no action.  Zero callers. */
    (void)actor;
}

/* display_draw_parts  E1: 0x41E3E4 | E2: 0x421F24 */
void draw_parts(void) {
    /* Bug 48: asm at 0x421F62 gates actor visibility on view-transform Z
     * during moving_camera. If transformed Z > 0x2000 → clear Visible bit;
     * else set Visible. Was missing — actors past far-plane got drawn
     * during cam interpolation. */
    if (moving_camera) {
        for (actor_t *a = root_thing; a; a = a->next_in_display_list) {
            vector_t out;
            view_transform(&out, &a->position_vector);
            if (out.Z > 0x2000)
                a->flags &= (uint16_t)~0x8;
            else
                a->flags |= 0x8;
        }
    }

    /* Bug 55: assign area_to_clear per actor + split rendering into 4
     * passes in DrawParts. Was allocating a temp
     * ClearTab slot without ever storing its address in actor->area_to_clear,
     * so ellipse renderer read NULL and never wrote the bbox back.
     * Result: dirty rect always empty → no clear next frame → trails. */

    /* Pass 1: assign area_to_clear + init bbox + find_view_positions. */
    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x08)) continue;
        if (actor->flags & 0x0800) continue;
        if (make_backgrounds && actor == selected_thing) continue;

        if (number_to_clear[db] >= 150) {
            beep_message("ClearTab overflow!");
            actor->area_to_clear = &null_area_to_clear;
        } else {
            actor->area_to_clear = &clear_tab[db][number_to_clear[db]++];
        }
        actor->area_to_clear->left   = 0x7FFF;
        actor->area_to_clear->top    = 0x7FFF;
        actor->area_to_clear->right  = -0x7FFF;
        actor->area_to_clear->bottom = -0x7FFF;

        find_view_positions(actor);
    }

    /* Pass 2: shadows. */
    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x08)) continue;
        if (actor->flags & 0x0800) continue;
        if (make_backgrounds && actor == selected_thing) continue;
        put_shadows(actor);
    }

    /* Pass 3: parts + triangles. */
    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x08)) continue;
        if (actor->flags & 0x0800) continue;
        if (make_backgrounds && actor == selected_thing) continue;
        put_parts_not_shadows(actor);
        put_triangles(actor);
    }

    /* Pass 4: smoke. */
    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x08)) continue;
        if (actor->flags & 0x0800) continue;
        if (make_backgrounds && actor == selected_thing) continue;
        put_smoke(actor);
    }

    /* Pass 5: clamp each area_to_clear to viewport edges. */
    for (actor_t *actor = root_thing; actor; actor = actor->next_in_display_list) {
        if (!(actor->flags & 0x08)) continue;
        if (actor->flags & 0x0800) continue;
        subarea_t *a = actor->area_to_clear;
        if (!a) continue;
        if (a->left   < left_edge)     a->left   = left_edge;
        if (a->top    < top_edge)      a->top    = top_edge;
        if (a->right  > right_edge - 1)  a->right  = right_edge - 1;
        if (a->bottom > bottom_edge - 1) a->bottom = bottom_edge - 1;
    }
}

/* display_show_parts  E1: 0x41E6C4 | E2: 0x422208 */
void show_parts(void) {
    if (game_version == GAME_VERSION_E1) {
        set_palette(colour_map);
    } else {
        if (set_palette_flag == 1) {
            set_palette(view_cmap);
            set_palette_flag = 0;
        } else if (set_palette_flag == 2) {
            do_fade_to_black(last_fade_factor);
            set_palette_flag = 0;
        } else if (set_palette_flag == 3) {
            do_fade_to_white(last_fade_factor);
            set_palette_flag = 0;
        }
    }

    /* Flip double buffer */
    db = 1 - db;
    draw_debug_overlay();
    flip_win95();
    game_up_and_running = 1;
}

/* display_make_identity  E1: 0x41E720 | E2: 0x422300 */
void make_identity(matrix3x3_t *mtx) {
    memset(mtx, 0, sizeof(matrix3x3_t));
    mtx->_11 = 0x4000;
    mtx->_22 = 0x4000;
    mtx->_33 = 0x4000;
}

/* display_rotate_about_x_422368 — POST-multiply: mtx = mtx * Rx.
 * Distinct from pre_rotate_about_x (which does Rx*mtx). asm inlines the
 * post-multiply; previously translated as pre-multiply, validated wrong
 * by diff_unicorn harness and corrected here. */
void rotate_about_x(matrix3x3_t *mtx, int16_t angle) {
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    matrix3x3_t rot;

    make_identity(&rot);
    rot._22 = c;
    rot._23 = -s;
    rot._32 = s;
    rot._33 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, mtx, &rot);  /* post-multiply: tmp = mtx * Rx */
    *mtx = tmp;
}

/* display_rotate_about_y_422460 — POST-multiply: mtx = mtx * Ry. */
void rotate_about_y(matrix3x3_t *mtx, int16_t angle) {
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    matrix3x3_t rot;

    make_identity(&rot);
    rot._11 = c;
    rot._13 = s;
    rot._31 = -s;
    rot._33 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, mtx, &rot);  /* post-multiply: tmp = mtx * Ry */
    *mtx = tmp;
}

/* display_rotate_about_z_422538 — POST-multiply: mtx = mtx * Rz. */
void rotate_about_z(matrix3x3_t *mtx, int16_t angle) {
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    matrix3x3_t rot;

    make_identity(&rot);
    rot._11 = c;
    rot._12 = -s;
    rot._21 = s;
    rot._22 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, mtx, &rot);  /* post-multiply: tmp = mtx * Rz */
    *mtx = tmp;
}

/* display_matrix_inverse_422628 — transpose (valid for orthogonal matrices) */
void matrix_inverse(matrix3x3_t *out, matrix3x3_t *in) {
    out->_11 = in->_11; out->_12 = in->_21; out->_13 = in->_31;
    out->_21 = in->_12; out->_22 = in->_22; out->_23 = in->_32;
    out->_31 = in->_13; out->_32 = in->_23; out->_33 = in->_33;
}

/* display_rotate_vector_about_x  E1: 0x41EA64 | E2: 0x422644 */
void rotate_vector_about_x(vector_t *vec, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    rotate_about_x(&rot, angle);
    vector_t tmp;
    matrix_vector(vec, &tmp, &rot);
    *vec = tmp;
}

/* display_rotate_vector_about_y  E1: 0x41EAAC | E2: 0x42268C */
void rotate_vector_about_y(vector_t *vec, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    rotate_about_y(&rot, angle);
    vector_t tmp;
    matrix_vector(vec, &tmp, &rot);
    *vec = tmp;
}

/* display_rotate_vector_about_z  E1: 0x41EAF4 | E2: 0x4226D4 */
void rotate_vector_about_z(vector_t *vec, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    rotate_about_z(&rot, angle);
    vector_t tmp;
    matrix_vector(vec, &tmp, &rot);
    *vec = tmp;
}

/* display_pre_rotate_about_x  E1: 0x41EB3C | E2: 0x42271C */
void pre_rotate_about_x(matrix3x3_t *mtx, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    rot._22 = c;  rot._23 = -s;
    rot._32 = s;  rot._33 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, &rot, mtx);
    *mtx = tmp;
}

/* display_pre_rotate_about_y  E1: 0x41EB80 | E2: 0x422760 */
void pre_rotate_about_y(matrix3x3_t *mtx, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    rot._11 = c;  rot._13 = s;
    rot._31 = -s; rot._33 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, &rot, mtx);
    *mtx = tmp;
}

/* display_pre_rotate_about_z  E1: 0x41EBC4 | E2: 0x4227A4 */
void pre_rotate_about_z(matrix3x3_t *mtx, int16_t angle) {
    matrix3x3_t rot;
    make_identity(&rot);
    int16_t s = sine_table[(unsigned short)angle];
    int16_t c = cosn_table[(unsigned short)angle];
    rot._11 = c;  rot._12 = -s;
    rot._21 = s;  rot._22 = c;

    matrix3x3_t tmp;
    matrix_mult(&tmp, &rot, mtx);
    *mtx = tmp;
}

/* display_c_matrix_vector_422818 — C wrapper for matrix_vector */
void c_matrix_vector(vector_t *out, matrix3x3_t *mtx, vector_t *in) {
    matrix_vector(in, out, mtx);
}

/* display_make_rot_matrix_4228B8
 * ASM uses PRE-multiply in order Rz, Rx, Ry → result = Ry * Rx * Rz.
 * POST-multiply equivalently: Y, X, Z. */
void make_rot_matrix(matrix3x3_t *mtx, int16_t rx, int16_t ry, int16_t rz) {
    make_identity(mtx);
    if (ry) rotate_about_y(mtx, ry);
    if (rx) rotate_about_x(mtx, rx);
    if (rz) rotate_about_z(mtx, rz);
}

/* display_add_vector  E1: 0x41EE38 | E2: 0x422A18 */
void add_vector(vector_t *result, vector_t *input) {
    result->X += input->X;
    result->Y += input->Y;
    result->Z += input->Z;
}

/* display_subtract_vector  E1: 0x41EE64 | E2: 0x422A44 */
void subtract_vector(vector_t *result, vector_t *input) {
    result->X -= input->X;
    result->Y -= input->Y;
    result->Z -= input->Z;
}

/* display_copy_vector  E1: 0x41EE90 | E2: 0x422A70 */
void copy_vector(vector_t *dst, vector_t *src) {
    dst->X = src->X;
    dst->Y = src->Y;
    dst->Z = src->Z;
}

/* helper: copy_matrix */
void copy_matrix(matrix3x3_t *dst, matrix3x3_t *src) {
    memcpy(dst, src, sizeof(matrix3x3_t));
}

/* helper: div_vector */
void div_vector(vector_t *v, int16_t divisor) {
    if (divisor == 0) return;
    v->X /= divisor;
    v->Y /= divisor;
    v->Z /= divisor;
}

/* helper: sum_vector */
void sum_vector(vector_t *out, vector_t *a, vector_t *b) {
    out->X = a->X + b->X;
    out->Y = a->Y + b->Y;
    out->Z = a->Z + b->Z;
}

/* helper: sub_vector */
void sub_vector(vector_t *out, vector_t *a, vector_t *b) {
    out->X = a->X - b->X;
    out->Y = a->Y - b->Y;
    out->Z = a->Z - b->Z;
}

/* display_calculate_view_matrices_422ABC.
 * asm non-editor path: identity, then post-mult Rx(view_rot.X), then
 * post-mult Ry(view_rot.Y). Z component of view_rot is NEVER applied
 * (asm reads `view_pos+0` for the Z slot which is uninitialized pad =
 * always zero). Previous C order Z→X→Y was wrong.
 */
void calculate_view_matrices(void) {
    /* Bug 62: apply Rz first in CalculateViewMatrices.
     * Order Z, X, Y post-mul → M = Rz * Rx * Ry. Prior port skipped Rz on
     * belief asm reads pad byte for third rotation; camera data legitimately
     * writes view_rot.Z (camera roll) so must apply. */
    make_identity(&view_matrix);
    if (view_rot.Z) rotate_about_z(&view_matrix, view_rot.Z);
    if (view_rot.X) rotate_about_x(&view_matrix, view_rot.X);
    if (view_rot.Y) rotate_about_y(&view_matrix, view_rot.Y);
}

/* display_calculate_rot_matrix  E1: 0x41F334 | E2: 0x422F14 */
void calculate_rot_matrix(matrix3x3_t *mtx, vector_t *rotation) {
    make_identity(mtx);
    if (rotation->X) rotate_about_x(mtx, rotation->X);
    if (rotation->Y) rotate_about_y(mtx, rotation->Y);
    if (rotation->Z) rotate_about_z(mtx, rotation->Z);
}

/* display_view_transform_422FD8.
 * Near-clip threshold: asm sets Z=0 when post-view Z < 128, signaling
 * "near-clipped" to downstream rasterizers (which test Z==0 to skip).
 * Previous C just left Z at its raw post-rotation value (e.g. 106), so
 * very-close points rendered at tiny near-camera screen coords instead
 * of being culled.
 */
void view_transform(vector_t *out, vector_t *world_pos) {
    vector_t rel;
    rel.X = world_pos->X - view_pos.X;
    rel.Y = world_pos->Y - view_pos.Y;
    rel.Z = world_pos->Z - view_pos.Z;

    matrix_vector(&rel, out, &view_matrix);

    if (out->Z >= 128) {
        perspective_transform(out);
    } else {
        out->Z = 0;
    }
}

/* display_perspective_transform_423070
 * Non-uniform perspective (reference algorithm):
 *   coeff_x = ZoomFactor / Z, coeff_x *= 2
 *   coeff_y = 7/8 * ZoomFactor / Z, coeff_y = coeff_y * 12/5
 *   persp_x = coeff_x * X >> 10
 *   persp_y = coeff_y * Y >> 10
 * Result is in 16x pixel scale; screen center added by drawing code.
 */
void perspective_transform(vector_t *point) {
    if (point->Z <= 0) { point->Z = 0; return; }

    int32_t coeff_x = zoom_factor / point->Z;
    int32_t coeff_y = coeff_x - (coeff_x >> 3);  /* 7/8 */
    coeff_x = coeff_x * screen_width / 320;
    coeff_y = coeff_y * screen_height / 200;

    int32_t persp_x = (coeff_x * (int32_t)point->X) >> 10;
    int32_t persp_y = (coeff_y * (int32_t)point->Y) >> 10;

    if (abs(persp_x) < 0x7FFF) {
        point->X = (int16_t)persp_x;
        if (abs(persp_y) < 0x7FFF) {
            point->Y = (int16_t)persp_y;
            return;
        }
    }
    point->Z = 0;
}

/* display_xx_long_view_transform  E1: 0x41F6B0 | E2: 0x423290 */
void xx_long_view_transform(long_vector_t *out, vector_t *world_pos) {
    vector_t rel;
    rel.X = world_pos->X - view_pos.X;
    rel.Y = world_pos->Y - view_pos.Y;
    rel.Z = world_pos->Z - view_pos.Z;

    long_vector_t long_rel;
    long_rel.l_X = rel.X;
    long_rel.l_Y = rel.Y;
    long_rel.l_Z = rel.Z;

    matrix_long_vector(&long_rel, out, &view_matrix);
}

/* display_long_view_transform  E1: 0x41F848 | E2: 0x423428 */
void long_view_transform(long_vector_t *out, vector_t *world_pos) {
    xx_long_view_transform(out, world_pos);
}

/* display_long_perspective_trans  E1: 0x41F8A8 | E2: 0x423488 */
void long_perspective_transform(long_vector_t *point) {
    if (point->l_Z <= 0) { point->l_Z = 0; return; }

    int32_t coeff_x = zoom_factor / point->l_Z;
    int32_t coeff_y = coeff_x - (coeff_x >> 3);  /* 7/8 */
    coeff_x = coeff_x * screen_width / 320;
    coeff_y = coeff_y * screen_height / 200;

    point->l_X = (coeff_x * point->l_X) >> 10;
    point->l_Y = (coeff_y * point->l_Y) >> 10;
}

/* display_perspec_trans_no_overflow_chk  E1: 0x41F9B0 | E2: 0x423590 */
void perspec_trans_no_overflow_chk(vector_t *point) {
    if (point->Z <= 0) return;

    int32_t coeff_x = zoom_factor / point->Z;
    int32_t coeff_y = coeff_x - (coeff_x >> 3);  /* 7/8 */
    coeff_x = coeff_x * screen_width / 320;
    coeff_y = coeff_y * screen_height / 200;

    point->X = (int16_t)((coeff_x * (int32_t)point->X) >> 10);
    point->Y = (int16_t)((coeff_y * (int32_t)point->Y) >> 10);
}

/* display_print_matrix  E1: 0x41F9F4 | E2: 0x4235D4 */
void print_matrix(matrix3x3_t *mtx) {
    (void)mtx; /* Debug only */
}

/* display_print_vector  E1: 0x41FA40 | E2: 0x423620 */
void print_vector(vector_t *vec) {
    (void)vec; /* Debug only */
}

/* display_find_position_of_extremity  E1: 0x41FA64 | E2: 0x423644 */
void find_position_of_extremity(part_t *part) {
    if (!part) return;

    if (part->flags & 0x100) return;

    if (part->flags & 0x200) {
        if (part->flags & 0x4000) {
            vector_t tmp;
            matrix_vector(&part->AbsPosition, &tmp, &part->parent_actor->matrix33_2);
            part->joint_position.X = part->parent_actor->actor_center.X + tmp.X;
            part->joint_position.Y = part->parent_actor->actor_center.Y + tmp.Y;
            part->joint_position.Z = part->parent_actor->actor_center.Z + tmp.Z;
            copy_matrix(&part->matrix_1, &part->parent_actor->matrix33_2);
        } else {
            make_identity(&part->matrix_1);
            copy_vector(&part->joint_position, &part->AbsPosition);
        }
    } else {
        vector_t tmp;
        matrix_vector(&part->AbsPosition, &tmp, &part->parent_actor->matrix_1);
        part->joint_position.X = part->parent_actor->position_vector.X + tmp.X;
        part->joint_position.Y = part->parent_actor->position_vector.Y + tmp.Y;
        part->joint_position.Z = part->parent_actor->position_vector.Z + tmp.Z;
        copy_matrix(&part->matrix_1, &part->parent_actor->matrix_1);
    }

    if (part->Rotate.Y) rotate_about_y(&part->matrix_1, part->Rotate.Y);
    if (part->Rotate.X) rotate_about_x(&part->matrix_1, part->Rotate.X);
    if (part->Rotate.Z) rotate_about_z(&part->matrix_1, part->Rotate.Z);

    matrix_vector(&part->VECTOR_RelCentre, &part->ellipse_center, &part->matrix_1);
    add_vector(&part->ellipse_center, &part->joint_position);
}

/* display_find_positions_423858 — core recursive skeleton solver
 *
 * Walks the part tree rooted at 'parent', computing world‑space joint
 * positions (joint_position) and ellipse centres (ellipse_center) for every limb.
 *
 * The original 32‑bit code cast a part_t* to actor_t* for the
 * recursive call, relying on the first 80 bytes being layout‑compatible.
 * On 64‑bit the pointer sizes differ so we pass the parent context
 * explicitly instead.
 *
 * parent_joint_position  – parent's joint world position
 * parent_matrix    – parent's accumulated rotation matrix
 * parent_offset    – actor->field_6E_vect (root) or part->VECTOR_RelCentre (sub‑limb)
 * parent_type      – actor->type (root) or part->type (sub‑limb)
 * first_part       – first part (limb) to process
 * skip_first       – if true, start from first_part->next (a2 flag in original)
 */
static void find_positions_recursive(
    vector_t *parent_joint_position,
    matrix3x3_t *parent_matrix,
    vector_t *parent_offset,
    int parent_type,
    part_t *first_part,
    int skip_first)
{
    static int fp_log = 0;
    part_t *part = first_part;
    if (skip_first && part) part = part->next;
    if (!part) return;

    while (part) {
        if (fp_log < 20) {
            DBG_LOG(2, "[FP] part=%p type=%d ptype=%d Offset=(%d,%d,%d) RelCentre=(%d,%d,%d) parent_f18=(%d,%d,%d) parent_off=(%d,%d,%d)\n",
                (void*)part, part->type, parent_type,
                part->Offset.X, part->Offset.Y, part->Offset.Z,
                part->VECTOR_RelCentre.X, part->VECTOR_RelCentre.Y, part->VECTOR_RelCentre.Z,
                parent_joint_position->X, parent_joint_position->Y, parent_joint_position->Z,
                parent_offset->X, parent_offset->Y, parent_offset->Z);
            fp_log++;
        }
        /* 1. Compute the offset vector for this limb joint */
        vector_t input;
        if (parent_type == 7) {
            copy_vector(&input, &part->Offset);
        } else {
            copy_vector(&input, parent_offset);
            add_vector(&input, &part->Offset);
        }

        /* 2. Transform offset into world space → field_50 */
        matrix_vector(&input, &part->field_50, parent_matrix);

        /* 3. Joint world position = parent joint pos + transformed offset */
        copy_vector(&part->joint_position, parent_joint_position);
        add_vector(&part->joint_position, &part->field_50);

        /* 4. Build part rotation matrix */
        if (part->flags & 0x10) {
            copy_matrix(&part->matrix_1, &part->matr_d);
        } else {
            copy_matrix(&part->matrix_1, parent_matrix);
        }

        /* 5. Bug 58: TwoPartsLimb 2-part IK.
         * Flag 0x20 (TwoPartsLimb): part is upper limb, part->actor_parts_list
         * = middle (elbow/knee), middle->actor_parts_list = extremity
         * (hand/foot). Extremity target position drives IK: upper Y+X face
         * extremity, then arcsin joint solve chooses upper/middle X rotations
         * so joint chain reaches extremity. Was missing → arms/legs held at
         * default keyframe pose, no IK bending. */
        if ((part->flags & 0x20) && part->actor_parts_list &&
                part->actor_parts_list->actor_parts_list) {
            part_t *second = part->actor_parts_list;
            part_t *extremity = second->actor_parts_list;

            find_position_of_extremity(extremity);

            vector_t world_d;
            world_d.X = extremity->joint_position.X - part->joint_position.X;
            world_d.Y = extremity->joint_position.Y - part->joint_position.Y;
            world_d.Z = extremity->joint_position.Z - part->joint_position.Z;

            matrix3x3_t inv;
            matrix_inverse(&inv, &part->matrix_1);
            vector_t part_d;
            matrix_vector(&world_d, &part_d, &inv);

            int16_t rot_y = arctan(part_d.X, part_d.Z);
            if (rot_y) rotate_vector_about_y(&part_d, (int16_t)-rot_y);
            int16_t rot_x = arctan((int16_t)-part_d.Y, part_d.Z);
            if (rot_x) rotate_vector_about_x(&part_d, (int16_t)-rot_x);

            int dist_to_second = part->def_RelCentre.Z + second->def_offset.Z;
            int dist_to_extremity = second->def_RelCentre.Z + extremity->def_offset.Z;
            int total_dist = dist_to_second + dist_to_extremity;

            int16_t bend_near = 0, bend_far = 0;

            if (total_dist <= part_d.Z) {
                /* Extremity out of reach — squash upper/middle Z instead. */
                if (total_dist != 0) {
                    int stretch = (part_d.Z * 0x2000) / total_dist;
                    part->VECTOR_Squash.Z = (int16_t)((stretch * part->def_Squash.Z) / 0x2000);
                    calculate_squash(part);
                    part->VECTOR_RelCentre.Z = (int16_t)((stretch * part->def_RelCentre.Z) / 0x2000);
                    second->Offset.Z = (int16_t)((stretch * second->def_offset.Z) / 0x2000);
                    second->VECTOR_Squash.Z = (int16_t)((stretch * second->def_Squash.Z) / 0x2000);
                    calculate_squash(second);
                    second->VECTOR_RelCentre.Z = (int16_t)((stretch * second->def_RelCentre.Z) / 0x2000);
                    extremity->Offset.Z = (int16_t)((stretch * extremity->def_offset.Z) / 0x2000);
                }
            } else {
                /* Restore def sizes, then run law-of-cosines-style IK. */
                if (part->VECTOR_Squash.Z != part->def_Squash.Z) {
                    part->VECTOR_Squash.Z = part->def_Squash.Z;
                    calculate_squash(part);
                }
                part->VECTOR_RelCentre.Z = part->def_RelCentre.Z;
                second->Offset.Z = second->def_offset.Z;
                if (second->VECTOR_Squash.Z != second->def_Squash.Z) {
                    second->VECTOR_Squash.Z = second->def_Squash.Z;
                    calculate_squash(second);
                }
                second->VECTOR_RelCentre.Z = second->def_RelCentre.Z;
                extremity->Offset.Z = extremity->def_offset.Z;

                if (part_d.Z != 0) {
                    if (dist_to_second != 0 && dist_to_extremity != 0) {
                        int cos_term = ((dist_to_second * dist_to_second
                                    - dist_to_extremity * dist_to_extremity)
                                   / part_d.Z) >> 1;
                        int arg1 = (part_d.Z / 2 + cos_term) * 0x4000 / dist_to_second;
                        int arg2 = (part_d.Z / 2 - cos_term) * 0x4000 / dist_to_extremity;
                        bend_near = (int16_t)(0x4000 - arcsin((int16_t)arg1));
                        bend_far = (int16_t)(0x4000 - arcsin((int16_t)arg2));
                    }
                } else {
                    bend_far = 0x4000;
                    bend_near = 0x4000;
                }
            }
            second->Rotate.X = (int16_t)(-(bend_near + bend_far));
            second->Rotate.Y = 0;
            second->Rotate.Z = 0;

            if (rot_y) rotate_about_y(&part->matrix_1, rot_y);
            if (rot_x) rotate_about_x(&part->matrix_1, rot_x);

            int16_t rot_z = part->Rotate.Z;
            if (part->flags & 0x80) {  /* FollowExtremity */
                vector_t v;
                v.X = extremity->matrix_1._13 - extremity->matrix_1._12;
                v.Y = extremity->matrix_1._23 - extremity->matrix_1._22;
                v.Z = extremity->matrix_1._33 - extremity->matrix_1._32;
                matrix3x3_t m;
                matrix_inverse(&m, &part->matrix_1);
                vector_t r;
                matrix_vector(&v, &r, &m);
                rot_z = (int16_t)(rot_z + arctan(r.X, (int16_t)-r.Y));
            }
            if (rot_z) rotate_about_z(&part->matrix_1, rot_z);
            if (bend_near) rotate_about_x(&part->matrix_1, bend_near);

            matrix_vector(&part->VECTOR_RelCentre, &part->ellipse_center, &part->matrix_1);
            add_vector(&part->ellipse_center, &part->joint_position);

            part->flags |= 0x8000;
            for (point_t *pt = part->points_list; pt; pt = pt->next) {
                matrix_vector(&pt->offset_point, &pt->world_position, &part->matrix_1);
                add_vector(&pt->world_position, &part->ellipse_center);
            }

            /* Position second part relative to solved upper. */
            find_a_position((actor_t *)part, second);

            /* Bug 64: FindPositions(core, is_sub_part) also sets PositionFound
             * flag and transforms points on the core BEFORE recursing. Prior
             * port only did this on `part` and skipped `second` /
             * `extremity` — their points rendered with stale world coords
             * and their PositionFound flag never got set (breaks downstream
             * blocked_parts_list constraint checks). */
            if (second->type == 4) {
                second->flags |= 0x8000;
                for (point_t *pt = second->points_list; pt; pt = pt->next) {
                    matrix_vector(&pt->offset_point, &pt->world_position, &second->matrix_1);
                    add_vector(&pt->world_position, &second->ellipse_center);
                }
            }
            if (extremity->type == 4) {
                extremity->flags |= 0x8000;
                for (point_t *pt = extremity->points_list; pt; pt = pt->next) {
                    matrix_vector(&pt->offset_point, &pt->world_position, &extremity->matrix_1);
                    add_vector(&pt->world_position, &extremity->ellipse_center);
                }
            }

            /* Recurse into any siblings that hang off part/second/extremity. */
            if (part->actor_parts_list && part->actor_parts_list->next) {
                find_positions_recursive(
                    &part->joint_position, &part->matrix_1, &part->VECTOR_RelCentre,
                    part->type, part->actor_parts_list, 1);
            }
            if (second->actor_parts_list && second->actor_parts_list->next) {
                find_positions_recursive(
                    &second->joint_position, &second->matrix_1, &second->VECTOR_RelCentre,
                    second->type, second->actor_parts_list, 1);
            }
            if (extremity->actor_parts_list) {
                find_positions_recursive(
                    &extremity->joint_position, &extremity->matrix_1, &extremity->VECTOR_RelCentre,
                    extremity->type, extremity->actor_parts_list, 0);
            }

            part = part->next;
            continue;
        }

        point_t *target_pt = part->field_12E_point_to_point;
        if (target_pt) {
            if (target_pt->parent_part &&
                    (target_pt->parent_part->flags & 0x8000)) {
                vector_t world_d;
                world_d.X = target_pt->world_position.X - part->joint_position.X;
                world_d.Y = target_pt->world_position.Y - part->joint_position.Y;
                world_d.Z = target_pt->world_position.Z - part->joint_position.Z;

                matrix3x3_t inv;
                matrix_inverse(&inv, &part->matrix_1);
                vector_t part_d;
                matrix_vector(&world_d, &part_d, &inv);

                int16_t rY = arctan(part_d.X, part_d.Z);
                part->Rotate.Y = rY;
                if (rY) rotate_vector_about_y(&part_d, (int16_t)-rY);

                int16_t rX = arctan((int16_t)-part_d.Y, part_d.Z);
                part->Rotate.X = rX;
                if (rX) rotate_vector_about_x(&part_d, (int16_t)-rX);

                int16_t squash_z = (int16_t)(part_d.Z / 2);
                part->VECTOR_Squash.Z = squash_z;
                part->def_Squash.Z    = squash_z;
                calculate_squash(part);
                part->VECTOR_RelCentre.X = 0;
                part->VECTOR_RelCentre.Y = 0;
                part->VECTOR_RelCentre.Z = squash_z;
                part->def_RelCentre.X = 0;
                part->def_RelCentre.Y = 0;
                part->def_RelCentre.Z = squash_z;

                if (rY) rotate_about_y(&part->matrix_1, rY);
                if (rX) rotate_about_x(&part->matrix_1, rX);

                int16_t rZ = arctan(part->matrix_1._12, part->matrix_1._22);
                part->Rotate.Z = rZ;
                if (rZ) rotate_about_z(&part->matrix_1, rZ);

                matrix_vector(&part->VECTOR_RelCentre, &part->ellipse_center, &part->matrix_1);
                add_vector(&part->ellipse_center, &part->joint_position);

                part->flags |= 0x8000;
                for (point_t *pt = part->points_list; pt; pt = pt->next) {
                    matrix_vector(&pt->offset_point, &pt->world_position, &part->matrix_1);
                    add_vector(&pt->world_position, &part->ellipse_center);
                }
                if (part->actor_parts_list) {
                    find_positions_recursive(
                        &part->joint_position, &part->matrix_1, &part->VECTOR_RelCentre,
                        part->type, part->actor_parts_list, 0);
                }
                part = part->next;
                continue;
            } else {
                part->blocked_part = blocked_parts_list[db_bpl];
                blocked_parts_list[db_bpl] = part;
                part = part->next;
                continue;
            }
        }

        /* 5. Apply per-part Euler rotations (Y → X → Z) */
        if (part->Rotate.Y) rotate_about_y(&part->matrix_1, part->Rotate.Y);
        if (part->Rotate.X) rotate_about_x(&part->matrix_1, part->Rotate.X);
        if (part->Rotate.Z) rotate_about_z(&part->matrix_1, part->Rotate.Z);

        /* 6. Ellipse world centre = matrix * VECTOR_RelCentre + joint_position */
        matrix_vector(&part->VECTOR_RelCentre, &part->ellipse_center, &part->matrix_1);
        add_vector(&part->ellipse_center, &part->joint_position);

        /* 7. Mark this part's positions as solved */
        part->flags |= 0x8000;

        /* 7b. Bug 57: transform this part's points into world space. */
        for (point_t *pt = part->points_list; pt; pt = pt->next) {
            matrix_vector(&pt->offset_point, &pt->world_position, &part->matrix_1);
            add_vector(&pt->world_position, &part->ellipse_center);
        }

        /* 8. Recurse into child parts (sub-limbs) */
        if (part->actor_parts_list) {
            find_positions_recursive(
                &part->joint_position,
                &part->matrix_1,
                &part->VECTOR_RelCentre,
                part->type,
                part->actor_parts_list,
                0);
        }

        part = part->next;
    }
}

/* display_find_positions_423858 — entry point */
void find_positions(actor_t *actor, int skip_first) {
    if (!actor) return;

    /* Set actor solved flag for non-type-7 actors */
    if (actor->type != 7) {
        actor->flags |= 0x8000;
        /* Transform repertoire vectors from local to world space */
        for (rephead_t *rep = actor->actor_reperture; rep; rep = *(rephead_t **)&rep->action_slots[18]) {
            vector_t *v1 = (vector_t *)&rep->action_slots[1];  /* vector1 */
            vector_t *v2 = (vector_t *)&rep->action_slots[7];  /* vector2 */
            matrix_vector(v1, v2, &actor->matrix_1);
            add_vector(v2, &actor->position_vector);
        }
    }

    /* Recursively compute positions for all limbs */
    find_positions_recursive(
        &actor->joint_position,
        &actor->matrix_1,
        &actor->field_6E_vect,
        actor->type,
        actor->actor_parts_list,
        skip_first);
}

/* display_find_a_position_424164 — Bug 63: was reading actor
 * `position_vector` (offset 0x84 in actor, but 0x84 in part is ellipse_center.Z)
 * and using wrong parent matrix. FindAPosition
 * takes a `part_core*` (actor or part-typed root) and reads shared-prefix
 * fields (matrix_1, joint_position, type). When core is Part (type != Actor),
 * offset also includes core->VECTOR_RelCentre. Rewrites to match. */
void find_a_position(actor_t *core, part_t *part) {
    if (!core || !part) return;

    if (part->flags & 0x10) copy_matrix(&part->matrix_1, &part->matr_d);
    else                    copy_matrix(&part->matrix_1, &core->matrix_1);

    if (part->Rotate.Y) rotate_about_y(&part->matrix_1, part->Rotate.Y);
    if (part->Rotate.X) rotate_about_x(&part->matrix_1, part->Rotate.X);
    if (part->Rotate.Z) rotate_about_z(&part->matrix_1, part->Rotate.Z);

    vector_t parent_offset = part->Offset;
    if (core->type != 7) {
        part_t *core_part = (part_t *)core;
        parent_offset.X += core_part->VECTOR_RelCentre.X;
        parent_offset.Y += core_part->VECTOR_RelCentre.Y;
        parent_offset.Z += core_part->VECTOR_RelCentre.Z;
    }

    vector_t world_offset;
    matrix_vector(&parent_offset, &world_offset, &core->matrix_1);
    part->joint_position.X = core->joint_position.X + world_offset.X;
    part->joint_position.Y = core->joint_position.Y + world_offset.Y;
    part->joint_position.Z = core->joint_position.Z + world_offset.Z;

    /* asm checks `type == 4` (ThingType::Part) — exact match, not != 7. */
    if (part->type == 4) {
        matrix_vector(&part->VECTOR_RelCentre, &part->ellipse_center, &part->matrix_1);
        part->ellipse_center.X += part->joint_position.X;
        part->ellipse_center.Y += part->joint_position.Y;
        part->ellipse_center.Z += part->joint_position.Z;
    }
}

/* display_find_positions_on_path_424318
 * Walks the next_in_path linear path chain (not the actor_parts_list tree). */
void find_positions_on_path(actor_t *actor) {
    if (!actor) return;

    if (actor->part_heap_link) {
        hold_thing_with_part(actor, actor->part_heap_link);
    } else {
        make_identity(&actor->matrix_1);
        if (actor->rotate_vector.Y)
            rotate_about_y(&actor->matrix_1, actor->rotate_vector.Y);
        if (actor->rotate_vector.X)
            rotate_about_x(&actor->matrix_1, actor->rotate_vector.X);
        if (actor->rotate_vector.Z)
            rotate_about_z(&actor->matrix_1, actor->rotate_vector.Z);
    }
    copy_vector(&actor->joint_position, &actor->position_vector);

    if (!actor->next_in_path) return;

    actor_t *core = actor;
    part_t *next_on_path;
    do {
        part_t *limb = core->next_in_path;

        vector_t offset;
        copy_vector(&offset, &limb->Offset);
        if (core->type != 7) {
            part_t *cp = (part_t *)core;
            offset.X += cp->VECTOR_RelCentre.X;
            offset.Y += cp->VECTOR_RelCentre.Y;
            offset.Z += cp->VECTOR_RelCentre.Z;
        }

        vector_t world_off;
        matrix_vector(&offset, &world_off, &core->matrix_1);
        limb->joint_position.X = core->joint_position.X + world_off.X;
        limb->joint_position.Y = core->joint_position.Y + world_off.Y;
        limb->joint_position.Z = core->joint_position.Z + world_off.Z;

        if (limb->flags & 0x10)
            copy_matrix(&limb->matrix_1, &limb->matr_d);
        else
            copy_matrix(&limb->matrix_1, &core->matrix_1);

        if (limb->flags & 0x20) {
            part_t *second = limb->actor_parts_list;
            if (!second) beep_message("2 part limb has no 2nd part");
            part_t *extremity = second ? second->actor_parts_list : NULL;
            if (!extremity) beep_message("2 part limb has no extremity");

            if (second && extremity) {
                find_position_of_extremity(extremity);

                vector_t world_d;
                world_d.X = extremity->joint_position.X - limb->joint_position.X;
                world_d.Y = extremity->joint_position.Y - limb->joint_position.Y;
                world_d.Z = extremity->joint_position.Z - limb->joint_position.Z;

                matrix3x3_t inv;
                matrix_inverse(&inv, &limb->matrix_1);
                vector_t part_d;
                matrix_vector(&world_d, &part_d, &inv);

                int16_t rot_y = arctan(part_d.X, part_d.Z);
                if (rot_y) rotate_vector_about_y(&part_d, (int16_t)-rot_y);
                int16_t rot_x = arctan((int16_t)-part_d.Y, part_d.Z);
                if (rot_x) rotate_vector_about_x(&part_d, (int16_t)-rot_x);

                int16_t d_second = limb->VECTOR_RelCentre.Z + second->Offset.Z;
                int16_t d_extremity = second->VECTOR_RelCentre.Z + extremity->Offset.Z;

                int16_t bend_near = 0, bend_far = 0;
                if (part_d.Z != 0) {
                    if (d_second != 0 && d_extremity != 0) {
                        int cos_term = ((int)d_second * d_second
                                   - (int)d_extremity * d_extremity)
                                  / part_d.Z >> 1;
                        int half = part_d.Z / 2;
                        bend_near = (int16_t)(0x4000 - arcsin(
                            (int16_t)((half + cos_term) * 0x4000 / d_second)));
                        bend_far = (int16_t)(0x4000 - arcsin(
                            (int16_t)((half - cos_term) * 0x4000 / d_extremity)));
                    }
                } else {
                    bend_far = 0x4000;
                    bend_near = 0x4000;
                }

                second->Rotate.X = (int16_t)(-(bend_near + bend_far));
                second->Rotate.Y = 0;
                second->Rotate.Z = 0;

                if (rot_y) rotate_about_y(&limb->matrix_1, rot_y);
                if (rot_x) rotate_about_x(&limb->matrix_1, rot_x);

                int16_t rot_z = limb->Rotate.Z;
                if (limb->flags & 0x80) {
                    vector_t v;
                    v.X = extremity->matrix_1._13 - extremity->matrix_1._12;
                    v.Y = extremity->matrix_1._23 - extremity->matrix_1._22;
                    v.Z = extremity->matrix_1._33 - extremity->matrix_1._32;
                    matrix3x3_t m;
                    matrix_inverse(&m, &limb->matrix_1);
                    vector_t r;
                    matrix_vector(&v, &r, &m);
                    rot_z = (int16_t)(rot_z + arctan(r.X, (int16_t)-r.Y));
                }
                if (rot_z) rotate_about_z(&limb->matrix_1, rot_z);
                if (bend_near) rotate_about_x(&limb->matrix_1, bend_near);

                matrix_vector(&limb->VECTOR_RelCentre, &limb->ellipse_center,
                              &limb->matrix_1);
                add_vector(&limb->ellipse_center, &limb->joint_position);

                if (second == limb->next_in_path) {
                    find_a_position((actor_t *)limb, second);
                    if (extremity == second->next_in_path)
                        core = (actor_t *)second;
                }
            }
        } else {
            if (limb->Rotate.Y) rotate_about_y(&limb->matrix_1, limb->Rotate.Y);
            if (limb->Rotate.X) rotate_about_x(&limb->matrix_1, limb->Rotate.X);
            if (limb->Rotate.Z) rotate_about_z(&limb->matrix_1, limb->Rotate.Z);

            matrix_vector(&limb->VECTOR_RelCentre, &limb->ellipse_center,
                          &limb->matrix_1);
            add_vector(&limb->ellipse_center, &limb->joint_position);
        }

        next_on_path = core->next_in_path->next_in_path;
        core = (actor_t *)core->next_in_path;
    } while (next_on_path);
}

/* display_find_rotations_on_path_424858
 * Walks next_in_path path chain, computing rotation matrices only. */
void find_rotations_on_path(actor_t *actor) {
    if (!actor) return;

    make_identity(&actor->matrix_1);
    if (actor->rotate_vector.Y)
        rotate_about_y(&actor->matrix_1, actor->rotate_vector.Y);
    if (actor->rotate_vector.X)
        rotate_about_x(&actor->matrix_1, actor->rotate_vector.X);
    if (actor->rotate_vector.Z)
        rotate_about_z(&actor->matrix_1, actor->rotate_vector.Z);

    actor_t *c = actor;
    while (c->next_in_path) {
        part_t *child = c->next_in_path;
        if (child->flags & 0x10)
            copy_matrix(&child->matrix_1, &child->matr_d);
        else
            copy_matrix(&child->matrix_1, &c->matrix_1);

        if (child->Rotate.Y) rotate_about_y(&child->matrix_1, child->Rotate.Y);
        if (child->Rotate.X) rotate_about_x(&child->matrix_1, child->Rotate.X);
        if (child->Rotate.Z) rotate_about_z(&child->matrix_1, child->Rotate.Z);

        c = (actor_t *)child;
    }
}

/* display_adjust_for_anchored_part  E1: 0x420D20 | E2: 0x424900 */
void adjust_for_anchored_part(actor_t *actor) {
    /* No-op: adjusts part positions for anchored parts.
       Zero callers.  May need implementing if anchored-part rendering is added. */
    if (!actor) return;
}

/* display_find_view_positions  E1: 0x420DE4 | E2: 0x4249C4 */
void find_view_positions(actor_t *actor) {
    if (!actor) return;

    /* Transform all parts to view space (recursive tree traversal) */
    part_t *part = actor->actor_parts_list;
    while (part) {
        view_transform_ellipse(part);

        /* Transform all points to view space */
        point_t *point = part->points_list;
        while (point) {
            view_transform(&point->screen_coord, &point->world_position);
            point = point->next;
        }

        /* Recurse into child parts (part shares layout with actor) */
        find_view_positions((actor_t *)part);

        part = part->next;
    }
}

/* display_put_parts_select  E1: 0x4210AC | E2: 0x424C8C */
void put_parts_select(actor_t *actor) {
    if (!actor) return;
    put_parts_not_shadows(actor);
    put_triangles(actor);
}

/* display_put_parts_not_shadows  E1: 0x421104 | E2: 0x424CE4 */
void put_parts_not_shadows(actor_t *actor) {
    if (!actor) return;

    part_t *part = actor->actor_parts_list;
    while (part) {
        if (!(part->flags & 0x400) && !(part->flags & 0x42)) {
            if (part->flags & 0x1) {
                put_a_cuboid(part);
            } else {
                put_an_ellipse(part);
            }
        }
        part = part->next_in_display_list;
    }
}

/* display_put_shadows  E1: 0x421148 | E2: 0x424D28 */
void put_shadows(actor_t *actor) {
    if (!actor) return;

    part_t *part = actor->actor_parts_list;
    while (part) {
        /* Bug 50: asm at 0x424D61 skips parts with flags & 0x400 (dirty/bg) */
        if (!(part->flags & 0x400) && (part->flags & 0x42) && part->color == 0) {
            put_an_ellipse(part);
        }
        /* asm increments select_num per iteration; skip — editor-only. */
        part = part->next_in_display_list;
    }
}

/* display_put_smoke  E1: 0x421188 | E2: 0x424D68 */
void put_smoke(actor_t *actor) {
    if (!actor) return;

    part_t *part = actor->actor_parts_list;
    while (part) {
        if (!(part->flags & 0x400) && (part->flags & 0x42) && part->color != 0) {
            put_an_ellipse(part);
        }
        /* asm increments select_num per iteration; skip — editor-only. */
        part = part->next_in_display_list;
    }
}

/* display_put_triangles  E1: 0x4211C8 | E2: 0x424DA8 */
void put_triangles(actor_t *actor) {
    if (!actor) return;

    /* Bug 51: asm at 0x424DFA gates on flags & 0x400 (dirty) AND
     * per-LOD masks: LOD 0 → skip if flags & 6, LOD 1 → skip if flags & 4,
     * LOD 2 → always draw. Was `tri_use_flag <= level_of_detail`, wrong. */
    tri_t *tri = actor->polygone_tri_list;
    while (tri) {
        uint16_t f = tri->tri_use_flag;
        bool draw = !(f & 0x400);
        if (draw) {
            if (level_of_detail == 0) {
                if (f & 0x6) draw = false;
            } else if (level_of_detail == 1) {
                if (f & 0x4) draw = false;
            } else if (level_of_detail != 2) {
                draw = false;
            }
        }
        if (draw)
            put_a_triangle(tri);
        tri = tri->next;
    }
}

/* display_put_a_triangle_424E44 — Bug 59: was doing back-face cull
 * here + passing NULL shade. Correct: delegate cull to draw_polygon and
 * fetch the shade-target triangle via
 * parent_actor->_TriangleTab->field_0[tri_shade_name] when tri_shade_name>=0. */
void put_a_triangle(tri_t *tri) {
    if (!tri || !tri->parent_actor) return;

    tri_t *shade = NULL;
    if (tri->tri_shade_name >= 0 && tri->parent_actor->_TriangleTab)
        shade = tri->parent_actor->_TriangleTab->field_0[tri->tri_shade_name];

    int target_plane = (tri->parent_actor->flags & 0x400) ? 2 : (1 - db);
    draw_polygon(tri, target_plane, shade);
}

/* display_view_transform_ellipse  E1: 0x4212C0 | E2: 0x424EA0 */
void view_transform_ellipse(part_t *part) {
    if (!part) return;

    static int vte_log = 0;
    if (vte_log < 10) {
        DBG_LOG(2, "[VTE] part=%p ellipse_center=(%d,%d,%d) view_pos=(%d,%d,%d) vm_row3=(%d,%d,%d)\n",
            (void*)part,
            part->ellipse_center.X, part->ellipse_center.Y, part->ellipse_center.Z,
            view_pos.X, view_pos.Y, view_pos.Z,
            view_matrix._31, view_matrix._32, view_matrix._33);
        vte_log++;
    }

    /* Transform center to view space */
    vector_t rel;
    rel.X = part->ellipse_center.X - view_pos.X;
    rel.Y = part->ellipse_center.Y - view_pos.Y;
    rel.Z = part->ellipse_center.Z - view_pos.Z;

    matrix_vector(&rel, &part->vector_persp, &view_matrix);

    /* Clamp Z */
    if (part->vector_persp.Z < 128) {
        part->vector_persp.Z = 0;
    }

    /* Save un-perspectived view-space X/Y for FindEllipse correction */
    part->persp_origin.X = part->vector_persp.X;
    part->persp_origin.Y = part->vector_persp.Y;
}

/* display_put_a_cuboid  E1: 0x421408 | E2: 0x424FE8 */
void put_a_cuboid(part_t *part) {
    if (!part) return;

    /* Face vertex index tables (12 triangles = 2 per face) */
    static const int fva[12] = { 0, 3, 4, 7, 4, 2, 5, 0, 7, 2, 7, 1 };
    static const int fvb[12] = { 2, 1, 5, 6, 6, 0, 4, 1, 3, 6, 5, 3 };
    static const int fvc[12] = { 1, 2, 6, 5, 0, 6, 1, 4, 6, 3, 3, 5 };

    int16_t sx = part->VECTOR_Squash.X;
    int16_t sy = part->VECTOR_Squash.Y;
    int16_t sz = part->VECTOR_Squash.Z;

    /* A zero half-extent gives the box no volume: the eight corners collapse
     * into four coincident pairs, eight of the twelve faces become zero-area
     * and get dropped by the backface test, and the four that survive are the
     * two windings of the collapsed side face — which paint as a solid flat
     * quad. Such parts are pivot/helper geometry and must not draw. */
    if (!sx || !sy || !sz)
        return;

    /* 8 corner vertices */
    point_t points[8];
    memset(points, 0, sizeof(points));

    points[0].offset_point = (vector_t){{{ sx,  sy,  sz}}};
    points[1].offset_point = (vector_t){{{-sx,  sy,  sz}}};
    points[2].offset_point = (vector_t){{{ sx, -sy,  sz}}};
    points[3].offset_point = (vector_t){{{-sx, -sy,  sz}}};
    points[4].offset_point = (vector_t){{{ sx,  sy, -sz}}};
    points[5].offset_point = (vector_t){{{-sx,  sy, -sz}}};
    points[6].offset_point = (vector_t){{{ sx, -sy, -sz}}};
    points[7].offset_point = (vector_t){{{-sx, -sy, -sz}}};

    for (int i = 0; i < 8; i++) {
        vector_t world;
        matrix_vector(&points[i].offset_point, &world, &part->matrix_1);
        points[i].world_position.X = world.X + part->ellipse_center.X;
        points[i].world_position.Y = world.Y + part->ellipse_center.Y;
        points[i].world_position.Z = world.Z + part->ellipse_center.Z;
        view_transform(&points[i].screen_coord, &points[i].world_position);
    }

    /* Set up triangle descriptor */
    tri_t draw_tri;
    memset(&draw_tri, 0, sizeof(draw_tri));
    draw_tri.parent_actor = part->parent_actor;
    draw_tri.tri_color_3 = part->color;
    draw_tri.shade_multiplier = part->color_shade;

    int plane = (part->parent_actor && (part->parent_actor->flags & 0x400)) ? 2 : 1 - db;

    texture_t *tex = NULL;
    int16_t tex_idx = part->part_texture_index;
    if (tex_idx >= 0 && !select_flag) {
        if (tex_idx < TEXTURE_TAB_SIZE)
            tex = texture_tab[tex_idx];
        if (tex)
            tex->textur_time = game_time;
        draw_tri.texture_name_index = tex_idx;
    }

    tri_t shade_tri;
    memset(&shade_tri, 0, sizeof(shade_tri));

    /* ECSTATICA_TRACE_CUBOID=<name_index>: one line per part per frame with the
     * projected corners, the screen bbox, how many of the twelve faces survive
     * the cross_z backface test, and which subarea the dirty rect lands in. */
    {
        static int tidx = -2;
        static int nlines = 0;
        if (tidx == -2) {
            const char *e = getenv("ECSTATICA_TRACE_CUBOID");
            tidx = (e && *e) ? atoi(e) : -1;
        }
        if (tidx >= 0 && part->parent_actor
            && part->parent_actor->name_index == tidx && nlines++ < 4000) {
            int lo_x = 0x7FFF, hi_x = -0x7FFF, lo_y = 0x7FFF, hi_y = -0x7FFF;
            int zero_z = 0;
            for (int i = 0; i < 8; i++) {
                int X = points[i].screen_coord.X, Y = points[i].screen_coord.Y;
                if (!points[i].screen_coord.Z) zero_z++;
                if (X < lo_x) lo_x = X;
                if (X > hi_x) hi_x = X;
                if (Y < lo_y) lo_y = Y;
                if (Y > hi_y) hi_y = Y;
            }
            int front = 0, degen = 0;
            for (int i = 0; i < 12; i++) {
                const point_t *p1 = &points[fva[i]], *p2 = &points[fvb[i]], *p3 = &points[fvc[i]];
                int cz = (p2->screen_coord.Y - p3->screen_coord.Y)
                       * (p1->screen_coord.X - p3->screen_coord.X)
                       - (p1->screen_coord.Y - p3->screen_coord.Y)
                       * (p2->screen_coord.X - p3->screen_coord.X);
                if (cz > 0) front++;
                else if (cz == 0) degen++;
            }
            subarea_t *ac = part->parent_actor->area_to_clear;
            const char *acw = !ac ? "NULL"
                : (ac == &null_area_to_clear) ? "null_area"
                : (ac >= &sub_area_to_clear[0] && ac < &sub_area_to_clear[20]) ? "SUBTITLE"
                : (ac == &part->parent_actor->bounding_box) ? "bbox" : "cleartab";
            fprintf(stderr, "[CUB] part=%3d col=%3d shade=%5d plane=%d tex=%d "
                            "sq=(%d,%d,%d) sx=[%d..%d] sy=[%d..%d] z0=%d "
                            "front=%d degen=%d ac=%s\n",
                    part->name_index, part->color, part->color_shade, plane,
                    tex ? (int)tex_idx : -1, sx, sy, sz,
                    lo_x >> 4, hi_x >> 4, lo_y >> 4, hi_y >> 4, zero_z,
                    front, degen, acw);
            fflush(stderr);
        }
    }

    for (int i = 0; i < 12; i++) {
        draw_tri.point1 = &points[fva[i]];
        draw_tri.point2 = &points[fvb[i]];
        draw_tri.point3 = &points[fvc[i]];

        /* asm display_put_a_cuboid_424FE8+423: sets and clears bit 3 of
         * tri_use_flag rather than assigning the whole field, so the rest of
         * the flag survives across the twelve faces. */
        if (i & 1)
            draw_tri.tri_use_flag |= 0x08;
        else
            draw_tri.tri_use_flag &= (uint16_t)~0x08;

        /* asm +3A7: the faces come in pairs making up one quad, and the shading
         * triangle is always the first of the pair — the original indexes the
         * vertex tables one entry back for odd i. Shading each half off its own
         * vertices instead gave the two halves of every quad different shades. */
        int si = (i & 1) ? i - 1 : i;
        shade_tri.point1 = &points[fva[si]];
        shade_tri.point2 = &points[fvb[si]];
        shade_tri.point3 = &points[fvc[si]];

        if (tex && !eagle_card)
            draw_textured_tri(&draw_tri, plane, &shade_tri);
        else
            draw_triangle_ell(&draw_tri, plane, &shade_tri);
    }
}

/* display_put_an_ellipse  E1: 0x4218B8 | E2: 0x425508 */
void put_an_ellipse(part_t *part) {
    if (!part) return;

    /* Perspective transform center — applied IN PLACE */
    if (part->vector_persp.Z)
        perspective_transform(&part->vector_persp);
    if (!part->vector_persp.Z) {
        return;
    }

    /* Gameplay path (asm 0x425580): PRE-multiply view rotations in
     * order Ry, Rx, Rz.  matrix_2 = Rz * Rx * Ry * matrix_1.
     * asm_matrix_mult uses ESI*EBX→EDI; callers set ESI=rot, EBX=matrix_2. */
    {
        matrix3x3_t rot, result;
        copy_matrix(&part->matrix_2, &part->matrix_1);
        if (view_rot.Y) {
            make_rot_matrix(&rot, 0, view_rot.Y, 0);
            matrix_mult(&result, &rot, &part->matrix_2);
            copy_matrix(&part->matrix_2, &result);
        }
        if (view_rot.X) {
            make_rot_matrix(&rot, view_rot.X, 0, 0);
            matrix_mult(&result, &rot, &part->matrix_2);
            copy_matrix(&part->matrix_2, &result);
        }
        if (view_rot.Z) {
            make_rot_matrix(&rot, 0, 0, view_rot.Z);
            matrix_mult(&result, &rot, &part->matrix_2);
            copy_matrix(&part->matrix_2, &result);
        }
    }

    /* Compute ellipse on screen and shade */
    find_ellipse(part);
    {
        int plane = 1 - db;
        if (part->parent_actor && (part->parent_actor->flags & 0x400)) {
            plane = 2;
        }
        shade_ellipse(part, plane);
    }
}

/* display_put_a_circle  E1: ? | E2: 0x424E68 */
void put_a_circle(part_t *part) {
    part->projected_axes.X = part->VECTOR_Squash.Z;
    part->projected_axes.Y = part->VECTOR_Squash.Z;
    part->projected_axes.Z = 0;
    int plane = 1 - db;
    shade_ellipse(part, plane);
}

/* display_long_view_trans_ellipse  E1: ? | E2: 0x424F6C */
void long_view_trans_ellipse(part_t *part) {
    long_vector_t rel;
    rel.l_X = part->ellipse_center.X - view_pos.X;
    rel.l_Y = part->ellipse_center.Y - view_pos.Y;
    rel.l_Z = part->ellipse_center.Z - view_pos.Z;

    long_vector_t out;
    matrix_long_vector(&rel, &out, &view_matrix);

    part->filler_13E[0] = (int32_t)out.l_X;
    part->filler_13E[1] = (int32_t)out.l_Y;
    part->filler_13E[2] = (int32_t)out.l_Z;

    if (near_clip > part->filler_13E[2])
        part->filler_13E[2] = 0;

    part->filler_13E[3] = part->filler_13E[0];
    part->filler_13E[4] = part->filler_13E[1];
}

/* display_put_a_line  E1: ? | E2: 0x4257B0 */
void put_a_line(part_t *part) {
    int plane = db >> 24;

    int16_t hw = screen_width / 2;
    int16_t hh = screen_height / 2;

    int16_t x0 = (int16_t)(part->vector_persp.X >> 4) + hw;
    int16_t y0 = (int16_t)(part->vector_persp.Y >> 4) + hh;
    int16_t x1 = (int16_t)(part->vector_persp.Z >> 4) + hw;
    int16_t y1 = (int16_t)(part->field_90 >> 4) + hh;

    /* Clip to screen edges */
    if (x0 < left_edge) {
        if (x1 < left_edge) return;
        if (x1 != x0)
            y0 = y0 + (int16_t)((int)(y1 - y0) * (left_edge - x0) / (x1 - x0));
        x0 = left_edge;
    } else if (x1 < left_edge) {
        if (x0 != x1)
            y1 = y1 + (int16_t)((int)(y0 - y1) * (left_edge - x1) / (x0 - x1));
        x1 = left_edge;
    }

    if (y0 < top_edge) {
        if (y1 < top_edge) return;
        if (y1 != y0)
            x0 = x0 + (int16_t)((int)(x1 - x0) * (top_edge - y0) / (y1 - y0));
        y0 = top_edge;
    } else if (y1 < top_edge) {
        if (y0 != y1)
            x1 = x1 + (int16_t)((int)(x0 - x1) * (top_edge - y1) / (y0 - y1));
        y1 = top_edge;
    }

    if (x0 > right_edge) {
        if (x1 > right_edge) return;
        if (x1 != x0)
            y0 = y0 + (int16_t)((int)(y1 - y0) * (right_edge - x0) / (x1 - x0));
        x0 = right_edge;
    } else if (x1 > right_edge) {
        if (x0 != x1)
            y1 = y1 + (int16_t)((int)(y0 - y1) * (right_edge - x1) / (x0 - x1));
        x1 = right_edge;
    }

    if (y0 > bottom_edge) {
        if (y1 > bottom_edge) return;
        if (y1 != y0)
            x0 = x0 + (int16_t)((int)(x1 - x0) * (bottom_edge - y0) / (y1 - y0));
        y0 = bottom_edge;
    } else if (y1 > bottom_edge) {
        if (y0 != y1)
            x1 = x1 + (int16_t)((int)(x0 - x1) * (bottom_edge - y1) / (y0 - y1));
        y1 = bottom_edge;
    }

    move_pen(plane, x0, y0);
    draw(plane, x1, y1);

    /* Draw second line offset by 1 pixel for thickness */
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > dy) {
        move_pen(plane, x0, y0 + 1);
        draw(plane, x1, y1 + 1);
    } else {
        move_pen(plane, x0 + 1, y0);
        draw(plane, x1 + 1, y1);
    }

    /* Update parent actor's area_to_clear bounding box */
    actor_t *actor = part->parent_actor;
    if (actor && actor->area_to_clear) {
        subarea_t *area = actor->area_to_clear;
        if (x0 < area->left) area->left = x0;
        if (y0 < area->top) area->top = y0;
        if (x0 + 2 > area->right) area->right = x0 + 2;
        if (y0 + 2 > area->bottom) area->bottom = y0 + 2;
        if (x1 < area->left) area->left = x1;
        if (y1 < area->top) area->top = y1;
        if (x1 + 2 > area->right) area->right = x1 + 2;
        if (y1 + 2 > area->bottom) area->bottom = y1 + 2;
    }
}

/* display_xxx_stick_to_background  E1: ? | E2: 0x425C4C */
void xxx_stick_to_background(actor_t *actor) {
    if (!actor) return;

    /* Remove from display list */
    if (actor == root_thing) {
        root_thing = actor->next_in_display_list;
    } else {
        actor_t *prev = root_thing;
        while (prev && prev->next_in_display_list) {
            if (prev->next_in_display_list == actor) {
                prev->next_in_display_list = actor->next_in_display_list;
                break;
            }
            prev = prev->next_in_display_list;
        }
    }

    /* Add to stuck_thing_list */
    actor->field_10E = (int16_t)(intptr_t)stuck_thing_list;
    stuck_thing_list = actor;

    /* Init bounding box to full range */
    actor->area_to_clear = (subarea_t *)&actor->bounding_box;
    actor->bounding_box.top = 0x7FFF;
    actor->bounding_box.left = actor->bounding_box.top;
    actor->bounding_box.bottom = (int16_t)0x8001;
    actor->bounding_box.right = actor->bounding_box.bottom;
}

