/**
 * render_gl_shaders.h
 *
 * GLSL 330 sources, as string literals.
 *
 * Not data files: the game already has a data-path problem on several targets
 * and shipping loose .glsl files next to the .FAN archives would be a
 * regression.
 *
 * Every one of these ends at a palette index, which is then expanded through
 * view_cmap. Keeping the original's shade_map / shade_tab / palette chain
 * intact — rather than substituting a modern lighting model — is what makes
 * the hardware mode look like Ecstatica rather than like a remake.
 */

#ifndef RENDER_GL_SHADERS_H
#define RENDER_GL_SHADERS_H

#ifdef ECS_ENABLE_GL

/* Shared by every fragment shader: turn a palette index into linear RGB.
 * view_cmap holds 6-bit VGA components, already scaled to 8 bits at upload the
 * same way win.c:60 does it for frame dumps.
 *
 * An ordinary sampler2D, not a usampler2D: the palette is the one table here
 * stored as a normalized GL_RGB8 rather than an integer format, so the sampler
 * type has to match or the fetch is undefined — which shows up as a screen of
 * saturated primaries rather than as an error. The index textures either side
 * of it are GL_R8UI and do use integer samplers. */
#define ECS_GLSL_PALETTE \
    "uniform sampler2D u_palette;\n" \
    "vec3 pal_rgb(uint idx) {\n" \
    "    return texelFetch(u_palette, ivec2(int(idx), 0), 0).rgb;\n" \
    "}\n"

/* Every pass that paints the scene writes two attachments: RGB for display, and
 * the palette index it came from.
 *
 * The index is not a debugging aid — the shadow, smoke and beam modes remap the
 * destination pixel through a table that is index-to-index (SHADOW.DAT, loaded
 * at init.c:723), so an RGB destination could not be run through them at all.
 * Keeping the index alongside the colour is what lets those three modes be
 * exact rather than approximated with a blend factor. */
#define ECS_GLSL_DUAL_OUT \
    "layout(location = 0) out vec4 o_col;\n" \
    "layout(location = 1) out uint o_idx;\n"

/* ── Full-screen pass ──────────────────────────────────────────
 * One triangle covering the viewport, generated from gl_VertexID so there is
 * no vertex buffer to bind. Used by the background and the 2D composite.
 */
static const char *VS_FULLSCREEN =
    "#version 330 core\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
    /* V is flipped because the two conventions disagree: row 0 of an engine
     * plane is the top of the screen, row 0 of a GL framebuffer is the bottom.
     * The 3D passes need no equivalent — build_proj_matrix already negates Y,
     * so engine-down lands at NDC-bottom on its own. */
    "    v_uv = vec2(p.x, 1.0 - p.y);\n"
    "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

/* ── Background ────────────────────────────────────────────────
 * Replaces prepare_parts' background blit and clear_masking's depth restore in
 * one draw. The colour comes from bitmap[3] as palette indices; the depth comes
 * from mask_map[2], which is linear view-space Z in int16.
 *
 * gl_FragDepth converts that linear Z into the same non-linear range
 * build_proj_matrix produces, so geometry drawn afterwards can use ordinary
 * interpolated depth and keep early-Z. This is the single point where the two
 * depth conventions are reconciled.
 */
static const char *FS_BACKGROUND =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    ECS_GLSL_DUAL_OUT
    "uniform usampler2D u_bg_index;\n"
    "uniform isampler2D u_bg_depth;\n"
    "uniform float u_near;\n"
    "uniform float u_far;\n"
    ECS_GLSL_PALETTE
    "void main() {\n"
    "    ivec2 sz = textureSize(u_bg_index, 0);\n"
    "    ivec2 tc = ivec2(v_uv * vec2(sz));\n"
    "    tc = clamp(tc, ivec2(0), sz - 1);\n"
    "    uint idx = texelFetch(u_bg_index, tc, 0).r;\n"
    "    o_col = vec4(pal_rgb(idx), 1.0);\n"
    "    o_idx = idx;\n"
    "    float z = float(texelFetch(u_bg_depth, tc, 0).r);\n"
    /* The engine clears the mask to 0x7FFF for 'nothing here'; anything at or
     * past the far plane must not occlude, so it is pinned to the far end. */
    "    if (z >= u_far || z <= 0.0) { gl_FragDepth = 1.0; return; }\n"
    "    z = max(z, u_near);\n"
    "    float ndc = (u_far + u_near) / (u_far - u_near)\n"
    "              - 2.0 * u_far * u_near / ((u_far - u_near) * z);\n"
    "    gl_FragDepth = clamp(ndc * 0.5 + 0.5, 0.0, 1.0);\n"
    "}\n";

/* ── 2D composite ──────────────────────────────────────────────
 * The engine's menus, subtitles, HUD art and cursor still write bytes into the
 * 8bpp plane; rewriting them would not improve them. So the plane is uploaded
 * and drawn on top, and the test for "did the 2D layer touch this pixel" is a
 * comparison against the pristine background that is already resident as a
 * texture — no sentinel colour, no engine change, no second upload.
 *
 * u_force = 1 for menu and requester screens, where there is no 3D content and
 * the whole plane is the frame.
 */
static const char *FS_COMPOSITE =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "out vec4 o_col;\n"
    "uniform usampler2D u_ui_index;\n"
    "uniform usampler2D u_bg_index;\n"
    "uniform int u_force;\n"
    ECS_GLSL_PALETTE
    "void main() {\n"
    "    ivec2 sz = textureSize(u_ui_index, 0);\n"
    "    ivec2 tc = ivec2(v_uv * vec2(sz));\n"
    "    tc = clamp(tc, ivec2(0), sz - 1);\n"
    "    uint ui = texelFetch(u_ui_index, tc, 0).r;\n"
    "    if (u_force == 0) {\n"
    "        uint bg = texelFetch(u_bg_index, tc, 0).r;\n"
    "        if (ui == bg) discard;\n"
    "    }\n"
    "    o_col = vec4(pal_rgb(ui), 1.0);\n"
    "}\n";

/* ── Flat triangles ────────────────────────────────────────────
 * The palette index is computed on the CPU (render.c face_palette_index), so
 * the fragment stage only expands it. That is deliberate: the original's face
 * shade depends on arctan tables and a fixed-point normal decomposition, and
 * reproducing it exactly costs less on the CPU than approximating it here.
 */
static const char *VS_TRI =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_pos;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "layout(location = 2) in uint a_pal;\n"
    "layout(location = 3) in int  a_layer;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_proj;\n"
    "flat out uint v_pal;\n"
    "flat out int  v_layer;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    v_pal   = a_pal;\n"
    "    v_layer = a_layer;\n"
    "    v_uv    = a_uv;\n"
    "    gl_Position = u_proj * (u_view * vec4(a_pos, 1.0));\n"
    "}\n";

static const char *FS_TRI_FLAT =
    "#version 330 core\n"
    "flat in uint v_pal;\n"
    ECS_GLSL_DUAL_OUT
    ECS_GLSL_PALETTE
    "void main() { o_col = vec4(pal_rgb(v_pal), 1.0); o_idx = v_pal; }\n";

/* Textured faces are unlit in the original: tex_tri_line_win95 (asm_f.c:458)
 * writes the sampled texel straight to the framebuffer with no shade applied.
 * Sampling stays integer/nearest — filtering palette indices would average
 * index 10 with index 200 and produce an unrelated colour. */
static const char *FS_TRI_TEX =
    "#version 330 core\n"
    "flat in int v_layer;\n"
    "in vec2 v_uv;\n"
    ECS_GLSL_DUAL_OUT
    "uniform usampler2DArray u_textures;\n"
    ECS_GLSL_PALETTE
    "void main() {\n"
    "    ivec2 t = ivec2(v_uv) & ivec2(127);\n"
    "    uint idx = texelFetch(u_textures, ivec3(t, v_layer), 0).r;\n"
    "    o_col = vec4(pal_rgb(idx), 1.0);\n"
    "    o_idx = idx;\n"
    "}\n";

/* ── Debug map ─────────────────────────────────────────────────
 * The collision map drawn as geometry in place of the pre-rendered background.
 * Colour comes straight down as RGB rather than through the palette: this is a
 * diagnostic view and its coding has to stay legible whatever palette the
 * current camera loaded. o_idx is written all the same so the shadow and smoke
 * passes still have something coherent to sample.
 */
static const char *VS_MAP =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_pos;\n"
    "layout(location = 1) in vec3 a_rgb;\n"
    "uniform mat4 u_view;\n"
    "uniform mat4 u_proj;\n"
    "out vec3 v_rgb;\n"
    "void main() {\n"
    "    v_rgb = a_rgb;\n"
    "    gl_Position = u_proj * (u_view * vec4(a_pos, 1.0));\n"
    "}\n";

static const char *FS_MAP =
    "#version 330 core\n"
    "in vec3 v_rgb;\n"
    ECS_GLSL_DUAL_OUT
    "void main() { o_col = vec4(v_rgb, 1.0); o_idx = 0u; }\n";

/* ── Ellipsoids ────────────────────────────────────────────────
 * A screen-aligned quad per instance; the fragment shader intersects the view
 * ray against the quadric analytically. Not a tessellated sphere: this gives an
 * exact silhouette at every distance with four vertices, and an exact per-pixel
 * depth, which is what compositing against the background mask needs.
 *
 * The vertex shader works in view space, where the ellipsoid is
 * centre + rot * diag(axes) * u, and emits a quad big enough to cover the
 * bounding sphere.
 */
static const char *VS_ELLIPSOID =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_centre;\n"
    "layout(location = 1) in vec3 a_axes;\n"
    "layout(location = 2) in vec3 a_rot0;\n"     /* rows of the view-space R */
    "layout(location = 3) in vec3 a_rot1;\n"
    "layout(location = 4) in vec3 a_rot2;\n"
    "layout(location = 5) in float a_radius;\n"
    "layout(location = 6) in ivec2 a_style;\n"   /* colour, colour_shade >> 7 */
    "uniform mat4 u_proj;\n"
    "flat out vec3 v_centre;\n"
    "flat out vec3 v_rot0;\n"
    "flat out vec3 v_rot1;\n"
    "flat out vec3 v_rot2;\n"
    "flat out vec3 v_axes;\n"
    "flat out ivec2 v_style;\n"
    "out vec3 v_ray;\n"
    "void main() {\n"
    "    v_centre = a_centre;\n"
    "    v_rot0 = a_rot0; v_rot1 = a_rot1; v_rot2 = a_rot2;\n"
    "    v_axes   = a_axes;\n"
    "    v_style  = a_style;\n"
    "    vec2 corner = vec2((gl_VertexID & 1) == 0 ? -1.0 : 1.0,\n"
    "                       (gl_VertexID & 2) == 0 ? -1.0 : 1.0);\n"
    /* The quad only has to cover the ellipsoid's screen footprint — depth comes
     * from the fragment shader — so it sits at the centre's depth, where it
     * cannot be clipped by the near plane while the centre itself is in front
     * of it. A point on the bounding sphere is at worst radius nearer than the
     * centre, and projecting that back onto the centre plane magnifies its
     * offset by centre.z / (centre.z - radius); this is that bound. */
    "    float denom = max(a_centre.z - a_radius, 1.0);\n"
    "    float ext = a_radius * (a_centre.z + length(a_centre.xy)) / denom;\n"
    "    ext = clamp(ext * 1.05, a_radius, 32767.0);\n"
    "    vec3 vp = vec3(a_centre.xy + corner * ext, a_centre.z);\n"
    "    v_ray = vp;\n"
    "    gl_Position = u_proj * vec4(vp, 1.0);\n"
    "}\n";

static const char *FS_ELLIPSOID =
    "#version 330 core\n"
    "flat in vec3 v_centre;\n"
    "flat in vec3 v_rot0;\n"
    "flat in vec3 v_rot1;\n"
    "flat in vec3 v_rot2;\n"
    "flat in vec3 v_axes;\n"
    "flat in ivec2 v_style;\n"
    "in vec3 v_ray;\n"
    ECS_GLSL_DUAL_OUT
    "uniform usampler2D u_shade_map;\n"
    "uniform usampler3D u_shade_tab;\n"
    "uniform float u_near;\n"
    "uniform float u_far;\n"
    "uniform int u_moving_camera;\n"
    "uniform int u_enhanced_light;\n"
    ECS_GLSL_PALETTE
    "void main() {\n"
    "    mat3 Rt = mat3(v_rot0, v_rot1, v_rot2);\n"  /* columns = rows of R */
    "    vec3 d = normalize(v_ray);\n"
    /* Ellipsoid is  p = c + R * diag(axes) * u  with |u| = 1, so the inverse
     * map is  u = (R^T (p - c)) / axes.  Rt's columns are R's rows, which makes
     * Rt itself R transpose, so Rt * v applies the inverse rotation. */
    "    vec3 o2 = -(Rt * v_centre) / v_axes;\n"
    "    vec3 d2 =  (Rt * d) / v_axes;\n"
    "    float a = dot(d2, d2);\n"
    "    float b = 2.0 * dot(o2, d2);\n"
    "    float c = dot(o2, o2) - 1.0;\n"
    "    float disc = b * b - 4.0 * a * c;\n"
    "    if (disc < 0.0) discard;\n"           /* the exact silhouette */
    "    float sq = sqrt(disc);\n"
    "    float t = (-b - sq) / (2.0 * a);\n"
    "    if (t <= 0.0) t = (-b + sq) / (2.0 * a);\n"
    "    if (t <= 0.0) discard;\n"
    "    vec3 u = o2 + t * d2;\n"
    "    vec3 p = t * d;\n"                    /* view-space hit; eye at origin */
    "    if (p.z < u_near) discard;\n"
    "    float ndc = (u_far + u_near) / (u_far - u_near)\n"
    "              - 2.0 * u_far * u_near / ((u_far - u_near) * p.z);\n"
    "    gl_FragDepth = clamp(ndc * 0.5 + 0.5, 0.0, 1.0);\n"
    /* Surface normal: grad of |M^-1 (p-c)|^2 is  R * (u / axes)  up to scale.
     * Multiplying from the right by Rt applies R, the transpose of Rt.
     *
     * The original looks shade_map up in the projected disc frame, which makes
     * its lighting screen-fixed rather than world-fixed — a part rotating in
     * place does not change shade. The view-space normal reproduces that for a
     * sphere exactly and generalises it to a true ellipsoid surface, which the
     * column sweep could only approximate. Enhanced lighting instead uses the
     * object-space direction, so parts light consistently as they turn. */
    "    vec3 n = normalize((u / v_axes) * Rt);\n"
    "    vec2 sc = (u_enhanced_light != 0) ? normalize(u).xy : n.xy;\n"
    "    ivec2 smp = ivec2(clamp(sc * 64.0 + 64.0, vec2(0.0), vec2(127.0)));\n"
    "    uint sm = texelFetch(u_shade_map, smp, 0).r;\n"
    "    int shade = int(sm & 0x7Fu);\n"
    /* ellipse.c:133 — the fog band base depends on whether the camera is in
     * motion; getting this wrong makes parts jump brightness on a cut.
     * colour_shade arrives pre-shifted by 7 so the product stays in range. */
    "    int zi = int(p.z);\n"
    "    int depth_shade = (u_moving_camera != 0) ? (159 - (zi >> 5))\n"
    "                                             : (191 - (zi >> 7));\n"
    "    depth_shade = clamp(depth_shade, 0, 127);\n"
    "    int band = clamp((v_style.y * depth_shade) >> 7, 0, 127);\n"
    "    uint idx = texelFetch(u_shade_tab,\n"
    "                          ivec3(shade, band, v_style.x), 0).r;\n"
    "    o_col = vec4(pal_rgb(idx), 1.0);\n"
    "    o_idx = idx;\n"
    "}\n";

/* ── Shadows, smoke and beams ──────────────────────────────────
 * These three do not paint a surface. They take whatever is already in the
 * framebuffer and remap it through a table, which is why they need the scene's
 * palette index and depth as textures rather than as the bound attachments.
 *
 * The conditions come straight from the span routines in asm_f.c, expressed on
 * the two roots of the same quadric the solid pass uses:
 *
 *   smoke  (asm_f.c:425)  near <= scene            -> table 0
 *   shadow (asm_f.c:386)  near <= scene <= far     -> table 1
 *   beam   (asm_f.c:346)  near <= scene            -> table 2 if also <= far,
 *                                                     else table 0
 *
 * "scene" is the depth already there, so an ordinary depth test cannot express
 * any of them — shadow in particular needs the surface to be *inside* the
 * ellipsoid, which is a test against both roots at once.
 */
static const char *FS_ELL_MODULATE =
    "#version 330 core\n"
    "flat in vec3 v_centre;\n"
    "flat in vec3 v_rot0;\n"
    "flat in vec3 v_rot1;\n"
    "flat in vec3 v_rot2;\n"
    "flat in vec3 v_axes;\n"
    "flat in ivec2 v_style;\n"
    "in vec3 v_ray;\n"
    ECS_GLSL_DUAL_OUT
    "uniform usampler2D u_scene_index;\n"
    "uniform sampler2D  u_scene_depth;\n"
    "uniform usampler3D u_shadow_tab;\n"   /* 256 x 16 x 3 */
    "uniform float u_near;\n"
    "uniform float u_far;\n"
    "uniform int u_mode;\n"                /* 1 shadow, 2 smoke, 3 beam */
    ECS_GLSL_PALETTE
    "void main() {\n"
    "    mat3 Rt = mat3(v_rot0, v_rot1, v_rot2);\n"
    "    vec3 d = normalize(v_ray);\n"
    "    vec3 o2 = -(Rt * v_centre) / v_axes;\n"
    "    vec3 d2 =  (Rt * d) / v_axes;\n"
    "    float a = dot(d2, d2);\n"
    "    float b = 2.0 * dot(o2, d2);\n"
    "    float c = dot(o2, o2) - 1.0;\n"
    "    float disc = b * b - 4.0 * a * c;\n"
    "    if (disc < 0.0) discard;\n"
    "    float sq = sqrt(disc);\n"
    "    float t_near = (-b - sq) / (2.0 * a);\n"
    "    float t_far  = (-b + sq) / (2.0 * a);\n"
    "    if (t_far <= 0.0) discard;\n"
    "    float z_near = (t_near * d).z;\n"
    "    float z_far  = (t_far  * d).z;\n"
    /* Recover the scene's view-space Z from the stored depth — the inverse of
     * the mapping build_proj_matrix and the background pass both apply. */
    "    ivec2 tc = ivec2(gl_FragCoord.xy);\n"
    "    float dv = texelFetch(u_scene_depth, tc, 0).r;\n"
    "    float ndc = dv * 2.0 - 1.0;\n"
    "    float denom = (u_far + u_near) - ndc * (u_far - u_near);\n"
    "    if (denom <= 0.0) discard;\n"
    "    float z_scene = 2.0 * u_far * u_near / denom;\n"
    "    if (z_near > z_scene) discard;\n"
    "    int table;\n"
    "    if (u_mode == 1) {\n"
    "        if (z_far < z_scene) discard;\n"
    "        table = 1;\n"
    "    } else if (u_mode == 2) {\n"
    "        table = 0;\n"
    "    } else {\n"
    "        table = (z_far >= z_scene) ? 2 : 0;\n"
    "    }\n"
    "    uint src = texelFetch(u_scene_index, tc, 0).r;\n"
    "    uint idx = texelFetch(u_shadow_tab,\n"
    "                          ivec3(int(src), v_style.x, table), 0).r;\n"
    "    o_col = vec4(pal_rgb(idx), 1.0);\n"
    "    o_idx = idx;\n"
    "}\n";

#endif /* ECS_ENABLE_GL */
#endif /* RENDER_GL_SHADERS_H */
