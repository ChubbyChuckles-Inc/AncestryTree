#include "render.h"

#include "render_internal.h"

#include "camera_controller.h"
#include "layout.h"
#include "person.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include "render_labels.h"
#include <raymath.h>
#include <rlgl.h>
#endif

#ifndef DEG2RAD
#define DEG2RAD (0.01745329251994329577f)
#endif

static RenderColor render_color_make(unsigned char r, unsigned char g, unsigned char b,
                                     unsigned char a)
{
    RenderColor color = {r, g, b, a};
    return color;
}

static void render_copy_color(RenderColor *dst, const RenderColor *src)
{
    if (!dst || !src)
    {
        return;
    }
    memcpy(dst, src, sizeof(RenderColor));
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static void render_update_selection_particles(RenderState *state, const LayoutResult *layout,
                                              const Camera3D *camera, const Person *selected_person,
                                              float delta_seconds);
static void render_draw_selection_particles(RenderState *state, const Camera3D *camera);
#else
static void render_update_selection_particles(RenderState *state, const LayoutResult *layout,
                                              const Camera3D *camera, const Person *selected_person,
                                              float delta_seconds)
{
    (void)state;
    (void)layout;
    (void)camera;
    (void)selected_person;
    (void)delta_seconds;
}

static void render_draw_selection_particles(RenderState *state, const Camera3D *camera)
{
    (void)state;
    (void)camera;
}
#endif

static void render_set_default_config(RenderConfig *config)
{
    if (!config)
    {
        return;
    }
    config->sphere_radius                   = 0.6f;
    config->glow_intensity                  = 0.85f;
    config->glow_min_strength               = 0.35f;
    config->glow_pulse_speed                = 1.4f;
    config->connection_radius               = 0.05f;
    config->connection_antialiasing         = true;
    config->connection_style_parent_child   = RENDER_CONNECTION_STYLE_BEZIER;
    config->connection_style_spouse         = RENDER_CONNECTION_STYLE_STRAIGHT;
    config->alive_color                     = render_color_make(0, 195, 255, 255);
    config->deceased_color                  = render_color_make(200, 120, 240, 255);
    config->selected_outline_color          = render_color_make(255, 255, 255, 255);
    config->connection_color_parent_child   = render_color_make(64, 200, 255, 200);
    config->connection_color_spouse         = render_color_make(255, 180, 120, 200);
    config->show_connections                = true;
    config->show_grid                       = true;
    config->show_overlay                    = true;
    config->show_name_panels                = true;
    config->show_profile_images             = true;
    config->name_panel_font_size            = 26.0f;
    config->enable_frustum_culling          = true;
    config->enable_lod                      = true;
    config->lod_near_distance               = 14.0f;
    config->lod_far_distance                = 42.0f;
    config->culling_margin                  = 1.2f;
    config->show_background_gradient        = true;
    config->background_gradient_top         = render_color_make(10, 30, 64, 255);
    config->background_gradient_bottom      = render_color_make(2, 8, 20, 255);
    config->enable_fog                      = true;
    config->fog_start_distance              = 18.0f;
    config->fog_end_distance                = 78.0f;
    config->fog_color                       = render_color_make(8, 20, 42, 255);
    config->enable_rim_lighting             = true;
    config->rim_intensity                   = 1.1f;
    config->rim_power                       = 2.6f;
    config->rim_color                       = render_color_make(100, 240, 255, 255);
    config->enable_selection_particles      = true;
    config->selection_particle_capacity     = 48U;
    config->selection_particle_lifetime     = 1.6f;
    config->selection_particle_speed_min    = 1.8f;
    config->selection_particle_speed_max    = 3.6f;
    config->selection_particle_repeat_delay = 2.4f;
}

void render_state_init(RenderState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    render_set_default_config(&state->config);
    state->render_width                       = 0;
    state->render_height                      = 0;
    state->render_target_ready                = false;
    state->visible_nodes                      = NULL;
    state->visibility_capacity                = 0U;
    state->visible_node_count                 = 0U;
    state->visibility_mask_ready              = false;
    state->selection_particles_last_person    = NULL;
    state->selection_particles_last_origin[0] = 0.0f;
    state->selection_particles_last_origin[1] = 0.0f;
    state->selection_particles_last_origin[2] = 0.0f;
    state->selection_particles_timer          = 0.0f;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    state->glow_shader_ready      = false;
    state->glow_intensity_loc     = -1;
    state->glow_time_loc          = -1;
    state->glow_min_strength_loc  = -1;
    state->glow_pulse_speed_loc   = -1;
    state->glow_rim_intensity_loc = -1;
    state->glow_rim_power_loc     = -1;
    state->glow_rim_color_loc     = -1;
    state->glow_rim_enable_loc    = -1;
    state->ambient_strength       = 0.2f;
    state->light_direction[0]     = -0.3f;
    state->light_direction[1]     = -1.0f;
    state->light_direction[2]     = -0.2f;
    state->scene_target.id        = 0;
    state->label_system           = NULL;
    state->label_system_ready     = false;
    state->active_camera          = NULL;
#endif
}

RenderConfig render_config_default(void)
{
    RenderConfig config;
    render_set_default_config(&config);
    return config;
}

bool render_config_validate(const RenderConfig *config)
{
    if (!config)
    {
        return false;
    }
    if (!(config->sphere_radius > 0.0f))
    {
        return false;
    }
    if (!(config->glow_intensity >= 0.0f))
    {
        return false;
    }
    if (!(config->glow_min_strength >= 0.0f && config->glow_min_strength <= 1.0f))
    {
        return false;
    }
    if (!(config->glow_pulse_speed > 0.0f))
    {
        return false;
    }
    if (!(config->connection_radius >= 0.0f))
    {
        return false;
    }
    if (config->connection_style_parent_child < RENDER_CONNECTION_STYLE_STRAIGHT ||
        config->connection_style_parent_child > RENDER_CONNECTION_STYLE_BEZIER)
    {
        return false;
    }
    if (config->connection_style_spouse < RENDER_CONNECTION_STYLE_STRAIGHT ||
        config->connection_style_spouse > RENDER_CONNECTION_STYLE_BEZIER)
    {
        return false;
    }
    if (!(config->name_panel_font_size >= 1.0f))
    {
        return false;
    }
    if (config->enable_lod)
    {
        if (!(config->lod_near_distance > 0.0f))
        {
            return false;
        }
        if (!(config->lod_far_distance > config->lod_near_distance))
        {
            return false;
        }
    }
    if (!(config->culling_margin >= 0.0f))
    {
        return false;
    }
    if (config->enable_fog)
    {
        if (!(config->fog_start_distance >= 0.0f))
        {
            return false;
        }
        if (!(config->fog_end_distance > config->fog_start_distance))
        {
            return false;
        }
    }
    if (!(config->rim_intensity >= 0.0f))
    {
        return false;
    }
    if (!(config->rim_power >= 0.5f))
    {
        return false;
    }
    if (config->enable_selection_particles)
    {
        if (config->selection_particle_capacity == 0U)
        {
            return false;
        }
        if (!(config->selection_particle_lifetime > 0.0f))
        {
            return false;
        }
        if (!(config->selection_particle_speed_min > 0.0f))
        {
            return false;
        }
        if (!(config->selection_particle_speed_max >= config->selection_particle_speed_min))
        {
            return false;
        }
        if (!(config->selection_particle_repeat_delay >= 0.0f))
        {
            return false;
        }
    }
    return true;
}

static bool render_set_error(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0U)
    {
        return false;
    }
    const char *text = message ? message : "render error";
#if defined(_MSC_VER)
    (void)strncpy_s(buffer, buffer_size, text, _TRUNCATE);
#else
    (void)snprintf(buffer, buffer_size, "%s", text);
#endif
    return false;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static const char *render_glow_vertex_shader(void)
{
#if defined(GRAPHICS_API_OPENGL_33)
    return "#version 330\n"
           "in vec3 vertexPosition;\n"
           "in vec2 vertexTexCoord;\n"
           "in vec3 vertexNormal;\n"
           "in vec4 vertexColor;\n"
           "out vec2 fragTexCoord;\n"
           "out vec4 fragColor;\n"
           "out vec3 fragNormal;\n"
           "out vec3 fragPosition;\n"
           "uniform mat4 mvp;\n"
           "uniform mat4 matModel;\n"
           "uniform mat4 matNormal;\n"
           "void main(){\n"
           "    fragTexCoord = vertexTexCoord;\n"
           "    fragColor = vertexColor;\n"
           "    fragNormal = (matNormal * vec4(vertexNormal, 0.0)).xyz;\n"
           "    fragPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;\n"
           "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
           "}";
#else
    return "#version 100\n"
           "attribute vec3 vertexPosition;\n"
           "attribute vec2 vertexTexCoord;\n"
           "attribute vec3 vertexNormal;\n"
           "attribute vec4 vertexColor;\n"
           "varying vec2 fragTexCoord;\n"
           "varying vec4 fragColor;\n"
           "varying vec3 fragNormal;\n"
           "varying vec3 fragPosition;\n"
           "uniform mat4 mvp;\n"
           "uniform mat4 matModel;\n"
           "uniform mat4 matNormal;\n"
           "void main(){\n"
           "    fragTexCoord = vertexTexCoord;\n"
           "    fragColor = vertexColor;\n"
           "    fragNormal = (matNormal * vec4(vertexNormal, 0.0)).xyz;\n"
           "    fragPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;\n"
           "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
           "}";
#endif
}

static const char *render_glow_fragment_shader(void)
{
#if defined(GRAPHICS_API_OPENGL_33)
    return "#version 330\n"
           "in vec2 fragTexCoord;\n"
           "in vec4 fragColor;\n"
           "in vec3 fragNormal;\n"
           "in vec3 fragPosition;\n"
           "out vec4 finalColor;\n"
           "uniform float glowIntensity;\n"
           "uniform float timeSeconds;\n"
           "uniform float glowMinStrength;\n"
           "uniform float glowPulseSpeed;\n"
           "uniform float rimIntensity;\n"
           "uniform float rimPower;\n"
           "uniform vec3 rimColor;\n"
           "uniform int enableRim;\n"
           "uniform vec3 cameraPosition;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    float minStrength = clamp(glowMinStrength, 0.0, 1.0);\n"
           "    float speed = max(glowPulseSpeed, 0.05);\n"
           "    float wobble = minStrength + (1.0 - minStrength) * (0.5 + 0.5 * sin(timeSeconds * "
           "speed));\n"
           "    vec3 emissive = base.rgb * glow * wobble;\n"
           "    vec3 color = base.rgb + emissive;\n"
           "    if (enableRim != 0) {\n"
           "        vec3 normal = normalize(fragNormal);\n"
           "        vec3 viewDir = normalize(cameraPosition - fragPosition);\n"
           "        float ndotv = max(0.0, dot(normal, viewDir));\n"
           "        float rimTerm = pow(clamp(1.0 - ndotv, 0.0, 1.0), max(rimPower, 0.0001));\n"
           "        color += clamp(rimIntensity, 0.0, 8.0) * rimTerm * rimColor;\n"
           "    }\n"
           "    color = clamp(color, 0.0, 1.0);\n"
           "    finalColor = vec4(color, base.a);\n"
           "}";
#else
    return "#version 100\n"
           "precision mediump float;\n"
           "varying vec2 fragTexCoord;\n"
           "varying vec4 fragColor;\n"
           "varying vec3 fragNormal;\n"
           "varying vec3 fragPosition;\n"
           "uniform float glowIntensity;\n"
           "uniform float timeSeconds;\n"
           "uniform float glowMinStrength;\n"
           "uniform float glowPulseSpeed;\n"
           "uniform float rimIntensity;\n"
           "uniform float rimPower;\n"
           "uniform vec3 rimColor;\n"
           "uniform int enableRim;\n"
           "uniform vec3 cameraPosition;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    float minStrength = clamp(glowMinStrength, 0.0, 1.0);\n"
           "    float speed = max(glowPulseSpeed, 0.05);\n"
           "    float wobble = minStrength + (1.0 - minStrength) * (0.5 + 0.5 * sin(timeSeconds * "
           "speed));\n"
           "    vec3 emissive = base.rgb * glow * wobble;\n"
           "    vec3 color = base.rgb + emissive;\n"
           "    if (enableRim != 0) {\n"
           "        vec3 normal = normalize(fragNormal);\n"
           "        vec3 viewDir = normalize(cameraPosition - fragPosition);\n"
           "        float ndotv = max(0.0, dot(normal, viewDir));\n"
           "        float rimTerm = pow(clamp(1.0 - ndotv, 0.0, 1.0), max(rimPower, 0.0001));\n"
           "        color += clamp(rimIntensity, 0.0, 8.0) * rimTerm * rimColor;\n"
           "    }\n"
           "    color = clamp(color, 0.0, 1.0);\n"
           "    gl_FragColor = vec4(color, base.a);\n"
           "}";
#endif
}

static Color render_color_to_raylib(RenderColor color)
{
    Color result = {color.r, color.g, color.b, color.a};
    return result;
}

static Color render_color_apply_intensity(Color color, float intensity)
{
    if (intensity < 0.0f)
    {
        intensity = 0.0f;
    }
    if (intensity > 1.0f)
    {
        intensity = 1.0f;
    }
    Color result = color;
    float bias   = 5.0f;
    result.r     = (unsigned char)fminf(255.0f, color.r * intensity + bias);
    result.g     = (unsigned char)fminf(255.0f, color.g * intensity + bias);
    result.b     = (unsigned char)fminf(255.0f, color.b * intensity + bias);
    return result;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static Color render_apply_fog_color(const RenderState *state, Color color, float distance);
static void render_draw_segment(RenderState *state, const Vector3 *a, const Vector3 *b,
                                RenderColor color)
{
    if (!state)
    {
        return;
    }
    Color ray_color = render_color_to_raylib(color);
    float radius    = state->config.connection_radius;
    if (state->active_camera)
    {
        Vector3 midpoint   = Vector3Scale(Vector3Add(*a, *b), 0.5f);
        float fog_distance = Vector3Distance(state->active_camera->position, midpoint);
        ray_color          = render_apply_fog_color(state, ray_color, fog_distance);
    }

    if (radius > 0.0f)
    {
        float distance = Vector3Distance(*a, *b);
        float scale    = 1.0f + fminf(distance * 0.12f, 2.5f);
        float adjusted = radius * scale;
        DrawCylinderEx(*a, *b, adjusted, adjusted, 18, ray_color);
        return;
    }

    if (state->config.connection_antialiasing)
    {
        rlEnableSmoothLines();
        float width = 1.6f + 0.15f * Vector3Distance(*a, *b);
        if (width > 6.0f)
        {
            width = 6.0f;
        }
        rlSetLineWidth(width);
        DrawLine3D(*a, *b, ray_color);
        rlSetLineWidth(1.0f);
        rlDisableSmoothLines();
        return;
    }

    DrawLine3D(*a, *b, ray_color);
}

static void render_evaluate_cubic(const Vector3 *p0, const Vector3 *p1, const Vector3 *p2,
                                  const Vector3 *p3, float t, Vector3 *result)
{
    float it  = 1.0f - t;
    float b0  = it * it * it;
    float b1  = 3.0f * it * it * t;
    float b2  = 3.0f * it * t * t;
    float b3  = t * t * t;
    result->x = b0 * p0->x + b1 * p1->x + b2 * p2->x + b3 * p3->x;
    result->y = b0 * p0->y + b1 * p1->y + b2 * p2->y + b3 * p3->y;
    result->z = b0 * p0->z + b1 * p1->z + b2 * p2->z + b3 * p3->z;
}

static void render_draw_connection_straight(RenderState *state, const float start[3],
                                            const float end[3], RenderColor color)
{
    Vector3 a = {start[0], start[1], start[2]};
    Vector3 b = {end[0], end[1], end[2]};
    render_draw_segment(state, &a, &b, color);
}

static void render_draw_connection_bezier(RenderState *state, const float start[3],
                                          const float end[3], RenderColor color)
{
    if (!state)
    {
        return;
    }

    Vector3 p0        = {start[0], start[1], start[2]};
    Vector3 p3        = {end[0], end[1], end[2]};
    Vector3 direction = Vector3Subtract(p3, p0);
    float distance    = Vector3Length(direction);
    if (distance < 0.0001f)
    {
        render_draw_segment(state, &p0, &p3, color);
        return;
    }

    float arc_height = fmaxf(0.6f, distance * 0.25f);
    Vector3 up       = {0.0f, 1.0f, 0.0f};
    Vector3 right    = Vector3CrossProduct(direction, up);
    if (Vector3Length(right) < 0.0001f)
    {
        right = Vector3Normalize((Vector3){1.0f, 0.0f, 0.0f});
    }
    else
    {
        right = Vector3Normalize(right);
    }
    float lateral_offset = distance * 0.12f;
    Vector3 p1           = Vector3Add(Vector3Add(p0, Vector3Scale(direction, 0.18f)),
                                      Vector3Scale(right, lateral_offset));
    p1.y += arc_height * 0.6f;
    Vector3 p2 = Vector3Add(Vector3Add(p0, Vector3Scale(direction, 0.68f)),
                            Vector3Scale(right, -lateral_offset));
    p2.y += arc_height * 0.6f;

    const size_t segment_count = 16U;
    Vector3 previous           = p0;
    for (size_t index = 1U; index <= segment_count; ++index)
    {
        float t = (float)index / (float)segment_count;
        Vector3 current;
        render_evaluate_cubic(&p0, &p1, &p2, &p3, t, &current);
        render_draw_segment(state, &previous, &current, color);
        previous = current;
    }
}

static void render_draw_connection(RenderState *state, const float start[3], const float end[3],
                                   RenderColor color, RenderConnectionStyle style)
{
    if (style == RENDER_CONNECTION_STYLE_BEZIER)
    {
        render_draw_connection_bezier(state, start, end, color);
    }
    else
    {
        render_draw_connection_straight(state, start, end, color);
    }
}
#else
static void render_draw_connection(RenderState *state, const float start[3], const float end[3],
                                   RenderColor color, RenderConnectionStyle style)
{
    (void)state;
    (void)start;
    (void)end;
    (void)color;
    (void)style;
}
#endif

static void render_apply_lighting(const RenderState *state)
{
    (void)state;
    // Placeholder for future custom lighting. raylib uses fixed function lights by default.
}

static void render_draw_holographic_overlay(const RenderState *state)
{
    (void)state;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    int screen_width  = GetScreenWidth();
    int screen_height = GetScreenHeight();
    const Color tint  = {12, 20, 38, 72};
    DrawRectangle(0, 0, screen_width, screen_height, tint);

    const Color horizontal = {0, 220, 255, 36};
    const Color vertical   = {0, 160, 255, 22};
    for (int y = 24; y < screen_height; y += 40)
    {
        DrawLine(0, y, screen_width, y, horizontal);
    }
    for (int x = 0; x < screen_width; x += 64)
    {
        DrawLine(x, 0, x, screen_height, vertical);
    }
#endif
}

static void render_release_render_target(RenderState *state)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state)
    {
        return;
    }
    if (state->render_target_ready)
    {
        UnloadRenderTexture(state->scene_target);
        state->scene_target.id     = 0;
        state->render_target_ready = false;
    }
#else
    (void)state;
#endif
}

static bool render_ensure_batch_capacity(RenderState *state, size_t required_capacity)
{
    if (!state)
    {
        return false;
    }
    if (required_capacity == 0U)
    {
        return true;
    }
    if (state->batch_capacity >= required_capacity && state->batch_alive_nodes &&
        state->batch_deceased_nodes)
    {
        return true;
    }

    size_t new_capacity          = required_capacity + (required_capacity / 2U) + 4U;
    const LayoutNode **alive_new = (const LayoutNode **)malloc(new_capacity * sizeof(*alive_new));
    const LayoutNode **deceased_new =
        (const LayoutNode **)malloc(new_capacity * sizeof(*deceased_new));
    if (!alive_new || !deceased_new)
    {
        free(alive_new);
        free(deceased_new);
        return false;
    }

    free((void *)state->batch_alive_nodes);
    free((void *)state->batch_deceased_nodes);
    state->batch_alive_nodes    = alive_new;
    state->batch_deceased_nodes = deceased_new;
    state->batch_capacity       = new_capacity;
    return true;
}

static bool render_ensure_visibility_capacity(RenderState *state, size_t required_count)
{
    if (!state)
    {
        return false;
    }
    if (required_count == 0U)
    {
        return true;
    }
    if (state->visible_nodes && state->visibility_capacity >= required_count)
    {
        return true;
    }

    size_t new_capacity = required_count + (required_count / 2U) + 4U;
    unsigned char *mask = (unsigned char *)malloc(new_capacity * sizeof(*mask));
    if (!mask)
    {
        free(state->visible_nodes);
        state->visible_nodes       = NULL;
        state->visibility_capacity = 0U;
        return false;
    }

    free(state->visible_nodes);
    state->visible_nodes       = mask;
    state->visibility_capacity = new_capacity;
    return true;
}

static void render_visibility_mark_all_visible(RenderState *state, size_t count)
{
    if (!state)
    {
        return;
    }
    if (!state->visible_nodes || state->visibility_capacity < count)
    {
        state->visibility_mask_ready = false;
        state->visible_node_count    = count;
        return;
    }
    memset(state->visible_nodes, 1, count * sizeof(*state->visible_nodes));
    state->visibility_mask_ready = true;
    state->visible_node_count    = count;
}

static bool render_node_visible_by_index(const RenderState *state, size_t index)
{
    if (!state || !state->visibility_mask_ready || !state->visible_nodes)
    {
        return true;
    }
    if (index >= state->visibility_capacity)
    {
        return true;
    }
    return state->visible_nodes[index] != 0U;
}

static bool render_is_node_visible(const RenderState *state, const LayoutResult *layout,
                                   const LayoutNode *node)
{
    if (!state || !layout || !node)
    {
        return false;
    }
    size_t index = (size_t)(node - layout->nodes);
    if (index >= layout->count)
    {
        return false;
    }
    return render_node_visible_by_index(state, index);
}

static bool render_find_person_position_with_index(const LayoutResult *layout, const Person *person,
                                                   float out_position[3], size_t *out_index)
{
    if (!layout || !person)
    {
        return false;
    }
    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        if (node->person == person)
        {
            if (out_position)
            {
                out_position[0] = node->position[0];
                out_position[1] = node->position[1];
                out_position[2] = node->position[2];
            }
            if (out_index)
            {
                *out_index = index;
            }
            return true;
        }
    }
    return false;
}

static int render_select_sphere_segments(const RenderState *state, float distance)
{
    if (!state || !state->config.enable_lod)
    {
        return 32;
    }
    float near_distance = state->config.lod_near_distance;
    float far_distance  = state->config.lod_far_distance;
    if (!(far_distance > near_distance) || distance <= near_distance)
    {
        return 32;
    }
    if (distance >= far_distance)
    {
        return 16;
    }
    float midpoint = (near_distance + far_distance) * 0.5f;
    if (distance <= midpoint)
    {
        return 24;
    }
    return 20;
}

static Color render_apply_fog_color(const RenderState *state, Color color, float distance)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state || !state->config.enable_fog)
    {
        return color;
    }
    float start = state->config.fog_start_distance;
    float end   = state->config.fog_end_distance;
    if (!(end > start))
    {
        return color;
    }
    float t = (distance - start) / (end - start);
    if (t <= 0.0f)
    {
        return color;
    }
    Color fog = render_color_to_raylib(state->config.fog_color);
    if (t >= 1.0f)
    {
        return fog;
    }
    Color result;
    result.r = (unsigned char)fminf(
        255.0f, fmaxf(0.0f, (float)color.r + ((float)fog.r - (float)color.r) * t));
    result.g = (unsigned char)fminf(
        255.0f, fmaxf(0.0f, (float)color.g + ((float)fog.g - (float)color.g) * t));
    result.b = (unsigned char)fminf(
        255.0f, fmaxf(0.0f, (float)color.b + ((float)fog.b - (float)color.b) * t));
    result.a = (unsigned char)fminf(
        255.0f, fmaxf(0.0f, (float)color.a + ((float)fog.a - (float)color.a) * t));
    return result;
#else
    (void)state;
    (void)distance;
    return color;
#endif
}

static void render_draw_background_gradient(const RenderState *state)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state || !state->config.show_background_gradient)
    {
        return;
    }
    int width  = state->render_width;
    int height = state->render_height;
    if (width <= 0 || height <= 0)
    {
        width  = GetScreenWidth();
        height = GetScreenHeight();
    }
    if (width <= 0 || height <= 0)
    {
        return;
    }
    Rectangle bounds = {0.0f, 0.0f, (float)width, (float)height};
    Color top        = render_color_to_raylib(state->config.background_gradient_top);
    Color bottom     = render_color_to_raylib(state->config.background_gradient_bottom);
    DrawRectangleGradientEx(bounds, top, top, bottom, bottom);
#else
    (void)state;
#endif
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static void render_color_as_vec3(RenderColor color, float out_values[3])
{
    if (!out_values)
    {
        return;
    }
    out_values[0] = (float)color.r / 255.0f;
    out_values[1] = (float)color.g / 255.0f;
    out_values[2] = (float)color.b / 255.0f;
}

static void render_apply_rim_overlay(const RenderState *state, const Vector3 *position,
                                     float radius, float distance, int segments)
{
    if (!state || !position || !state->config.enable_rim_lighting)
    {
        return;
    }
    float rim_strength = state->config.rim_intensity;
    if (!(rim_strength > 0.0f))
    {
        return;
    }
    Color rim_color  = render_color_to_raylib(state->config.rim_color);
    rim_color        = render_apply_fog_color(state, rim_color, distance);
    int rim_segments = segments;
    if (rim_segments < 12)
    {
        rim_segments = 12;
    }
    float rim_radius = radius * (1.02f + fminf(rim_strength, 4.0f) * 0.06f);
    BeginBlendMode(BLEND_ADD_COLORS);
    rlDisableDepthMask();
    DrawSphereEx(*position, rim_radius, rim_segments, rim_segments, rim_color);
    rlEnableDepthMask();
    EndBlendMode();
}

static void render_reset_selection_particle_state(RenderState *state)
{
    if (!state)
    {
        return;
    }
    selection_particles_reset(&state->selection_particles);
    state->selection_particles_last_person    = NULL;
    state->selection_particles_last_origin[0] = 0.0f;
    state->selection_particles_last_origin[1] = 0.0f;
    state->selection_particles_last_origin[2] = 0.0f;
    state->selection_particles_timer          = 0.0f;
}

static void render_update_selection_particles(RenderState *state, const LayoutResult *layout,
                                              const Camera3D *camera, const Person *selected_person,
                                              float delta_seconds)
{
    if (!state)
    {
        return;
    }
    SelectionParticleSystem *system = &state->selection_particles;
    if (!state->config.enable_selection_particles || !system->particles || system->capacity == 0U)
    {
        render_reset_selection_particle_state(state);
        return;
    }

    if (delta_seconds > 0.0f)
    {
        selection_particles_update(system, delta_seconds);
    }

    if (!selected_person || !layout)
    {
        state->selection_particles_last_person = NULL;
        return;
    }

    float origin[3];
    if (!render_find_person_position(layout, selected_person, origin))
    {
        state->selection_particles_last_person = NULL;
        return;
    }

    bool selection_changed             = selected_person != state->selection_particles_last_person;
    const float displacement_threshold = fmaxf(0.02f, state->config.sphere_radius * 0.25f);
    float displacement_sq              = 0.0f;
    if (!selection_changed)
    {
        float dx        = origin[0] - state->selection_particles_last_origin[0];
        float dy        = origin[1] - state->selection_particles_last_origin[1];
        float dz        = origin[2] - state->selection_particles_last_origin[2];
        displacement_sq = dx * dx + dy * dy + dz * dz;
    }

    bool trigger_burst = false;
    if (selection_changed)
    {
        selection_particles_reset(system);
        trigger_burst                    = true;
        state->selection_particles_timer = 0.0f;
    }
    else
    {
        state->selection_particles_timer += delta_seconds;
        if (displacement_sq > displacement_threshold * displacement_threshold)
        {
            trigger_burst                    = true;
            state->selection_particles_timer = 0.0f;
        }
        else if (state->config.selection_particle_repeat_delay <= 0.0f)
        {
            if (selection_particles_active_count(system) == 0U)
            {
                trigger_burst                    = true;
                state->selection_particles_timer = 0.0f;
            }
        }
        else if (state->selection_particles_timer >= state->config.selection_particle_repeat_delay)
        {
            trigger_burst                    = true;
            state->selection_particles_timer = 0.0f;
        }
    }

    if (trigger_burst)
    {
        selection_particles_trigger_burst(system, origin,
                                          state->config.selection_particle_speed_min,
                                          state->config.selection_particle_speed_max);
    }

    state->selection_particles_last_person    = selected_person;
    state->selection_particles_last_origin[0] = origin[0];
    state->selection_particles_last_origin[1] = origin[1];
    state->selection_particles_last_origin[2] = origin[2];

    (void)camera;
}

static void render_draw_selection_particles(RenderState *state, const Camera3D *camera)
{
    if (!state || !state->config.enable_selection_particles)
    {
        return;
    }
    const SelectionParticleSystem *system = &state->selection_particles;
    if (!system->particles || system->capacity == 0U)
    {
        return;
    }
    size_t active = selection_particles_active_count(system);
    if (active == 0U)
    {
        return;
    }

    Color base_color = render_color_to_raylib(state->config.selected_outline_color);
    BeginBlendMode(BLEND_ADD_COLORS);
    rlDisableDepthMask();
    for (size_t index = 0U; index < system->capacity; ++index)
    {
        const SelectionParticle *particle = &system->particles[index];
        if (!particle->active)
        {
            continue;
        }
        float life_fraction = 0.0f;
        if (particle->lifetime > 0.0f)
        {
            life_fraction = particle->age / particle->lifetime;
            if (life_fraction > 1.0f)
            {
                life_fraction = 1.0f;
            }
        }
        float fade       = 1.0f - life_fraction;
        float radius     = fmaxf(0.1f, state->config.sphere_radius * (0.18f + 0.32f * fade));
        Color draw_color = base_color;
        float alpha      = (float)base_color.a * (0.35f + 0.55f * fade);
        if (alpha > 255.0f)
        {
            alpha = 255.0f;
        }
        draw_color.a     = (unsigned char)alpha;
        Vector3 position = {particle->position[0], particle->position[1], particle->position[2]};
        float distance   = camera ? Vector3Distance(camera->position, position) : 0.0f;
        draw_color       = render_apply_fog_color(state, draw_color, distance);
        DrawSphereEx(position, radius, 10, 10, draw_color);
    }
    rlEnableDepthMask();
    EndBlendMode();
}

static void render_configure_glow_shader(RenderState *state, const Camera3D *camera)
{
    if (!state || !state->glow_shader_ready)
    {
        return;
    }
    /* Manual check: orbit the camera around a populated tree and confirm the pulse speed and rim
     * highlights stay consistent. */
    float glow_value = state->config.glow_intensity;
    SetShaderValue(state->glow_shader, state->glow_intensity_loc, &glow_value,
                   SHADER_UNIFORM_FLOAT);
    if (state->glow_time_loc >= 0)
    {
        float time_seconds = (float)GetTime();
        SetShaderValue(state->glow_shader, state->glow_time_loc, &time_seconds,
                       SHADER_UNIFORM_FLOAT);
    }
    if (state->glow_min_strength_loc >= 0)
    {
        float min_strength = state->config.glow_min_strength;
        if (min_strength < 0.0f)
        {
            min_strength = 0.0f;
        }
        if (min_strength > 1.0f)
        {
            min_strength = 1.0f;
        }
        SetShaderValue(state->glow_shader, state->glow_min_strength_loc, &min_strength,
                       SHADER_UNIFORM_FLOAT);
    }
    if (state->glow_pulse_speed_loc >= 0)
    {
        float pulse_speed = state->config.glow_pulse_speed;
        if (!(pulse_speed > 0.0f))
        {
            pulse_speed = 0.1f;
        }
        SetShaderValue(state->glow_shader, state->glow_pulse_speed_loc, &pulse_speed,
                       SHADER_UNIFORM_FLOAT);
    }
    if (state->glow_rim_intensity_loc >= 0)
    {
        float rim_intensity = fmaxf(0.0f, state->config.rim_intensity);
        SetShaderValue(state->glow_shader, state->glow_rim_intensity_loc, &rim_intensity,
                       SHADER_UNIFORM_FLOAT);
    }
    if (state->glow_rim_power_loc >= 0)
    {
        float rim_power = state->config.rim_power;
        if (!(rim_power >= 0.0f))
        {
            rim_power = 1.0f;
        }
        SetShaderValue(state->glow_shader, state->glow_rim_power_loc, &rim_power,
                       SHADER_UNIFORM_FLOAT);
    }
    if (state->glow_rim_color_loc >= 0)
    {
        float rim_color_vec[3];
        render_color_as_vec3(state->config.rim_color, rim_color_vec);
        SetShaderValue(state->glow_shader, state->glow_rim_color_loc, rim_color_vec,
                       SHADER_UNIFORM_VEC3);
    }
    if (state->glow_rim_enable_loc >= 0)
    {
        int enable_rim = state->config.enable_rim_lighting ? 1 : 0;
        SetShaderValue(state->glow_shader, state->glow_rim_enable_loc, &enable_rim,
                       SHADER_UNIFORM_INT);
    }
    if (state->glow_shader.locs[SHADER_LOC_VECTOR_VIEW] >= 0)
    {
        float view_position[3] = {0.0f, 0.0f, 0.0f};
        if (camera)
        {
            view_position[0] = camera->position.x;
            view_position[1] = camera->position.y;
            view_position[2] = camera->position.z;
        }
        SetShaderValue(state->glow_shader, state->glow_shader.locs[SHADER_LOC_VECTOR_VIEW],
                       view_position, SHADER_UNIFORM_VEC3);
    }
}
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static bool render_update_visibility(RenderState *state, const LayoutResult *layout,
                                     const Camera3D *camera)
{
    if (!state || !layout)
    {
        return false;
    }

    size_t count = layout->count;
    if (count == 0U)
    {
        state->visibility_mask_ready = true;
        state->visible_node_count    = 0U;
        return true;
    }

    if (!render_ensure_visibility_capacity(state, count))
    {
        state->visibility_mask_ready = false;
        state->visible_node_count    = count;
        return false;
    }

    if (!state->visible_nodes)
    {
        state->visibility_mask_ready = false;
        state->visible_node_count    = count;
        return false;
    }

    if (!state->config.enable_frustum_culling || !camera)
    {
        render_visibility_mark_all_visible(state, count);
        return true;
    }

    float aspect = 1.7777f;
    if (state->render_width > 0 && state->render_height > 0)
    {
        aspect = (float)state->render_width / (float)state->render_height;
    }
    else
    {
        int screen_width  = GetScreenWidth();
        int screen_height = GetScreenHeight();
        if (screen_width > 0 && screen_height > 0)
        {
            aspect = (float)screen_width / (float)screen_height;
        }
    }
    if (!(aspect > 0.0f))
    {
        aspect = 1.7777f;
    }

    Vector3 forward = Vector3Subtract(camera->target, camera->position);
    if (Vector3Length(forward) < 0.0001f)
    {
        forward = (Vector3){0.0f, 0.0f, -1.0f};
    }
    else
    {
        forward = Vector3Normalize(forward);
    }

    Vector3 up = camera->up;
    if (Vector3Length(up) < 0.0001f)
    {
        up = (Vector3){0.0f, 1.0f, 0.0f};
    }
    else
    {
        up = Vector3Normalize(up);
    }

    Vector3 right = Vector3CrossProduct(forward, up);
    if (Vector3Length(right) < 0.0001f)
    {
        right = (Vector3){1.0f, 0.0f, 0.0f};
    }
    else
    {
        right = Vector3Normalize(right);
    }

    up = Vector3Normalize(Vector3CrossProduct(right, forward));

    float fovy_radians = camera->fovy * DEG2RAD;
    if (fovy_radians < 0.001f)
    {
        fovy_radians = 60.0f * DEG2RAD;
    }
    float tan_half_fovy = tanf(fovy_radians * 0.5f);
    float tan_half_fovx = tan_half_fovy * aspect;

    float near_plane = 0.1f;
    float far_plane  = 1500.0f;

    unsigned char *mask = state->visible_nodes;
    float base_radius   = state->config.sphere_radius;
    if (!(base_radius > 0.0f))
    {
        base_radius = 0.6f;
    }
    float radius_margin = base_radius * state->config.culling_margin + 0.35f;

    size_t visible = 0U;
    for (size_t index = 0; index < count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        Vector3 position       = {node->position[0], node->position[1], node->position[2]};
        Vector3 to_node        = Vector3Subtract(position, camera->position);
        float forward_distance = Vector3DotProduct(to_node, forward);
        bool visible_node      = true;

        if (camera->projection == CAMERA_PERSPECTIVE)
        {
            if (forward_distance <= 0.0f)
            {
                visible_node = false;
            }
            else if (forward_distance < near_plane - radius_margin ||
                     forward_distance > far_plane + radius_margin)
            {
                visible_node = false;
            }
            else
            {
                float horizontal       = Vector3DotProduct(to_node, right);
                float vertical         = Vector3DotProduct(to_node, up);
                float horizontal_limit = forward_distance * tan_half_fovx + radius_margin;
                float vertical_limit   = forward_distance * tan_half_fovy + radius_margin;
                if (fabsf(horizontal) > horizontal_limit || fabsf(vertical) > vertical_limit)
                {
                    visible_node = false;
                }
            }
        }
        else
        {
            float half_height = camera->fovy * 0.5f;
            float half_width  = half_height * aspect;
            float horizontal  = Vector3DotProduct(to_node, right);
            float vertical    = Vector3DotProduct(to_node, up);
            float depth       = Vector3DotProduct(to_node, forward);
            if (fabsf(horizontal) > half_width + radius_margin ||
                fabsf(vertical) > half_height + radius_margin)
            {
                visible_node = false;
            }
            if (fabsf(depth) > far_plane + radius_margin)
            {
                visible_node = false;
            }
        }

        mask[index] = (unsigned char)(visible_node ? 1U : 0U);
        if (visible_node)
        {
            ++visible;
        }
    }

    state->visibility_mask_ready = true;
    state->visible_node_count    = visible;
    return true;
}
#else
static bool render_update_visibility(RenderState *state, const LayoutResult *layout,
                                     const void *camera)
{
    (void)camera;
    if (!state || !layout)
    {
        return false;
    }
    if (!render_ensure_visibility_capacity(state, layout->count))
    {
        state->visibility_mask_ready = false;
        state->visible_node_count    = layout->count;
        return false;
    }
    render_visibility_mark_all_visible(state, layout->count);
    return true;
}
#endif

static void render_draw_sphere(RenderState *state, const LayoutResult *layout,
                               const LayoutNode *node, bool is_alive, bool is_selected,
                               bool is_hovered, const Camera3D *camera)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!render_is_node_visible(state, layout, node))
    {
        return;
    }
    Vector3 position  = {node->position[0], node->position[1], node->position[2]};
    float base_radius = state->config.sphere_radius;
    if (is_hovered && !is_selected)
    {
        base_radius *= 1.08f;
    }
    if (is_selected)
    {
        base_radius *= 1.2f;
    }
    Color base_color =
        render_color_to_raylib(is_alive ? state->config.alive_color : state->config.deceased_color);

    Vector3 light_dir = {state->light_direction[0], state->light_direction[1],
                         state->light_direction[2]};
    if (Vector3Length(light_dir) < 0.0001f)
    {
        light_dir = Vector3Normalize((Vector3){-0.3f, -1.0f, -0.2f});
    }
    else
    {
        light_dir = Vector3Normalize(light_dir);
    }
    Vector3 to_camera = {0.0f, 0.0f, 1.0f};
    if (camera)
    {
        to_camera = Vector3Normalize(Vector3Subtract(camera->position, position));
    }
    Vector3 light_to_surface = Vector3Scale(light_dir, -1.0f);
    float diffuse            = Vector3DotProduct(light_to_surface, to_camera);
    if (diffuse < 0.0f)
    {
        diffuse = 0.0f;
    }
    float intensity = state->ambient_strength + (1.0f - state->ambient_strength) * diffuse;
    base_color      = render_color_apply_intensity(base_color, intensity);

    float camera_distance = 0.0f;
    if (camera)
    {
        camera_distance = Vector3Distance(camera->position, position);
    }
    base_color   = render_apply_fog_color(state, base_color, camera_distance);
    int segments = render_select_sphere_segments(state, camera_distance);
    if (segments < 12)
    {
        segments = 12;
    }

    bool use_shader = state->glow_shader_ready;
    if (use_shader)
    {
        render_configure_glow_shader(state, camera);
        BeginShaderMode(state->glow_shader);
    }
    DrawSphereEx(position, base_radius, segments, segments, base_color);
    if (use_shader)
    {
        EndShaderMode();
    }
    else
    {
        render_apply_rim_overlay(state, &position, base_radius, camera_distance, segments);
    }

    if (is_alive)
    {
        Color halo            = base_color;
        halo.a                = (unsigned char)((int)halo.a / 3);
        float scale_seed      = 0.35f + state->config.glow_min_strength * 0.5f;
        float hover_bonus     = is_hovered ? 0.2f : 0.0f;
        float intensity_scale = 1.0f + state->config.glow_intensity * (scale_seed + hover_bonus);
        float glow_radius     = base_radius * intensity_scale;
        int halo_segments     = segments > 16 ? segments - 8 : segments;
        if (halo_segments < 12)
        {
            halo_segments = 12;
        }
        halo = render_apply_fog_color(state, halo, camera_distance);
        DrawSphereEx(position, glow_radius, halo_segments, halo_segments, halo);
    }

    if (is_selected)
    {
        Color outline = render_color_to_raylib(state->config.selected_outline_color);
        outline       = render_apply_fog_color(state, outline, camera_distance);
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawSphereWires(position, base_radius * 1.02f, 18, 18, outline);
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }
#else
    (void)state;
    (void)layout;
    (void)node;
    (void)is_alive;
    (void)is_selected;
    (void)is_hovered;
    (void)camera;
#endif
}

static void render_draw_sphere_group(RenderState *state, const LayoutResult *layout,
                                     const LayoutNode *const *nodes, size_t count, bool is_alive,
                                     const Camera3D *camera)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state || !nodes || count == 0U || !camera)
    {
        return;
    }

    float base_radius = state->config.sphere_radius;
    if (!(base_radius > 0.0f))
    {
        base_radius = 0.6f;
    }

    Vector3 light_dir = {state->light_direction[0], state->light_direction[1],
                         state->light_direction[2]};
    if (Vector3Length(light_dir) < 0.0001f)
    {
        light_dir = Vector3Normalize((Vector3){-0.3f, -1.0f, -0.2f});
    }
    else
    {
        light_dir = Vector3Normalize(light_dir);
    }
    Vector3 light_to_surface = Vector3Scale(light_dir, -1.0f);

    Color base_color =
        render_color_to_raylib(is_alive ? state->config.alive_color : state->config.deceased_color);
    bool use_shader = state->glow_shader_ready && is_alive;
    if (use_shader)
    {
        render_configure_glow_shader(state, camera);
        BeginShaderMode(state->glow_shader);
    }

    for (size_t index = 0; index < count; ++index)
    {
        const LayoutNode *node = nodes[index];
        const Person *person   = node ? node->person : NULL;
        if (!node || !person)
        {
            continue;
        }
        if (!render_is_node_visible(state, layout, node))
        {
            continue;
        }
        Vector3 position  = {node->position[0], node->position[1], node->position[2]};
        Vector3 to_camera = {0.0f, 0.0f, 1.0f};
        if (camera)
        {
            to_camera = Vector3Normalize(Vector3Subtract(camera->position, position));
        }
        float diffuse = Vector3DotProduct(light_to_surface, to_camera);
        if (diffuse < 0.0f)
        {
            diffuse = 0.0f;
        }
        float intensity = state->ambient_strength + (1.0f - state->ambient_strength) * diffuse;
        Color lit_color = render_color_apply_intensity(base_color, intensity);
        float distance  = Vector3Distance(camera->position, position);
        lit_color       = render_apply_fog_color(state, lit_color, distance);
        int segments    = render_select_sphere_segments(state, distance);
        if (segments < 12)
        {
            segments = 12;
        }
        DrawSphereEx(position, base_radius, segments, segments, lit_color);
        if (!use_shader)
        {
            render_apply_rim_overlay(state, &position, base_radius, distance, segments);
        }
    }

    if (use_shader)
    {
        EndShaderMode();
    }

    if (is_alive)
    {
        float scale_seed      = 0.35f + state->config.glow_min_strength * 0.45f;
        float intensity_scale = 1.0f + state->config.glow_intensity * scale_seed;
        float glow_radius     = base_radius * intensity_scale;
        for (size_t index = 0; index < count; ++index)
        {
            const LayoutNode *node = nodes[index];
            const Person *person   = node ? node->person : NULL;
            if (!node || !person)
            {
                continue;
            }
            if (!render_is_node_visible(state, layout, node))
            {
                continue;
            }
            Vector3 position  = {node->position[0], node->position[1], node->position[2]};
            Vector3 to_camera = {0.0f, 0.0f, 1.0f};
            if (camera)
            {
                to_camera = Vector3Normalize(Vector3Subtract(camera->position, position));
            }
            float diffuse = Vector3DotProduct(light_to_surface, to_camera);
            if (diffuse < 0.0f)
            {
                diffuse = 0.0f;
            }
            float intensity  = state->ambient_strength + (1.0f - state->ambient_strength) * diffuse;
            Color halo_color = render_color_apply_intensity(base_color, intensity);
            halo_color.a     = (unsigned char)((int)halo_color.a / 3);
            float distance   = Vector3Distance(camera->position, position);
            halo_color       = render_apply_fog_color(state, halo_color, distance);
            int halo_segments = render_select_sphere_segments(state, distance);
            if (halo_segments > 16)
            {
                halo_segments -= 8;
            }
            if (halo_segments < 12)
            {
                halo_segments = 12;
            }
            DrawSphereEx(position, glow_radius, halo_segments, halo_segments, halo_color);
        }
    }
#else
    (void)state;
    (void)layout;
    (void)nodes;
    (void)count;
    (void)is_alive;
    (void)camera;
#endif
}

static void render_draw_label(RenderState *state, const LayoutResult *layout,
                              const LayoutNode *node, const Camera3D *camera, const Person *person,
                              bool is_selected, bool is_hovered)
{
    if (!state || !state->label_system_ready || !state->label_system ||
        !state->config.show_name_panels)
    {
        return;
    }
    if (!render_is_node_visible(state, layout, node))
    {
        return;
    }
    float request_font = state->config.name_panel_font_size;
    if (request_font < 1.0f)
    {
        request_font = 26.0f;
    }
    if (is_selected)
    {
        request_font *= 1.18f;
    }
    else if (is_hovered)
    {
        request_font *= 1.08f;
    }

    RenderLabelInfo info;
    if (!render_labels_acquire(state->label_system, person, state->config.show_profile_images,
                               request_font, &info) ||
        !info.valid)
    {
        return;
    }

    Rectangle source       = {0.0f, 0.0f, (float)info.texture.width, (float)info.texture.height};
    Vector3 base_position  = {node->position[0], node->position[1], node->position[2]};
    float vertical_offset  = fmaxf(state->config.sphere_radius * 1.6f, 0.5f);
    Vector3 label_position = {base_position.x, base_position.y + vertical_offset, base_position.z};

    float distance     = Vector3Distance(camera->position, label_position);
    float scale_factor = 0.0018f * distance;
    if (scale_factor < 0.14f)
    {
        scale_factor = 0.14f;
    }
    if (scale_factor > 0.48f)
    {
        scale_factor = 0.48f;
    }
    Vector2 size = {info.width_pixels * scale_factor, info.height_pixels * scale_factor};
    Color tint   = WHITE;
    tint.a       = (unsigned char)(is_selected ? 255 : (is_hovered ? 244 : 232));

    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawBillboardRec(*camera, info.texture, source, label_position, size, tint);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    Color tether_color = render_color_to_raylib(state->config.selected_outline_color);
    tether_color.a     = (unsigned char)((int)tether_color.a / 2);
    Vector3 tether_top = label_position;
    tether_top.y -= size.y * 0.5f;
    Vector3 tether_mid    = Vector3Scale(Vector3Add(base_position, tether_top), 0.5f);
    float tether_distance = Vector3Distance(camera->position, tether_mid);
    tether_color          = render_apply_fog_color(state, tether_color, tether_distance);
    DrawLine3D(base_position, tether_top, tether_color);
}
#endif /* ANCESTRYTREE_HAVE_RAYLIB */

bool render_init(RenderState *state, const RenderConfig *config, char *error_buffer,
                 size_t error_buffer_size)
{
    if (!state)
    {
        return render_set_error(error_buffer, error_buffer_size, "render state pointer is NULL");
    }
    render_state_init(state);

    if (config)
    {
        render_copy_color(&state->config.alive_color, &config->alive_color);
        render_copy_color(&state->config.deceased_color, &config->deceased_color);
        render_copy_color(&state->config.selected_outline_color, &config->selected_outline_color);
        render_copy_color(&state->config.connection_color_parent_child,
                          &config->connection_color_parent_child);
        render_copy_color(&state->config.connection_color_spouse, &config->connection_color_spouse);
        state->config.sphere_radius                 = config->sphere_radius;
        state->config.glow_intensity                = config->glow_intensity;
        state->config.glow_min_strength             = config->glow_min_strength;
        state->config.glow_pulse_speed              = config->glow_pulse_speed;
        state->config.connection_radius             = config->connection_radius;
        state->config.connection_antialiasing       = config->connection_antialiasing;
        state->config.connection_style_parent_child = config->connection_style_parent_child;
        state->config.connection_style_spouse       = config->connection_style_spouse;
        state->config.show_connections              = config->show_connections;
        state->config.show_grid                     = config->show_grid;
        state->config.show_overlay                  = config->show_overlay;
        state->config.show_name_panels              = config->show_name_panels;
        state->config.show_profile_images           = config->show_profile_images;
        state->config.name_panel_font_size          = config->name_panel_font_size;
        state->config.enable_frustum_culling        = config->enable_frustum_culling;
        state->config.enable_lod                    = config->enable_lod;
        state->config.lod_near_distance             = config->lod_near_distance;
        state->config.lod_far_distance              = config->lod_far_distance;
        state->config.culling_margin                = config->culling_margin;
        state->config.show_background_gradient      = config->show_background_gradient;
        render_copy_color(&state->config.background_gradient_top, &config->background_gradient_top);
        render_copy_color(&state->config.background_gradient_bottom,
                          &config->background_gradient_bottom);
        state->config.enable_fog         = config->enable_fog;
        state->config.fog_start_distance = config->fog_start_distance;
        state->config.fog_end_distance   = config->fog_end_distance;
        render_copy_color(&state->config.fog_color, &config->fog_color);
        state->config.enable_rim_lighting = config->enable_rim_lighting;
        state->config.rim_intensity       = config->rim_intensity;
        state->config.rim_power           = config->rim_power;
        render_copy_color(&state->config.rim_color, &config->rim_color);
        state->config.enable_selection_particles      = config->enable_selection_particles;
        state->config.selection_particle_capacity     = config->selection_particle_capacity;
        state->config.selection_particle_lifetime     = config->selection_particle_lifetime;
        state->config.selection_particle_speed_min    = config->selection_particle_speed_min;
        state->config.selection_particle_speed_max    = config->selection_particle_speed_max;
        state->config.selection_particle_repeat_delay = config->selection_particle_repeat_delay;
    }

    if (!render_config_validate(&state->config))
    {
        return render_set_error(error_buffer, error_buffer_size, "invalid render configuration");
    }

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (state->config.enable_selection_particles)
    {
        if (!selection_particles_setup(&state->selection_particles,
                                       state->config.selection_particle_capacity,
                                       state->config.selection_particle_lifetime))
        {
            return render_set_error(error_buffer, error_buffer_size,
                                    "failed to initialize selection particle system");
        }
    }
    else
    {
        (void)selection_particles_setup(&state->selection_particles, 0U,
                                        state->config.selection_particle_lifetime);
    }
#else
    (void)selection_particles_setup(&state->selection_particles, 0U,
                                    state->config.selection_particle_lifetime);
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    bool window_ready = IsWindowReady();
    if (window_ready)
    {
        const char *vertex_source   = render_glow_vertex_shader();
        const char *fragment_source = render_glow_fragment_shader();
        state->glow_shader          = LoadShaderFromMemory(vertex_source, fragment_source);
        if (state->glow_shader.id != 0)
        {
            state->glow_shader.locs[SHADER_LOC_MATRIX_MVP] =
                GetShaderLocation(state->glow_shader, "mvp");
            state->glow_shader.locs[SHADER_LOC_MATRIX_MODEL] =
                GetShaderLocation(state->glow_shader, "matModel");
            state->glow_shader.locs[SHADER_LOC_MATRIX_NORMAL] =
                GetShaderLocation(state->glow_shader, "matNormal");
            state->glow_shader.locs[SHADER_LOC_VECTOR_VIEW] =
                GetShaderLocation(state->glow_shader, "cameraPosition");
            state->glow_intensity_loc    = GetShaderLocation(state->glow_shader, "glowIntensity");
            state->glow_time_loc         = GetShaderLocation(state->glow_shader, "timeSeconds");
            state->glow_min_strength_loc = GetShaderLocation(state->glow_shader, "glowMinStrength");
            state->glow_pulse_speed_loc  = GetShaderLocation(state->glow_shader, "glowPulseSpeed");
            state->glow_rim_intensity_loc = GetShaderLocation(state->glow_shader, "rimIntensity");
            state->glow_rim_power_loc     = GetShaderLocation(state->glow_shader, "rimPower");
            state->glow_rim_color_loc     = GetShaderLocation(state->glow_shader, "rimColor");
            state->glow_rim_enable_loc    = GetShaderLocation(state->glow_shader, "enableRim");
            state->glow_shader_ready      = state->glow_intensity_loc >= 0 &&
                                       state->glow_shader.locs[SHADER_LOC_MATRIX_MVP] >= 0 &&
                                       state->glow_shader.locs[SHADER_LOC_MATRIX_MODEL] >= 0 &&
                                       state->glow_shader.locs[SHADER_LOC_MATRIX_NORMAL] >= 0;
        }
        else
        {
            state->glow_shader_ready = false;
        }
        if (!state->glow_shader_ready)
        {
            UnloadShader(state->glow_shader);
            state->glow_shader.id = 0;
        }
    }
    else
    {
        state->glow_shader_ready      = false;
        state->glow_shader.id         = 0;
        state->glow_intensity_loc     = -1;
        state->glow_time_loc          = -1;
        state->glow_min_strength_loc  = -1;
        state->glow_pulse_speed_loc   = -1;
        state->glow_rim_intensity_loc = -1;
        state->glow_rim_power_loc     = -1;
        state->glow_rim_color_loc     = -1;
        state->glow_rim_enable_loc    = -1;
    }

    if (!state->label_system)
    {
        state->label_system = (RenderLabelSystem *)calloc(1U, sizeof(RenderLabelSystem));
        if (state->label_system && !render_labels_init(state->label_system))
        {
            free(state->label_system);
            state->label_system = NULL;
        }
    }
    state->label_system_ready = (state->label_system != NULL) && window_ready;
    if (state->label_system_ready)
    {
        render_labels_set_base_font_size(state->label_system, state->config.name_panel_font_size);
        state->label_system_font_size_applied = state->config.name_panel_font_size;
    }
#endif

    state->initialized = true;
    return true;
}

void render_cleanup(RenderState *state)
{
    if (!state)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    render_release_render_target(state);
    if (state->glow_shader_ready)
    {
        UnloadShader(state->glow_shader);
    }
    if (state->label_system)
    {
        render_labels_shutdown(state->label_system);
        free(state->label_system);
        state->label_system       = NULL;
        state->label_system_ready = false;
    }
#endif
    free((void *)state->batch_alive_nodes);
    free((void *)state->batch_deceased_nodes);
    state->batch_alive_nodes    = NULL;
    state->batch_deceased_nodes = NULL;
    state->batch_capacity       = 0U;
    free(state->visible_nodes);
    state->visible_nodes         = NULL;
    state->visibility_capacity   = 0U;
    state->visible_node_count    = 0U;
    state->visibility_mask_ready = false;
    selection_particles_shutdown(&state->selection_particles);
    render_state_init(state);
}

bool render_resize(RenderState *state, int width, int height, char *error_buffer,
                   size_t error_buffer_size)
{
    if (!state)
    {
        return render_set_error(error_buffer, error_buffer_size, "render state pointer is NULL");
    }
    if (width <= 0 || height <= 0)
    {
        return render_set_error(error_buffer, error_buffer_size,
                                "render target dimensions must be positive");
    }

    state->render_width  = width;
    state->render_height = height;

#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    state->render_target_ready = false;
    (void)error_buffer;
    (void)error_buffer_size;
    return true;
#else
    if (!state->initialized)
    {
        state->render_target_ready = false;
        return render_set_error(error_buffer, error_buffer_size, "render state not initialized");
    }

    render_release_render_target(state);
    if (!IsWindowReady())
    {
        state->scene_target.id     = 0;
        state->render_target_ready = false;
        return true;
    }

    state->scene_target        = LoadRenderTexture(width, height);
    state->render_target_ready = state->scene_target.id != 0;
    if (!state->render_target_ready)
    {
        return render_set_error(error_buffer, error_buffer_size, "failed to create render target");
    }

    SetTextureFilter(state->scene_target.texture, TEXTURE_FILTER_BILINEAR);
    return true;
#endif
}

bool render_has_render_target(const RenderState *state)
{
    return (state && state->render_target_ready);
}

bool render_find_person_position(const LayoutResult *layout, const Person *person,
                                 float out_position[3])
{
    if (!out_position)
    {
        return false;
    }
    return render_find_person_position_with_index(layout, person, out_position, NULL);
}

size_t render_collect_parent_child_segments(const LayoutResult *layout,
                                            RenderConnectionSegment *segments,
                                            size_t segment_capacity)
{
    if (!layout || !segments || segment_capacity == 0U)
    {
        return 0U;
    }
    size_t count = 0U;
    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person   = node->person;
        if (!person || person->children_count == 0U)
        {
            continue;
        }
        for (size_t child_index = 0; child_index < person->children_count; ++child_index)
        {
            const Person *child = person->children[child_index];
            if (!child)
            {
                continue;
            }
            if (count >= segment_capacity)
            {
                return count;
            }
            if (!render_find_person_position(layout, child, segments[count].end))
            {
                continue;
            }
            segments[count].start[0] = node->position[0];
            segments[count].start[1] = node->position[1];
            segments[count].start[2] = node->position[2];
            ++count;
        }
    }
    return count;
}

size_t render_collect_spouse_segments(const LayoutResult *layout, RenderConnectionSegment *segments,
                                      size_t segment_capacity)
{
    if (!layout || !segments || segment_capacity == 0U)
    {
        return 0U;
    }
    size_t count = 0U;
    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person   = node->person;
        if (!person || person->spouses_count == 0U)
        {
            continue;
        }
        for (size_t spouse_index = 0; spouse_index < person->spouses_count; ++spouse_index)
        {
            const Person *partner = person->spouses[spouse_index].partner;
            if (!partner || partner->id <= person->id)
            {
                continue;
            }
            if (count >= segment_capacity)
            {
                return count;
            }
            if (!render_find_person_position(layout, partner, segments[count].end))
            {
                continue;
            }
            segments[count].start[0] = node->position[0];
            segments[count].start[1] = node->position[1];
            segments[count].start[2] = node->position[2];
            ++count;
        }
    }
    return count;
}

bool render_connections_render(RenderState *state, const LayoutResult *layout)
{
    if (!state || !state->initialized || !layout)
    {
        return false;
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)state;
    (void)layout;
    return false;
#else
    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person   = node->person;
        if (!person)
        {
            continue;
        }
        bool parent_visible = render_node_visible_by_index(state, index);
        for (size_t child_index = 0; child_index < person->children_count; ++child_index)
        {
            const Person *child = person->children[child_index];
            if (!child)
            {
                continue;
            }
            float child_position[3];
            size_t child_layout_index = 0U;
            if (render_find_person_position_with_index(layout, child, child_position,
                                                       &child_layout_index))
            {
                bool child_visible = render_node_visible_by_index(state, child_layout_index);
                if (parent_visible || child_visible)
                {
                    render_draw_connection(state, node->position, child_position,
                                           state->config.connection_color_parent_child,
                                           state->config.connection_style_parent_child);
                }
            }
        }
        for (size_t spouse_index = 0; spouse_index < person->spouses_count; ++spouse_index)
        {
            const Person *partner = person->spouses[spouse_index].partner;
            if (!partner || partner->id <= person->id)
            {
                continue;
            }
            float partner_position[3];
            size_t partner_layout_index = 0U;
            if (render_find_person_position_with_index(layout, partner, partner_position,
                                                       &partner_layout_index))
            {
                bool partner_visible = render_node_visible_by_index(state, partner_layout_index);
                if (parent_visible || partner_visible)
                {
                    render_draw_connection(state, node->position, partner_position,
                                           state->config.connection_color_spouse,
                                           state->config.connection_style_spouse);
                }
            }
        }
    }
    return true;
#endif
}

bool render_scene(RenderState *state, const LayoutResult *layout, const CameraController *camera,
                  const Person *selected_person, const Person *hovered_person)
{
    if (!state || !state->initialized || !layout || !camera)
    {
        return false;
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)selected_person;
    (void)hovered_person;
    (void)layout;
    (void)camera;
    return false;
#else
    const Camera3D *camera_data = camera_controller_get_camera(camera);
    if (!camera_data)
    {
        return false;
    }

    float delta_seconds = GetFrameTime();
    if (delta_seconds < 0.0f)
    {
        delta_seconds = 0.0f;
    }
    render_update_selection_particles(state, layout, camera_data, selected_person, delta_seconds);

    if (!render_update_visibility(state, layout, camera_data))
    {
        state->visibility_mask_ready = false;
        state->visible_node_count    = layout->count;
    }

    if (state->label_system)
    {
        state->label_system_ready = IsWindowReady();
        if (state->label_system_ready)
        {
            if (fabsf(state->label_system_font_size_applied - state->config.name_panel_font_size) >
                0.05f)
            {
                render_labels_set_base_font_size(state->label_system,
                                                 state->config.name_panel_font_size);
                state->label_system_font_size_applied = state->config.name_panel_font_size;
            }
        }
    }

    if (state->label_system_ready && state->label_system)
    {
        render_labels_begin_frame(state->label_system);
    }

    bool using_render_target =
        state->render_target_ready && state->render_width > 0 && state->render_height > 0;
    if (using_render_target)
    {
        BeginTextureMode(state->scene_target);
        if (state->config.show_background_gradient)
        {
            Color base = render_color_to_raylib(state->config.background_gradient_bottom);
            ClearBackground(base);
            render_draw_background_gradient(state);
        }
        else
        {
            const Color transparent = {0, 0, 0, 0};
            ClearBackground(transparent);
        }
    }
    else if (state->config.show_background_gradient)
    {
        render_draw_background_gradient(state);
    }

    state->active_camera = camera_data;
    BeginMode3D(*camera_data);
    render_apply_lighting(state);
    if (state->config.show_grid)
    {
        DrawGrid(24, 1.0f);
    }

    if (state->config.show_connections)
    {
        (void)render_connections_render(state, layout);
    }

    bool drew_with_batching = false;
    RenderBatcherGrouping grouping;
    render_batcher_grouping_reset(&grouping);
    const unsigned char *mask = (state->visibility_mask_ready ? state->visible_nodes : NULL);
    size_t mask_count         = state->visibility_mask_ready ? layout->count : 0U;
    if (render_ensure_batch_capacity(state, layout->count) &&
        render_batcher_plan(layout, selected_person, hovered_person, &grouping,
                            state->batch_alive_nodes, state->batch_capacity,
                            state->batch_deceased_nodes, state->batch_capacity, mask, mask_count))
    {
        render_draw_sphere_group(state, layout, grouping.alive_nodes, grouping.alive_count, true,
                                 camera_data);
        render_draw_sphere_group(state, layout, grouping.deceased_nodes, grouping.deceased_count,
                                 false, camera_data);

        if (grouping.hovered_node && grouping.hovered_node->person)
        {
            render_draw_sphere(state, layout, grouping.hovered_node,
                               grouping.hovered_node->person->is_alive, false, true, camera_data);
        }
        if (grouping.selected_node && grouping.selected_node->person)
        {
            render_draw_sphere(state, layout, grouping.selected_node,
                               grouping.selected_node->person->is_alive, true, false, camera_data);
        }
        drew_with_batching = true;
    }

    if (!drew_with_batching)
    {
        for (size_t index = 0; index < layout->count; ++index)
        {
            const LayoutNode *node = &layout->nodes[index];
            const Person *person   = node->person;
            if (!person)
            {
                continue;
            }
            bool is_selected = (selected_person == person);
            bool is_hovered  = (hovered_person == person) && !is_selected;
            render_draw_sphere(state, layout, node, person->is_alive, is_selected, is_hovered,
                               camera_data);
        }
    }

    render_draw_selection_particles(state, camera_data);

    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person   = node->person;
        if (!person)
        {
            continue;
        }
        bool is_selected = (selected_person == person);
        bool is_hovered  = (hovered_person == person) && !is_selected;
        render_draw_label(state, layout, node, camera_data, person, is_selected, is_hovered);
    }

    EndMode3D();
    state->active_camera = NULL;

    if (using_render_target)
    {
        EndTextureMode();
        Rectangle source      = {0.0f, 0.0f, (float)state->scene_target.texture.width,
                                 (float)-state->scene_target.texture.height};
        Rectangle destination = {0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight()};
        DrawTexturePro(state->scene_target.texture, source, destination, (Vector2){0.0f, 0.0f},
                       0.0f, WHITE);
    }

    if (state->config.show_overlay)
    {
        render_draw_holographic_overlay(state);
    }

    if (state->label_system_ready && state->label_system)
    {
        render_labels_end_frame(state->label_system);
    }
    return true;
#endif
}

/*
 * Manual validation checklist:
 * 1. Launch the application and confirm spheres render with glow around living persons.
 * 2. Observe parent/child and spouse connections to ensure lines link correct pairs.
 * 3. Resize the window repeatedly and confirm the render target regenerates without artifacts.
 * 4. Verify the holographic screen overlay animates above the 3D content and below the UI widgets.
 * 5. Hover and select multiple persons to ensure batched draws preserve highlight scaling and glow
 * order.
 * 6. Toggle connection style and antialiasing in debug UI to compare straight vs. bezier links and
 * confirm line smoothing is artifact free.
 * 7. Rotate and zoom the camera to confirm off-screen persons disappear while on-screen links
 * remain visible.
 * 8. Move far from the tree to verify sphere tessellation reduces smoothly without noticeable
 * popping.
 * 9. Select a person and observe the selection particle burst; hold the selection to ensure
 *    periodic bursts respect the configured repeat delay and fade cleanly with fog.
 */
