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
#define ZONE_COL_BLOCK   251  /* magenta — blocked cell     */
#define ZONE_COL_MATBLK  250  /* cyan — material-blocked    */
#define ZONE_COL_TERRAIN 249  /* dim grey — terrain cell    */
#define ZONE_COL_TRI     248  /* orange — triangle split    */
#define ZONE_COL_QUAD    247  /* light blue — quadrant split */

static void inject_debug_palette(void) {
    view_cmap[ZONE_COL_ACTION].R = 63; view_cmap[ZONE_COL_ACTION].G =  0; view_cmap[ZONE_COL_ACTION].B =  0;
    view_cmap[ZONE_COL_CAMERA].R =  0; view_cmap[ZONE_COL_CAMERA].G =  0; view_cmap[ZONE_COL_CAMERA].B = 63;
    view_cmap[ZONE_COL_BOTH].R  = 63; view_cmap[ZONE_COL_BOTH].G  = 63; view_cmap[ZONE_COL_BOTH].B  =  0;
    view_cmap[ZONE_COL_SPAWN].R  =  0; view_cmap[ZONE_COL_SPAWN].G  = 63; view_cmap[ZONE_COL_SPAWN].B  =  0;
    view_cmap[ZONE_COL_BLOCK].R  = 63; view_cmap[ZONE_COL_BLOCK].G  =  0; view_cmap[ZONE_COL_BLOCK].B  = 63;
    view_cmap[ZONE_COL_MATBLK].R =  0; view_cmap[ZONE_COL_MATBLK].G = 63; view_cmap[ZONE_COL_MATBLK].B = 63;
    view_cmap[ZONE_COL_TERRAIN].R = 30; view_cmap[ZONE_COL_TERRAIN].G = 30; view_cmap[ZONE_COL_TERRAIN].B = 30;
    view_cmap[ZONE_COL_TRI].R    = 63; view_cmap[ZONE_COL_TRI].G    = 40; view_cmap[ZONE_COL_TRI].B    =  0;
    view_cmap[ZONE_COL_QUAD].R   = 30; view_cmap[ZONE_COL_QUAD].G   = 50; view_cmap[ZONE_COL_QUAD].B   = 63;
}

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

static int cell_has_block_token(int map_elem_idx);
static int cell_has_blocking_material(int map_elem_idx);

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
        int blk_cfg  = map_elements[elem_idx].block_config;
        int mat      = map_elements[elem_idx].material;
        snprintf(buf, sizeof(buf), "Zone:%d Code:%d CAM:%d BC:%d Mat:%d",
                 elem_idx, code_idx, cam_idx, blk_cfg, mat);
        overlay_text_at(4, 4 + tx_h + 1, buf);

        int has_bt = cell_has_block_token(elem_idx);
        int has_bm = cell_has_blocking_material(elem_idx);
        if (has_bt || has_bm) {
            snprintf(buf, sizeof(buf), "BLOCKED: %s%s",
                     has_bt ? "TOKEN " : "", has_bm ? "MAT" : "");
            overlay_text_at(4, 4 + (tx_h + 1) * 2, buf);
        }
    }
}

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

static int cell_has_block_token(int map_elem_idx) {
    int16_t code_idx = map_elements[map_elem_idx].code_index_p1 & 0x3FFF;
    if (code_idx <= 0) return 0;
    code_t *code = code_tab[code_idx - 1];
    if (!code) return 0;
    int token_idx = code->token_store_index;
    if (!token_idx || !token_store) return 0;
    int16_t *token = &token_store[token_idx];
    while (*token) {
        if (*token == CT_BLOCK_ACTOR || *token == CT_BLOCK_WANDERERS ||
            *token == CT_BLOCK_ALL || *token == CT_BLOCK_AQUATIC)
            return 1;
        ++token;
    }
    return 0;
}

static int cell_has_blocking_material(int map_elem_idx) {
    int mat = map_elements[map_elem_idx].material;
    if (mat < 0 || mat >= 30) return 0;
    return (material_flags[mat] & 4) != 0;
}

static void draw_cell_subdivision(int gx, int gz, int16_t gy, int block_config,
                                  uint8_t outline_col) {
    int16_t x0 = (int16_t)((gx - 64) * 512);
    int16_t x1 = (int16_t)((gx - 63) * 512);
    int16_t z0 = (int16_t)((gz - 64) * 512);
    int16_t z1 = (int16_t)((gz - 63) * 512);
    int16_t mx = (int16_t)(x0 + 256);
    int16_t mz = (int16_t)(z0 + 256);

    int cx[6], cy[6], ok[6];
    ok[0] = proj(x0, gy, z0, &cx[0], &cy[0]);
    ok[1] = proj(x1, gy, z0, &cx[1], &cy[1]);
    ok[2] = proj(x1, gy, z1, &cx[2], &cy[2]);
    ok[3] = proj(x0, gy, z1, &cx[3], &cy[3]);
    ok[4] = proj(mx, gy, mz, &cx[4], &cy[4]);
    ok[5] = 0;

    if (block_config == 1) {
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) & 3;
            if (ok[i] && ok[j])
                draw_line(cx[i], cy[i], cx[j], cy[j], outline_col);
        }
        return;
    }

    if (block_config >= 2 && block_config <= 5) {
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) & 3;
            if (ok[i] && ok[j])
                draw_line(cx[i], cy[i], cx[j], cy[j], ZONE_COL_TERRAIN);
        }
        /* bc2: norm_x >= norm_z → diagonal (x0,z0)-(x1,z1), active = (x0z0,x1z0,x1z1)
         * bc3: norm_x <= -norm_z → diagonal (x1,z0)-(x0,z1), active = (x0z0,x1z0,x0z1)
         * bc4: norm_x > -norm_z → same diagonal as 3, active = (x1z0,x1z1,x0z1)
         * bc5: norm_x < norm_z → same diagonal as 2, active = (x0z0,x0z1,x1z1) */
        int d0, d1;
        if (block_config == 2 || block_config == 5) {
            d0 = 0; d1 = 2; /* diagonal corner0-corner2: (x0,z0)-(x1,z1) */
        } else {
            d0 = 1; d1 = 3; /* diagonal corner1-corner3: (x1,z0)-(x0,z1) */
        }
        if (ok[d0] && ok[d1])
            draw_line(cx[d0], cy[d0], cx[d1], cy[d1], ZONE_COL_TRI);

        /* Draw active triangle edges in highlight color */
        int t[3];
        switch (block_config) {
            case 2: t[0]=0; t[1]=1; t[2]=2; break; /* x0z0, x1z0, x1z1 */
            case 3: t[0]=0; t[1]=1; t[2]=3; break; /* x0z0, x1z0, x0z1 */
            case 4: t[0]=1; t[1]=2; t[2]=3; break; /* x1z0, x1z1, x0z1 */
            case 5: t[0]=0; t[1]=3; t[2]=2; break; /* x0z0, x0z1, x1z1 */
            default: return;
        }
        for (int i = 0; i < 3; i++) {
            int j = (i + 1) % 3;
            if (ok[t[i]] && ok[t[j]])
                draw_line(cx[t[i]], cy[t[i]], cx[t[j]], cy[t[j]], ZONE_COL_TRI);
        }
        return;
    }

    if (block_config >= 6) {
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) & 3;
            if (ok[i] && ok[j])
                draw_line(cx[i], cy[i], cx[j], cy[j], ZONE_COL_TERRAIN);
        }
        /* Draw cross through center */
        int smx, smy;
        ok[4] = proj(mx, gy, mz, &smx, &smy);
        int emx[4], emy[4], eok[4];
        eok[0] = proj(mx, gy, z0, &emx[0], &emy[0]); /* top edge mid */
        eok[1] = proj(x1, gy, mz, &emx[1], &emy[1]); /* right edge mid */
        eok[2] = proj(mx, gy, z1, &emx[2], &emy[2]); /* bottom edge mid */
        eok[3] = proj(x0, gy, mz, &emx[3], &emy[3]); /* left edge mid */

        if (eok[0] && eok[2])
            draw_line(emx[0], emy[0], emx[2], emy[2], ZONE_COL_QUAD);
        if (eok[1] && eok[3])
            draw_line(emx[1], emy[1], emx[3], emy[3], ZONE_COL_QUAD);

        /* Highlight active quadrants: bit1=+x-z, bit2=-x-z, bit4=+x+z, bit8=-x+z */
        struct { int bits; int c0; int c1; } quads[4] = {
            {1, 1, 0}, /* +x,-z: corner1(x1,z0) → edge mids top,right */
            {2, 0, 0}, /* -x,-z: corner0(x0,z0) → edge mids left,top  */
            {4, 2, 1}, /* +x,+z: corner2(x1,z1) → edge mids right,bot */
            {8, 3, 3}, /* -x,+z: corner3(x0,z1) → edge mids bot,left  */
        };
        int qedge[4][2] = {{0,1},{3,0},{1,2},{2,3}}; /* edge mid pairs per quadrant */
        for (int q = 0; q < 4; q++) {
            if (!(block_config & quads[q].bits)) continue;
            int ci = quads[q].c0;
            int e0 = qedge[q][0], e1 = qedge[q][1];
            if (ok[ci] && eok[e0])
                draw_line(cx[ci], cy[ci], emx[e0], emy[e0], ZONE_COL_QUAD);
            if (ok[ci] && eok[e1])
                draw_line(cx[ci], cy[ci], emx[e1], emy[e1], ZONE_COL_QUAD);
        }
        return;
    }
}

static void draw_terrain_cells(void) {
    if (!selected_thing) return;

    int pgx = (selected_thing->position_vector.X >> 9) + 64;
    int pgz = (selected_thing->position_vector.Z >> 9) + 64;

    for (int gz = pgz - 10; gz <= pgz + 10; gz++) {
        for (int gx = pgx - 10; gx <= pgx + 10; gx++) {
            if (gz < 0 || gz >= 128 || gx < 0 || gx >= 128) continue;
            uint16_t start = new_map[gz][gx];
            if (start == 0xFFFF || start == 0) continue;

            uint16_t idx = start;
            for (;;) {
                if ((int)idx >= top_of_map_elements) break;
                map_area_element_t *e = &map_elements[idx];
                int16_t gy = (int16_t)((128 - (int)e->def_height) << height_shift);
                int bc = e->block_config;

                uint8_t col = ZONE_COL_TERRAIN;
                if (cell_has_block_token(idx))
                    col = ZONE_COL_BLOCK;
                else if (cell_has_blocking_material(idx))
                    col = ZONE_COL_MATBLK;

                if (bc > 0)
                    draw_cell_subdivision(gx, gz, gy, bc, col);

                if ((uint16_t)e->code_index_p1 & 0x8000) break;
                idx++;
            }
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
    draw_terrain_cells();
}
