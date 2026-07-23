/**
 * move.c
 *
 * Movement system: actor movement processing, walking, collision,
 * height finding, animation playback, behavioral state machine.
 * 49 functions prefixed with move_ in the original ASM.
 */

#include "move.h"
#include "asm_f.h"
#include "display.h"
#include "edit.h"
#include "ellipse.h"
#include "file.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "menu.h"
#include "music.h"
#include "req.h"
#include "topo.h"
#include <string.h>
#include <stdlib.h>
#include "compat.h"
#ifndef _WIN32
#include <strings.h>
#endif

static int16_t e1_pick_up_hand = 0;

/* ══════════════════════════════════════════════════════════════
 *  Main Movement Loop
 * ══════════════════════════════════════════════════════════════ */

/* move_do_movement_427998 — called every frame from game loop */
void do_movement(void) {
    int some_time = x_time;
    break_do_movement = 0;
    num_info_lines = 0;

    /* Wait for next clock tick */
    int wait_time = my_time();
    while (wait_time == some_time)
        wait_time = my_time();
    x_time = wait_time;

    int local_game_time = x_time - some_time;
    if (slow_motion) {
        local_game_time /= 3;
        if (!local_game_time)
            local_game_time = 1;
    }
    if (stop_the_clock) {
        local_game_time = 0;
        stop_the_clock = false;
    }

    int16_t save_game_timer = (int16_t)game_timer;
    game_time += 3 * local_game_time;

    if (game_timer > 0) {
        show_timer(game_timer, game_timer == local_game_time, game_timer - local_game_time);
        game_timer -= local_game_time;
        if (save_game_timer == local_game_time)
            game_timer = -1;
    }

    interval = local_game_time;

    check_encounter();

    /* Demo: auto-press space while intro is active. CT_END_OF_INTRO clears
     * intro_flag from inside the scene scripts; once cleared, player regains
     * control. */
    if (demo_option && intro_flag) {
        space_pressed = true;
    }


    /* ── Phase 1: Process all actors in display list ── */
    actor_t *actor = root_thing;
    if (root_thing) {
        while (1) {
            action_t *action = actor->actor_act.act_action;

            if (action && (action->action_flags & 2)) {
                /* Actor is playing a scene action */
                set_vector(&actor->actor_velocity, 0, 0, 0);

                if (actor->actor_act.flags & 0x400) {
                    /* This actor's scene action has completed */
                    actor->actor_act.act_action = NULL;
                    actor->flags |= 0x40;
                    if (!moving_camera)
                        actor->flags |= 0x400;
                    scene_t *actor_scene = actor->actor_scene;
                    actor->actor_scene = NULL;

                    DBG_LOG(1, "[SCENE] P1 actor %d '%s' act-complete scene=%d rep=%p rep_idx=%d def_rep=%d bh=%d\n",
                            actor->name_index,
                            (actor->name_index >= 0 && actor->name_index < THING_TAB_SIZE) ? thing_names[actor->name_index].field_0 : "?",
                            actor_scene ? actor_scene->scene_index : -1,
                            (void*)actor->actor_reperture,
                            actor->actor_rep_index,
                            actor->default_repert,
                            actor->actor_behavior);

                    /* Only walk scripts if actor was bound to a scene. */
                    if (actor_scene) {
                        /* Check if ALL scripts for this scene are done */
                        bool all_done = true;
                        int blocker_ai = -1;
                        for (script_t *script = actor_scene->scene_script_list;
                             script; script = script->next_script) {
                            if (script->script_action.action_flags & 0x20) {
                                DBG_LOG(1, "[SCENE] P1   script actor=%d SKIP (flags&0x20)\n", script->script_actor_index);
                                continue;
                            }
                            int script_actor_idx = script->script_actor_index;
                            actor_t *script_actor = (script_actor_idx >= 0 && script_actor_idx < THING_TAB_SIZE) ? thing_tab[script_actor_idx] : NULL;
                            if (!script_actor) {
                                DBG_LOG(1, "[SCENE] P1   script actor=%d NOT in thing_tab\n", script_actor_idx);
                            } else if (script_actor->actor_act.act_action != &script->script_action) {
                                DBG_LOG(1, "[SCENE] P1   script actor=%d act_action MISMATCH (act=%p script=%p scene=%d)\n",
                                        script_actor_idx, (void*)script_actor->actor_act.act_action,
                                        (void*)&script->script_action, actor_scene->scene_index);
                            } else if (!(thing_name_flags[script_actor_idx] & 2)) {
                                DBG_LOG(1, "[SCENE] P1   script actor=%d name_flags no bit2 (flags=0x%x)\n",
                                        script_actor_idx, thing_name_flags[script_actor_idx]);
                            } else {
                                all_done = false;
                                if (blocker_ai < 0) blocker_ai = script_actor_idx;
                            }
                        }
                        if (all_done) {
                            DBG_LOG(1, "[SCENE] P1 all_done scene=%d code2=%d\n",
                                    actor_scene->scene_index, actor_scene->scene_code_2);
                            scene_name_flags[actor_scene->scene_index] |= 4;
                            int16_t code_idx = actor_scene->scene_code_2;
                            if (code_idx >= 0) {
                                code_t *code = code_tab[code_idx];
                                if (code)
                                    execute_code(code, NULL);
                                else
                                    DBG_LOG(1, "[SCENE] P1 code2=%d is NULL!\n", code_idx);
                            } else {
                                DBG_LOG(1, "[SCENE] P1 scene=%d has no code2\n",
                                        actor_scene->scene_index);
                            }
                        } else {
                            DBG_LOG(1, "[SCENE] P1 scene=%d NOT all_done, blocker=%d\n",
                                    actor_scene->scene_index, blocker_ai);
                        }
                    }
                } else {
                    /* Update all acts in the actor's act chain */
                    for (act_t *act = actor->actor_act_list; act; act = act->next)
                        update_act(act, actor, local_game_time);

                    update_act(&actor->actor_act, actor, local_game_time);
                    free_spent_acts(actor);
                }
            } else {
                /* Not in a scene action — run behaviour AI */
                actor->flags &= 0xFF7F;  /* clear 0x80 */
                if (actor->actor_behavior != BH_EXTERNAL)
                    behaviour(actor, local_game_time);
            }

            if (break_do_movement)
                return;

            actor = actor->next_in_display_list;
            if (!actor)
                break;
        }
    }

    /* ── Phase 2: Process root scenes — check for completed scripts ── */
    scene_t *current_scene = NULL;
    if (root_scene) {
        for (scene_t *scene = root_scene; scene; scene = scene->next_scene) {
            bool all_done = true;
            for (script_t *script = scene->scene_script_list;
                 script; script = script->next_script) {
                if (!(script->script_action.action_flags & 0x20)) {
                    int script_actor_idx = script->script_actor_index;
                    actor_t *script_actor = thing_tab[script_actor_idx];
                    if (script_actor &&
                        &script->script_action == script_actor->actor_act.act_action) {
                        if (thing_name_flags[script_actor_idx] & 2)
                            all_done = false;
                    }
                }
            }

            if (all_done) {
                /* Phase 1 marks bit 4 first when it fires scene_code_2 through
                 * the actor-act-complete path. If bit 4 already set, Phase 1
                 * already handled scene_code_2 — don't re-fire it here. */
                bool already_finished = (scene_name_flags[scene->scene_index] & 4) != 0;
                DBG_LOG(1, "[SCENE] P2 all_done scene=%d already_finished=%d code2=%d code_idx=%d\n",
                        scene->scene_index, already_finished,
                        scene->scene_code_2, scene->scene_code_index);
                scene_name_flags[scene->scene_index] |= 4;
                if (!current_scene)
                    root_scene = scene->next_scene;
                else
                    current_scene->next_scene = scene->next_scene;
                scene->scene_use_flag &= 0xFFFE;
                /* E1 fallback: fire scene_code_2 when Phase 1 missed it AND
                 * the scene has no scene_code_index. Scenes with a running
                 * code_index (intro cascade 741/501/508/507/503) already
                 * chain via their own per-frame code — firing scene_code_2
                 * here restarts scene 498 repeatedly on skip.
                 * Only code_idx=-1 scenes (intro scene 139) rely on Phase 2
                 * to fire CT_END_OF_INTRO; without this the intro deadlocks. */
                if (game_version == GAME_VERSION_E1 && !already_finished
                    && scene->scene_code_index < 0
                    && scene->scene_code_2 >= 0) {
                    code_t *c2 = code_tab[scene->scene_code_2];
                    if (c2) execute_code(c2, NULL);
                }
            } else {
                scene->scene_time = game_time;
                int16_t code_idx = scene->scene_code_index;
                if (code_idx >= 0) {
                    DBG_LOG(1, "[SCENE] P2 per-frame scene=%d code_idx=%d\n",
                            scene->scene_index, code_idx);
                    code_t *code = code_tab[code_idx];
                    if (code)
                        execute_code(code, NULL);
                }
                current_scene = scene;
            }
        }
    }

    /* ── Phase 3: Rendering ── */
    if (need_clear_graphics) clear_graphics();
    prepare_parts();
    draw_stuck_parts();
    draw_parts();
    if (need_draw_graphics) draw_graphics();
    show_parts();

    /* ── Phase 4: Game-over check ── */
    if (selected_thing &&
        selected_thing->actor_behavior == BH_DEAD &&
        selected_thing->action_delay <= 0) {
        DBG_LOG(1, "[DEAD] game-over check: selected_thing=%p behavior=%d action_delay=%d\n",
                (void*)selected_thing,
                selected_thing->actor_behavior,
                selected_thing->action_delay);
        if (game_version == 1) {
            int scene_id;
            switch (selected_camera) {
            case 5: case 6: case 42:
                scene_id = female ? 4 : 2;
                break;
            case 44: case 85: case 103: case 111:
                scene_id = female ? 3 : 1;
                break;
            default:
                scene_id = (rand() % 2) ? (female ? 3 : 1)
                                        : (female ? 4 : 2);
                break;
            }
            play_dead_scene(scene_id);
        } else {
            key_esc_was_forced = 1;
        }
    } else if (!selected_thing && game_version == 2) {
        key_esc_was_forced = 1;
    }
    if (game_timer < 0) {
        game_timer = 0;
        int16_t scene_idx = find_scene_name_index("olddie2");
        if (scene_idx < 0)
            do_info_req("Can't find scene: 'olddie2'");
        else
            play_dead_scene(scene_idx);
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Wander — obstacle-probing pathfinding
 * ══════════════════════════════════════════════════════════════ */

/* Hit direction mapping table (from ASM dword_427140) */
static const int16_t hit_dir_table[8] = { 5, 8, 7, 6, 3, 0, 1, 2 };

/* move_do_wander  E1: 0x4237E4 | E2: 0x42A00C */
int16_t do_wander(actor_t *actor) {
    int dir_x = (actor->actor_box_size * sine_table[(uint16_t)actor->rotate_vector.Y]) >> 14;
    int dir_z = (actor->actor_box_size * cosn_table[(uint16_t)actor->rotate_vector.Y]) >> 14;

    vector_t probe_right, probe_fwd, probe_left;
    probe_right.Y = probe_left.Y = probe_fwd.Y = actor->position_vector.Y;

    if (actor->wander_direction) {
        probe_fwd.X = 2 * dir_x + actor->position_vector.X;
        probe_fwd.Z = 2 * dir_z + actor->position_vector.Z;
        probe_left.X = 2 * dir_x - (dir_z + (dir_z >> 2)) + actor->position_vector.X;
        probe_left.Z = ((dir_x >> 2) + dir_x) + 2 * dir_z + actor->position_vector.Z;
        probe_right.X = 2 * dir_x + (dir_z + (dir_z >> 2)) + actor->position_vector.X;
        probe_right.Z = 2 * dir_z - (dir_x + (dir_x >> 2)) + actor->position_vector.Z;
    } else {
        probe_fwd.X = 2 * dir_x + actor->position_vector.X;
        probe_fwd.Z = 2 * dir_z + actor->position_vector.Z;
        probe_left.X = 2 * dir_x - dir_z + actor->position_vector.X;
        probe_left.Z = dir_x + 2 * dir_z + actor->position_vector.Z;
        probe_right.X = 2 * dir_x + dir_z + actor->position_vector.X;
        probe_right.Z = -dir_x + 2 * dir_z + actor->position_vector.Z;
    }

    int16_t block_flags = 0;
    int height_diff = abs(find_height_now(&probe_fwd, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 1;

    height_diff = abs(find_height_now(&probe_left, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 2;

    height_diff = abs(find_height_now(&probe_right, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 4;

    int16_t result;
    if (actor->wander_direction) {
        switch (block_flags) {
        case 0:
            result = 1;
            actor->wander_direction = 0;
            break;
        default: /* 1-7 */
            result = (actor->wander_direction < 0) ? 3 : 5;
            break;
        }
    } else {
        switch (block_flags) {
        case 0: case 6:
            result = 1;
            break;
        case 1: case 7:
            actor->wander_direction = 2 * my_rand();
            result = 1000;
            break;
        case 2: case 3:
            actor->wander_direction = 1;
            result = 5;
            break;
        case 4: case 5:
            actor->wander_direction = -1;
            result = 3;
            break;
        default:
            result = 1;
            break;
        }
    }
    return result;
}

/* move_do_new_wander  E1: 0x423B3C | E2: 0x42A364 */
int16_t do_new_wander(actor_t *actor) {
    int dir_z = (actor->actor_box_size * cosn_table[(uint16_t)actor->Rotate.Y]) >> 15;
    int dir_x = (actor->actor_box_size * sine_table[(uint16_t)actor->Rotate.Y]) >> 15;

    vector_t vv21, vv22, v22_probe;
    vv22.Y = vv21.Y = actor->position_vector.Y;
    v22_probe.Y = actor->position_vector.Y;

    if (actor->wander_direction) {
        vv21.X = 2 * dir_x + actor->position_vector.X;
        vv21.Z = 2 * dir_z + actor->position_vector.Z;
        vv22.X = 2 * dir_x + actor->position_vector.X - dir_z - (dir_z >> 2);
        vv22.Z = dir_x + 2 * dir_z + actor->position_vector.Z + (dir_x >> 2);
        v22_probe.X = 2 * dir_x + actor->position_vector.X + dir_z + (dir_z >> 2);
        v22_probe.Z = 2 * dir_z + actor->position_vector.Z - dir_x - (dir_x >> 2);
    } else {
        vv21.X = 2 * dir_x + actor->position_vector.X;
        vv21.Z = 2 * dir_z + actor->position_vector.Z;
        vv22.X = 2 * dir_x + actor->position_vector.X - dir_z;
        vv22.Z = dir_x + 2 * dir_z + actor->position_vector.Z;
        v22_probe.X = dir_z + actor->position_vector.X + 2 * dir_x;
        v22_probe.Z = 2 * dir_z + actor->position_vector.Z - dir_x;
    }

    int16_t block_flags = 0;
    int height_diff = abs(find_height_now(&vv21, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 1;

    height_diff = abs(find_height_now(&vv22, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 2;

    height_diff = abs(find_height_now(&v22_probe, actor) - actor->position_vector.Y);
    if (height_diff > 256) block_flags |= 4;

    int16_t result;
    if (actor->wander_direction) {
        switch (block_flags) {
        case 0:
            result = 1;
            actor->wander_direction = 0;
            break;
        case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            result = (actor->wander_direction <= 0) ? 3 : 5;
            break;
        default:
            result = 1;
            break;
        }
    } else {
        switch (block_flags) {
        case 0: case 6:  result = 1; break;
        case 1: case 7:  actor->wander_direction = 2 * my_rand(); result = 1000; break;
        case 2: case 3:  actor->wander_direction = 1; result = 5; break;
        case 4: case 5:  actor->wander_direction = -1; result = 3; break;
        default:         result = 1; break;
        }
    }
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  Behaviour — AI state machine dispatch (~1555 lines in reference)
 * ══════════════════════════════════════════════════════════════ */

/* move_behaviour  E1: ? | E2: 0x427554 */
void behaviour(actor_t *actor, int game_time_arg) {
    if (!actor) return;

    int one_shot_action = 0;  /* one-shot action flag */

    /* ── Repertoire management ── */
    actor->state_flags &= 0xFFFD;
    if (actor == selected_thing) {
        actor->rotate_vector.Z = 0;
        actor->rotate_vector.X = 0;
    }

    if (actor->actor_rep_index < 0) {
        if (actor->actor_rep_index == -2)
            do_info2_req("Actor has -2 RepName",
                         thing_names[actor->name_index].field_0);
        rephead_t *rep = actor->actor_reperture;
        if (rep)
            rep->rep_use_flag &= 0xFFFD;
        actor->actor_reperture = NULL;
    } else {
        rephead_t *rep = actor->actor_reperture;
        if (rep) {
            if (rep->rep_index != actor->actor_rep_index) {
                rep->rep_use_flag &= 0xFFFD;
                check_rep_loaded(actor->actor_rep_index);
                actor->actor_reperture = repertoire_tab[actor->actor_rep_index];
                if (actor->move_type == 1000)
                    actor->flags |= 0x40;
            }
        } else {
            check_rep_loaded(actor->actor_rep_index);
            actor->actor_reperture = repertoire_tab[actor->actor_rep_index];
        }
        if (actor->actor_reperture)
            actor->actor_reperture->rep_time = game_time;
    }

    /* ── Force action / timed action processing ── */
    action_t *action = actor->force_action_to_execute;
    int action_type = 1;
    if (!action) {
        action_type = 2;
        action = actor->queued_action;
    }
    if (!action) {
        taction_t *taction = actor->tactions_list;
        if (taction && game_time - taction->taction_time > 0) {
            action_type = 3;
            check_action_loaded(taction->taction_index);
            action = action_tab[taction->taction_index];
        }
    }

    if (actor->actor_behavior == BH_DYING || actor->actor_behavior == BH_DEAD
        || actor->actor_hitpoints < 0)
        action = NULL;
    if (action && actor->actor_act.act_action
        && (actor->actor_act.flags & 0x40) && !(actor->flags & 0x40))
        action = NULL;

    if (action) {
        actor->actor_act.act_action = action;
        actor->actor_act.duration = action->act_duration;
        if (actor->actor_Speed_factor != 100)
            actor->actor_act.duration = 100 * actor->actor_act.duration / actor->actor_Speed_factor;
        actor->actor_act.key_progress = 0;
        actor->actor_act.actor_keys_list = action->key_list;
        actor->actor_act.flags = (action->action_flags & 0xFE) | 0xC0;
        update_thing(actor);
        actor->move_type = -1;
        if (game_version == GAME_VERSION_E1) {
            if (actor->actor_behavior != BH_RECOVERING &&
                actor->actor_behavior != BH_GET_HIT)
                actor->flags &= ~0x2000;
            actor->flags &= ~0x40;
        } else {
            actor->flags = (actor->flags | 0x2000) & ~0x40;
        }
        if (action_type == 1) {
            if (actor->state_flags & 0x10)
                actor->actor_act.flags |= 0x1000;
            actor->force_action_to_execute = NULL;
        } else if (action_type == 2) {
            actor->queued_action = NULL;
        } else if (action_type == 3) {
            taction_t *taction = actor->tactions_list;
            actor->tactions_list = taction->next;
            free_taction(taction);
        }
    }

    /* ── Early exit if no repertoire and no action ── */
    rephead_t *repertoire = actor->actor_reperture;
    /* E1 hero has default_repert=0 → rep->rep_index=0 → this exit fires and
     * BH_JOYSTICK is never assigned → keyboard dead. Hero must fall through
     * to the joystick assignment even without a valid repertoire. */
    bool hero_needs_input =
        (actor == selected_thing) && joystick_control &&
        actor->actor_behavior != BH_GET_HIT &&
        actor->actor_behavior != BH_DYING &&
        actor->actor_behavior != BH_DEAD &&
        actor->actor_behavior != BH_RECOVERING;
    if (!hero_needs_input &&
        (!repertoire || !repertoire->rep_index) && !actor->actor_act.act_action) {
        if (actor->action_delay > 0) {
            actor->action_delay -= game_time_arg;
            if (actor->action_delay < 0)
                actor->action_delay = 0;
        }
        actor->flags |= 0x400;
        return;
    }

    /* ── Find interaction target ── */
    int16_t next_move = 1000;
    actor_t *interact_actor;
    if (actor == selected_thing) {
        if (joystick_control) {
            if (actor->actor_behavior != BH_GET_HIT &&
                actor->actor_behavior != BH_DYING &&
                actor->actor_behavior != BH_DEAD &&
                actor->actor_behavior != BH_RECOVERING)
                actor->actor_behavior = BH_JOYSTICK;
            interact_actor = NULL;
        } else if (actor->actor_behavior == BH_FOLLOW) {
            interact_actor = (actor->interact_target_index < 0) ? NULL : thing_tab[actor->interact_target_index];
        } else {
            interact_actor = NULL;
            for (actor_t *fa = root_thing; fa; fa = fa->next_in_display_list) {
                if (fa != actor) {
                    actor_t *holder = (actor_t *)fa->part_heap_link;
                    if (holder) holder = holder->parent_actor;
                    if (holder != actor) {
                        if (fa->actor_reperture && fa->actor_reperture->action_slots[34] >= 0) {
                            int16_t direction, distance;
                            find_direction_and_distance(&direction, &distance,
                                fa->position_vector.X - actor->position_vector.X,
                                fa->position_vector.Z - actor->position_vector.Z);
                            if (distance < 1000) {
                                interact_actor = fa;
                                if (distance < 700)
                                    break;
                            }
                        }
                    }
                }
            }
        }
    } else if (selected_thing && selected_thing->actor_behavior != BH_DEAD
               && selected_thing->actor_behavior != BH_DYING) {
        if (game_version == GAME_VERSION_E1) {
            interact_actor = selected_thing;
        } else {
            if (actor->state_flags & 8)
                interact_actor = look_for_a_fight(actor);
            else
                interact_actor = selected_thing;
        }
    } else {
        interact_actor = NULL;
    }

    /* ── Get interaction parts ── */
    part_t *interact_part = NULL;
    if (interact_actor) {
        interact_part = interact_actor->_PartTab ? interact_actor->_PartTab->field_0[2] : NULL;
        if (!interact_part) {
            interact_part = interact_actor->actor_parts_list;
            if (!interact_part)
                interact_actor = NULL;
        }
    }

    part_t *part = actor->_PartTab ? actor->_PartTab->field_0[2] : NULL;
    if (!part) {
        part = actor->actor_parts_list;
        if (!part)
            interact_actor = NULL;
    }
    if (interact_actor && interact_actor->actor_parts_list
        && (interact_actor->actor_parts_list->flags & 0x42))
        interact_actor = NULL;

    /* ── Calculate direction/distance to target ── */
    int target_direction = 0;     /* direction to target */
    int16_t target_distance = 0x7FFF;  /* distance to target */
    int16_t rel_angle = 0;
    int vertical_direction = 0; (void)vertical_direction;     /* up/down direction — computed but no downstream reader ported yet */
    int los_blocked = 0;     /* visibility blocked flag */

    if (!interact_actor || (interact_actor->flags & 0x80)) {
        rel_angle = 0;
        target_distance = 0x7FFF;
    } else {
        int16_t dx = interact_part->ellipse_center.X - part->ellipse_center.X;
        int16_t dz = interact_part->ellipse_center.Z - part->ellipse_center.Z;
        int16_t dy = interact_part->ellipse_center.Y - part->ellipse_center.Y;
        target_direction = arctan(dx, dz);
        target_distance = (int16_t)(abs(dz) < abs(dx)
            ? abs(dz) / 2 + abs(dx)
            : abs(dx) / 2 + abs(dz));

        if (abs(dy) <= 768 || abs(dy) <= target_distance) {
            if (target_distance / 3 >= abs(dy))
                vertical_direction = 0;
            else if (dy <= 0)
                vertical_direction = -1;
            else
                vertical_direction = 1;
        } else {
            target_distance = 0x7FFF;
        }

        int16_t facing = (int16_t)arctan(part->matrix_1._13, part->matrix_1._33);
        rel_angle = facing - (int16_t)target_direction;

        if (actor->interact_state & 2) {
            los_blocked = 1;
        }
        if (!los_blocked) {
            vector_t step;
            set_vector(&step, dx, dy, dz);
            int16_t max_dim = abs(dx);
            if (abs(dy) > max_dim) max_dim = abs(dy);
            if (abs(dz) > max_dim) max_dim = abs(dz);
            int16_t subdivisions = max_dim / 64;
            if (subdivisions >= 2)
                div_vector(&step, subdivisions);
            vector_t dst;
            copy_vector(&dst, &part->ellipse_center);
            if (subdivisions > 0) {
                for (int sub_step = 0; sub_step < subdivisions; sub_step++) {
                    add_vector(&dst, &step);
                    if (find_height_now_vis(&dst) < dst.Y) {
                        los_blocked = 1;
                        break;
                    }
                }
            }
        }

        actor->interact_state |= 1;
        if (los_blocked) {
            actor->interact_state &= 0xFFFE;
            if (actor->interact_timer) {
                int16_t dx = actor->target_position.X - actor->position_vector.X;
                int16_t dz = actor->target_position.Z - actor->position_vector.Z;
                int16_t dy = actor->target_position.Y - actor->position_vector.Y;
                target_direction = arctan(dx, dz);
                target_distance = (int16_t)(abs(dx) < abs(dz)
                    ? abs(dx) / 2 + abs(dz)
                    : abs(dz) / 2 + abs(dx));
                if (abs(dy) <= 768 || abs(dy) <= target_distance)
                    rel_angle = actor->Rotate.Y - (int16_t)target_direction;
                else {
                    interact_actor = NULL;
                    target_distance = 0x7FFF;
                    rel_angle = 0;
                }
                if (actor->actor_rep_index == 1
                    && repertoire_tab[2] && repertoire_tab[2]->thing_index == actor->name_index) {
                    actor->actor_rep_index = 2;
                    actor->flags |= 0x40;
                }
            } else {
                if (interact_actor->hold_timer) {
                    if (actor->actor_rep_index == 1
                        && repertoire_tab[2] && repertoire_tab[2]->thing_index == actor->name_index) {
                        actor->actor_rep_index = 2;
                        actor->flags |= 0x40;
                    }
                } else if (actor->actor_rep_index == 2) {
                    actor->actor_rep_index = 1;
                }
                interact_actor = NULL;
                target_distance = 0x7FFF;
                rel_angle = 0;
            }
        } else {
            actor->interact_timer = 5000;
            copy_vector(&actor->target_position, &interact_actor->position_vector);
            if (actor->actor_rep_index == 1
                && repertoire_tab[2] && repertoire_tab[2]->thing_index == actor->name_index) {
                actor->actor_rep_index = 2;
                actor->flags |= 0x40;
            }
        }
    }

    /* ── Decrement timers ── */
    if (actor->event_timer > 0) actor->event_timer -= game_time_arg;
    if (actor->event_timer < 0) actor->event_timer = 0;
    if (actor->hold_timer > 0) actor->hold_timer -= game_time_arg;
    if (actor->hold_timer < 0) actor->hold_timer = 0;
    if (actor->interact_timer > 0) actor->interact_timer -= game_time_arg;
    if (actor->interact_timer < 0) actor->interact_timer = 0;
    if (actor->interact_cooldown > 0) actor->interact_cooldown -= game_time_arg;
    if (actor->interact_cooldown < 0) actor->interact_cooldown = 0;

    /* ── Prepare for behavior switches ── */
    actor->action_variant = -1;
    if (actor->actor_behavior != BH_WANDER) {
        if ((actor->actor_behavior != BH_CHASE && actor->actor_behavior != BH_GO_CLOSER)
            || !actor->event_timer)
            actor->wander_direction = 0;
    }
    if (actor->action_variant >= 0) {
        next_move = actor->move_type;
        goto center_function;
    }
    actor->action_variant = 20 * (uint16_t)(2 * my_rand()) >> 16;

    /* ══════ Switch 1: Behavior state transitions ══════ */
    int16_t attack_range = (game_version == GAME_VERSION_E1) ? 900 : 6000;

    switch (actor->actor_behavior) {
    case BH_STOP:
    case BH_TURN_LEFT:
    case BH_TURN_RIGHT:
        if (!interact_actor || interact_actor->actor_hitpoints < 0
            || actor->range_threshold <= 12288
            || (game_version != GAME_VERSION_E1 && (actor->flags & 0x0200))) {
            if (actor->action_delay < 0) {
                actor->actor_behavior = BH_WANDER;
                actor->action_delay = (int16_t)((1200 * (uint16_t)(2 * my_rand()) >> 16) + 60);
            } else {
                actor->action_delay -= game_time_arg;
            }
        } else if (target_distance >= attack_range) {
            if (target_distance >= 4000) {
                if (actor->action_delay < 0) {
                    actor->actor_behavior = BH_WANDER;
                    actor->action_delay = (int16_t)((1200 * (uint16_t)(2 * my_rand()) >> 16) + 60);
                } else {
                    actor->action_delay -= game_time_arg;
                }
            } else {
                actor->actor_behavior = BH_CHASE;
            }
        } else {
            actor->actor_behavior = BH_ATTACK;
        }
        break;

    case BH_WANDER:
        if (!interact_actor || interact_actor->actor_hitpoints < 0
            || actor->range_threshold <= 12288 || target_distance >= 8000) {
            if (target_distance >= 900) {
                if (actor->action_delay < 0) {
                    actor->actor_behavior = BH_STOP;
                    if (actor == selected_thing)
                        actor->action_delay = (int16_t)((60 * (uint16_t)(2 * my_rand()) >> 16) + 60);
                    else
                        actor->action_delay = (int16_t)((120 * (uint16_t)(2 * my_rand()) >> 16) + 60);
                } else {
                    actor->action_delay -= game_time_arg;
                }
            } else {
                actor->actor_behavior = BH_RETREAT;
            }
        } else if (target_distance >= attack_range) {
            if (target_distance < 8000)
                actor->actor_behavior = BH_CHASE;
        } else {
            actor->actor_behavior = BH_ATTACK;
        }
        break;

    case BH_CHASE:
    case BH_ATTACK:
        if (!interact_actor || interact_actor->actor_hitpoints < 0
            || (game_version != GAME_VERSION_E1 && (actor->flags & 0x0200))) {
            actor->actor_behavior = BH_STOP;
            actor->action_delay = (int16_t)((600 * (uint16_t)(2 * my_rand()) >> 16) + 60);
        } else {
            if (target_distance >= 8000)
                actor->actor_behavior = BH_WANDER;
            else if (target_distance >= attack_range)
                actor->actor_behavior = BH_GO_CLOSER;
            else
                actor->actor_behavior = BH_ATTACK;
        }
        break;

    case BH_GO_CLOSER:
        if (!interact_actor || interact_actor->actor_hitpoints < 0
            || (game_version != GAME_VERSION_E1 && (actor->flags & 0x0200))) {
            actor->actor_behavior = BH_STOP;
            actor->action_delay = (int16_t)((600 * (uint16_t)(2 * my_rand()) >> 16) + 60);
        } else if (target_distance >= 700) {
            if (target_distance >= 8000)
                actor->actor_behavior = BH_WANDER;
            else
                actor->actor_behavior = BH_GO_CLOSER;
        } else {
            actor->actor_behavior = BH_ATTACK;
        }
        break;

    case BH_RETREAT:
        if (!interact_actor || interact_actor->actor_hitpoints < 0 || actor->range_threshold <= 12288) {
            if (target_distance > 1800)
                actor->actor_behavior = BH_WANDER;
        } else if (target_distance >= attack_range) {
            actor->actor_behavior = BH_WANDER;
        } else if (target_distance < 2000) {
            actor->actor_behavior = BH_CHASE;
        } else {
            actor->actor_behavior = BH_ATTACK;
        }
        break;

    case BH_JOYSTICK:
        if (!joystick_control || actor != selected_thing)
            actor->actor_behavior = BH_WANDER;
        break;

    case BH_DYING:
        if (actor->flags & 0x40) {
            DBG_LOG(1, "[DEAD] BH_DYING->make_dead: actor='%s' is_hero=%d\n",
                    thing_names[actor->name_index].field_0, actor == selected_thing);
            make_dead(actor);
        }
        break;

    case BH_SLEEP:
        if (actor->flags & 8)
            actor->actor_behavior = BH_WANDER;
        break;

    case BH_GET_HIT:
        actor->range_threshold = 0x7FFF;
        break;

    case BH_RECOVERING:
        if (game_version == GAME_VERSION_E1) {
            if (actor->flags & 0x40) {
                actor->actor_behavior = BH_STOP;
                actor->action_delay = -1;
            }
        } else {
            if ((actor->flags & 0x40) || !(actor->flags & 0x20FF))
                actor->actor_behavior = BH_STOP;
        }
        break;

    case BH_DEAD:
        if (actor == selected_thing && actor->action_delay > 0) {
            actor->action_delay -= game_time_arg;
            if (actor->action_delay < 0)
                actor->action_delay = 0;
        }
        break;

    case BH_FOLLOW:
        break;

    default:
        quit("unknown behaviour mode");
        break;
    }

    /* ══════ Switch 2: Movement direction selection ══════ */
    switch (actor->actor_behavior) {
    case BH_CHASE:
    case BH_GO_CLOSER:
        if (actor->event_timer)
            next_move = do_new_wander(actor);
        else {
            if (abs(rel_angle) >= 4096)
                next_move = (rel_angle <= 0) ? 5 : 3;
            else
                next_move = 1;
        }
        break;

    case BH_RETREAT:
        if (abs(rel_angle) <= 24576)
            next_move = (rel_angle >= 0) ? 5 : 3;
        else
            next_move = 1;
        break;

    case BH_WANDER:
        next_move = do_wander(actor);
        break;

    case BH_FOLLOW:
        if (actor->actor_reperture && actor->actor_reperture->action_slots[48] >= 0
            && target_distance < 700 && selected_thing) {
            if (abs(selected_thing->rotate_vector.Y - (int16_t)target_direction - 128) < 6144
                && (extra_keys_pressed[55] || ctrl_pressed || extra_keys_pressed[57] || alt_pressed)) {
                next_move = 48;
                one_shot_action = 1;
            }
        } else if (actor->interact_target_index < 0) {
            next_move = do_wander(actor);
        } else if (!thing_tab[actor->interact_target_index]) {
            next_move = do_wander(actor);
        } else {
            int16_t dirn, dist;
            dist = target_distance;
            find_dirn_and_dist(&dirn, &dist,
                thing_tab[actor->interact_target_index]->position_vector.X - actor->position_vector.X,
                thing_tab[actor->interact_target_index]->position_vector.Z - actor->position_vector.Z);
            dirn = (int16_t)target_direction;
            dist = target_distance;
            int16_t angle_diff = (int16_t)target_direction - actor->rotate_vector.Y;
            if (actor->interact_state & 1) {
                actor->interact_state &= 0xFFF9;
                if (dist < 400) {
                    if (abs(angle_diff) >= 4096)
                        next_move = (angle_diff >= 0) ? 5 : 3;
                    else
                        next_move = 7;
                    actor->interact_cooldown = 250;
                } else if (dist <= 800) {
                    if (actor->interact_cooldown)
                        next_move = 1000;
                    else if (abs(angle_diff) < 4096)
                        next_move = 1000;
                    else
                        next_move = (angle_diff >= 0) ? 5 : 3;
                } else {
                    if (abs(angle_diff) >= 0x2000)
                        next_move = (angle_diff >= 0) ? 5 : 3;
                    else {
                        next_move = 1;
                        if (abs(angle_diff) > 4096) {
                            if (angle_diff >= 0) turn_actor(actor, 512 * game_time_arg);
                            else turn_actor(actor, -512 * game_time_arg);
                        }
                    }
                    actor->interact_cooldown = 250;
                }
            } else if (actor->interact_state & 2) {
                next_move = do_wander(actor);
            } else {
                if (target_distance < 400)
                    actor->interact_state |= 4;
                if (dist < 800 && (actor->interact_state & 4))
                    next_move = 1;
                else if (actor->interact_state & 4) {
                    next_move = 1;
                    actor->interact_state |= 2;
                } else {
                    if (abs(angle_diff) >= 0x2000)
                        next_move = (angle_diff >= 0) ? 5 : 3;
                    else {
                        next_move = 1;
                        if (abs(angle_diff) > 4096) {
                            if (angle_diff >= 0) turn_actor(actor, game_time_arg << 9);
                            else turn_actor(actor, -512 * game_time_arg);
                        }
                    }
                }
            }
        }
        break;

    case BH_STOP:
        next_move = 1000;
        break;

    case BH_ATTACK:
        if (abs(rel_angle) >= 2048) {
            if (abs(rel_angle) < 4096) {
                next_move = 1;
                if (actor->actor_reperture && actor->actor_reperture->action_slots[1] >= 0) {
                    if (rel_angle <= 0) turn_actor(actor, 512 * game_time_arg);
                    else turn_actor(actor, -512 * game_time_arg);
                }
            } else {
                next_move = (rel_angle > 0) ? 3 : 5;
            }
        } else if (actor != selected_thing) {
            next_move = actor->move_type;
            if (next_move >= 9 && next_move != 1000 && !(actor->flags & 0x40))
                goto center_function;
            if (!los_blocked) {
                /* NPC attack next_move selection */
                if (game_version == GAME_VERSION_E1) {
                    /* E1: two tiers — close (<700) and mid (700-900) */
                    if (target_distance >= 700) {
                        uint16_t random_value = 2 * my_rand();
                        if (random_value > 0xC000u) {
                            next_move = 1;
                            actor->actor_behavior = BH_GO_CLOSER;
                        } else if (random_value > 0x8000u) {
                            one_shot_action = 1; next_move = 9;
                        } else if (random_value > 0x4000u) {
                            one_shot_action = 1; next_move = 10;
                        } else {
                            one_shot_action = 1; next_move = 11;
                        }
                    } else {
                        uint16_t random_value = 2 * my_rand();
                        if (random_value > 0xC000u) {
                            one_shot_action = 1; next_move = 9;
                        } else if (random_value > 0x8000u) {
                            one_shot_action = 1; next_move = 10;
                        } else if (random_value > 0x4000u) {
                            one_shot_action = 1; next_move = 11;
                        } else {
                            next_move = 1000;
                        }
                    }
                } else {
                    /* E2: three tiers with ranged attacks */
                    int melee_only = 1;
                    if (actor->actor_reperture && actor->actor_reperture->action_slots[80] >= 0)
                        melee_only = 0;
                    if (target_distance >= 900) {
                        if (target_distance >= 3000) {
                            uint16_t random_value = 2 * my_rand();
                            one_shot_action = 1;
                            if (random_value <= 0xAAAAu)
                                next_move = (random_value <= 0x5555u) ? 17 : 16;
                            else
                                next_move = 15;
                            if (actor->actor_reperture && actor->actor_reperture->action_slots[next_move] < 0) {
                                next_move = 1; one_shot_action = 0;
                            }
                        } else {
                            one_shot_action = 1;
                            uint16_t random_value = 2 * my_rand();
                            if (random_value <= 0xAAAAu)
                                next_move = (random_value <= 0x5555u) ? 14 : 13;
                            else
                                next_move = 12;
                            if (actor->actor_reperture && actor->actor_reperture->action_slots[next_move] < 0) {
                                next_move = 1; one_shot_action = 0;
                            }
                        }
                    } else {
                        uint16_t random_value = 2 * my_rand();
                        one_shot_action = 1;
                        if (random_value <= 0xC000u) {
                            if (melee_only) {
                                if (random_value <= 0x8000u)
                                    next_move = (random_value <= 0x4000u) ? 11 : 10;
                                else
                                    next_move = 9;
                            } else {
                                next_move = (int16_t)((9 * (uint16_t)(2 * my_rand()) >> 16) + 80);
                            }
                        } else {
                            next_move = 1;
                            one_shot_action = 0;
                            actor->actor_behavior = BH_GO_CLOSER;
                        }
                    }
                }
                goto center_function;
            }
            next_move = 1;
        } else if ((next_move && next_move != 2 && next_move != 4) || actor->move_type == 1000 || (actor->flags & 0x40)) {
            if (target_distance >= 700) {
                uint16_t random_value = 2 * my_rand();
                if (random_value > 0xC000u) {
                    next_move = 1; actor->actor_behavior = BH_GO_CLOSER;
                } else if (random_value > 0x9000u) next_move = 0;
                else if (random_value > 0x6000u) next_move = 4;
                else if (random_value > 0x3000u) next_move = 2;
            } else {
                uint16_t random_value = 2 * my_rand();
                if (random_value > 0xC000u) next_move = 0;
                else if (random_value > 0x8000u) next_move = 4;
                else if (random_value > 0x4000u) next_move = 2;
            }
            goto center_function;
        }
        break;

    case BH_JOYSTICK: {
        int walk_only = 1;
        int key_right = extra_keys_pressed[77];
        int key_left = extra_keys_pressed[75];
        int key_down = extra_keys_pressed[80];
        int key_up = extra_keys_pressed[72];

        if (game_version == GAME_VERSION_E1) {
            /* E1 BH_JOYSTICK (IDA 0x429452): key check order matches original.
             * No velocity pre-checks (E2-only). Diagonal turn uses
             * move_direction not game_time_arg. */
            next_move = 1000;

            if (key_left && key_up) {
                next_move = 1;
                turn_actor(actor, -(actor->move_direction << 4));
            } else if (key_right && key_up) {
                next_move = 1;
                turn_actor(actor, actor->move_direction << 4);
            } else if (extra_keys_pressed[71]) {
                next_move = 0;
            } else if (key_up) {
                next_move = 1;
            } else if (extra_keys_pressed[73]) {
                next_move = 2;
            } else if (key_left) {
                next_move = 3;
            } else if (key_right) {
                next_move = 5;
            } else if (extra_keys_pressed[56]) {
                if (actor->_PartTab->field_0[1] && actor->_PartTab->field_0[1]->actor_2_held)
                    next_move = 40;
                else if (actor->_PartTab->field_0[0] && actor->_PartTab->field_0[0]->actor_2_held)
                    next_move = 45;
                else
                    next_move = 39;
            } else if (keys_pressed[42]) {
                next_move = key_up ? 2 : 0;
            } else if (extra_keys_pressed[79]) {
                e1_pick_up_hand = 1;
                next_move = actor->move_type;
                if (next_move < 36 || next_move > 40 || (actor->flags & 0x40)) {
                    part_t *hand = actor->_PartTab->field_0[1];
                    if (hand && hand->actor_2_held)
                        next_move = 4 + 36;
                    else
                        next_move = look_for_pick_up(actor) + 36;
                }
            } else if (key_down) {
                next_move = 7;
            } else if (extra_keys_pressed[81]) {
                e1_pick_up_hand = 0;
                next_move = actor->move_type;
                if (next_move < 41 || next_move > 45 || (actor->flags & 0x40)) {
                    part_t *hand = actor->_PartTab->field_0[0];
                    if (hand && hand->actor_2_held)
                        next_move = 4 + 41;
                    else
                        next_move = look_for_pick_up(actor) + 41;
                }
            } else if (space_pressed) {
                e1_pick_up_hand = 0;
                next_move = actor->move_type;
                if (next_move < 36 || next_move > 40 || (actor->flags & 0x40)) {
                    part_t *hand = actor->_PartTab->field_0[0];
                    if (hand && hand->actor_2_held)
                        next_move = 4 + 36;
                    else
                        next_move = look_for_pick_up(actor) + 36;
                }
            }
        } else {
            /* E2: ctrl toggles run (uses different direction slots) */
            bool use_run = ctrl_pressed;

            if (actor->actor_reperture && actor->actor_reperture->action_slots[180] >= 0)
                walk_only = 0;
            next_move = 1000;

            if (actor->actor_velocity.Y > 0) {
                if (actor->move_type != 4) next_move = 24;
                goto center_function;
            }
            if (actor->actor_velocity.Y < 0) {
                next_move = 4;
            } else if (space_pressed) {
                next_move = actor->move_type;
                if (next_move < 36 || next_move > 40 || (actor->flags & 0x40))
                    next_move = look_for_pick_up(actor) + 36;
            } else if (extra_keys_pressed[56]) { /* Left Alt — flip/roll */
                if (actor->_PartTab->field_0[7] && actor->_PartTab->field_0[7]->actor_2_held)
                    next_move = 40;
                else if (actor->_PartTab->field_0[8] && actor->_PartTab->field_0[8]->actor_2_held)
                    next_move = 45;
                else
                    next_move = 39;
            } else if (keys_pressed[42]) { /* Left Shift — jump */
                next_move = key_up ? 2 : 0;
            } else if (ctrl_pressed && alt_pressed) {
                int best_target = 0;
                if (actor->actor_reperture && actor->actor_reperture->thing_index >= 0)
                    best_target = find_best_target_up_down(actor);
                if (best_target < 0) {
                    if (key_up) { next_move = actor->move_type; if (next_move != 204) next_move = 208; }
                    else if (key_left) { next_move = actor->move_type; if (next_move != 205) next_move = 209; }
                    else if (key_right) { next_move = actor->move_type; if (next_move != 206) next_move = 210; }
                    else if (key_down) { next_move = actor->move_type; if (next_move != 207) next_move = 211; }
                } else {
                    if (key_up) { next_move = actor->move_type; if (next_move != 208) next_move = 204; }
                    else if (key_left) { next_move = actor->move_type; if (next_move != 209) next_move = 205; }
                    else if (key_right) { next_move = actor->move_type; if (next_move != 210) next_move = 206; }
                    else if (key_down) { next_move = actor->move_type; if (next_move != 211) next_move = 207; }
                }
            } else if (!use_run) {
                if (alt_pressed) {
                    if (key_up) next_move = 196;
                    else if (key_left) next_move = 197;
                    else if (key_right) next_move = 198;
                    else if (key_down) next_move = 199;
                } else if (key_left && key_up) {
                    next_move = walk_only ? 1 : 180;
                    turn_actor(actor, -16 * actor->move_direction);
                } else if (key_right && key_up) {
                    next_move = walk_only ? 1 : 180;
                    turn_actor(actor, 16 * actor->move_direction);
                } else if (!walk_only) {
                    if (key_up) next_move = 180;
                    else if (key_left) next_move = 181;
                    else if (key_right) next_move = 182;
                    else if (key_down) next_move = 183;
                } else if (key_up) next_move = 1;
                else if (key_left) next_move = 3;
                else if (key_right) next_move = 5;
                else if (key_down) next_move = 7;
            } else { /* ctrl pressed, no alt */
                if (walk_only) {
                    if (key_up) next_move = 0;
                    else if (key_left) next_move = 2;
                    else if (key_right) next_move = 6;
                    else if (key_down) next_move = 8;
                } else {
                    int bt = 0;
                    if (actor->actor_reperture && actor->actor_reperture->action_slots[184] >= 0)
                        bt = find_best_target_up_down(actor);

                    int middle = 0, down = 0, up = 0;
                    if (key_up)         { middle = 188; down = 184; up = 192; }
                    else if (key_left)  { middle = 189; down = 185; up = 193; }
                    else if (key_right) { middle = 190; down = 186; up = 194; }
                    else if (key_down)  { middle = 191; down = 187; up = 195; }

                    if (middle) {
                        int a = actor->move_type;
                        if (bt == 0) {
                            if (a != down && a != up) a = middle;
                        } else if (bt < 0) {
                            if (a != middle && a != up) a = down;
                        } else {
                            if (a != middle && a != down) a = up;
                        }
                        next_move = a;
                    }
                }
            }
        }
        goto center_function;
    }

    case BH_TURN_LEFT:
        next_move = 3;
        break;

    case BH_TURN_RIGHT:
        next_move = 5;
        break;

    case BH_GET_HIT: {
        if (game_version == GAME_VERSION_E1) {
            if (actor->actor_hitpoints < 0) {
                uint16_t rv = 2 * my_rand();
                next_move = (rv <= 0xAAAAu) ? 32 : 31;
                actor->actor_behavior = BH_DYING;
                thing_name_flags[actor->name_index] |= 4;
            } else {
                uint16_t rv = 2 * my_rand();
                next_move = (rv <= 0xAAAAu) ? 35 : 34;
                actor->actor_behavior = BH_RECOVERING;
            }
            goto center_function;
        }
        one_shot_action = 1;
        int has_die_action = 0, has_fall_action = 0;
        if (actor->actor_reperture) {
            if (actor->actor_reperture->action_slots[31] >= 0) has_die_action = 1;
            if (actor->actor_reperture->action_slots[150] >= 0) has_fall_action = 1;
        }
        if (!has_die_action && !has_fall_action)
            actor->actor_hitpoints = 0;

        if (actor->hit_type == 3) { /* hit_type == HitType::Fall */
            if (actor->actor_hitpoints >= -50) {
                next_move = (actor->actor_hitpoints >= 0) ? 28 : 29;
            } else {
                next_move = 27;
            }
            if (actor->actor_reperture && actor->actor_reperture->action_slots[next_move] < 0) {
                if (actor->actor_hitpoints >= 0) next_move = 50;
                else if (has_fall_action) next_move = 150;
                else next_move = 31;
            }
            if (actor->actor_hitpoints >= 0)
                actor->actor_behavior = BH_RECOVERING;
            else {
                DBG_LOG(1, "[DEAD] BH_DYING via fall: actor='%s' hp=%d code_at_hp=%d dead_code=%d\n",
                        thing_names[actor->name_index].field_0, actor->actor_hitpoints,
                        actor->code_at_hp_change, actor->dead_code_index);
                actor->actor_behavior = BH_DYING;
                thing_name_flags[actor->name_index] |= 4;
            }
        } else if (actor->actor_hitpoints < 0 && has_die_action) {
            uint16_t random_value = 2 * my_rand();
            if (random_value >= 0x5555u)
                next_move = (random_value <= 0xAAAAu) ? 32 : 31;
            else
                next_move = 30;
            DBG_LOG(1, "[DEAD] BH_DYING via die_action: actor='%s' hp=%d code_at_hp=%d dead_code=%d\n",
                    thing_names[actor->name_index].field_0, actor->actor_hitpoints,
                    actor->code_at_hp_change, actor->dead_code_index);
            actor->actor_behavior = BH_DYING;
            thing_name_flags[actor->name_index] |= 4;
        } else {
            /* Hit direction-based reaction */
            int16_t hit_octant = (((actor->rotate_vector.Y - actor->hit_angle) + 4096) >> 13) & 7;
            int16_t dir_val = hit_dir_table[hit_octant];
            if (actor->actor_hitpoints >= 0)
                next_move = 3 * dir_val + 50;
            else
                next_move = 3 * dir_val + 150;

            uint16_t random_value = 2 * my_rand();
            if (dir_val == 3) {
                if (random_value < 0x2AAAu) {
                    /* base next_move */
                } else if (random_value < 0x5555u) {
                    next_move++;
                } else if (random_value >= 0x8000u) {
                    if (random_value >= 0xAAAAu) {
                        next_move += (random_value >= 0xD555u) ? 5 : 4;
                    } else {
                        next_move += 3;
                    }
                }
            } else {
                if (random_value >= 0x5555u && random_value < 0xAAAAu) {
                    next_move++;
                }
            }

            if (actor->actor_hitpoints >= 0)
                actor->actor_behavior = BH_RECOVERING;
            else {
                DBG_LOG(1, "[DEAD] BH_DYING via hit_dir: actor='%s' hp=%d code_at_hp=%d dead_code=%d has_die=%d has_fall=%d\n",
                        thing_names[actor->name_index].field_0, actor->actor_hitpoints,
                        actor->code_at_hp_change, actor->dead_code_index, has_die_action, has_fall_action);
                actor->actor_behavior = BH_DYING;
                thing_name_flags[actor->name_index] |= 4;
            }
        }
        goto center_function;
    }

    case BH_DYING:
        next_move = actor->move_type;
        break;

    case BH_RECOVERING:
        if (game_version == GAME_VERSION_E1 &&
            (!actor->actor_act.act_action || (actor->flags & 0x40))) {
            actor->actor_behavior = BH_STOP;
            actor->flags &= ~0x2000;
            next_move = 0;
        } else {
            next_move = actor->move_type;
        }
        break;

    case BH_DEAD:
        next_move = -1;
        break;

    case BH_SLEEP:
        break;

    default:
        quit("unknown behaviour mode");
        break;
    }

    /* ══════ CENTER_FUNCTION — next_move selection and playback ══════ */
center_function:
    /* InbetweenPos calls for special repertoire types (simplified) */
    if (actor->actor_reperture && actor->actor_reperture->rep_index > 0) {
        int16_t rep_idx = actor->actor_reperture->rep_index;
        bool is_ranged = (next_move >= 188 && next_move < 191) || (next_move >= 184 && next_move < 187) || (next_move >= 192 && next_move < 195);
        if (rep_idx == 11) {
            if (!is_ranged && (next_move != 205 && next_move != 209))
                interpolate_pos(actor, game_time_arg, 4096);
            else
                interpolate_pos(actor, game_time_arg, 1280);
        } else if (rep_idx == 14) {
            if (is_ranged)
                interpolate_pos(actor, game_time_arg, 1280);
            else if (next_move == 204 || next_move == 208)
                interpolate_pos(actor, game_time_arg, 4096);
        } else if (rep_idx == 15) {
            if (next_move == 184 || next_move == 188 || next_move == 192)
                interpolate_pos(actor, game_time_arg, 4096);
        } else if (rep_idx == 16) {
            if (next_move == 184 || next_move == 188 || next_move == 192)
                interpolate_pos(actor, game_time_arg, 4096);
            else if (!is_ranged && (next_move == 205 || next_move == 209))
                interpolate_pos(actor, game_time_arg, 4096);
        } else {
            if (is_ranged)
                interpolate_pos(actor, game_time_arg, 1280);
        }
    }

    if (next_move >= 0) {
        if (!actor->actor_reperture || actor->actor_reperture->action_slots[next_move] < 0)
            next_move = 1000;

        if (next_move == 1000) {
            /* Idle next_move */
            if (!(actor->flags & 0x40)) {
                if (1000 == actor->move_type)
                    goto label_917;
                if (actor != selected_thing
                    && actor->actor_act.act_action
                    && (actor->actor_act.flags & (0x40 | 0x20FF)))
                    goto label_917;
            }

            action_t *current_action = NULL;
            if (actor->end_action_index < 0) {
                if (actor->actor_reperture) {
                    int action_idx_rand;
                    if (game_version == GAME_VERSION_E1) {
                        action_idx_rand = (int16_t)((6 * (uint16_t)(2 * my_rand())) >> 16);
                    } else {
                        int random_value = 100 * (2 * my_rand()) >> 16;
                        if (random_value < 30) action_idx_rand = 0;
                        else if (random_value < 55) action_idx_rand = 1;
                        else if (random_value < 72) action_idx_rand = 2;
                        else if (random_value < 85) action_idx_rand = 3;
                        else if (random_value < 95) action_idx_rand = 4;
                        else action_idx_rand = 5;

                        if (action_idx_rand == actor->action_index) {
                            action_idx_rand = actor->action_index + 1;
                            if (action_idx_rand > 5) action_idx_rand = 0;
                        }
                    }
                    actor->action_index = (int16_t)action_idx_rand;
                    int idle_base = (game_version == GAME_VERSION_E1) ? 17 : 20;
                    int16_t actor_action = actor->actor_reperture->action_slots[idle_base + action_idx_rand];
                    if (actor_action >= 0) {
                        check_action_loaded(actor_action);
                        current_action = action_tab[actor_action];
                    }
                }
            } else {
                check_action_loaded(actor->end_action_index);
                current_action = action_tab[actor->end_action_index];
            }

            if (current_action) {
                actor->actor_act.act_action = current_action;
                actor->actor_act.duration = current_action->act_duration;
                if (actor->actor_Speed_factor != 100)
                    actor->actor_act.duration = 100 * current_action->act_duration / actor->actor_Speed_factor;
                actor->actor_act.key_progress = 0;
                actor->actor_act.actor_keys_list = current_action->key_list;
                actor->actor_act.flags = current_action->action_flags & 0xFFFE;
                if (actor->end_action_index >= 0)
                    actor->actor_act.flags |= 0x20;
                actor->end_action_index = -1;
            } else {
                actor->actor_act.act_action = NULL;
            }
            update_thing(actor);
            if (actor->actor_behavior != BH_RECOVERING && actor->actor_behavior != BH_GET_HIT)
                actor->flags &= 0xDFFF;
        } else {
            /* Non-idle next_move from repertoire */
            if (!(actor->flags & 0x40)) {
                if (game_version == GAME_VERSION_E1) {
                    if ((next_move == actor->move_type)
                        || (actor->actor_act.act_action
                            && (actor->actor_act.flags & 0x40)))
                        goto label_917;
                } else {
                    if ((next_move == actor->move_type)
                        || (actor->actor_act.act_action
                            && (actor->actor_act.flags & 0x40)
                            && actor->actor_behavior != BH_RECOVERING
                            && actor->actor_behavior != BH_DYING))
                        goto label_917;
                }
            }

            action_t *current_action = NULL;
            if (actor->actor_reperture) {
                int16_t actor_action = actor->actor_reperture->action_slots[next_move];
                if (actor->actor_reperture->rep_index >= 0) {
                    check_action_loaded(actor_action);
                    current_action = action_tab[actor_action];
                }
            }

            if (current_action) {
                actor->actor_act.act_action = current_action;
                actor->actor_act.duration = current_action->act_duration;
                if (actor->actor_Speed_factor != 100)
                    actor->actor_act.duration = 100 * current_action->act_duration / actor->actor_Speed_factor;
                actor->actor_act.key_progress = 0;
                actor->actor_act.actor_keys_list = current_action->key_list;
                actor->actor_act.flags = current_action->action_flags;
                if (actor->actor_behavior == BH_RECOVERING)
                    actor->actor_act.flags &= 0xFFFE;
                if (game_version == GAME_VERSION_E1) {
                    if (next_move >= 36 && next_move <= 45) {
                        if (!(actor->actor_act.flags & 1))
                            actor->actor_act.flags |= 0x40;
                        actor->end_action_index = -1;
                    }
                } else {
                    if ((next_move >= 36 && next_move <= 45) || one_shot_action) {
                        if (!(actor->actor_act.flags & 1))
                            actor->actor_act.flags |= 0x40;
                    }
                    actor->end_action_index = -1;
                }
            } else {
                actor->actor_act.act_action = NULL;
            }
            update_thing(actor);
            if (game_version == GAME_VERSION_E1) {
                if (actor->actor_behavior != BH_RECOVERING && actor->actor_behavior != BH_GET_HIT)
                    actor->flags &= 0xDFFF;
            } else {
                if (actor->actor_behavior != BH_RECOVERING && actor->actor_behavior != BH_GET_HIT
                    && (!actor->actor_act.act_action || !(actor->actor_act.flags & 0x80)))
                    actor->flags &= 0xDFFF;
            }
        }
        actor->move_type = next_move;
    }

    /* ══════ LABEL_917 — final act advance ══════ */
label_917:
    actor->state_flags &= 0xFFFB;
    action_t *cur_action = actor->actor_act.act_action;
    actor->flags &= 0xFFBF;
    if (cur_action) {
        cur_action->action_time = game_time;
        if (cur_action->key_list && cur_action->key_list->key_event_list)
            actor_last_act[actor->name_index] = cur_action->next_action_index + 1;
    }
    advance_act(&actor->actor_act, actor, game_time_arg);
    if (actor->actor_act.act_action) {
        if ((int32_t)actor->actor_act.duration * actor->actor_act.key_progress >> 16 > 20) {
            if (actor->actor_reperture && actor->actor_reperture->action_slots[9] >= 0)
                actor->flags &= 0xDFFF;
        }
    }
    if (game_version != GAME_VERSION_E1) {
        if (actor == selected_thing
            || (actor->actor_reperture && actor->actor_reperture->action_slots[1] >= 0))
            update_velocity(actor);
    }

    if (actor->actor_act.act_action) {
        if (!(actor->actor_act.flags & 1) && actor->actor_act.key_progress == 0xFFFF) {
            actor->flags |= 0x40;
            if (actor->actor_act.flags & 0x10) {
                remove_from_display_list(actor);
                thing_name_flags[actor->name_index] &= 0xFFFD;
            }
            actor->actor_act.act_action = NULL;
        }
    } else {
        actor->flags |= 0x40;
    }
}

/* move_unmake2_part_limb  E1: 0x426B14 | E2: 0x42D590 */
void unmake2_part_limb(actor_t *actor) {
    if (!actor) return;
    actor->flags &= ~0x20;  /* Clear 2-part limb flag */
}

/* ══════════════════════════════════════════════════════════════
 *  Check steps / Recover hit points (E2 stubs)
 * ══════════════════════════════════════════════════════════════ */

/* move_check_steps  E1: ? | E2: 0x429FC8 */
void check_steps(void) {
    /* E2 stub — just returns */
}

/* move_recover_hit_points  E1: ? | E2: 0x42A67C */
void recover_hit_points(void) {
    /* E2 stub — just returns */
}

/* ══════════════════════════════════════════════════════════════
 *  Act heap management — find_free_act, spawn_action
 * ══════════════════════════════════════════════════════════════ */

/* move_find_free_act  E1: 0x4263FC | E2: 0x42CE78 */
act_t *find_free_act(void) {
    for (int i = 0; i < ACT_SIZE; ++i) {
        if (!(act_arr[i].flags & 0x100)) {
            act_arr[i].flags = 0x100;
            return &act_arr[i];
        }
    }
    return NULL;
}

/* move_spawn_action  E1: 0x42644C | E2: 0x42CEC8 */
void spawn_action(event_t *event, actor_t *actor, int some_time) {
    act_t *act = find_free_act();
    if (act) {
        act->act_action = action_tab[event->param1];
        act->duration = event->param2;
        act->loop_count = event->param3 & 0xFF;
        act->anim_param = (event->param3 >> 8) & 0xFF;
        act->key_progress = 0;
        act->actor_keys_list = act->act_action->key_list;
        update_act(act, actor, some_time);
        if (act->flags & 0x0400) {
            act->flags = 0;
        } else {
            /* Append to tail of actor's act list */
            if (actor->actor_act_list) {
                act_t *tail = actor->actor_act_list;
                while (tail->next)
                    tail = tail->next;
                tail->next = act;
                act->next = NULL;
            } else {
                actor->actor_act_list = act;
                act->next = NULL;
            }
        }
    } else {
        beep_error("can't spawn action");
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Act Playback — complete_act, position_act, free_spent_acts
 * ══════════════════════════════════════════════════════════════ */

/* move_complete_act  E1: 0x42489C | E2: 0x42AF78 */
void complete_act(act_t *act, actor_t *actor) {
    if (!act || !actor || !act->act_action) return;

    for (key_state_t *key = act->actor_keys_list; key; key = key->next) {
        for (event_t *event = key->key_event_list; event; event = event->next) {
            /* asm move_complete_act_42B004+1C: `mov ebx, 0FFFFh` zero-extends
             * to 65535 in 32-bit ebx; spawn_action then reads it as uint16. */
            modify_part(event, actor, 0xFFFF, act->act_action);
        }
    }

    act->actor_keys_list = act->act_action->key_list;
    act->key_progress = 0;
    if (act->act_action->action_flags & 4)
        update_thing(actor);
}

/* move_position_act  E1: 0x424AA4 | E2: 0x42B2AC */
void position_act(act_t *act, uint16_t some_duration, actor_t *actor) {
    if (!act || !actor || !act->act_action) return;

    if (some_duration < act->key_progress)
        complete_act(act, actor);

    key_state_t *key;
    for (key = act->actor_keys_list; key; key = key->next) {
        if (some_duration < key->KEY_position)
            break;

        int16_t some_time;
        if (act->act_action->action_flags & 2)
            some_time = (int16_t)(some_duration - key->KEY_position);
        else
            some_time = (int16_t)(act->duration * (some_duration - key->KEY_position) >> 16);

        for (event_t *event = key->key_event_list; event; event = event->next) {
            modify_part(event, actor, some_time, act->act_action);
        }

        if (key == act->actor_keys_list && !(act->act_action->action_flags & 2))
            default_modifieds(actor);

        act->key_progress = key->KEY_position;
    }

    if (key) {
        int denom = key->KEY_position - act->key_progress;
        if (denom > 0) {
            int some_time = ((some_duration - act->key_progress) << 14) / denom;
            if (some_time) {
                for (event_t *event = key->key_event_list; event; event = event->next)
                    advance_part(event, (int16_t)some_time, actor, act->act_action);

                if (!(act->act_action->action_flags & 2))
                    advance_def_modifieds(actor, some_time);
            }
        }
    }

    act->actor_keys_list = key;
    act->key_progress = some_duration;
}

/* move_free_spent_acts */
void free_spent_acts(actor_t *actor) {
    if (!actor || !actor->actor_act_list) return;

    /* Clear flags on leading spent acts */
    for (act_t *act = actor->actor_act_list; act; act = act->next) {
        if (!(act->flags & 0x400)) break;
        act->flags = 0;
    }

    /* Unlink interior spent acts */
    for (act_t *act = actor->actor_act_list; act && act->next; act = act->next) {
        if (act->next->flags & 0x400) {
            act->next->flags = 0;
            act->next = act->next->next;
        }
    }
}

/* move_default_modifieds */
void default_modifieds(actor_t *actor) {
    if (!actor) return;
    for (part_t *part = actor->actor_parts_list; part; part = part->next_in_display_list) {
        int16_t v2 = part->def_pos_flags;
        if (v2) {
            part->def_pos_flags = ~part->position_flags & v2;
            int flags = part->def_pos_flags;
            if (flags & 1)
                copy_vector(&part->AbsPosition, &part->def_position);
            if (flags & 2)
                copy_vector(&part->Rotate, &part->def_rotate);
            if (flags & 4) {
                copy_vector(&part->Offset, &part->def_offset);
                calc_rel_offset(part);
            }
            if (flags & 8) {
                copy_vector(&part->VECTOR_RelCentre, &part->def_RelCentre);
                calc_rel_centre(part);
            }
            if (flags & 0x10) {
                copy_vector(&part->VECTOR_Squash, &part->def_Squash);
                calculate_squash(part);
                update_relatives(part);
            }
            if (flags & 0x20)
                part->color = part->default_color;
            part->def_pos_flags = 0;
        }
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Direction / Distance
 * ══════════════════════════════════════════════════════════════ */

/* move_find_dirn_and_dist_428548 — see find_direction_and_distance for the
 * truncate-then-abs note. */
void find_dirn_and_dist(int16_t *dir, int16_t *dist, int16_t dx, int16_t dz) {
    if (!dir || !dist) return;

    uint16_t angle = arctan(dx, dz);
    *dir = (int16_t)angle;

    int abs_dx = abs(dx);
    int abs_dz = abs(dz);
    int d;
    if (dx || dz) {
        if (abs_dz <= abs_dx) {
            int value = sine_table[angle];
            d = value ? ((int)dx << 14) / value : 0;
        } else {
            int value = cosn_table[angle];
            d = value ? ((int)dz << 14) / value : 0;
        }
    } else {
        d = 0;
    }
    int16_t low = (int16_t)d;
    *dist = (int16_t)(low < 0 ? -low : low);
}

/* move_find_direction_and_distance_427160.
 * NOTE: asm computes int32 d, then truncates to int16 BEFORE taking abs.
 * Doing abs in full int width then truncating yields wrong sign when |d|
 * overflows int16 such that low-16 sign-extension flips polarity. */
void find_direction_and_distance(int16_t *dir, int16_t *dist, int16_t dx, int16_t dz) {
    uint16_t angle = arctan(dx, dz);
    *dir = (int16_t)angle;

    if (!dx && !dz) {
        *dist = 0;
        return;
    }

    int d;
    if (abs(dz) <= abs(dx)) {
        int value = sine_table[angle];
        d = value ? ((int)dx << 14) / value : 0;
    } else {
        int value = cosn_table[angle];
        d = value ? ((int)dz << 14) / value : 0;
    }
    int16_t low = (int16_t)d;
    *dist = (int16_t)(low < 0 ? -low : low);
}

/* move_find_direction_and_distance_4285E0 — swapped param order (dz, dx) */
void find_direction_and_distance_zx(int16_t *dir, int16_t *dist, int16_t dz, int16_t dx) {
    find_dirn_and_dist(dir, dist, dx, dz);
}

/* ══════════════════════════════════════════════════════════════
 *  Actor State — turn, death, smart bomb
 * ══════════════════════════════════════════════════════════════ */

/* move_turn_actor_42490C — rebuild matrix33_2 from rotate_vector */
void turn_actor(actor_t *actor, int16_t angle) {
    if (!actor) return;
    actor->rotate_vector.Y += angle;
    actor->state_flags |= 2;
    make_identity(&actor->matrix33_2);
    if (actor->rotate_vector.Y)
        rotate_about_y(&actor->matrix33_2, actor->rotate_vector.Y);
    if (actor->rotate_vector.X)
        rotate_about_x(&actor->matrix33_2, actor->rotate_vector.X);
    if (actor->rotate_vector.Z)
        rotate_about_z(&actor->matrix33_2, actor->rotate_vector.Z);
}

/* move_make_dead  E1: 0x4236AC | E2: 0x42739C */
void make_dead(actor_t *actor) {
    if (!actor) return;

    if (actor->actor_reperture &&
        (actor->actor_reperture->action_slots[80] >= 0 ||
         actor->actor_reperture->action_slots[9] >= 0) &&
        actor != selected_thing)
        ++kill_count;

    actor->actor_behavior = BH_DEAD;
    if (actor->actor_reperture) {
        actor->actor_reperture->rep_use_flag &= 0xFFFD;
        actor->actor_reperture = NULL;
    }
    actor->actor_rep_index = -1;
    actor->flags |= 0x400;
    actor->action_delay = 200;
    thing_name_flags[actor->name_index] |= 4;

    DBG_LOG(1, "[DEAD] make_dead: actor='%s' name_idx=%d is_hero=%d dead_code_idx=%d\n",
            thing_names[actor->name_index].field_0, actor->name_index,
            actor == selected_thing, actor->dead_code_index);

    int code_idx = actor->dead_code_index;
    if (code_idx >= 0) {
        if (code_tab[code_idx])
            execute_thing_code(actor, (int16_t)code_idx);
        else {
            char str1[104], str2[104];
            sprintf(str1, "Actor: %s", thing_names[actor->name_index].field_0);
            sprintf(str2, "Code:  %s", code_names[code_idx].field_0);
            do_info3_req("Can't find DeadCode", str1, str2);
        }
    }
}

/* move_smart_bomb_4287B8 — SmartBomb(actor, range, damage) */
void smart_bomb(actor_t *attacker, int range, int damage) {
    if (!attacker || !root_thing) return;

    int max_range = 100 * range;
    int min_damage = 5 * damage / 100;
    int max_damage = 40 * damage / 100;

    for (actor_t *victim = root_thing; victim; victim = victim->next_in_display_list) {
        if (!victim->actor_reperture) continue;
        if (thing_name_flags[victim->name_index] & 4) continue;
        rephead_t *vrep = victim->actor_reperture;
        /* Bug 54: Interact must be loaded, and target must be either a
         * walker (WalkLeft loaded) or the hero. */
        if (vrep->action_slots[34] < 0) continue;
        if (vrep->action_slots[3] < 0 && victim != selected_thing) continue;
        if (victim == attacker) continue;

        int dx = victim->position_vector.X - attacker->position_vector.X;
        int dz = victim->position_vector.Z - attacker->position_vector.Z;
        int abs_dx = abs(dx);
        int abs_dz = abs(dz);
        if (abs_dx >= max_range || abs_dz >= max_range) continue;

        uint16_t angle = arctan(dx, dz);
        int dist = 0;
        if (dx || dz) {
            if (abs_dz <= abs_dx) {
                dist = sine_table[angle];
                if (dist) dist = (dx << 14) / dist;
            } else {
                dist = cosn_table[angle];
                if (dist) dist = (dz << 14) / dist;
            }
        }
        if (dist < 0) dist = -dist;

        int dy = abs(victim->position_vector.Y - attacker->position_vector.Y);
        if (dist + dy >= max_range) continue;

        victim->hit_type = 1;  /* HitType::Normal */
        victim->hit_angle = (int16_t)arctan(dx, dz);

        /* Bug 54: attacker.hit_code_id == null → apply default damage;
         * else run attacker's hit code. Then run victim's hp-change code. */
        if (attacker->actor_hit_code < 0) {
            victim->actor_behavior = BH_GET_HIT;
            victim->flags |= 0x2000;   /* CannotBeHit */

            int applied_damage;
            if (victim == selected_thing && no_die)      applied_damage = 0;
            else if (victim == selected_thing)           applied_damage = min_damage;
            else                                         applied_damage = max_damage;

            if (victim == selected_thing) {
                int factor = armour_factor ? armour_factor : 1;
                int scale = (difficulty == 0) ? 50 : (difficulty == 1) ? 100 : 200;
                victim->actor_hitpoints -= (int16_t)(scale * applied_damage / factor);
            } else {
                victim->actor_hitpoints -= (int16_t)applied_damage;
            }
        } else {
            if (attacker->actor_hit_code >= 0 && attacker->actor_hit_code < CODE_TAB_SIZE) {
                code_t *hc = code_tab[attacker->actor_hit_code];
                if (hc) execute_code(hc, victim);
            }
        }

        if (victim->code_at_hp_change >= 0 && victim->code_at_hp_change < CODE_TAB_SIZE) {
            code_t *hpc = code_tab[victim->code_at_hp_change];
            DBG_LOG(1, "[DEAD] code_at_hp_change: actor='%s' code_idx=%d code=%p hp=%d is_hero=%d\n",
                    thing_names[victim->name_index].field_0, victim->code_at_hp_change,
                    (void*)hpc, victim->actor_hitpoints, victim == selected_thing);
            if (hpc) execute_code(hpc, victim);
        }

        if (victim == selected_thing)
            draw_life_bar();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  ModifyPart — apply event to part at a given time
 * ══════════════════════════════════════════════════════════════ */

/* move_modify_part  E1: 0x424FEC | E2: 0x42B7F4 */
void modify_part(event_t *event, actor_t *actor, int some_time, action_t *action) {
    if (!event) return;

    int16_t et_flags = event_type_flags[event->event_type];

    /* Handle creation events that don't require an existing actor/part lookup */
    switch (event->event_type) {
    case ADD_THING: {
        actor_t *new_actor = add_thing();
        selected_thing = new_actor;
        new_actor->name_index = event->param1;
        new_actor->last_actor_direction = last_actor_dir;
        thing_tab[new_actor->name_index] = new_actor;
        return;
    }
    case ADD_PART_TO_THING: {
        if (!actor) return;
        part_t *p = add_part(actor);
        if (!p) return;
        p->name_index = event->param1;
        static int apt_log = 0;
        if (apt_log < 10)
            fprintf(stderr, "[EVT] ADD_PART actor=%p _PartTab=%p name_idx=%d part=%p stored? ",
                (void*)actor, (void*)actor->_PartTab, event->param1, (void*)p);
        if (actor->_PartTab) {
            actor->_PartTab->field_0[event->param1] = p;
            if (apt_log < 10)
                fprintf(stderr, "YES verify[%d]=%p\n", event->param1,
                    (void*)actor->_PartTab->field_0[event->param1]);
        } else {
            if (apt_log < 10)
                fprintf(stderr, "NO _PartTab\n");
        }
        apt_log++;
        return;
    }
    default:
        break;
    }

    /* All other events require an existing actor */
    if (!actor) return;

    part_t *part = NULL;
    point_t *point = NULL;
    tri_t *triangle = NULL;

    if (et_flags & 0x10) {
        /* Part-targeted event */
        if (event->event_index < 0) return;
        if (et_flags & 0x200) {
            for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list) {
                if (p->actor_2_held && p->actor_2_held->_PartTab) {
                    part = p->actor_2_held->_PartTab->field_0[event->event_index];
                    if (part) break;
                }
            }
            if (!part) return;
            actor = part->parent_actor;
        } else {
            if (actor->_PartTab)
                part = actor->_PartTab->field_0[event->event_index];
            if (!part) {
                if (actor->part_heap_link) {
                    actor = actor->part_heap_link->parent_actor;
                    if (!(actor->state_flags & 1)) {
                        if (event->event_type != INTERACT) return;
                    } else {
                        if (actor->_PartTab)
                            part = actor->_PartTab->field_0[event->event_index];
                    }
                    if (!part && event->event_type != INTERACT) return;
                } else {
                    for (part_t *p = actor->actor_parts_list; p; p = p->next_in_display_list) {
                        if (p->actor_2_held && p->actor_2_held->_PartTab) {
                            part = p->actor_2_held->_PartTab->field_0[event->event_index];
                            if (part) break;
                        }
                    }
                    if (!part && event->event_type != INTERACT) return;
                    if (part) actor = part->parent_actor;
                }
            }
        }
    } else if (et_flags & 0x20) {
        /* Triangle-targeted event */
        if (event->event_index < 0) return;
        if (actor->_TriangleTab)
            triangle = actor->_TriangleTab->field_0[event->event_index];
        if (!triangle) return;
    } else if (et_flags & 0x40) {
        /* Point-targeted event */
        if (event->event_index < 0) return;
        if (actor->_PointTab)
            point = actor->_PointTab->field_0[event->event_index];
        if (!point) return;
    }

    switch (event->event_type) {
    case ROTATE_THING: {
        part_t *wp = actor->part_heap_link;
        if (wp && (wp->parent_actor->state_flags & 1))
            actor = wp->parent_actor;
        if (actor->state_flags & 0x20) {
            update_thing(actor);
            actor->state_flags &= ~0x20;
        }
        actor->rotate_vector.X += event->param1 - actor->Rotate.X;
        actor->rotate_vector.Y += event->param2 - actor->Rotate.Y;
        actor->rotate_vector.Z += event->param3 - actor->Rotate.Z;
        set_vector(&actor->Rotate, event->param1, event->param2, event->param3);
        break;
    }
    case MOVE_THING: {
        part_t *wp = actor->part_heap_link;
        if (wp && (wp->parent_actor->state_flags & 1))
            actor = wp->parent_actor;
        if (actor->state_flags & 0x20) {
            update_thing(actor);
            actor->state_flags &= ~0x20;
        }
        vector_t input, output;
        input.X = event->param1 - actor->Offset.X;
        input.Y = event->param2 - actor->Offset.Y;
        input.Z = event->param3 - actor->Offset.Z;
        set_vector(&actor->Offset, event->param1, event->param2, event->param3);
        c_matrix_vector(&output, &actor->matrix33_2, &input);
        update_position(actor, &output);
        check_visibility(actor);
        break;
    }
    case SCRIPT_MOVE:
        set_vector(&actor->position_vector, event->param1, event->param2, event->param3);
        update_thing(actor);
        break;
    case SCRIPT_TURN:
        set_vector(&actor->rotate_vector, event->param1, event->param2, event->param3);
        update_thing(actor);
        break;
    case SPAWN_ACTION:
        spawn_action(event, actor, some_time);
        break;
    case START_POSITION:
        set_vector(&actor->start_position, event->param1, event->param2, event->param3);
        fprintf(stderr, "[EVT] START_POSITION actor=%p pos=(%d,%d,%d)\n",
            (void*)actor, event->param1, event->param2, event->param3);
        break;
    case HELD_OFFSET:
        set_vector(&actor->held_offset, event->param1, event->param2, event->param3);
        break;
    case HELD_ROTATE:
        set_vector(&actor->held_rotate, event->param1, event->param2, event->param3);
        break;
    case HELD_OFF_LEFT:
        set_vector(&actor->held_off_left, event->param1, event->param2, event->param3);
        break;
    case HELD_ROT_LEFT:
        set_vector(&actor->held_rot_left, event->param1, event->param2, event->param3);
        break;
    case THING_FLAGS:
        actor->flags = (event->param3 & event->param1) | (~event->param3 & actor->flags);
        if (event->param3 == -1)
            actor->state_flags = event->param2;
        if (event->param3 == -1 && (thing_name_flags[actor->name_index] & 0x40)) {
            if (thing_name_flags[actor->name_index] & 0x10)
                actor->flags |= 0x1000;
            else
                actor->flags &= ~0x1000;
            if (thing_name_flags[actor->name_index] & 0x20)
                actor->flags |= 0x20;
            else
                actor->flags &= ~0x20;
        } else {
            if (actor->flags & 0x1000) thing_name_flags[actor->name_index] |= 0x10;
            else thing_name_flags[actor->name_index] &= ~0x10;
            if (actor->flags & 0x20) thing_name_flags[actor->name_index] |= 0x20;
            else thing_name_flags[actor->name_index] &= ~0x20;
            if (event->param3 == -1) thing_name_flags[actor->name_index] |= 0x40;
        }
        break;
    case THING_CODE:
        actor->code_at_hp_change = event->param1 - 1;
        actor->actor_hit_code = event->param2 - 1;
        actor->actor_init_code = event->param3 - 1;
        break;
    case THING_CODE_2:
        actor->picked_up_code = event->param1 - 1;
        actor->dead_code_index = event->param2 - 1;
        break;
    case REORIENT_THING:
        actor->state_flags |= 0x20;
        break;
    case BACKGROUND:
        if (event->param1 == 1) {
            if (actor->flags & 0x400)
                clear_a_stuck_thing(actor);
            actor->flags &= ~0x400;
        } else
            actor->flags |= 0x400;
        break;
    case ROTATE:
    case N68:
        if (!part) break;
        set_vector(&part->Rotate, event->param1, event->param2, event->param3);
        part->flags &= ~0x4200;
        part->position_flags |= 2;
        break;
    case OFFSET:
        if (!part) break;
        set_vector(&part->Offset, event->param1, event->param2, event->param3);
        calc_rel_offset(part);
        part->position_flags |= 4;
        break;
    case POSITION:
    case N69:
        if (!part) break;
        set_vector(&part->AbsPosition, event->param1, event->param2, event->param3);
        part->flags &= ~0x4200;
        part->position_flags |= 1;
        break;
    case DISP_PNT:
        if (!part) break;
        set_vector(&part->displacement_point, event->param1, event->param2, event->param3);
        break;
    case VECTOR1: {
        static int v1_log = 0;
        if (v1_log < 10)
            fprintf(stderr, "[EVT] VECTOR1 actor=%p part=%p ev_idx=%d _PartTab=%p val=(%d,%d,%d)\n",
                (void*)actor, (void*)part, event->event_index,
                actor ? (void*)actor->_PartTab : NULL,
                event->param1, event->param2, event->param3);
        v1_log++;
        if (!part) break;
        set_vector(&part->VECTOR_Squash, event->param1, event->param2, event->param3);
        calculate_squash(part);
        update_relatives(part);
        part->position_flags |= 0x10;
        break;
    }
    case VECTOR2:
        if (!part) break;
        set_vector(&part->VECTOR_RelCentre, event->param1, event->param2, event->param3);
        calc_rel_centre(part);
        part->position_flags |= 8;
        break;
    case COLOUR:
        if (!part) break;
        part->color = (uint8_t)event->param1;
        part->position_flags |= 0x20;
        break;
    case SHADE:
        if (!part) break;
        part->color_shade = event->param1;
        break;
    case TYPE:
        if (!part) break;
        part->type = (uint16_t)event->param1;
        break;
    case FLAGS:
        if (!part) break;
        part->flags = event->param1 | (~event->param2 & part->flags);
        break;
    case END_ACTION:
        actor->end_action_index = event->param1;
        break;
    case ABSOLUTE_POS:
        if (!part) break;
        set_vector(&part->AbsPosition, event->param1, event->param2, event->param3);
        if (action && action->action_flags & 2)
            part->flags |= 0x0200;
        else
            part->flags |= 0x4200;
        break;
    case ABSOLUTE_ROT:
        if (!part) break;
        set_vector(&part->Rotate, event->param1, event->param2, event->param3);
        if (action && action->action_flags & 2)
            part->flags |= 0x0200;
        else
            part->flags |= 0x4200;
        break;
    case OFFSET_POINT:
        if (!point) break;
        set_vector(&point->offset_point, event->param1, event->param2, event->param3);
        break;
    case COLOUR_TRIANGLE:
        if (!triangle) break;
        triangle->tri_color_3 = event->param1;
        triangle->tri_color_4 = event->param2;
        if (event->param2 >= 16)
            triangle->tri_color_4 = 1;
        if (event->param3 & 0x8000)
            triangle->shade_multiplier = event->param3 & 0x7FFF;
        break;
    case TRIANGLE_FLAGS:
        if (!triangle) break;
        triangle->tri_use_flag = (event->param2 & event->param1) | (~event->param2 & triangle->tri_use_flag);
        break;
    case ACTOR_REP:
        DBG_LOG(1, "[EVT] ACTOR_REP actor=%d p1=%d p2=%d p3=%d (rep_idx=%d->%d def=%d)\n",
                actor ? actor->name_index : -1, event->param1, event->param2, event->param3,
                actor ? actor->actor_rep_index : -999, event->param1,
                actor ? actor->default_repert : -999);
        actor->actor_rep_index = event->param1;
        if (event->param2)
            actor->default_repert = event->param3;
        break;
    case DEF_ROTATE:
        if (!part) break;
        set_vector(&part->def_rotate, event->param1, event->param2, event->param3);
        break;
    case DEF_OFFSET:
        if (!part) break;
        set_vector(&part->def_offset, event->param1, event->param2, event->param3);
        break;
    case DEF_VECTOR1:
        if (!part) break;
        set_vector(&part->def_Squash, event->param1, event->param2, event->param3);
        break;
    case DEF_VECTOR2:
        if (!part) break;
        set_vector(&part->def_RelCentre, event->param1, event->param2, event->param3);
        break;
    case DEF_COLOUR:
        if (!part) break;
        part->default_color = (uint8_t)event->param1;
        break;
    case DEF_FLAGS:
        if (!part) break;
        part->default_flags = (uint16_t)event->param1;
        break;
    case DEF_POSITION:
        if (!part) break;
        set_vector(&part->def_position, event->param1, event->param2, event->param3);
        break;
    case INTERACT:
        /* Interaction events dispatched by sub-type */
        switch (event->param1) {
        case 0: /* CheckPartHit */
            if (part) check_part_hit(part);
            break;
        case 1: /* CheckPickUp */
            DBG_LOG(1, "[INTERACT] CheckPickUp part=%p parent=%d param=%d\n",
                    (void *)part, part ? part->parent_actor->name_index : -1, (int)event->param2);
            if (part) check_pick_up(part, event->param2);
            break;
        case 2: /* CheckPutDown */
            DBG_LOG(1, "[INTERACT] CheckPutDown part=%p parent=%d held=%d\n",
                    (void *)part, part ? part->parent_actor->name_index : -1,
                    (part && part->actor_2_held) ? part->actor_2_held->name_index : -1);
            if (part) check_put_down(part, action ? (action->action_flags & 2) : 0);
            break;
        case 3: /* HoldThingWithPart */ {
            if (!part) break;
            actor_t *a = thing_tab[event->param2];
            DBG_LOG(1, "[INTERACT] HoldThingWithPart part=%p parent=%d actor=%d '%s'\n",
                    (void *)part, part->parent_actor->name_index, event->param2,
                    (thing_names && event->param2 >= 0 && event->param2 < THING_TAB_SIZE) ? thing_names[event->param2].field_0 : "?");
            if (a) {
                if (a->part_heap_link) {
                    ((part_t *)a->part_heap_link)->actor_2_held = NULL;
                }
                part->actor_2_held = a;
                a->part_heap_link = part;
            }
            break;
        }
        case 4: /* ExecutePartCode */
            if (event->param2 && code_tab[event->param2 - 1]) {
                if (part)
                    execute_part_code(part, (int16_t)(event->param2 - 1));
                else
                    execute_code(code_tab[event->param2 - 1], NULL);
            }
            break;
        case 5:
            if (sound_fx_on && sound_is_on) {
                if (game_version == GAME_VERSION_E1 && event->param2 >= 0 && event->param2 < E1_SOUND_TAB_SIZE) {
                    const char *sname = sound_names[event->param2].field_0;
                    bool is_female_sound = false;
                    bool is_male_sound = false;
                    if (sname[0] == 'f' || sname[0] == 'F') {
                        if (sname[1] == '_' || sname[1] == 'f' || sname[1] == 'F')
                            is_female_sound = true;
                        else if ((sname[1] == 'h' || sname[1] == 'H') && (sname[2] == 'h' || sname[2] == 'H' || sname[2] == 'o' || sname[2] == 'O'))
                            is_female_sound = true;
                        else if (strncasecmp(sname, "fgethit", 7) == 0 || strncasecmp(sname, "femscrm", 7) == 0 || strncasecmp(sname, "female", 6) == 0)
                            is_female_sound = true;
                        if (!is_female_sound) {
                            for (int si = 0; si < E1_SOUND_TAB_SIZE; si++) {
                                if (si != event->param2 && sound_names[si].field_0[0]
                                    && strcasecmp(sound_names[si].field_0, sname + 1) == 0) {
                                    is_female_sound = true;
                                    break;
                                }
                            }
                        }
                    } else if (sname[0]) {
                        if ((sname[0] == 'h' || sname[0] == 'H') && (strncasecmp(sname, "hh", 2) == 0 || strncasecmp(sname, "hotel", 5) == 0))
                            is_male_sound = true;
                        else if (strncasecmp(sname, "male", 4) == 0)
                            is_male_sound = true;
                        if (!is_male_sound) {
                            char fbuf[28];
                            fbuf[0] = 'f';
                            strncpy(fbuf + 1, sname, sizeof(fbuf) - 2);
                            fbuf[sizeof(fbuf) - 1] = '\0';
                            for (int si = 0; si < E1_SOUND_TAB_SIZE; si++) {
                                if (si != event->param2 && sound_names[si].field_0[0]
                                    && strcasecmp(sound_names[si].field_0, fbuf) == 0) {
                                    is_male_sound = true;
                                    break;
                                }
                            }
                        }
                    }
                    if ((is_female_sound && !female) || (is_male_sound && female))
                        break;
                }
                play_sound_ecstatica(actor, event->param2, action ? (action->action_flags & 2) : 0, 0);
            }
            break;
        case 6: /* FireBullet (E2) */
            if (part) fire_bullet(part);
            break;
        case 7: /* BloodSpurt (E2) */
            if (part) blood_spurt(part);
            break;
        case 8: /* SpawnActor (E2) */
            if (part) spawn_actor(part, event->param2, event->param3, 0, 1);
            break;
        case 9: /* SpawnActor2 (E2) */
            if (part) spawn_actor(part, event->param2, event->param3, 1, 1);
            break;
        default:
            break;
        }
        return; /* INTERACT returns directly, like reference */
    case ADD_PART: {
        static int ap_log = 0;
        if (ap_log < 20)
            fprintf(stderr, "[EVT] ADD_PART ev_idx=%d param1=%d part=%p actor=%p\n",
                event->event_index, event->param1, (void*)part, (void*)actor);
        ap_log++;
        if (!part) break;
        int16_t part_id = event->param1;
        if (part->parent_actor && part->parent_actor->_PartTab &&
            !part->parent_actor->_PartTab->field_0[part_id]) {
            part_t *p = add_part((actor_t *)part);
            if (p) {
                p->name_index = part_id;
                p->parent_actor->_PartTab->field_0[part_id] = p;
            }
        }
        break;
    }
    case CUT_PART:
        if (part) remove_part(part);
        break;
    case TWO_PART_LIMB:
        if (part) make_2_part_limb(part);
        break;
    case UNMAKE_LIMB:
        if (part) unmake_2_part_limb(part);
        break;
    case ADD_POINT: {
        if (!part) break;
        point_t *new_pt = add_point(part);
        if (new_pt) {
            new_pt->point_index = event->param1;
            if (part->parent_actor && part->parent_actor->_PointTab)
                part->parent_actor->_PointTab->field_0[new_pt->point_index] = new_pt;
        }
        break;
    }
    case ADD_TRIANGLE: {
        if (event->param1 >= 0 && event->param2 >= 0 && event->param3 >= 0 &&
            actor->_PointTab) {
            point_t *pts[4];
            pts[0] = actor->_PointTab->field_0[event->param1];
            pts[1] = actor->_PointTab->field_0[event->param2];
            pts[2] = actor->_PointTab->field_0[event->param3];
            pts[3] = NULL;
            if (pts[0] && pts[1] && pts[2]) {
                tri_t *t = add_triangle(actor, pts);
                if (t) {
                    t->tri_index = event->event_index;
                    if (actor->_TriangleTab)
                        actor->_TriangleTab->field_0[event->event_index] = t;
                }
            }
        }
        break;
    }
    case MAKE_QUAD:
        if (triangle && !triangle->quad_point4 && triangle->parent_actor &&
            triangle->parent_actor->_PointTab &&
            triangle->parent_actor->_PointTab->field_0[event->param1]) {
            triangle->quad_point4 = triangle->parent_actor->_PointTab->field_0[event->param1];
        }
        break;
    case POINT_TO_POINT:
        if (!part) break;
        if (event->param1 < 0) {
            part->field_12E_point_to_point = NULL;
        } else if (part->parent_actor && part->parent_actor->_PointTab) {
            part->field_12E_point_to_point = part->parent_actor->_PointTab->field_0[event->param1];
        }
        break;
    case TRI_SHADE_NAME:
        if (triangle)
            triangle->tri_shade_name = event->param1;
        break;
    case PART_TEXTURE:
        /* Textures not used in the shipped game */
        break;
    default:
        break;
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Part coordinate transforms
 * ══════════════════════════════════════════════════════════════ */

/* move_make_part_absolute  E1: 0x426C24 | E2: 0x42D6A0 */
void make_part_absolute(part_t *part) {
    if (part->flags & 0x200)
        return;

    actor_t *actor = part->parent_actor;
    make_identity(&actor->matrix_1);
    if (actor->Rotate.Y)
        rotate_about_y(&actor->matrix_1, (uint16_t)actor->Rotate.Y);
    if (actor->Rotate.X)
        rotate_about_x(&actor->matrix_1, (uint16_t)actor->Rotate.X);
    if (actor->Rotate.Z)
        rotate_about_z(&actor->matrix_1, (uint16_t)actor->Rotate.Z);
    find_position_of_extremity(part);
    find_relative_rotations(part, &actor->matrix_1);
    copy_vector(&part->AbsPosition, &part->joint_position);
    part->flags |= 0x200;
}

/* move_make_part_base_relative  E1: 0x426CB8 | E2: 0x42D734 */
void make_part_base_relative(part_t *part) {
    if (part->flags & 0x200)
        return;

    actor_t *actor = part->parent_actor;
    find_position_of_extremity(part);

    vector_t input;
    for (int i = 0; i < 3; i++)
        input.data[i] = part->joint_position.data[i] - actor->actor_center.data[i];

    matrix3x3_t inv;
    matrix_inverse(&actor->matrix33_2, &inv);
    vector_t output;
    c_matrix_vector(&output, &inv, &input);

    part->AbsPosition.X = output.X;
    part->AbsPosition.Y = output.Y;
    part->AbsPosition.Z = output.Z;

    matrix3x3_t combined;
    matrix_mult(&inv, &part->matrix_1, &combined);
    find_relative_rotations(part, &combined);
    part->flags |= 0x4200;
}

/* move_make_part_relative  E1: 0x426D58 | E2: 0x42D7D4 */
void make_part_relative(part_t *part) {
    if (!(part->flags & 0x200))
        return;

    actor_t *actor = part->parent_actor;
    make_identity(&actor->matrix_2);
    if (actor->rotate_vector.Y)
        rotate_about_y(&actor->matrix_2, (uint16_t)actor->rotate_vector.Y);
    if (actor->rotate_vector.X)
        rotate_about_x(&actor->matrix_2, (uint16_t)actor->rotate_vector.X);
    if (actor->rotate_vector.Z)
        rotate_about_z(&actor->matrix_2, (uint16_t)actor->rotate_vector.Z);
    find_position_of_extremity(part);

    vector_t input;
    input.X = part->joint_position.X - actor->position_vector.X;
    input.Y = part->joint_position.Y - actor->position_vector.Y;
    input.Z = part->joint_position.Z - actor->position_vector.Z;

    matrix3x3_t inv;
    matrix_inverse(&actor->matrix_2, &inv);
    vector_t output;
    c_matrix_vector(&output, &inv, &input);
    copy_vector(&part->AbsPosition, &output);

    matrix3x3_t combined;
    matrix_mult(&inv, &part->matrix_1, &combined);
    find_relative_rotations(part, &combined);
    part->flags &= 0xBDFF;  /* clear 0x4200 */
}

/* ══════════════════════════════════════════════════════════════
 *  Sound — play_sound_ecstatica, find_footstep_sound
 * ══════════════════════════════════════════════════════════════ */

/* move_find_footstep_sound_42FC84 — return sound index for hero_material. */
int find_footstep_sound(void) {
    switch (hero_material) {
    case 0: case 5: case 8:                 return 7;
    case 3: case 23:                        return 8;
    case 4: case 17:                        return 3;
    case 7:                                 return 1;
    case 13:                                return 10;
    case 16:                                return 2;
    case 18: case 19: case 21:              return 9;
    case 24: case 25: case 27: case 29:     return 4;
    case 28:                                return 5;
    case 26:                                return 11;
    default:                                return 0;
    }
}

/* move_play_sound_ecstatica  E1: ? | E2P: 0x42FCE0 */
void play_sound_ecstatica(actor_t *actor, int sound_index, int volume_flags, int direct_volume) {
    if (!sound_fx_on || !sound_is_on)
        return;

    int dist_volume = 127;

    if (direct_volume) {
        dist_volume = direct_volume;
    } else if (actor && selected_thing && actor != selected_thing) {
        int16_t direction, distance;
        find_direction_and_distance(&direction, &distance,
            actor->position_vector.X - selected_thing->position_vector.X,
            actor->position_vector.Z - selected_thing->position_vector.Z);
        distance += (int16_t)abs(actor->position_vector.Y - selected_thing->position_vector.Y);
        if (volume_flags)
            distance -= 1024;
        if (distance < 0)
            distance = 0;
        dist_volume = (0x2000 - distance) >> 6;
        if (dist_volume > 127) dist_volume = 127;
        if (dist_volume < 0) dist_volume = 0;
    }

    if (dist_volume <= 0)
        return;

    if (game_version == 2 && actor == selected_thing && sound_index == 0) {
        sound_index = find_footstep_sound();
        if (sound_index == 0) return;
    }

    check_sound_loaded(sound_index);
    sound_t *sound = sound_tab[sound_index];
    if (!sound) return;

    if (sound->volume <= 100)
        dist_volume = sound->volume * dist_volume / 100;
    else
        dist_volume = 127;

    start_playing_sample(sound, volume_flags, dist_volume);
}

/* move_see_if_anything_hit_42EFDC — collision detection for combat.
 * Called with a hitting part; iterates all actors and checks if the part
 * intersects.  On hit: sets behavior to GetHit, computes damage, runs
 * hit-code scripts.  Ref: move.c SeeIfAnyhingHit (line 4768). */
void see_if_anything_hit(part_t *part) {
    if (!part || !part->parent_actor) return;

    actor_t *hitter_actor = part->parent_actor;

    /* Determine the holding actor (if part belongs to a held weapon) */
    actor_t *holder = NULL;
    if (hitter_actor->part_heap_link) {
        holder = hitter_actor->part_heap_link->parent_actor;
    }

    DBG_LOG(2, "[HIT] see_if_anything_hit: hitter=%d holder=%d\n",
            hitter_actor->name_index, holder ? holder->name_index : -1);

    for (actor_t *target = root_thing; target; target = target->next_in_display_list) {
        if (target == selected_thing && (target->flags & 0x2000)) {
            DBG_LOG(1, "[HIT] hero 0x2000 set: bh=%d flags=0x%x act=%p aflags=0x%x\n",
                    target->actor_behavior, target->flags,
                    (void*)target->actor_act.act_action,
                    target->actor_act.flags);
        }
        /* Skip self, holder, and ally actors */
        if (target == hitter_actor) continue;
        if (target == holder) continue;
        if (target->name_index == hitter_actor->spawner_index) continue;

        /* Skip if target is held by the hitter */
        if (target->part_heap_link &&
            target->part_heap_link->parent_actor == hitter_actor) continue;

        /* Skip if target has no repertoire or no Interact action */
        if (!target->actor_reperture || target->actor_reperture->action_slots[34] < 0) continue;

        /* Distance check: horizontal */
        int16_t direction, distance;
        find_direction_and_distance(&direction, &distance,
            part->joint_position.X - target->position_vector.X,
            part->joint_position.Z - target->position_vector.Z);
        if (distance >= target->actor_box_size) {
            DBG_LOG(1, "[HIT] SKIP dist: h=%d t=%d d=%d box=%d\n",
                    hitter_actor->name_index, target->name_index, distance, target->actor_box_size);
            continue;
        }

        /* Distance check: vertical */
        int16_t vert = part->joint_position.Y - target->position_vector.Y;
        if (vert <= -2560 || vert >= 512) {
            DBG_LOG(1, "[HIT] SKIP vert: h=%d t=%d v=%d\n",
                    hitter_actor->name_index, target->name_index, vert);
            continue;
        }

        /* Skip dying/dead actors */
        if (target->actor_behavior == BH_DYING || target->actor_behavior == BH_DEAD) {
            DBG_LOG(1, "[HIT] SKIP dead: t=%d\n", target->name_index);
            continue;
        }

        /* Skip actors with CannotBeHit flag (0x2000) */
        if (target->flags & 0x2000) {
            DBG_LOG(1, "[HIT] SKIP 0x2000: t=%d flags=0x%x\n", target->name_index, target->flags);
            continue;
        }

        /* Skip scene-linked actors */
        if (target->actor_scene) {
            DBG_LOG(1, "[HIT] SKIP scene: t=%d\n", target->name_index);
            continue;
        }

        if (target->actor_act.act_action && (target->actor_act.flags & 0x80)) {
            DBG_LOG(1, "[HIT] SKIP prot: t=%d aflags=0x%x\n", target->name_index, target->actor_act.flags);
            continue;
        }

        /* Scene 8 filter: if scene 8 has started, hitter has spawner_index >= 0,
           target has no WalkLeft action (field_2[3] < 0), and target is not hero → skip */
        if ((scene_name_flags[8] & 2) &&
            hitter_actor->spawner_index >= 0 &&
            target->actor_reperture->action_slots[3] < 0 &&
            target != selected_thing) {
            continue;
        }

        /* --- HIT CONFIRMED --- */

        /* Set hit angle and hit type */
        target->hit_angle = arctan(
            target->position_vector.X - hitter_actor->position_vector.X,
            target->position_vector.Z - hitter_actor->position_vector.Z);
        target->hit_type = 1;  /* hit_type = Normal */

        /* Play random hit sound (E2 only — E1 uses keyframe events) */
        if (game_version == 2 && sound_fx_on && sound_is_on) {
            int snd = ((2 * my_rand()) <= 0x8000) ? 13 : 12;
            play_sound_ecstatica(hitter_actor, snd, 0, 0);
        }

        /* Determine the effective hitter.
         * If the hitter is a held puzzle-item (book, cross, etc.) with its own
         * hit-code, use the item so the item's code fires on target.
         * Otherwise use the holder (weapon/fist combat path). */
        actor_t *effective_hitter;
        if (holder && hitter_actor->actor_hit_code >= 0)
            effective_hitter = hitter_actor;  /* puzzle item: use item's code */
        else
            effective_hitter = holder ? holder : hitter_actor;

        DBG_LOG(1, "[HIT] hitter=%d holder=%d effective=%d target=%d\n",
                hitter_actor->name_index,
                holder ? holder->name_index : -1,
                effective_hitter->name_index,
                target->name_index);

        /* Check if hitter has a hit-code script */
        if (effective_hitter->actor_hit_code >= 0) {
            /* Execute hit code on target */
            code_t *code = code_tab[effective_hitter->actor_hit_code];
            if (code) {
                execute_code(code, target);
            } else {
                do_info3_req("Can't find HitCode",
                    thing_names[effective_hitter->name_index].field_0,
                    code_names[effective_hitter->actor_hit_code].field_0);
            }
        } else {
            /* Default hit behavior: set GetHit, apply damage */
            target->actor_behavior = BH_GET_HIT;
            target->flags |= 0x2000;  /* CannotBeHit */

            int damage = 0;

            if (target == selected_thing) {
                /* Hero is being hit */
                if (!no_die) {
                    damage = 5 * effective_hitter->actor_strength_factor / 100;
                }
            } else {
                /* NPC is being hit */
                damage = 40 * effective_hitter->actor_strength_factor / 100;

                actor_t *weapon = NULL;
                part_t *right_arm = NULL;

                if (holder) {
                    /* Part belongs to a weapon held by holder */
                    weapon = part->parent_actor;
                } else {
                    /* Part belongs to the fighter directly; check right hand for weapon */
                    right_arm = hitter_actor->_PartTab ? hitter_actor->_PartTab->field_0[8] : NULL;
                    if (right_arm) {
                        weapon = right_arm->actor_2_held;
                    }
                }

                if (!weapon) {
                    if (right_arm) {
                        /* Hitting with bare hand: halve damage */
                        damage /= 2;
                    }
                    /* else: no arms at all, keep full damage */
                } else {
                    /* Apply weapon bonus */
                    if (weapon->actor_magic) {
                        damage += 20 * (weapon->actor_magic_factor - 100) / 100;
                        adjust_magic(weapon, -1);
                    } else {
                        damage += 20 * (weapon->actor_strength_factor - 100) / 100;
                    }
                }
            }

            /* Apply damage (difficulty scaling for hero) */
            if (target == selected_thing) {
                int scaled;
                if (difficulty == 0) {
                    scaled = 50 * damage / armour_factor;
                } else if (difficulty == 1) {
                    scaled = 100 * damage / armour_factor;
                } else {
                    scaled = 200 * damage / armour_factor;
                }
                target->actor_hitpoints -= (int16_t)scaled;
            } else {
                target->actor_hitpoints -= (int16_t)damage;
            }
        }

        /* Execute target's hp-change code if present */
        if (target->code_at_hp_change >= 0) {
            code_t *code = code_tab[target->code_at_hp_change];
            DBG_LOG(1, "[DEAD] check_hits hp_change: actor='%s' code_idx=%d code=%p hp=%d is_hero=%d\n",
                    thing_names[target->name_index].field_0, target->code_at_hp_change,
                    (void*)code, target->actor_hitpoints, target == selected_thing);
            if (code) {
                execute_code(code, target);
            } else {
                do_info3_req("Can't find GetHitCode",
                    thing_names[target->name_index].field_0,
                    code_names[target->code_at_hp_change].field_0);
            }
        }

        /* Update life bar if hero was hit */
        if (target == selected_thing) {
            draw_life_bar();
        }
    }
}

/* move_look_for_pick_up_42DC9C (E2) / 427128 (E1) — find a pickable item in front of actor */
int look_for_pick_up(actor_t *actor) {
    if (!actor) return 3;

    int fwd_x = ((int)actor->actor_box_size * sine_table[(uint16_t)actor->rotate_vector.Y]) >> 14;
    int fwd_z = ((int)actor->actor_box_size * cosn_table[(uint16_t)actor->rotate_vector.Y]) >> 14;
    int target_x = actor->position_vector.X + fwd_x;
    int target_z = actor->position_vector.Z + fwd_z;

    int pre_filter = (game_version == GAME_VERSION_E1)
        ? 2 * actor->actor_box_size
        : 7 * actor->actor_box_size / 2;

    int best_dist = 0x7FFF;
    actor_t *found_thing = NULL;
    int found_height = 0;

    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        if (!(t->flags & 0x20)) continue;  /* CanBePickedUp */
        if (t == actor) continue;
        if (t->part_heap_link || !t->actor_parts_list) continue;

        int16_t rel_x = t->position_vector.X - target_x;
        int ax = rel_x <= 0 ? -rel_x : rel_x;
        if (ax > pre_filter) continue;

        int16_t rel_z = t->position_vector.Z - target_z;
        int az = rel_z <= 0 ? -rel_z : rel_z;
        if (az > pre_filter) continue;

        int16_t dir, dist;
        find_direction_and_distance(&dir, &dist, rel_x, rel_z);

        if (game_version == GAME_VERSION_E1) {
            if (dist <= actor->actor_box_size && dist < best_dist) {
                found_thing = t;
                best_dist = dist;
                found_height = (int)actor->position_vector.Y
                    - 2 * (int)actor->actor_box_size
                    - (int)t->position_vector.Y;
            }
        } else {
            int16_t rel_h = actor->position_vector.Y - t->actor_parts_list->joint_position.Y;
            if (rel_h <= 700 && rel_h >= -200 &&
                dist <= actor->actor_box_size && dist < best_dist) {
                found_thing = t;
                best_dist = dist;
                found_height = rel_h;
            }
        }
    }

    actor->target_actor = found_thing;
    if (!found_thing)
        return 3;  /* PickUp_Nothing */

    if (game_version == GAME_VERSION_E1) {
        int bs = actor->actor_box_size;
        if (found_thing->flags & 4) {
            if (found_height < -bs) return 5;
            if (found_height <= bs) return 6;
            return 7;
        }
        if (found_height < -bs) return 0;
        if (found_height <= bs) return 1;
        return 2;
    }

    if (found_thing->flags & 4) {  /* Dead */
        if (found_height < 100)  return 5;
        if (found_height <= 400) return 6;
        return 7;
    }
    if (found_height < 100)  return 0;
    if (found_height <= 400) return 1;
    return 2;
}

/* move_look_for_a_fight_42E684 — find nearest enemy in range */
actor_t *look_for_a_fight(actor_t *actor) {
    if (!actor) return NULL;

    int best_angle_diff = 0x10000000;
    actor_t *best = NULL;

    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        if (thing_name_flags[t->name_index] & 4) continue;  /* Dead */
        if (!(t->flags & 0x1000)) continue;                 /* not fightable */
        if (t == actor) continue;

        int dx = t->position_vector.X - actor->position_vector.X;
        int dz = t->position_vector.Z - actor->position_vector.Z;
        int adx = dx <= 0 ? -dx : dx;
        int adz = dz <= 0 ? -dz : dz;
        if (adx >= 1024 || adz >= 1024) continue;

        uint16_t dir = arctan((int16_t)dx, (int16_t)dz);
        int dist;
        if (!dx && !dz) {
            dist = 0;
        } else if (adz <= adx) {
            int s = sine_table[dir];
            if (!s) { dist = 0; } else { dist = (dx << 14) / s; }
        } else {
            int c = cosn_table[dir];
            if (!c) { dist = 0; } else { dist = (dz << 14) / c; }
        }
        if (dist < 0) dist = -dist;
        if (dist >= 4096) continue;

        int angle_diff = (int16_t)(actor->rotate_vector.Y - dir);
        if (angle_diff < 0) angle_diff = -angle_diff;
        if (angle_diff < best_angle_diff) {
            best_angle_diff = angle_diff;
            best = t;
        }
    }
    return best;
}

/* move_find_best_target_up_down_42E7DC — returns 1 (target above), -1 (below), 0 (same level) */
int find_best_target_up_down(actor_t *actor) {
    if (!actor) return 0;

    int best_score = 0x10000000;
    actor_t *best = NULL;

    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        if (!t->actor_reperture) continue;
        if (thing_name_flags[t->name_index] & 4) continue;  /* Dead */
        if (t->actor_reperture->action_slots[34] < 0) continue;  /* no Interact action */
        if (t == actor) continue;

        /* Check if target is a "good" fight target (not 0x200, has KillDown) */
        int is_good = 0;
        if (t->actor_reperture && !(t->flags & 0x0200) &&
            t->actor_reperture->action_slots[9] >= 0)
            is_good = 1;

        int dx = t->position_vector.X - actor->position_vector.X;
        int dz = t->position_vector.Z - actor->position_vector.Z;
        int adx = dx <= 0 ? -dx : dx;
        int adz = dz <= 0 ? -dz : dz;
        if (adx >= 1280 || adz >= 1280) continue;

        uint16_t dir = arctan((int16_t)dx, (int16_t)dz);
        int dist;
        if (!dx && !dz) {
            dist = 0;
        } else if (adz <= adx) {
            int s = sine_table[dir];
            if (!s) { dist = 0; } else { dist = (dx << 14) / s; }
        } else {
            int c = cosn_table[dir];
            if (!c) { dist = 0; } else { dist = (dz << 14) / c; }
        }
        if (dist < 0) dist = -dist;

        int dy = t->position_vector.Y - actor->position_vector.Y;
        int ady = dy <= 0 ? -dy : dy;
        if (dist + ady >= 1280) continue;

        int16_t angle_diff = (int16_t)(dir - actor->rotate_vector.Y);
        if (angle_diff < 0) angle_diff = -angle_diff;
        if (angle_diff >= 0x6000) continue;

        int effective = angle_diff;
        if (!is_good)
            effective += 0x8000;  /* lower priority */
        if (effective < best_score) {
            best_score = effective;
            best = t;
        }
    }

    if (!best) return 0;

    /* Compare head heights */
    part_t *target_head = best->_PartTab ? best->_PartTab->field_0[2] : NULL;
    if (!target_head) target_head = best->actor_parts_list;
    part_t *self_head = actor->_PartTab ? actor->_PartTab->field_0[2] : NULL;
    if (!self_head) return 0;
    if (!target_head) return 0;

    int height_diff = self_head->joint_position.Y - target_head->joint_position.Y;
    if (height_diff > 300)  return 1;   /* target above */
    if (height_diff < -300) return -1;  /* target below */
    return 0;
}

/* move_interpolate_pos_42ED6C — interpolate facing angle toward best target */
void interpolate_pos(actor_t *actor, int game_time, int range) {
    if (!actor) return;

    int best_score = 0x10000000;
    actor_t *best = NULL;
    int16_t best_dir = 0;
    int16_t best_angle_diff = 0;

    for (actor_t *t = root_thing; t; t = t->next_in_display_list) {
        if (!t->actor_reperture) continue;
        if (thing_name_flags[t->name_index] & 4) continue;  /* Dead */
        if (t->actor_reperture->action_slots[34] < 0) continue;  /* no Interact */
        if (t == actor) continue;

        int is_good = 0;
        if (t->actor_reperture && !(t->flags & 0x0200) &&
            t->actor_reperture->action_slots[9] >= 0)
            is_good = 1;

        int dx = t->position_vector.X - actor->position_vector.X;
        int dz = t->position_vector.Z - actor->position_vector.Z;
        int adx = dx <= 0 ? -dx : dx;
        int adz = dz <= 0 ? -dz : dz;
        if (adx >= range || adz >= range) continue;

        uint16_t dir = arctan((int16_t)dx, (int16_t)dz);
        int dist;
        if (!dx && !dz) {
            dist = 0;
        } else if (adz <= adx) {
            int s = sine_table[dir];
            if (!s) { dist = 0; } else { dist = (dx << 14) / s; }
        } else {
            int c = cosn_table[dir];
            if (!c) { dist = 0; } else { dist = (dz << 14) / c; }
        }
        if (dist < 0) dist = -dist;

        int dy = t->position_vector.Y - actor->position_vector.Y;
        int ady = dy <= 0 ? -dy : dy;
        int total = dist + ady;
        if (total >= range) continue;

        /* Must be Visible or within 0x500 */
        if (!(t->flags & 8) && total >= 0x500) continue;

        int16_t angle_diff = (int16_t)(dir - actor->rotate_vector.Y);
        int16_t abs_angle = angle_diff <= 0 ? -angle_diff : angle_diff;
        if (abs_angle >= 0x6000) continue;

        int effective = abs_angle;
        if (!is_good)
            effective += 0x8000;
        if (effective < best_score) {
            best_score = effective;
            best = t;
            best_dir = (int16_t)dir;
            best_angle_diff = (int16_t)(dir - actor->rotate_vector.Y);
        }
    }

    if (!best) return;

    action_t *act = actor->actor_act.act_action;
    if (!act) return;

    int pos;
    if (act->act_duration != 0)
        pos = (game_time << 16) / act->act_duration;
    else
        pos = 0x10000;

    uint16_t remains = 0xFFFF - actor->actor_act.key_progress;
    if (actor->actor_act.key_progress != 0xFFFF && pos <= remains) {
        actor->rotate_vector.Y += (int16_t)(((pos << 14) / remains * best_angle_diff) >> 14);
    } else {
        actor->rotate_vector.Y = best_dir;
    }
}

/* move_find_2_part_rot_z  E1: ? | E2P: 0x42D3F4 */
void find_2_part_rot_z(part_t *part) {
    part_t *second = (part_t *)part->actor_parts_list;
    if (!second) {
        beep_error("2 part limb has no 2nd part");
        return;
    }
    part_t *extremity = (part_t *)second->actor_parts_list;
    if (!extremity) {
        beep_error("2 part limb has no extremity");
        return;
    }

    /* Walk up holding_actor chain */
    part_t *p = extremity;
    while (p->holding_actor) {
        ((part_t *)p->holding_actor)->next_in_path = p;
        p = (part_t *)p->holding_actor;
    }
    extremity->next_in_path = NULL;

    find_positions_on_path(extremity->parent_actor);

    matrix3x3_t matr_int;
    if (part->flags & 0x10) {  /* Loosen */
        matr_int = part->matr_d;
    } else {
        matr_int = part->parent_actor->matrix_1;
    }

    vector_t input;
    input.X = extremity->joint_position.X - part->joint_position.X;
    input.Y = extremity->joint_position.Y - part->joint_position.Y;
    input.Z = extremity->joint_position.Z - part->joint_position.Z;

    matrix3x3_t inv;
    matrix_inverse(&matr_int, &inv);
    vector_t output;
    c_matrix_vector(&output, &inv, &input);

    int16_t ry = arctan(output.X, output.Z);
    if (ry)
        rotate_vector_about_y(&output, -ry);
    int16_t rx = arctan(-output.Y, output.Z);
    if (rx)
        rotate_vector_about_x(&output, -rx);
    if (ry)
        rotate_about_y(&matr_int, ry);
    if (rx)
        rotate_about_x(&matr_int, rx);

    int16_t twist = 0;
    if (part->flags & 0x80) {  /* FollowExtremity */
        input.X = extremity->matrix_1._13 - extremity->matrix_1._12;
        input.Y = extremity->matrix_1._23 - extremity->matrix_1._22;
        input.Z = extremity->matrix_1._33 - extremity->matrix_1._32;
        matrix_inverse(&matr_int, &inv);
        c_matrix_vector(&output, &inv, &input);
        twist = arctan(output.X, -output.Y);
    }

    input.X = part->matrix_1._13;
    input.Y = part->matrix_1._23;
    input.Z = part->matrix_1._33;
    matrix_inverse(&matr_int, &inv);
    c_matrix_vector(&output, &inv, &input);

    part->Rotate.Z = arctan(output.X, -output.Y) - twist;
}

/* move_make_2_part_limb  E1: ? | E2P: 0x42D554 */
void make_2_part_limb(part_t *part) {
    part_t *second = (part_t *)part->actor_parts_list;
    if (!second) {
        beep_error("2 part limb has no 2nd part");
        return;
    }
    part_t *extremity = (part_t *)second->actor_parts_list;
    if (!extremity) {
        beep_error("2 part limb has no extremity");
        return;
    }

    find_2_part_rot_z(part);

    actor_t *actor = part->parent_actor;

    /* Compute extremity position relative to actor */
    vector_t input;
    input.X = extremity->joint_position.X - actor->position_vector.X;
    input.Y = extremity->joint_position.Y - actor->position_vector.Y;
    input.Z = extremity->joint_position.Z - actor->position_vector.Z;

    matrix3x3_t inv;
    matrix_inverse(&actor->matrix_1, &inv);
    vector_t output;
    c_matrix_vector(&output, &inv, &input);

    extremity->AbsPosition.X = output.X;
    extremity->AbsPosition.Y = output.Y;
    extremity->AbsPosition.Z = output.Z;

    matrix3x3_t combined;
    matrix_mult(&inv, &extremity->matrix_1, &combined);
    find_relative_rotations(extremity, &combined);

    part->flags |= 0x20;  /* TwoPartsLimb */
}

/* move_unmake_2_part_limb  E1: ? | E2P: 0x42D61C */
void unmake_2_part_limb(part_t *part) {
    if (!(part->flags & 0x20))
        return;

    part_t *second = (part_t *)part->actor_parts_list;
    if (!second) {
        beep_error("2 part limb has no 2nd part");
        return;
    }
    part_t *extremity = (part_t *)second->actor_parts_list;
    if (!extremity) {
        beep_error("2 part limb has no extremity");
        return;
    }

    /* Walk up holding_actor chain, setting next_in_path */
    part_t *p = extremity;
    while (p->holding_actor) {
        ((part_t *)p->holding_actor)->next_in_path = p;
        p = (part_t *)p->holding_actor;
    }
    extremity->next_in_path = NULL;

    find_positions_on_path(extremity->parent_actor);

    /* Decompose part rotation relative to its parent */
    matrix3x3_t inv, combined;

    matrix_inverse(&((part_t *)part->holding_actor)->matrix_1, &inv);
    matrix_mult(&inv, &part->matrix_1, &combined);
    find_relative_rotations(part, &combined);

    matrix_inverse(&part->matrix_1, &inv);
    matrix_mult(&inv, &second->matrix_1, &combined);
    find_relative_rotations(second, &combined);

    matrix_inverse(&second->matrix_1, &inv);
    matrix_mult(&inv, &extremity->matrix_1, &combined);
    find_relative_rotations(extremity, &combined);

    part->flags &= ~0x20;  /* clear TwoPartsLimb */
}

/* move_blood_spurt_42F85C */
void blood_spurt(part_t *part) {
    if (!part || !part->parent_actor) return;

    /* Chain 1: owner held_by_part. */
    actor_t *owner_actor = part->parent_actor;
    part_t *held_by_part = owner_actor->part_heap_link;
    if (held_by_part) {
        part_t *cursor = held_by_part;
        if (cursor->holding_actor) {
            do {
                part_t *nxt = (part_t *)cursor->holding_actor;
                nxt->next_in_path = cursor;
                cursor = nxt;
            } while (cursor->holding_actor);
        }
        held_by_part->next_in_path = NULL;
        if (held_by_part->parent_actor)
            find_positions_on_path(held_by_part->parent_actor);
    }
    /* Chain 2: from part itself. */
    for (part_t *i = part; i->holding_actor; i = (part_t *)i->holding_actor)
        ((part_t *)i->holding_actor)->next_in_path = i;
    part->next_in_path = NULL;
    find_positions_on_path(part->parent_actor);

    vector_t input = { .X = 0, .Y = 0, .Z = 0x4000 };
    vector_t direction;
    matrix3x3_t m1 = part->matrix_1;
    matrix_vector(&input, &direction, &m1);
    int16_t r1 = (int16_t)my_rand();
    rotate_vector_about_x(&direction, (int16_t)(((2 * r1) - 0x7FFF) >> 8));
    int16_t r2 = (int16_t)my_rand();
    rotate_vector_about_y(&direction, (int16_t)(((2 * r2) - 0x7FFF) >> 8));
    int16_t r3 = (int16_t)my_rand();
    rotate_vector_about_z(&direction, (int16_t)(((2 * r3) - 0x7FFF) >> 8));

    check_actor_loaded_by_index(5);
    check_action_loaded(1);
    if (thing_tab[5] && action_tab[1]) {
        copy_vector(&thing_tab[5]->position_vector, &part->ellipse_center);
        find_relative_rot_vector(&thing_tab[5]->rotate_vector, &part->matrix_1);
        add_to_display_list(thing_tab[5]);
        thing_tab[5]->actor_act.act_action = NULL;
        force_action(thing_tab[5], action_tab[1], 1);
    }
}

/* move_fire_bullet_42F8C4 */
void fire_bullet(part_t *part) {
    if (!part || !part->parent_actor) return;
    int hit_found = 0;

    /* Chain 1: owner held_by_part. */
    actor_t *owner_actor = part->parent_actor;
    part_t *held_by_part = owner_actor->part_heap_link;
    if (held_by_part) {
        part_t *cursor = held_by_part;
        if (cursor->holding_actor) {
            do {
                part_t *nxt = (part_t *)cursor->holding_actor;
                nxt->next_in_path = cursor;
                cursor = nxt;
            } while (cursor->holding_actor);
        }
        held_by_part->next_in_path = NULL;
        if (held_by_part->parent_actor) {
            find_positions_on_path(held_by_part->parent_actor);
            held_by_part->parent_actor->hold_timer = 120;
        }
    }
    /* Chain 2: from part itself. */
    for (part_t *i = part; i->holding_actor; i = (part_t *)i->holding_actor)
        ((part_t *)i->holding_actor)->next_in_path = i;
    part->next_in_path = NULL;
    find_positions_on_path(part->parent_actor);

    vector_t input = { .X = 0, .Y = 0, .Z = 0x4000 };
    vector_t direction;
    matrix3x3_t m1 = part->matrix_1;
    matrix_vector(&input, &direction, &m1);
    int16_t r1 = (int16_t)my_rand();
    rotate_vector_about_x(&direction, (int16_t)(((2 * r1) - 0x7FFF) >> 8));
    int16_t r2 = (int16_t)my_rand();
    rotate_vector_about_y(&direction, (int16_t)(((2 * r2) - 0x7FFF) >> 8));
    int16_t r3 = (int16_t)my_rand();
    rotate_vector_about_z(&direction, (int16_t)(((2 * r3) - 0x7FFF) >> 8));

    int16_t hit_angle_val = arctan(direction.X, direction.Z);
    vector_t pos;
    copy_vector(&pos, &part->ellipse_center);

    direction.X = (int16_t)(((int32_t)direction.X << 7) >> 14);
    direction.Y = (int16_t)(((int32_t)direction.Y << 7) >> 14);
    direction.Z = (int16_t)(((int32_t)direction.Z << 7) >> 14);

    for (int step = 0; step < 20; ++step) {
        vector_t prev = pos;
        pos.X += direction.X;
        pos.Y += direction.Y;
        pos.Z += direction.Z;

        for (actor_t *target = root_thing; target; target = target->next_in_display_list) {
            if (target == part->parent_actor) continue;

            int hittable = 0;
            rephead_t *rep = target->actor_reperture;
            if (rep && rep->action_slots[34 /* Interact */] >= 0) hittable = 1;

            if (hittable) {
                part_t *held = part->parent_actor->part_heap_link;
                if (held && target == held->parent_actor) hittable = 0;
            }

            int b = target->actor_behavior;
            if (b == BH_DYING || b == BH_DEAD
                || (target->flags & 0x2000)  /* CannotBeHit */
                || target->actor_scene)
                hittable = 0;

            if (b == BH_EXTERNAL) hittable = 1;

            if (!hittable) continue;

            int16_t dir, dist;
            find_direction_and_distance(&dir, &dist,
                (int16_t)(pos.X - target->position_vector.X),
                (int16_t)(pos.Z - target->position_vector.Z));
            if (dist >= 192) continue;

            target->hit_angle = hit_angle_val;
            target->hit_type = 2;           /* HitType::Range */
            if (target->actor_behavior != BH_EXTERNAL) {
                target->actor_behavior = BH_GET_HIT;
                target->flags |= 0x2000;     /* CannotBeHit */
                if (target == selected_thing && no_die) {
                    target->actor_hitpoints -= 10;
                } else {
                    target->actor_hitpoints -= 110;
                }
            }
            hit_found = 1;
            break;
        }
        if (hit_found) break;

        if (find_height_now_vis(&pos) < pos.Y) {
            pos = prev;
            direction.X >>= 3; direction.Y >>= 3; direction.Z >>= 3;
            for (int sub_step = 0; sub_step < 8; ++sub_step) {
                pos.X += direction.X; pos.Y += direction.Y; pos.Z += direction.Z;
                if (find_height_now_vis(&pos) < pos.Y) break;
            }
            check_actor_loaded_by_index(4);
            check_action_loaded(0);
            if (thing_tab[4] && action_tab[0]) {
                copy_vector(&thing_tab[4]->position_vector, &pos);
                int16_t r4 = (int16_t)my_rand();
                thing_tab[4]->rotate_vector.Z = 0;
                thing_tab[4]->rotate_vector.Y = (int16_t)(2 * r4);
                thing_tab[4]->rotate_vector.X = 0;
                add_to_display_list(thing_tab[4]);
                thing_tab[4]->actor_act.act_action = NULL;
                force_action(thing_tab[4], action_tab[0], 1);
            }
            return;
        }
    }
}

/* move_check_part_hit  E1: 0x427CE0 | E2: 0x42FA6C */
void check_part_hit(part_t *part) {
    if (part->flags & 0x20) {
        /* TwoPartsLimb: walk 3-part chain */
        part_t *second = (part_t *)part->actor_parts_list;
        part_t *extremity = second ? (part_t *)second->actor_parts_list : NULL;
        if (!extremity) {
            /* Fallback: treat as single part */
            goto single_part;
        }

        part_t *p = extremity;
        while (p->holding_actor) {
            ((part_t *)p->holding_actor)->next_in_path = p;
            p = (part_t *)p->holding_actor;
        }
        extremity->next_in_path = NULL;

        find_positions_on_path(extremity->parent_actor);
        see_if_anything_hit(part);
        see_if_anything_hit(second);
        see_if_anything_hit(extremity);
    } else {
    single_part:
        ;
        part_t *p = part;
        while (p->holding_actor) {
            ((part_t *)p->holding_actor)->next_in_path = p;
            p = (part_t *)p->holding_actor;
        }
        part->next_in_path = NULL;

        find_positions_on_path(part->parent_actor);
        see_if_anything_hit(part);
    }

    /* Check held thing */
    if (part->actor_2_held) {
        find_positions((actor_t *)part->actor_2_held, 0);
        part_t *hp = (part_t *)part->actor_2_held->actor_parts_list;
        for (; hp; hp = hp->next_in_display_list)
            see_if_anything_hit(hp);
    }
}

/* move_check_pick_up  E1: 0x427308 | E2: 0x42DE2C */
void check_pick_up(part_t *part, int16_t param) {
    if (game_version == GAME_VERSION_E1)
        e1_pick_up_hand = 0;

    /* Walk holding_actor chain, set next_in_path */
    part_t *p = part;
    while (p->holding_actor) {
        ((part_t *)p->holding_actor)->next_in_path = p;
        p = (part_t *)p->holding_actor;
    }
    part->next_in_path = NULL;

    find_positions_on_path(part->parent_actor);

    actor_t *target = part->parent_actor->target_actor;
    if (!target) return;

    int16_t dir, dist;
    find_direction_and_distance(&dir, &dist,
        part->joint_position.X - target->position_vector.X,
        part->joint_position.Z - target->position_vector.Z);

    int16_t dy = part->joint_position.Y - target->position_vector.Y;
    if (dy < 0) dy = -dy;
    if (dist > dy) dy = dist;

    /* Drop currently held thing */
    if (part->actor_2_held) {
        hold_thing_with_part(part->actor_2_held, part);
        if (game_version == GAME_VERSION_E1) {
            part->actor_2_held->part_heap_link = NULL;
        } else {
            int16_t ground = find_height_now(&part->actor_2_held->position_vector, NULL);
            int16_t target_y = ground - 30;
            int16_t delta = target_y - part->actor_2_held->position_vector.Y;
            if (delta < 0) delta = -delta;
            if (delta >= 64) {
                copy_vector(&part->actor_2_held->position_vector, &part->parent_actor->position_vector);
                part->actor_2_held->position_vector.Y -= 30;
            } else {
                part->actor_2_held->position_vector.Y = target_y;
            }
            part->actor_2_held->rotate_vector.Z = 0;
            part->actor_2_held->rotate_vector.X = 0;
            part->actor_2_held->part_heap_link = NULL;
        }
    }

    /* Pick up the target */
    part->actor_2_held = target;
    target->part_heap_link = part;

    if (game_version == GAME_VERSION_E1) {
        DBG_LOG(1, "[PICKUP] E1 pickup actor=%d param=%d hand_part=%p\n",
                target->name_index, (int)param, (void *)part);
        if (param && param < CODE_TAB_SIZE && code_tab[param])
            execute_code_with_part(code_tab[param], target, part);
    } else {
        int16_t code_id = target->picked_up_code;
        if (code_id && code_id < CODE_TAB_SIZE && code_tab[code_id])
            execute_code(code_tab[code_id], target);
    }

    if (selected_thing == part->parent_actor) {
        int16_t f = part->parent_actor->action_state;
        if (f == 0)      part->parent_actor->action_state = 3;
        else if (f == 1) part->parent_actor->action_state = 4;
        else if (f == 2) part->parent_actor->action_state = 5;
        else             DBG_LOG(1, "[PICKUP] WARN: action_state=%d (not 0/1/2) no transition (actor=%d)\n",
                                 (int)f, target ? target->name_index : -1);
        DBG_LOG(1, "[PICKUP] action_state: %d → %d (actor=%d part=%p)\n",
                (int)f, (int)part->parent_actor->action_state,
                target ? target->name_index : -1, (void *)part);
        update_game_icons();
    }
}

/* move_check_put_down  E1: 0x42706C | E2: 0x42DAE8 */
void check_put_down(part_t *part, int flags) {
    actor_t *held = part->actor_2_held;
    if (!held) return;

    /* Walk holding_actor chain */
    part_t *p = part;
    while (p->holding_actor) {
        ((part_t *)p->holding_actor)->next_in_path = p;
        p = (part_t *)p->holding_actor;
    }
    part->next_in_path = NULL;

    find_positions_on_path(part->parent_actor);
    hold_thing_with_part(part->actor_2_held, part);

    if (game_version != GAME_VERSION_E1 && !flags) {
        int16_t ground = find_height_now(&held->position_vector, NULL);
        int16_t target_y = ground - 30;
        int16_t delta = target_y - held->position_vector.Y;
        if (delta < 0) delta = -delta;
        if (delta >= 64) {
            copy_vector(&held->position_vector, &part->parent_actor->position_vector);
            held->position_vector.Y -= 30;
        } else {
            held->position_vector.Y = target_y;
        }
        held->rotate_vector.Z = 0;
        held->rotate_vector.X = 0;
    }

    part->actor_2_held->part_heap_link = NULL;
    part->actor_2_held = NULL;

    if (selected_thing == part->parent_actor) {
        int16_t f = part->parent_actor->action_state;
        if (f == 3)      part->parent_actor->action_state = 0;
        else if (f == 4) part->parent_actor->action_state = 1;
        else if (f == 5) part->parent_actor->action_state = 2;
        update_game_icons();
    }
}

/* ══════════════════════════════════════════════════════════════
 *  Additional E2 functions
 * ══════════════════════════════════════════════════════════════ */

/* move_add_act_to_act_list  E1: ? | E2: 0x42CF80 */
void add_act_to_act_list(act_t *act, actor_t *actor) {
    act_t *head = actor->actor_act_list;
    if (!head) {
        actor->actor_act_list = act;
        act->next = NULL;
        return;
    }
    act_t *tail = head;
    while (tail->next)
        tail = tail->next;
    tail->next = act;
    act->next = NULL;
}

/* move_init_act_heap  E1: ? | E2: 0x42D028 */
void init_act_heap(void) {
    for (int i = 0; i < ACT_SIZE; i++)
        act_arr[i].flags = 0;
}

/* move_loosen_joint  E1: ? | E2: 0x42D118 */
void loosen_joint(actor_t *actor) {
    part_t *self = (part_t *)actor;

    if (self->flags & 0x10)  /* already loosened */
        return;

    if (self->actor_parts_list) {
        part_t *walk = self;
        while (walk->actor_parts_list) {
            walk->actor_parts_list->next_in_path = walk;
            walk = walk->actor_parts_list;
        }
    }
    self->next_in_path = NULL;

    find_rotations_on_path(self->parent_actor);
    find_inverse_of_attitude(self->actor_parts_list, &self->field_FE);

    part_t *child = self->actor_parts_list;
    self->matr_d = child->matrix_1;

    self->flags |= 0x10;  /* Loosen */
}

/* move_find_inverse_of_attitude  E1: ? | E2: 0x42D1C4 */
void find_inverse_of_attitude(part_t *part, matrix3x3_t *output) {
    make_identity(output);

    part_t *p = part;
    int found_loosened = 0;

    if (p->actor_parts_list) {
        while (1) {
            if (p->Rotate.Z)
                rotate_about_z(output, -p->Rotate.Z);
            if (p->Rotate.X)
                rotate_about_x(output, -p->Rotate.X);
            if (p->Rotate.Y)
                rotate_about_y(output, -p->Rotate.Y);

            if (p->flags & 0x10) {  /* Loosen */
                matrix3x3_t temp;
                matrix_mult(&temp, output, &p->field_FE);
                *output = temp;
                found_loosened = 1;
                break;
            }
            if (!p->actor_parts_list)
                break;
            p = p->actor_parts_list;
        }
    }

    if (!found_loosened) {
        actor_t *actor = part->parent_actor;
        if (actor->rotate_vector.Z)
            rotate_about_z(output, -actor->rotate_vector.Z);
        if (actor->rotate_vector.X)
            rotate_about_x(output, -actor->rotate_vector.X);
        if (actor->rotate_vector.Y)
            rotate_about_y(output, -actor->rotate_vector.Y);
    }
}

/* move_make_path  E1: ? | E2: 0x42D2BC */
void make_path(part_t *part) {
    if (!part->actor_parts_list)
        goto done;
    {
        part_t *p = part;
        while (p->actor_parts_list) {
            p->actor_parts_list->next_in_path = p;
            p = (part_t *)p->actor_parts_list;
        }
    }
done:
    part->next_in_path = NULL;
}

/* move_fix_part / move_unfix_part  E1: ? | E2: 0x42D8B8 */
void fix_part(void) {
    /* E2 stub — just returns */
}

void unfix_part(void) {
    /* E2 stub — just returns */
}

/* move_reorient_thing  E1: ? | E2: 0x42DAE0 */
void reorient_thing(actor_t *actor) {
    actor->state_flags |= 0x20;
}

/* move_likely_target_up_or_down  E1: ? | E2: 0x42E04C */
void likely_target_up_or_down(actor_t *actor, int *out_dir, int *out_updown) {
    int best_dir = 0;
    int best_updown = 0;

    part_t *hero_part = NULL;
    if (selected_thing)
        hero_part = selected_thing->_PartTab->field_0[2];

    if (!hero_part) {
        *out_dir = 0;
        *out_updown = 0;
        return;
    }

    int best_score = 0x7FFFFFFF;
    actor_t *best_actor = NULL;

    /* Pass 1: non-scenery actors */
    actor_t *thing = root_thing;
    while (thing) {
        if (!(thing_name_flags[thing->name_index] & 4)) {
            part_t *tpart = thing->_PartTab ? thing->_PartTab->field_0[2] : NULL;
            int valid = 0;
            if (tpart && thing != selected_thing && (thing->flags & 0x1000))
                valid = 1;
            if (valid) {
                int16_t dx = (int16_t)(thing->position_vector.X >> 16) -
                             (int16_t)(selected_thing->position_vector.X >> 16);
                int16_t dz = (int16_t)(thing->position_vector.Z >> 16) -
                             (int16_t)(selected_thing->position_vector.Z >> 16);

                int16_t tgt_y = (int16_t)(tpart->ellipse_center.Y >> 16);
                int16_t hero_y = (int16_t)(hero_part->ellipse_center.Y >> 16);
                int y_diff = -(tgt_y - hero_y);
                int abs_y_diff = y_diff < 0 ? -y_diff : y_diff;

                int16_t direction, distance;
                find_dirn_and_dist(&direction, &distance, dx, dz);
                if (abs_y_diff > distance)
                    distance = (int16_t)abs_y_diff;

                int16_t rel_angle = selected_thing->rotate_vector.Y - direction;
                int abs_angle = rel_angle > 0 ? rel_angle : -rel_angle;
                int score;
                if (abs_angle >= 0x4000)
                    score = 0x7FFFFFFF;
                else
                    score = (int)abs_angle * distance;

                if (score < best_score) {
                    best_score = score;
                    best_actor = thing;
                    best_dir = distance / 3;
                    int abs_y_comp = abs_y_diff;
                    if (best_dir < abs_y_comp) {
                        if (abs_y_comp / 2 > best_dir) {
                            best_dir = y_diff < 0 ? 3 : 1;
                        } else {
                            best_dir = y_diff < 0 ? 2 : best_dir;
                            if (y_diff >= 0) best_dir = 1;
                        }
                    } else {
                        best_dir ^= best_dir; /* 0 */
                    }

                    if (y_diff > 0)       best_updown = 1;
                    else if (y_diff > -20) best_updown = 2;
                    else if (y_diff > -40) best_updown = 3;
                    else if (y_diff > -60) best_updown = 4;
                    else                   best_updown = 5;
                }
            }
        }
        thing = (actor_t *)thing->next_in_display_list;
    }

    if (best_actor) {
        *out_dir = best_dir;
        *out_updown = best_updown;
        return;
    }

    /* Pass 2: scenery actors */
    thing = root_thing;
    while (thing) {
        if (thing_name_flags[thing->name_index] & 4) {
            part_t *tpart = thing->_PartTab ? thing->_PartTab->field_0[2] : NULL;
            int valid = 0;
            if (tpart && thing != selected_thing && (thing->flags & 0x1000))
                valid = 1;
            if (valid) {
                int16_t dx = (int16_t)(selected_thing->position_vector.X >> 16) -
                             (int16_t)(thing->position_vector.X >> 16);
                int16_t dz = (int16_t)(thing->position_vector.Z >> 16) -
                             (int16_t)(selected_thing->position_vector.Z >> 16);

                int16_t tgt_y = (int16_t)(tpart->ellipse_center.Y >> 16);
                int16_t hero_y = (int16_t)(hero_part->ellipse_center.Y >> 16);
                int y_diff = -(tgt_y - hero_y);
                int abs_y_diff = y_diff < 0 ? -y_diff : y_diff;

                int16_t direction, distance;
                find_dirn_and_dist(&direction, &distance, dx, dz);
                if (abs_y_diff > distance)
                    distance = (int16_t)abs_y_diff;

                int16_t rel_angle = selected_thing->rotate_vector.Y - direction;
                int abs_angle = rel_angle;
                if (abs_angle < 0) abs_angle = -abs_angle;
                int score;
                if (abs_angle >= 0x4000)
                    score = 0x7FFFFFFF;
                else
                    score = (int)abs_angle * distance;

                if (score < best_score) {
                    best_score = score;
                    best_dir = distance / 3;
                    int abs_y_comp = abs_y_diff;
                    if (best_dir < abs_y_comp) {
                        if (abs_y_comp / 2 > best_dir) {
                            best_dir = y_diff < 0 ? 3 : 1;
                        } else {
                            best_dir = y_diff < 0 ? 2 : best_dir;
                            if (y_diff >= 0) best_dir = 1;
                        }
                    } else {
                        best_dir ^= best_dir;
                    }

                    if (y_diff > 0)       best_updown = 1;
                    else if (y_diff > -20) best_updown = 2;
                    else if (y_diff > -40) best_updown = 3;
                    else if (y_diff > -60) best_updown = 4;
                    else                   best_updown = 5;
                }
            }
        }
        thing = (actor_t *)thing->next_in_display_list;
    }

    *out_dir = best_dir;
    *out_updown = best_updown;
}

/* move_check_fight_rep  E1: ? | E2: 0x42E49C */
void check_fight_rep(actor_t *actor) {
    actor->actor_rep_index = 7;

    actor_t *thing = root_thing;
    while (thing) {
        if (!(thing_name_flags[thing->name_index] & 4) &&
            (thing->flags & 0x1000) &&
            thing != actor)
        {
            int16_t dx = (int16_t)(thing->position_vector.X >> 16) -
                         (int16_t)(actor->position_vector.X >> 16);
            int abs_dx = dx > 0 ? dx : -dx;
            if (abs_dx >= 0x400)
                goto next;

            int16_t dz = (int16_t)(thing->position_vector.Z >> 16) -
                         (int16_t)(actor->position_vector.Z >> 16);
            int abs_dz = dz > 0 ? dz : -dz;
            if (abs_dz >= 0x400)
                goto next;

            uint16_t angle = arctan(dx, dz);
            int distance;
            if (!dx && !dz) {
                distance = 0;
            } else if (abs_dz > abs_dx) {
                int s = sine_table[angle];
                distance = s ? ((int)dz << 14) / s : 0;
            } else {
                int c = cosn_table[angle];
                distance = c ? ((int)dx << 14) / c : 0;
            }
            if (distance < 0) distance = -distance;
            if (distance >= 0x400)
                goto next;

            int16_t rel_angle = actor->rotate_vector.Y - (int16_t)angle;
            int abs_rel = rel_angle > 0 ? (int)rel_angle : -(int)rel_angle;
            if (abs_rel >= 0x4000)
                goto next;

            actor->actor_rep_index = 5;
        }
next:
        thing = (actor_t *)thing->next_in_display_list;
    }
}
