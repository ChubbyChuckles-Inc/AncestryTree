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

static RenderColor render_color_make(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
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

static void render_set_default_config(RenderConfig *config)
{
    if (!config)
    {
        return;
    }
    config->sphere_radius = 0.6f;
    config->glow_intensity = 0.85f;
    config->connection_radius = 0.05f;
    config->alive_color = render_color_make(0, 195, 255, 255);
    config->deceased_color = render_color_make(200, 120, 240, 255);
    config->selected_outline_color = render_color_make(255, 255, 255, 255);
    config->connection_color_parent_child = render_color_make(64, 200, 255, 200);
    config->connection_color_spouse = render_color_make(255, 180, 120, 200);
    config->show_connections = true;
    config->show_grid = true;
    config->show_overlay = true;
    config->show_name_panels = true;
    config->show_profile_images = true;
    config->name_panel_font_size = 26.0f;
}

void render_state_init(RenderState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    render_set_default_config(&state->config);
    state->render_width = 0;
    state->render_height = 0;
    state->render_target_ready = false;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    state->glow_shader_ready = false;
    state->glow_time_loc = -1;
    state->ambient_strength = 0.2f;
    state->light_direction[0] = -0.3f;
    state->light_direction[1] = -1.0f;
    state->light_direction[2] = -0.2f;
    state->scene_target.id = 0;
    state->label_system = NULL;
    state->label_system_ready = false;
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
    if (!(config->connection_radius >= 0.0f))
    {
        return false;
    }
    if (!(config->name_panel_font_size >= 1.0f))
    {
        return false;
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
           "in vec4 vertexColor;\n"
           "out vec2 fragTexCoord;\n"
           "out vec4 fragColor;\n"
           "uniform mat4 mvp;\n"
           "void main(){\n"
           "    fragTexCoord = vertexTexCoord;\n"
           "    fragColor = vertexColor;\n"
           "    gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
           "}";
#else
    return "#version 100\n"
           "attribute vec3 vertexPosition;\n"
           "attribute vec2 vertexTexCoord;\n"
           "attribute vec4 vertexColor;\n"
           "varying vec2 fragTexCoord;\n"
           "varying vec4 fragColor;\n"
           "uniform mat4 mvp;\n"
           "void main(){\n"
           "    fragTexCoord = vertexTexCoord;\n"
           "    fragColor = vertexColor;\n"
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
           "out vec4 finalColor;\n"
           "uniform float glowIntensity;\n"
           "uniform float timeSeconds;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    float wobble = 0.35 + 0.15 * sin(timeSeconds * 1.4);\n"
           "    vec3 emissive = base.rgb * glow * wobble;\n"
           "    finalColor = vec4(base.rgb + emissive, base.a);\n"
           "}";
#else
    return "#version 100\n"
           "precision mediump float;\n"
           "varying vec2 fragTexCoord;\n"
           "varying vec4 fragColor;\n"
           "uniform float glowIntensity;\n"
           "uniform float timeSeconds;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    float wobble = 0.35 + 0.15 * sin(timeSeconds * 1.4);\n"
           "    vec3 emissive = base.rgb * glow * wobble;\n"
           "    gl_FragColor = vec4(base.rgb + emissive, base.a);\n"
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
    float bias = 5.0f;
    result.r = (unsigned char)fminf(255.0f, color.r * intensity + bias);
    result.g = (unsigned char)fminf(255.0f, color.g * intensity + bias);
    result.b = (unsigned char)fminf(255.0f, color.b * intensity + bias);
    return result;
}

static float render_distance_between_points(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

static void render_draw_connection(RenderState *state, const float start[3], const float end[3], RenderColor color)
{
    if (!state)
    {
        return;
    }
    Vector3 a = {start[0], start[1], start[2]};
    Vector3 b = {end[0], end[1], end[2]};
    Color ray_color = render_color_to_raylib(color);

    float radius = state->config.connection_radius;
    if (radius > 0.0f)
    {
        float distance = render_distance_between_points(start, end);
        float scale = 1.0f + fminf(distance * 0.12f, 2.5f);
        float adjusted = radius * scale;
        DrawCylinderEx(a, b, adjusted, adjusted, 18, ray_color);
    }
    else
    {
        DrawLine3D(a, b, ray_color);
    }
}

static void render_apply_lighting(const RenderState *state)
{
    (void)state;
    // Placeholder for future custom lighting. raylib uses fixed function lights by default.
}

static void render_draw_holographic_overlay(const RenderState *state)
{
    (void)state;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    const Color tint = {12, 20, 38, 72};
    DrawRectangle(0, 0, screen_width, screen_height, tint);

    const Color horizontal = {0, 220, 255, 36};
    const Color vertical = {0, 160, 255, 22};
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
        state->scene_target.id = 0;
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
    if (state->batch_capacity >= required_capacity && state->batch_alive_nodes && state->batch_deceased_nodes)
    {
        return true;
    }

    size_t new_capacity = required_capacity + (required_capacity / 2U) + 4U;
    const LayoutNode **alive_new = (const LayoutNode **)malloc(new_capacity * sizeof(*alive_new));
    const LayoutNode **deceased_new = (const LayoutNode **)malloc(new_capacity * sizeof(*deceased_new));
    if (!alive_new || !deceased_new)
    {
        free(alive_new);
        free(deceased_new);
        return false;
    }

    free((void *)state->batch_alive_nodes);
    free((void *)state->batch_deceased_nodes);
    state->batch_alive_nodes = alive_new;
    state->batch_deceased_nodes = deceased_new;
    state->batch_capacity = new_capacity;
    return true;
}

static void render_draw_sphere(RenderState *state, const LayoutNode *node, bool is_alive, bool is_selected,
                               bool is_hovered, const Camera3D *camera)
{
    Vector3 position = {node->position[0], node->position[1], node->position[2]};
    float base_radius = state->config.sphere_radius;
    if (is_hovered && !is_selected)
    {
        base_radius *= 1.08f;
    }
    if (is_selected)
    {
        base_radius *= 1.2f;
    }
    Color base_color = render_color_to_raylib(is_alive ? state->config.alive_color : state->config.deceased_color);

    Vector3 light_dir = {state->light_direction[0], state->light_direction[1], state->light_direction[2]};
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
    float diffuse = Vector3DotProduct(light_to_surface, to_camera);
    if (diffuse < 0.0f)
    {
        diffuse = 0.0f;
    }
    float intensity = state->ambient_strength + (1.0f - state->ambient_strength) * diffuse;
    base_color = render_color_apply_intensity(base_color, intensity);

    if (state->glow_shader_ready)
    {
        float glow_value = state->config.glow_intensity;
        SetShaderValue(state->glow_shader, state->glow_intensity_loc, &glow_value, SHADER_UNIFORM_FLOAT);
        if (state->glow_time_loc >= 0)
        {
            float time_seconds = (float)GetTime();
            SetShaderValue(state->glow_shader, state->glow_time_loc, &time_seconds, SHADER_UNIFORM_FLOAT);
        }
        BeginShaderMode(state->glow_shader);
        DrawSphereEx(position, base_radius, 32, 32, base_color);
        EndShaderMode();
    }
    else
    {
        DrawSphereEx(position, base_radius, 32, 32, base_color);
    }

    if (is_alive)
    {
        Color halo = base_color;
        halo.a = (unsigned char)((int)halo.a / 3);
        float intensity_scale = 1.0f + state->config.glow_intensity * (is_hovered ? 0.55f : 0.4f);
        float glow_radius = base_radius * intensity_scale;
        DrawSphereEx(position, glow_radius, 24, 24, halo);
    }

    if (is_selected)
    {
        Color outline = render_color_to_raylib(state->config.selected_outline_color);
        rlDisableBackfaceCulling();
        rlDisableDepthMask();
        DrawSphereWires(position, base_radius * 1.02f, 18, 18, outline);
        rlEnableDepthMask();
        rlEnableBackfaceCulling();
    }
}

static void render_draw_sphere_group(RenderState *state, const LayoutNode *const *nodes, size_t count, bool is_alive,
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

    Vector3 light_dir = {state->light_direction[0], state->light_direction[1], state->light_direction[2]};
    if (Vector3Length(light_dir) < 0.0001f)
    {
        light_dir = Vector3Normalize((Vector3){-0.3f, -1.0f, -0.2f});
    }
    else
    {
        light_dir = Vector3Normalize(light_dir);
    }
    Vector3 light_to_surface = Vector3Scale(light_dir, -1.0f);

    Color base_color = render_color_to_raylib(is_alive ? state->config.alive_color : state->config.deceased_color);
    bool use_shader = state->glow_shader_ready && is_alive;
    if (use_shader)
    {
        float glow_value = state->config.glow_intensity;
        SetShaderValue(state->glow_shader, state->glow_intensity_loc, &glow_value, SHADER_UNIFORM_FLOAT);
        if (state->glow_time_loc >= 0)
        {
            float time_seconds = (float)GetTime();
            SetShaderValue(state->glow_shader, state->glow_time_loc, &time_seconds, SHADER_UNIFORM_FLOAT);
        }
        BeginShaderMode(state->glow_shader);
    }

    for (size_t index = 0; index < count; ++index)
    {
        const LayoutNode *node = nodes[index];
        const Person *person = node ? node->person : NULL;
        if (!node || !person)
        {
            continue;
        }
        Vector3 position = {node->position[0], node->position[1], node->position[2]};
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
        DrawSphereEx(position, base_radius, 32, 32, lit_color);
    }

    if (use_shader)
    {
        EndShaderMode();
    }

    if (is_alive)
    {
        float intensity_scale = 1.0f + state->config.glow_intensity * 0.4f;
        float glow_radius = base_radius * intensity_scale;
        for (size_t index = 0; index < count; ++index)
        {
            const LayoutNode *node = nodes[index];
            const Person *person = node ? node->person : NULL;
            if (!node || !person)
            {
                continue;
            }
            Vector3 position = {node->position[0], node->position[1], node->position[2]};
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
            Color halo_color = render_color_apply_intensity(base_color, intensity);
            halo_color.a = (unsigned char)((int)halo_color.a / 3);
            DrawSphereEx(position, glow_radius, 24, 24, halo_color);
        }
    }
#else
    (void)state;
    (void)nodes;
    (void)count;
    (void)is_alive;
    (void)camera;
#endif
}

static void render_draw_label(RenderState *state, const LayoutNode *node, const Camera3D *camera,
                              const Person *person, bool is_selected, bool is_hovered)
{
    if (!state || !state->label_system_ready || !state->label_system || !state->config.show_name_panels)
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
    if (!render_labels_acquire(state->label_system, person, state->config.show_profile_images, request_font, &info) ||
        !info.valid)
    {
        return;
    }

    Rectangle source = {0.0f, 0.0f, (float)info.texture.width, (float)info.texture.height};
    Vector3 base_position = {node->position[0], node->position[1], node->position[2]};
    float vertical_offset = fmaxf(state->config.sphere_radius * 1.6f, 0.5f);
    Vector3 label_position = {base_position.x, base_position.y + vertical_offset, base_position.z};

    float distance = Vector3Distance(camera->position, label_position);
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
    Color tint = WHITE;
    tint.a = (unsigned char)(is_selected ? 255 : (is_hovered ? 244 : 232));

    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    DrawBillboardRec(*camera, info.texture, source, label_position, size, tint);
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    Color tether_color = render_color_to_raylib(state->config.selected_outline_color);
    tether_color.a = (unsigned char)((int)tether_color.a / 2);
    Vector3 tether_top = label_position;
    tether_top.y -= size.y * 0.5f;
    DrawLine3D(base_position, tether_top, tether_color);
}
#endif /* ANCESTRYTREE_HAVE_RAYLIB */

bool render_init(RenderState *state, const RenderConfig *config, char *error_buffer, size_t error_buffer_size)
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
        render_copy_color(&state->config.connection_color_parent_child, &config->connection_color_parent_child);
        render_copy_color(&state->config.connection_color_spouse, &config->connection_color_spouse);
        state->config.sphere_radius = config->sphere_radius;
        state->config.glow_intensity = config->glow_intensity;
        state->config.connection_radius = config->connection_radius;
        state->config.show_connections = config->show_connections;
        state->config.show_grid = config->show_grid;
        state->config.show_overlay = config->show_overlay;
        state->config.show_name_panels = config->show_name_panels;
        state->config.show_profile_images = config->show_profile_images;
        state->config.name_panel_font_size = config->name_panel_font_size;
    }

    if (!render_config_validate(&state->config))
    {
        return render_set_error(error_buffer, error_buffer_size, "invalid render configuration");
    }

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    bool window_ready = IsWindowReady();
    if (window_ready)
    {
        const char *vertex_source = render_glow_vertex_shader();
        const char *fragment_source = render_glow_fragment_shader();
        state->glow_shader = LoadShaderFromMemory(vertex_source, fragment_source);
        if (state->glow_shader.id != 0)
        {
            state->glow_shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(state->glow_shader, "mvp");
            state->glow_intensity_loc = GetShaderLocation(state->glow_shader, "glowIntensity");
            state->glow_time_loc = GetShaderLocation(state->glow_shader, "timeSeconds");
            state->glow_shader_ready = state->glow_intensity_loc >= 0 &&
                                       state->glow_shader.locs[SHADER_LOC_MATRIX_MVP] >= 0;
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
        state->glow_shader_ready = false;
        state->glow_shader.id = 0;
        state->glow_intensity_loc = -1;
        state->glow_time_loc = -1;
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
        state->label_system = NULL;
        state->label_system_ready = false;
    }
#endif
    free((void *)state->batch_alive_nodes);
    free((void *)state->batch_deceased_nodes);
    state->batch_alive_nodes = NULL;
    state->batch_deceased_nodes = NULL;
    state->batch_capacity = 0U;
    render_state_init(state);
}

bool render_resize(RenderState *state, int width, int height, char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return render_set_error(error_buffer, error_buffer_size, "render state pointer is NULL");
    }
    if (width <= 0 || height <= 0)
    {
        return render_set_error(error_buffer, error_buffer_size, "render target dimensions must be positive");
    }

    state->render_width = width;
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
        state->scene_target.id = 0;
        state->render_target_ready = false;
        return true;
    }

    state->scene_target = LoadRenderTexture(width, height);
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

bool render_find_person_position(const LayoutResult *layout, const Person *person, float out_position[3])
{
    if (!layout || !person || !out_position)
    {
        return false;
    }
    for (size_t index = 0; index < layout->count; ++index)
    {
        if (layout->nodes[index].person == person)
        {
            out_position[0] = layout->nodes[index].position[0];
            out_position[1] = layout->nodes[index].position[1];
            out_position[2] = layout->nodes[index].position[2];
            return true;
        }
    }
    return false;
}

size_t render_collect_parent_child_segments(const LayoutResult *layout, RenderConnectionSegment *segments,
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
        const Person *person = node->person;
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
        const Person *person = node->person;
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
        const Person *person = node->person;
        if (!person)
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
            float child_position[3];
            if (render_find_person_position(layout, child, child_position))
            {
                render_draw_connection(state, node->position, child_position,
                                       state->config.connection_color_parent_child);
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
            if (render_find_person_position(layout, partner, partner_position))
            {
                render_draw_connection(state, node->position, partner_position,
                                       state->config.connection_color_spouse);
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

    if (state->label_system)
    {
        state->label_system_ready = IsWindowReady();
        if (state->label_system_ready)
        {
            if (fabsf(state->label_system_font_size_applied - state->config.name_panel_font_size) > 0.05f)
            {
                render_labels_set_base_font_size(state->label_system, state->config.name_panel_font_size);
                state->label_system_font_size_applied = state->config.name_panel_font_size;
            }
        }
    }

    if (state->label_system_ready && state->label_system)
    {
        render_labels_begin_frame(state->label_system);
    }

    bool using_render_target = state->render_target_ready && state->render_width > 0 && state->render_height > 0;
    if (using_render_target)
    {
        BeginTextureMode(state->scene_target);
        const Color transparent = {0, 0, 0, 0};
        ClearBackground(transparent);
    }

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
    if (render_ensure_batch_capacity(state, layout->count) &&
        render_batcher_plan(layout, selected_person, hovered_person, &grouping, state->batch_alive_nodes,
                            state->batch_capacity, state->batch_deceased_nodes, state->batch_capacity))
    {
        render_draw_sphere_group(state, grouping.alive_nodes, grouping.alive_count, true, camera_data);
        render_draw_sphere_group(state, grouping.deceased_nodes, grouping.deceased_count, false, camera_data);

        if (grouping.hovered_node && grouping.hovered_node->person)
        {
            render_draw_sphere(state, grouping.hovered_node, grouping.hovered_node->person->is_alive, false, true,
                               camera_data);
        }
        if (grouping.selected_node && grouping.selected_node->person)
        {
            render_draw_sphere(state, grouping.selected_node, grouping.selected_node->person->is_alive, true, false,
                               camera_data);
        }
        drew_with_batching = true;
    }

    if (!drew_with_batching)
    {
        for (size_t index = 0; index < layout->count; ++index)
        {
            const LayoutNode *node = &layout->nodes[index];
            const Person *person = node->person;
            if (!person)
            {
                continue;
            }
            bool is_selected = (selected_person == person);
            bool is_hovered = (hovered_person == person) && !is_selected;
            render_draw_sphere(state, node, person->is_alive, is_selected, is_hovered, camera_data);
        }
    }

    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person = node->person;
        if (!person)
        {
            continue;
        }
        bool is_selected = (selected_person == person);
        bool is_hovered = (hovered_person == person) && !is_selected;
        render_draw_label(state, node, camera_data, person, is_selected, is_hovered);
    }

    EndMode3D();

    if (using_render_target)
    {
        EndTextureMode();
        Rectangle source = {0.0f, 0.0f, (float)state->scene_target.texture.width,
                            (float)-state->scene_target.texture.height};
        Rectangle destination = {0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight()};
        DrawTexturePro(state->scene_target.texture, source, destination, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
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
 * 5. Hover and select multiple persons to ensure batched draws preserve highlight scaling and glow order.
 */
