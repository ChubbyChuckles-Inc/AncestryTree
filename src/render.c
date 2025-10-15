#include "render.h"

#include "camera_controller.h"
#include "layout.h"
#include "person.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
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
}

void render_state_init(RenderState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    render_set_default_config(&state->config);
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    state->glow_shader_ready = false;
    state->ambient_strength = 0.2f;
    state->light_direction[0] = -0.3f;
    state->light_direction[1] = -1.0f;
    state->light_direction[2] = -0.2f;
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
static const char *render_glow_fragment_shader(void)
{
#if defined(GRAPHICS_API_OPENGL_33)
    return "#version 330\n"
           "in vec2 fragTexCoord;\n"
           "in vec4 fragColor;\n"
           "out vec4 finalColor;\n"
           "uniform float glowIntensity;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    vec3 emissive = base.rgb * glow * 0.35;\n"
           "    finalColor = vec4(base.rgb + emissive, base.a);\n"
           "}";
#else
    return "#version 100\n"
           "precision mediump float;\n"
           "varying vec2 fragTexCoord;\n"
           "varying vec4 fragColor;\n"
           "uniform float glowIntensity;\n"
           "void main(){\n"
           "    vec4 base = fragColor;\n"
           "    float glow = clamp(glowIntensity, 0.0, 4.0);\n"
           "    vec3 emissive = base.rgb * glow * 0.35;\n"
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

static void render_draw_connection(RenderState *state, const float start[3], const float end[3], RenderColor color)
{
    if (!state)
    {
        return;
    }
    Vector3 a = {start[0], start[1], start[2]};
    Vector3 b = {end[0], end[1], end[2]};
    Color ray_color = render_color_to_raylib(color);

    if (state->config.connection_radius > 0.0f)
    {
        DrawCylinderEx(a, b, state->config.connection_radius, state->config.connection_radius, 12, ray_color);
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

static void render_draw_sphere(RenderState *state, const LayoutNode *node, bool is_alive, bool is_selected,
                               const Camera3D *camera)
{
    Vector3 position = {node->position[0], node->position[1], node->position[2]};
    float base_radius = state->config.sphere_radius;
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
        float glow_radius = base_radius * (1.0f + state->config.glow_intensity * 0.4f);
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
    }

    if (!render_config_validate(&state->config))
    {
        return render_set_error(error_buffer, error_buffer_size, "invalid render configuration");
    }

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    const char *fragment_source = render_glow_fragment_shader();
    state->glow_shader = LoadShaderFromMemory(NULL, fragment_source);
    if (state->glow_shader.id != 0)
    {
        state->glow_intensity_loc = GetShaderLocation(state->glow_shader, "glowIntensity");
        state->glow_shader_ready = state->glow_intensity_loc >= 0;
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
    if (state->glow_shader_ready)
    {
        UnloadShader(state->glow_shader);
    }
#endif
    render_state_init(state);
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
                  const Person *highlight_person)
{
    (void)highlight_person;
    if (!state || !state->initialized || !layout || !camera)
    {
        return false;
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)layout;
    (void)camera;
    return false;
#else
    const Camera3D *camera_data = camera_controller_get_camera(camera);
    if (!camera_data)
    {
        return false;
    }

    BeginMode3D(*camera_data);
    render_apply_lighting(state);
    DrawGrid(24, 1.0f);

    (void)render_connections_render(state, layout);

    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person = node->person;
        if (!person)
        {
            continue;
        }
        bool is_selected = (highlight_person == person);
        render_draw_sphere(state, node, person->is_alive, is_selected, camera_data);
    }

    EndMode3D();
    return true;
#endif
}

/*
 * Manual validation checklist:
 * 1. Launch the application and confirm spheres render with glow around living persons.
 * 2. Observe parent/child and spouse connections to ensure lines link correct pairs.
 * 3. Toggle selection debugging (highlight_person parameter) and verify outlines render.
 * 4. Resize the window and ensure renders persist without artifacts.
 */
