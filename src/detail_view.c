#include "detail_view.h"

#include "expansion.h"
#include "image_panel.h"
#include "render.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DETAIL_VIEW_ROTATION_SMOOTHING 5.0f

static float detail_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static void detail_copy_string(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0U)
    {
        return;
    }
    if (!source)
    {
        destination[0] = '\0';
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(destination, destination_size, source, _TRUNCATE);
#else
    (void)snprintf(destination, destination_size, "%s", source);
#endif
}

static float detail_wrap_angle(float angle)
{
    const float full_turn = (float)(2.0 * M_PI);
    while (angle < 0.0f)
    {
        angle += full_turn;
    }
    while (angle >= full_turn)
    {
        angle -= full_turn;
    }
    return angle;
}

typedef struct DetailViewTimelineSlot
{
    float base_angle;
    float height;
    float radius;
    float pulse_speed;
    float pulse_offset;
    float anchor[3];
} DetailViewTimelineSlot;

struct DetailViewSystem
{
    bool initialized;
    bool content_ready;
    DetailViewContent content;
    float rotation;
    float pulse_time;
    float detail_phase;
    float timeline_phase;
    float panel_phase;
    float detail_target_phase;
    float timeline_target_phase;
    float panel_target_phase;
    float base_timeline_radius;
    float base_panel_radius;
    DetailViewTimelineSlot timeline[DETAIL_VIEW_MAX_TIMELINE_SLOTS];
    size_t timeline_count;
    ImagePanelSystem *panel_system;
    float panel_angles[DETAIL_VIEW_MAX_PANEL_SLOTS];
    size_t panel_count;
    size_t panel_focus_index;
    bool panel_focus_locked;
};

static void detail_view_default_content(DetailViewContent *content)
{
    if (!content)
    {
        return;
    }
    memset(content, 0, sizeof(*content));
    detail_copy_string(content->name, sizeof(content->name), "Select a person");
    detail_copy_string(content->lifespan, sizeof(content->lifespan), "Awaiting selection");
    detail_copy_string(content->facts[0], sizeof(content->facts[0]),
                       "Choose an ancestor to explore their holographic records.");
    content->fact_count = 1U;
}

static void detail_view_reset_state(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    system->rotation = 0.0f;
    system->pulse_time = 0.0f;
    system->detail_phase = 0.0f;
    system->timeline_phase = 0.0f;
    system->panel_phase = 0.0f;
    system->detail_target_phase = 0.0f;
    system->timeline_target_phase = 0.0f;
    system->panel_target_phase = 0.0f;
    system->panel_focus_index = 0U;
    system->panel_focus_locked = false;
}

static void detail_view_configure_slots(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }

    system->base_timeline_radius = 0.74f;
    system->base_panel_radius = 0.58f;

    system->timeline_count = 8U;
    if (system->timeline_count > DETAIL_VIEW_MAX_TIMELINE_SLOTS)
    {
        system->timeline_count = DETAIL_VIEW_MAX_TIMELINE_SLOTS;
    }
    const float angle_step = (float)(2.0 * M_PI / (double)system->timeline_count);
    for (size_t index = 0; index < system->timeline_count; ++index)
    {
        DetailViewTimelineSlot *slot = &system->timeline[index];
        slot->base_angle = angle_step * (float)index;
        slot->height = (index % 2U == 0U) ? 0.36f : -0.30f;
        slot->radius = 1.0f;
        slot->pulse_speed = 0.85f + 0.12f * (float)(index % 3U);
        slot->pulse_offset = (float)index * 0.58f;
        slot->anchor[0] = cosf(slot->base_angle) * slot->radius;
        slot->anchor[1] = slot->height;
        slot->anchor[2] = sinf(slot->base_angle) * slot->radius;
    }

    if (system->panel_system)
    {
        ImagePanelLayoutParams params;
        memset(&params, 0, sizeof(params));
        params.desired_count = 6U;
        params.cone_inner_radius = 0.54f;
        params.cone_outer_radius = 0.68f;
        params.min_height = 0.28f;
        params.max_height = 0.72f;
        params.min_angle_radians = 0.58f;
        params.timeline_radius = system->base_timeline_radius;
        params.timeline_height = 0.0f;
        params.timeline_clear_margin = 0.12f;

        if (!image_panel_layout_init(system->panel_system, &params))
        {
            image_panel_reset(system->panel_system);
            system->panel_count = 0U;
        }
        else
        {
            system->panel_count = image_panel_count(system->panel_system);
            for (size_t index = 0; index < system->panel_count && index < DETAIL_VIEW_MAX_PANEL_SLOTS; ++index)
            {
                ImagePanelAnchor anchor;
                if (image_panel_get_anchor(system->panel_system, index, &anchor))
                {
                    system->panel_angles[index] = anchor.angle;
                }
                else
                {
                    system->panel_angles[index] = 0.0f;
                }
            }
        }
    }
    else
    {
        system->panel_count = 0U;
    }
}

DetailViewSystem *detail_view_create(void)
{
    DetailViewSystem *system = (DetailViewSystem *)calloc(1, sizeof(DetailViewSystem));
    if (!system)
    {
        return NULL;
    }
    system->panel_system = image_panel_create();
    if (!system->panel_system)
    {
        free(system);
        return NULL;
    }
    if (!detail_view_init(system))
    {
        image_panel_destroy(system->panel_system);
        free(system);
        return NULL;
    }
    return system;
}

void detail_view_destroy(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    detail_view_cleanup(system);
    image_panel_destroy(system->panel_system);
    free(system);
}

bool detail_view_init(DetailViewSystem *system)
{
    if (!system)
    {
        return false;
    }
    ImagePanelSystem *panel_system = system->panel_system;
    memset(system, 0, sizeof(*system));
    system->panel_system = panel_system;
    if (!system->panel_system)
    {
        system->panel_system = image_panel_create();
    }
    if (!system->panel_system)
    {
        return false;
    }
    detail_view_configure_slots(system);
    detail_view_default_content(&system->content);
    system->content_ready = true;
    detail_view_reset_state(system);
    system->initialized = true;
    return true;
}

void detail_view_cleanup(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    system->initialized = false;
    system->content_ready = false;
    system->timeline_count = 0U;
    system->panel_count = 0U;
    system->panel_focus_index = 0U;
    system->panel_focus_locked = false;
    detail_view_reset_state(system);
    if (system->panel_system)
    {
        image_panel_reset(system->panel_system);
    }
}

void detail_view_reset(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    if (!system->initialized)
    {
        (void)detail_view_init(system);
        return;
    }
    detail_view_configure_slots(system);
    detail_view_reset_state(system);
}

bool detail_view_set_content(DetailViewSystem *system, const DetailViewContent *content)
{
    if (!system)
    {
        return false;
    }
    if (!system->initialized)
    {
        if (!detail_view_init(system))
        {
            return false;
        }
    }
    if (!content)
    {
        detail_view_default_content(&system->content);
        system->content_ready = false;
        return true;
    }

    DetailViewContent sanitized;
    memset(&sanitized, 0, sizeof(sanitized));
    detail_copy_string(sanitized.name, sizeof(sanitized.name), content->name);
    detail_copy_string(sanitized.lifespan, sizeof(sanitized.lifespan), content->lifespan);

    size_t fact_limit = content->fact_count;
    if (fact_limit > DETAIL_VIEW_MAX_FACTS)
    {
        fact_limit = DETAIL_VIEW_MAX_FACTS;
    }
    for (size_t index = 0; index < fact_limit; ++index)
    {
        detail_copy_string(sanitized.facts[index], sizeof(sanitized.facts[index]), content->facts[index]);
    }
    sanitized.fact_count = fact_limit;

    system->content = sanitized;
    system->content_ready = true;
    return true;
}

bool detail_view_content_ready(const DetailViewSystem *system)
{
    return system && system->initialized && system->content_ready;
}

const DetailViewContent *detail_view_get_content(const DetailViewSystem *system)
{
    if (!system || !system->initialized)
    {
        return NULL;
    }
    return &system->content;
}

bool detail_view_get_timeline_info(const DetailViewSystem *system, size_t index, DetailViewTimelineInfo *out_info)
{
    if (!system || !out_info)
    {
        return false;
    }
    if (index >= system->timeline_count)
    {
        return false;
    }
    const DetailViewTimelineSlot *slot = &system->timeline[index];
    out_info->base_angle = slot->base_angle;
    out_info->height = slot->height;
    out_info->radius = slot->radius;
    return true;
}

bool detail_view_get_panel_info(const DetailViewSystem *system, size_t index, DetailViewPanelInfo *out_info)
{
    if (!system || !out_info || !system->panel_system)
    {
        return false;
    }
    ImagePanelAnchor anchor;
    if (!image_panel_get_anchor(system->panel_system, index, &anchor))
    {
        return false;
    }
    out_info->position[0] = anchor.position[0];
    out_info->position[1] = anchor.position[1];
    out_info->position[2] = anchor.position[2];
    out_info->normal[0] = anchor.normal[0];
    out_info->normal[1] = anchor.normal[1];
    out_info->normal[2] = anchor.normal[2];
    out_info->scale = anchor.width;
    return true;
}

float detail_view_get_timeline_phase(const DetailViewSystem *system)
{
    if (!system || !system->initialized)
    {
        return 0.0f;
    }
    return detail_clamp01(system->timeline_phase);
}

float detail_view_get_panel_phase(const DetailViewSystem *system)
{
    if (!system || !system->initialized)
    {
        return 0.0f;
    }
    return detail_clamp01(system->panel_phase);
}

float detail_view_get_detail_phase(const DetailViewSystem *system)
{
    if (!system || !system->initialized)
    {
        return 0.0f;
    }
    return detail_clamp01(system->detail_phase);
}

void detail_view_update(DetailViewSystem *system, float delta_seconds, const struct ExpansionState *expansion,
                        float timeline_phase, float panel_phase)
{
    if (!system)
    {
        return;
    }
    if (!system->initialized)
    {
        if (!detail_view_init(system))
        {
            return;
        }
    }

    float safe_delta = (delta_seconds > 0.0f) ? delta_seconds : 0.0f;
    float expansion_detail = expansion ? expansion_detail_activation(expansion) : 0.0f;
    float expansion_timeline = expansion ? expansion_timeline_activation(expansion) : expansion_detail;
    float expansion_panel = expansion ? expansion_panel_activation(expansion) : expansion_detail;

    float desired_timeline = detail_clamp01(fmaxf(timeline_phase, expansion_timeline));
    float desired_panel = detail_clamp01(fmaxf(panel_phase, expansion_panel));
    float desired_detail = detail_clamp01(fmaxf(expansion_detail, desired_panel));

    float smoothing = detail_clamp01(safe_delta * DETAIL_VIEW_ROTATION_SMOOTHING);
    system->detail_phase += (desired_detail - system->detail_phase) * smoothing;
    system->timeline_phase += (desired_timeline - system->timeline_phase) * smoothing;
    system->panel_phase += (desired_panel - system->panel_phase) * smoothing;
    system->detail_target_phase = desired_detail;
    system->timeline_target_phase = desired_timeline;
    system->panel_target_phase = desired_panel;

    float rotation_speed = 0.35f + 0.9f * system->timeline_phase;
    system->rotation += safe_delta * rotation_speed;
    system->pulse_time += safe_delta;

    float full_rotation = (float)(M_PI * 2.0);
    if (system->rotation > full_rotation)
    {
        system->rotation = fmodf(system->rotation, full_rotation);
    }
    else if (system->rotation < -full_rotation)
    {
        system->rotation = fmodf(system->rotation, full_rotation);
    }

    if (system->panel_system && system->panel_count > 0U)
    {
        float view_angle = detail_wrap_angle(system->rotation * 0.55f);
        float best_difference = FLT_MAX;
        size_t best_index = system->panel_focus_index;
        for (size_t index = 0; index < system->panel_count && index < DETAIL_VIEW_MAX_PANEL_SLOTS; ++index)
        {
            float panel_angle = system->panel_angles[index];
            float diff = fabsf(view_angle - panel_angle);
            if (diff > (float)M_PI)
            {
                diff = (float)(2.0 * M_PI) - diff;
            }
            if (diff < best_difference)
            {
                best_difference = diff;
                best_index = index;
            }
        }
        system->panel_focus_index = best_index;
        bool lock_focus = system->panel_phase > 0.75f;
        system->panel_focus_locked = lock_focus;
        image_panel_interaction(system->panel_system, safe_delta, system->panel_focus_index, lock_focus);
        for (size_t index = 0; index < system->panel_count && index < DETAIL_VIEW_MAX_PANEL_SLOTS; ++index)
        {
            ImagePanelAnchor anchor;
            if (image_panel_get_anchor(system->panel_system, index, &anchor))
            {
                system->panel_angles[index] = anchor.angle;
            }
        }
    }
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

/*
 * Manual validation checklist:
 * 1. Trigger a sphere expansion and confirm the holographic room spawns with orbiting timeline nodes.
 * 2. Observe that tree lighting dims while interior cones brighten as the detail view activates.
 * 3. Reverse the expansion and ensure the room collapses smoothly without camera pops or lingering overlays.
 */

static void detail_view_draw_overlay(const DetailViewSystem *system, float activation)
{
    if (!system || !system->content_ready)
    {
        return;
    }
    float alpha = detail_clamp01(activation);
    if (alpha <= 0.01f)
    {
        return;
    }

    int screen_width = GetScreenWidth();
    int screen_height = GetScreenHeight();
    int margin = screen_width < 960 ? 24 : 48;
    unsigned char title_alpha = (unsigned char)(alpha * 245.0f);
    unsigned char text_alpha = (unsigned char)(alpha * 210.0f);

    Color title_color = {200, 240, 255, title_alpha};
    Color subtitle_color = {120, 195, 255, text_alpha};
    Color hint_color = {90, 160, 255, text_alpha};

    DrawText(system->content.name, margin, margin, 34, title_color);
    DrawText(system->content.lifespan, margin, margin + 40, 22, subtitle_color);

    int fact_y = margin + 74;
    for (size_t index = 0; index < system->content.fact_count; ++index)
    {
        DrawText(system->content.facts[index], margin, fact_y, 20, subtitle_color);
        fact_y += 26;
    }

    DrawText("Press Backspace to exit detail view", margin, screen_height - margin, 18, hint_color);
}

void detail_view_render(const DetailViewSystem *system, const struct ExpansionState *expansion,
                        const struct RenderConfig *config, const struct Camera3D *camera)
{
    (void)camera;
    if (!system || !config)
    {
        return;
    }
    assert(system->initialized && "detail_view_render requires initialized detail view state");

    float origin_array[3] = {0.0f, 0.0f, 0.0f};
    if (expansion)
    {
        expansion_current_position(expansion, origin_array);
    }
    Vector3 origin = {origin_array[0], origin_array[1], origin_array[2]};

    float base_radius = config->sphere_radius;
    if (!(base_radius > 0.0f))
    {
        base_radius = 0.6f;
    }

    float expansion_scale = expansion ? expansion_primary_scale(expansion) : 1.0f;
    if (!(expansion_scale > 0.0f))
    {
        expansion_scale = 1.0f;
    }

    float room_radius = base_radius * expansion_scale * 1.25f;
    if (room_radius < base_radius * 1.05f)
    {
        room_radius = base_radius * 1.05f;
    }

    float timeline_activation = detail_clamp01(system->timeline_phase);
    float panel_activation = detail_clamp01(system->panel_phase);
    float detail_activation = detail_clamp01(system->detail_phase);
    float room_light = expansion ? expansion_room_light_factor(expansion) : panel_activation;

    rlDisableBackfaceCulling();
    Color interior = {18, 24, 40, (unsigned char)(80 + (int)(room_light * 145.0f))};
    DrawSphereEx(origin, room_radius, 36, 36, interior);
    rlEnableBackfaceCulling();

    Vector3 ring_normal = {0.0f, 1.0f, 0.0f};
    Color ring_primary = {72, 180, 255, (unsigned char)(90 + (int)(timeline_activation * 140.0f))};
    Color ring_secondary = {42, 120, 255, (unsigned char)(70 + (int)(timeline_activation * 110.0f))};
    float base_ring_radius = room_radius * system->base_timeline_radius;
    DrawCircle3D(origin, base_ring_radius, ring_normal, 90.0f, ring_primary);
    DrawCircle3D(origin, base_ring_radius * 0.88f, ring_normal, 90.0f, ring_secondary);

    float timeline_height_scale = room_radius * 0.45f;
    Color timeline_color = {120, 220, 255, (unsigned char)(120 + (int)(timeline_activation * 135.0f))};
    for (size_t index = 0; index < system->timeline_count; ++index)
    {
        const DetailViewTimelineSlot *slot = &system->timeline[index];
        float angle = slot->base_angle + system->rotation;
        float sin_angle = sinf(angle);
        float cos_angle = cosf(angle);
        float ring_radius = base_ring_radius * slot->radius;

        Vector3 node_position = {
            origin.x + cos_angle * ring_radius,
            origin.y + slot->height * timeline_height_scale,
            origin.z + sin_angle * ring_radius};

        Vector3 stem_base = {node_position.x, origin.y - room_radius * 0.28f, node_position.z};
        float pulse = 0.36f + 0.24f * sinf(system->pulse_time * slot->pulse_speed + slot->pulse_offset);
        float node_radius = (0.18f + pulse * 0.12f) * timeline_activation * room_radius * 0.18f;
        float stem_radius = 0.02f * timeline_activation * room_radius;

        if (node_radius > 0.0f)
        {
            DrawCylinderEx(stem_base, node_position, stem_radius, stem_radius * 0.6f, 12, timeline_color);
            DrawSphere(node_position, node_radius, timeline_color);
        }
    }

    image_panel_render(system->panel_system, panel_activation, origin_array, room_radius);

    Color exit_ring = {255, 200, 140, (unsigned char)(70 + (int)(panel_activation * 150.0f))};
    Vector2 exit_center = {origin.x, origin.z};
    DrawRing(exit_center, room_radius * 0.18f, room_radius * 0.21f, 0.0f, 360.0f, 48, exit_ring);

    detail_view_draw_overlay(system, detail_activation);
}

#else

void detail_view_render(const DetailViewSystem *system, const struct ExpansionState *expansion,
                        const struct RenderConfig *config, const struct Camera3D *camera)
{
    (void)system;
    (void)expansion;
    (void)config;
    (void)camera;
}

#endif
