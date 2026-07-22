/**
 * debug_overlay.c
 *
 * Visual debug overlay — toggled by Cmd+D.
 * Draws actor labels, player HUD, scene flags, and trigger zones
 * onto the framebuffer just before the page flip.
 */

#include "debug_overlay.h"
#include "display.h"
#include "game.h"
#include "init.h"
#include "map.h"
#include "topo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int16_t debug_overlay_active = 0;

/*
 * Reserved palette entries injected into view_cmap each frame before the flip.
 * VGA 6-bit values (0-63 per channel). Indices 252-255 are almost certainly
 * black/unused in Ecstatica's scene palette.
 */
#define ZONE_COL_ACTION  252  /* red    — action trigger  */
#define ZONE_COL_CAMERA  253  /* blue   — camera zone     */
#define ZONE_COL_BOTH    254  /* yellow — action + camera */
#define ZONE_COL_SPAWN   255  /* green  — wanderer spawn  */

static void inject_debug_palette(void) {
    view_cmap[ZONE_COL_ACTION].R = 63; view_cmap[ZONE_COL_ACTION].G =  0; view_cmap[ZONE_COL_ACTION].B =  0;
    view_cmap[ZONE_COL_CAMERA].R =  0; view_cmap[ZONE_COL_CAMERA].G =  0; view_cmap[ZONE_COL_CAMERA].B = 63;
    view_cmap[ZONE_COL_BOTH].R  = 63; view_cmap[ZONE_COL_BOTH].G  = 63; view_cmap[ZONE_COL_BOTH].B  =  0;
    view_cmap[ZONE_COL_SPAWN].R  =  0; view_cmap[ZONE_COL_SPAWN].G  = 63; view_cmap[ZONE_COL_SPAWN].B  =  0;
}

/* ── Low-level draw helpers ── */

static void draw_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    bitmap[db][y * hires_width + x] = (char)color;
}

static void draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* Project a world point to screen. Returns 1 on success, 0 if near-clipped. */
static int proj(int16_t wx, int16_t wy, int16_t wz, int *sx, int *sy) {
    vector_t world = { .X = wx, .Y = wy, .Z = wz };
    vector_t out;
    view_transform(&out, &world);
    if (out.Z == 0) return 0;
    *sx = screen_centre_x + (out.X >> 4);
    *sy = screen_centre_y + (out.Y >> 4);
    return 1;
}

/* Draw text at absolute screen coords, white on black. */
static void overlay_text_at(int sx, int sy, const char *str) {
    if (!str || !str[0]) return;
    if (sx < 0 || sx >= screen_width || sy < 0 || sy >= screen_height - tx_h)
        return;

    int16_t saved_fg   = a_pen_colour;
    int16_t saved_bg   = b_pen_colour;
    int16_t saved_mode = draw_mode[db];
    int16_t saved_px   = pen_position_x[db];
    int16_t saved_py   = pen_position_y[db];

    a_pen_colour       = 15;
    b_pen_colour       = 0;
    draw_mode[db]      = 2;
    pen_position_x[db] = (int16_t)sx;
    pen_position_y[db] = (int16_t)sy;
    text(db, str, 0);

    a_pen_colour       = saved_fg;
    b_pen_colour       = saved_bg;
    draw_mode[db]      = saved_mode;
    pen_position_x[db] = saved_px;
    pen_position_y[db] = saved_py;
}

/* ── Feature 1+2: player HUD ── */
static void draw_player_hud(void) {
    if (!selected_thing) return;
    char buf[80];
    vector_t *p = &selected_thing->position_vector;

    snprintf(buf, sizeof(buf), "X:%d Y:%d Z:%d CAM:%d",
             (int)p->X, (int)p->Y, (int)p->Z, (int)selected_camera);
    overlay_text_at(4, 4, buf);

    int elem_idx = find_map_element(p);
    if (elem_idx > 0) {
        int code_idx = map_elements[elem_idx].code_index_p1 & 0x3FFF;
        int cam_idx  = map_elements[elem_idx].camera_index;
        snprintf(buf, sizeof(buf), "Zone:%d Code:%d NextCAM:%d",
                 elem_idx, code_idx, cam_idx);
        overlay_text_at(4, 4 + tx_h + 1, buf);
    }
}

/* ── Feature 3: active scene flags ── */
static void draw_active_scene_flags(void) {
    int x = 4, y = 4 + (tx_h + 1) * 2 + tx_h + 2;
    int count = 0;
    overlay_text_at(4, y - tx_h - 1, "Fl:");
    x = 4 + 4 * tx_w;
    for (int i = 0; i < SCENE_TAB_SIZE; i++) {
        if (!(scene_name_flags[i] & 8)) continue;
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", i);
        overlay_text_at(x, y, buf);
        x += (int)strlen(buf) * tx_w + 2;
        if (++count >= 25 || x > screen_width - 30) break;
    }
}

/* ── Feature 4: actor labels at world positions ── */
static void draw_actor_labels(void) {
    for (actor_t *a = root_thing; a; a = a->next_in_display_list) {
        if (!a->name_index || a->name_index >= THING_TAB_SIZE) continue;
        int sx, sy;
        vector_t pos = a->position_vector;
        if (!proj(pos.X, pos.Y, pos.Z, &sx, &sy)) continue;
        sy -= tx_h + 2;
        if (sy < 0) continue;
        const char *name = thing_names ? thing_names[a->name_index].field_0 : "?";
        char buf[36];
        snprintf(buf, sizeof(buf), "%d:%s", (int)a->name_index, name);
        int lw = (int)strlen(buf) * tx_w;
        overlay_text_at(sx - lw / 2, sy, buf);
    }
}

/* ── Feature 5: trigger zone outlines ── */

/* Draw a projected quad outline (4 world corners → screen lines). */
static void draw_cell_outline(int gx, int gz, int16_t gy, uint8_t color) {
    int16_t x0 = (int16_t)((gx - 64) * 512);
    int16_t x1 = (int16_t)((gx - 63) * 512);  /* next cell edge */
    int16_t z0 = (int16_t)((gz - 64) * 512);
    int16_t z1 = (int16_t)((gz - 63) * 512);

    int cx[4], cy[4], ok[4];
    ok[0] = proj(x0, gy, z0, &cx[0], &cy[0]);
    ok[1] = proj(x1, gy, z0, &cx[1], &cy[1]);
    ok[2] = proj(x1, gy, z1, &cx[2], &cy[2]);
    ok[3] = proj(x0, gy, z1, &cx[3], &cy[3]);

    for (int i = 0; i < 4; i++) {
        int j = (i + 1) & 3;
        if (ok[i] && ok[j])
            draw_line(cx[i], cy[i], cx[j], cy[j], color);
    }
}

static void draw_trigger_zones(void) {
    if (!selected_thing) return;

    int pgx = (selected_thing->position_vector.X >> 9) + 64;
    int pgz = (selected_thing->position_vector.Z >> 9) + 64;

    for (int gz = pgz - 10; gz <= pgz + 10; gz++) {
        for (int gx = pgx - 10; gx <= pgx + 10; gx++) {
            if (gz < 0 || gz >= 128 || gx < 0 || gx >= 128) continue;
            uint16_t start = new_map[gz][gx];
            if (start == 0xFFFF || start == 0) continue;

            /* Walk element chain for this cell, accumulate types. */
            int has_action = 0, has_camera = 0, has_spawn = 0;
            int16_t gy = 0;
            uint16_t idx = start;
            for (;;) {
                if ((int)idx >= top_of_map_elements) break;
                map_area_element_t *e = &map_elements[idx];
                if (idx == start)
                    gy = (int16_t)((128 - (int)e->def_height) << height_shift);
                int code_raw = (uint16_t)e->code_index_p1;
                if ((code_raw & 0x3FFF) > 0) has_action = 1;
                if (e->camera_index > 0)      has_camera = 1;
                if (e->wanderer_spawn)         has_spawn  = 1;
                if (code_raw & 0x8000) break;  /* chain terminator */
                idx++;
            }

            if (!has_action&& !has_spawn) continue; //  && !has_camera 

            uint8_t color;
            if (has_action && has_camera) color = ZONE_COL_BOTH;
            else if (has_action)          color = ZONE_COL_ACTION;
            else if (has_camera)          color = ZONE_COL_CAMERA;
            else                          color = ZONE_COL_SPAWN;

            draw_cell_outline(gx, gz, gy, color);
        }
    }
}

void draw_debug_overlay(void) {
    if (!debug_overlay_active || !bitmap[db]) return;
    inject_debug_palette();
    draw_player_hud();
    draw_active_scene_flags();
    draw_actor_labels();
    draw_trigger_zones();
}
