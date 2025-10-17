#!/usr/bin/env python3
"""Script to create detail_view.c file"""

content = r"""#include "detail_view.h"

#include "expansion.h"
#include "render.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct DetailViewTimelineSlot
{
    float base_angle;
    float height;
    float radius;
    float pulse_speed;
    float pulse_offset;
} DetailViewTimelineSlot;

typedef struct DetailViewPanelSlot
{
    float base_angle;
    float height;
    float scale;
    float sway_speed;
    float sway_offset;
} DetailViewPanelSlot;

struct DetailViewSystem
{
    bool initialized;
    float rotation;
    float pulse_time;
    float timeline_phase;
    float panel_phase;
    DetailViewTimelineSlot timeline[12];
    size_t timeline_count;
    DetailViewPanelSlot panels[6];
    size_t panel_count;
};

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

static void detail_view_configure_slots(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }

    system->timeline_count = 8U;
    if (system->timeline_count > (sizeof(system->timeline) / sizeof(system->timeline[0])))
    {
        system->timeline_count = sizeof(system->timeline) / sizeof(system->timeline[0]);
    }

    const float angle_step = (float)(2.0 * M_PI / (double)system->timeline_count);
    for (size_t index = 0; index < system->timeline_count; ++index)
    {
        DetailViewTimelineSlot *slot = &system->timeline[index];
        slot->base_angle = angle_step * (float)index;
        slot->height = (index % 2U == 0U) ? 0.38f : -0.32f;
        slot->radius = 0.74f;
        slot->pulse_speed = 0.85f + 0.12f * (float)(index % 3U);
        slot->pulse_offset = (float)index * 0.58f;
    }

    system->panel_count = 5U;
    if (system->panel_count > (sizeof(system->panels) / sizeof(system->panels[0])))
    {
        system->panel_count = sizeof(system->panels) / sizeof(system->panels[0]);
    }

    const float panel_step = (float)(2.0 * M_PI / (double)system->panel_count);
    for (size_t index = 0; index < system->panel_count; ++index)
    {
        DetailViewPanelSlot *slot = &system->panels[index];
        slot->base_angle = panel_step * (float)index + 0.25f;
        slot->height = 0.65f - 0.18f * (float)(index % 3U);
        slot->scale = 1.15f + 0.12f * (float)(index % 2U);
        slot->sway_speed = 0.65f + 0.08f * (float)(index % 4U);
        slot->sway_offset = (float)index * 0.43f;
    }
}

DetailViewSystem *detail_view_create(void)
{
    DetailViewSystem *system = (DetailViewSystem *)calloc(1, sizeof(DetailViewSystem));
    if (!system)
    {
        return NULL;
    }
    detail_view_reset(system);
    return system;
}

void detail_view_destroy(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    free(system);
}

void detail_view_reset(DetailViewSystem *system)
{
    if (!system)
    {
        return;
    }
    memset(system, 0, sizeof(*system));
    detail_view_configure_slots(system);
    system->initialized = true;
}

void detail_view_update(DetailViewSystem *system, float delta_seconds, const struct ExpansionState *expansion,
                        float timeline_phase, float panel_phase)
{
    (void)expansion;
    if (!system)
    {
        return;
    }
    if (!system->initialized)
    {
        detail_view_reset(system);
    }

    assert(system->initialized && "detail_view_update requires initialized detail view state");

    float safe_delta = (delta_seconds > 0.0f) ? delta_seconds : 0.0f;
    float activation = detail_clamp01(timeline_phase);

    const float rotation_speed = 0.45f + 0.75f * activation;
    system->rotation += safe_delta * rotation_speed;
    system->pulse_time += safe_delta;
    system->timeline_phase = activation;
    system->panel_phase = detail_clamp01(panel_phase);
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

    float expansion_scale = 1.0f;
    if (expansion)
    {
        expansion_scale = expansion_primary_scale(expansion);
        if (!(expansion_scale > 0.0f))
        {
            expansion_scale = 1.0f;
        }
    }

    float room_radius = base_radius * expansion_scale * 1.25f;
    float activation = detail_clamp01(system->timeline_phase);
    float panel_activation = detail_clamp01(system->panel_phase);

    rlDisableBackfaceCulling();
    Color interior = {18, 24, 40, (unsigned char)(110 + (int)(activation * 120.0f))};
    DrawSphereEx(origin, room_radius, 36, 36, interior);
    rlEnableBackfaceCulling();

    Vector3 ring_normal = {0.0f, 1.0f, 0.0f};
    Color ring_primary = {72, 180, 255, (unsigned char)(90 + (int)(activation * 140.0f))};
    Color ring_secondary = {42, 120, 255, (unsigned char)(70 + (int)(activation * 110.0f))};
    float ring_radius = room_radius * 0.72f;
    DrawCircle3D(origin, ring_radius, ring_normal, 90.0f, ring_primary);
    DrawCircle3D(origin, ring_radius * 0.88f, ring_normal, 90.0f, ring_secondary);

    Color timeline_color = {120, 220, 255, (unsigned char)(120 + (int)(activation * 135.0f))};
    for (size_t index = 0; index < system->timeline_count; ++index)
    {
        const DetailViewTimelineSlot *slot = &system->timeline[index];
        float angle = slot->base_angle + system->rotation;
        float sin_angle = sinf(angle);
        float cos_angle = cosf(angle);

        Vector3 node_position = {
            origin.x + cos_angle * ring_radius,
            origin.y + slot->height,
            origin.z + sin_angle * ring_radius};

        Vector3 stem_base = {node_position.x, origin.y - 0.28f, node_position.z};
        float pulse = 0.36f + 0.24f * sinf(system->pulse_time * slot->pulse_speed + slot->pulse_offset);
        float node_radius = (0.18f + pulse * 0.12f) * activation;

        if (node_radius > 0.0f)
        {
            DrawCylinderEx(stem_base, node_position, 0.03f * activation, 0.012f * activation, 12, timeline_color);
            DrawSphere(node_position, node_radius, timeline_color);
        }
    }

    Color panel_face = {200, 228, 255, (unsigned char)(95 + (int)(panel_activation * 135.0f))};
    Color panel_wire = {90, 160, 255, panel_face.a};
    Color cone_light = {140, 210, 255, (unsigned char)(60 + (int)(panel_activation * 150.0f))};
    float panel_radius = room_radius * 0.58f;

    for (size_t index = 0; index < system->panel_count; ++index)
    {
        const DetailViewPanelSlot *slot = &system->panels[index];
        float angle = slot->base_angle + system->rotation * 0.55f;
        float sin_angle = sinf(angle);
        float cos_angle = cosf(angle);

        Vector3 panel_center = {
            origin.x + cos_angle * panel_radius,
            origin.y + slot->height,
            origin.z + sin_angle * panel_radius};

        Vector3 cone_base = {panel_center.x, origin.y - room_radius * 0.42f, panel_center.z};
        DrawCylinder(cone_base, panel_center, 0.05f * panel_activation, 0.0f, 18, cone_light);

        float sway = sinf(system->pulse_time * slot->sway_speed + slot->sway_offset) * 12.0f;
        float panel_scale = slot->scale * (0.9f + 0.3f * panel_activation);

        rlPushMatrix();
        rlTranslatef(panel_center.x, panel_center.y, panel_center.z);
        rlRotatef((float)(angle * (180.0 / M_PI)) + 90.0f + sway, 0.0f, 1.0f, 0.0f);
        rlRotatef(-12.0f, 1.0f, 0.0f, 0.0f);
        Vector3 panel_size = {panel_scale, panel_scale * 0.55f, 0.08f};
        DrawCubeV((Vector3){0.0f, 0.0f, 0.0f}, panel_size, panel_face);
        DrawCubeWiresV((Vector3){0.0f, 0.0f, 0.0f}, panel_size, panel_wire);
        rlPopMatrix();
    }

    Color exit_ring = {255, 200, 140, (unsigned char)(70 + (int)(panel_activation * 150.0f))};
    Vector3 exit_center = {origin.x, origin.y - room_radius * 0.45f, origin.z};
    DrawRing(exit_center, room_radius * 0.18f, room_radius * 0.2f, 0.0f, 360.0f, 32, exit_ring);
}

#else

struct DetailViewSystem
{
    int unused;
};

DetailViewSystem *detail_view_create(void)
{
    return NULL;
}

void detail_view_destroy(DetailViewSystem *system)
{
    (void)system;
}

void detail_view_reset(DetailViewSystem *system)
{
    (void)system;
}

void detail_view_update(DetailViewSystem *system, float delta_seconds, const struct ExpansionState *expansion,
                        float timeline_phase, float panel_phase)
{
    (void)system;
    (void)delta_seconds;
    (void)expansion;
    (void)timeline_phase;
    (void)panel_phase;
}

void detail_view_render(const DetailViewSystem *system, const struct ExpansionState *expansion,
                        const struct RenderConfig *config, const struct Camera3D *camera)
{
    (void)system;
    (void)expansion;
    (void)config;
    (void)camera;
}

#endif
"""

import os

output_path = os.path.join('src', 'detail_view.c')
with open(output_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write(content)

print(f"Successfully created {output_path}")
