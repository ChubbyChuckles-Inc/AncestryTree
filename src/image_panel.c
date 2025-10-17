#include "image_panel.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct ImagePanelSystem
{
    bool initialized;
    ImagePanelLayoutParams params;
    size_t count;
    float base_positions[IMAGE_PANEL_MAX_COUNT][3];
    float base_angles[IMAGE_PANEL_MAX_COUNT];
    float occlusion_offsets[IMAGE_PANEL_MAX_COUNT];
    float focus_weights[IMAGE_PANEL_MAX_COUNT];
    ImagePanelAnchor anchors[IMAGE_PANEL_MAX_COUNT];
};

static void ip_copy3(float dst[3], const float src[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void ip_cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static float ip_length3(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void ip_normalize3(float v[3])
{
    float length = ip_length3(v);
    if (length > 0.0001f)
    {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
    else
    {
        v[0] = 0.0f;
        v[1] = 1.0f;
        v[2] = 0.0f;
    }
}

static float ip_wrap_angle(float angle)
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

static float ip_abs(float value)
{
    return value >= 0.0f ? value : -value;
}

ImagePanelSystem *image_panel_create(void)
{
    ImagePanelSystem *system = (ImagePanelSystem *)calloc(1, sizeof(ImagePanelSystem));
    if (!system)
    {
        return NULL;
    }
    return system;
}

void image_panel_destroy(ImagePanelSystem *system)
{
    if (!system)
    {
        return;
    }
    free(system);
}

void image_panel_reset(ImagePanelSystem *system)
{
    if (!system)
    {
        return;
    }
    memset(system, 0, sizeof(*system));
}

static bool ip_validate_params(const ImagePanelLayoutParams *params)
{
    if (!params)
    {
        return false;
    }
    if (params->desired_count == 0U)
    {
        return false;
    }
    if (!(params->cone_inner_radius > 0.0f) || !(params->cone_outer_radius > 0.0f))
    {
        return false;
    }
    if (params->cone_inner_radius > params->cone_outer_radius)
    {
        return false;
    }
    if (params->min_height > params->max_height)
    {
        return false;
    }
    if (params->timeline_clear_margin < 0.0f)
    {
        return false;
    }
    return true;
}

bool image_panel_layout_init(ImagePanelSystem *system, const ImagePanelLayoutParams *params)
{
    if (!system || !ip_validate_params(params))
    {
        return false;
    }

    ImagePanelLayoutParams sanitized = *params;
    if (sanitized.min_angle_radians <= 0.0f)
    {
        sanitized.min_angle_radians = (float)(M_PI / 6.0);
    }

    size_t count = sanitized.desired_count;
    if (count > IMAGE_PANEL_MAX_COUNT)
    {
        count = IMAGE_PANEL_MAX_COUNT;
    }
    if (count == 0U)
    {
        return false;
    }
    if (count > 1U)
    {
        float separation_limit = (float)(2.0 * M_PI) / sanitized.min_angle_radians;
        size_t max_by_separation = (size_t)floorf(separation_limit + 1.0f); /* +1 to soften truncation */
        if (max_by_separation == 0U)
        {
            max_by_separation = 1U;
        }
        if (count > max_by_separation)
        {
            count = max_by_separation;
        }
    }

    memset(system, 0, sizeof(*system));
    system->params = sanitized;
    system->count = count;
    system->initialized = true;

    float height_range = sanitized.max_height - sanitized.min_height;
    float radius_range = sanitized.cone_outer_radius - sanitized.cone_inner_radius;
    const float base_width = 0.48f;
    const float base_height = 0.28f;

    for (size_t index = 0; index < count; ++index)
    {
        float fraction = ((float)index + 0.5f) / (float)count;
        float angle = (float)(2.0 * M_PI * fraction);
        float height = sanitized.min_height + fraction * (height_range);
        if (!(height_range > 0.0f))
        {
            height = sanitized.min_height;
        }
        float height_t = (height_range > 0.0001f) ? (height - sanitized.min_height) / height_range : 0.0f;
        if (height_t < 0.0f)
        {
            height_t = 0.0f;
        }
        if (height_t > 1.0f)
        {
            height_t = 1.0f;
        }
        float radius = sanitized.cone_inner_radius + height_t * radius_range;
        float position[3] = {
            cosf(angle) * radius,
            height,
            sinf(angle) * radius};

        system->base_angles[index] = ip_wrap_angle(angle);
        ip_copy3(system->base_positions[index], position);
        system->occlusion_offsets[index] = 0.0f;
        system->focus_weights[index] = 0.0f;

        ImagePanelAnchor anchor;
        memset(&anchor, 0, sizeof(anchor));
        ip_copy3(anchor.position, position);

        float to_origin[3] = {-position[0], -position[1], -position[2]};
        float cone_axis[3] = {0.0f, -1.0f, 0.0f};
        ip_normalize3(to_origin);
        float blended[3] = {
            to_origin[0] * 0.82f + cone_axis[0] * 0.18f,
            to_origin[1] * 0.82f + cone_axis[1] * 0.18f,
            to_origin[2] * 0.82f + cone_axis[2] * 0.18f};
        ip_normalize3(blended);

        float up_hint[3] = {0.0f, 1.0f, 0.0f};
        float tangent[3];
        ip_cross3(up_hint, blended, tangent);
        if (ip_length3(tangent) < 0.001f)
        {
            float backup[3] = {1.0f, 0.0f, 0.0f};
            ip_cross3(backup, blended, tangent);
        }
        ip_normalize3(tangent);
        float up[3];
        ip_cross3(blended, tangent, up);
        ip_normalize3(up);

        ip_copy3(anchor.normal, blended);
        ip_copy3(anchor.up, up);

        float radial = sqrtf(position[0] * position[0] + position[2] * position[2]);
        float difference = ip_abs(radial - sanitized.timeline_radius);
        float height_delta = ip_abs(position[1] - sanitized.timeline_height);
        bool sightline_clear = (difference >= sanitized.timeline_clear_margin) ||
                               (height_delta >= sanitized.timeline_clear_margin);

        anchor.width = base_width;
        anchor.height = base_height;
        anchor.occlusion_score = difference;
        anchor.sightline_clear = sightline_clear;
        anchor.focus_weight = 0.0f;
        anchor.angle = system->base_angles[index];

        system->anchors[index] = anchor;
    }

    return true;
}

static void ip_update_anchor(ImagePanelSystem *system, size_t index, float delta_seconds, size_t focus_index,
                             bool focus_lock)
{
    ImagePanelAnchor *anchor = &system->anchors[index];
    const float *base_position = system->base_positions[index];
    float radial = sqrtf(base_position[0] * base_position[0] + base_position[2] * base_position[2]);
    float base_height = base_position[1];

    float difference = ip_abs(radial - system->params.timeline_radius);
    float height_delta = ip_abs(base_height - system->params.timeline_height);

    float occlusion_target = 0.0f;
    bool clear = true;
    if (difference < system->params.timeline_clear_margin)
    {
        occlusion_target = system->params.timeline_clear_margin - difference;
        clear = false;
    }
    if (height_delta < system->params.timeline_clear_margin)
    {
        float deficit = system->params.timeline_clear_margin - height_delta;
        if (deficit > occlusion_target)
        {
            occlusion_target = deficit;
        }
        clear = false;
    }

    float smoothing = delta_seconds > 0.0f ? delta_seconds : 0.0f;
    if (smoothing > 0.25f)
    {
        smoothing = 0.25f;
    }

    float occlusion_smooth = smoothing * 6.0f;
    if (occlusion_smooth > 1.0f)
    {
        occlusion_smooth = 1.0f;
    }
    float focus_smooth = smoothing * 8.0f;
    if (focus_smooth > 1.0f)
    {
        focus_smooth = 1.0f;
    }

    float focus_target = (focus_lock && index == focus_index && index < system->count) ? 1.0f : 0.0f;
    system->focus_weights[index] += (focus_target - system->focus_weights[index]) * focus_smooth;
    anchor->focus_weight = system->focus_weights[index];

    system->occlusion_offsets[index] += (occlusion_target - system->occlusion_offsets[index]) * occlusion_smooth;

    float focus_scale = 1.0f - anchor->focus_weight * 0.22f;
    if (focus_scale < 0.65f)
    {
        focus_scale = 0.65f;
    }

    anchor->position[0] = base_position[0] * focus_scale;
    anchor->position[2] = base_position[2] * focus_scale;
    anchor->position[1] = base_height + system->occlusion_offsets[index] * 1.4f + anchor->focus_weight * 0.06f;

    float to_origin[3] = {-anchor->position[0], -(anchor->position[1] - system->params.timeline_height * 0.15f),
                          -anchor->position[2]};
    float cone_axis[3] = {0.0f, -1.0f, 0.0f};
    ip_normalize3(to_origin);
    float blended[3] = {
        to_origin[0] * 0.78f + cone_axis[0] * 0.22f,
        to_origin[1] * 0.78f + cone_axis[1] * 0.22f,
        to_origin[2] * 0.78f + cone_axis[2] * 0.22f};
    ip_normalize3(blended);

    float up_hint[3] = {0.0f, 1.0f, 0.0f};
    float tangent[3];
    ip_cross3(up_hint, blended, tangent);
    if (ip_length3(tangent) < 0.001f)
    {
        float backup[3] = {1.0f, 0.0f, 0.0f};
        ip_cross3(backup, blended, tangent);
    }
    ip_normalize3(tangent);
    float up[3];
    ip_cross3(blended, tangent, up);
    ip_normalize3(up);

    ip_copy3(anchor->normal, blended);
    ip_copy3(anchor->up, up);

    float new_radial = sqrtf(anchor->position[0] * anchor->position[0] + anchor->position[2] * anchor->position[2]);
    float new_height_delta = ip_abs(anchor->position[1] - system->params.timeline_height);
    bool final_clear = clear;
    if (!final_clear)
    {
        final_clear = (new_height_delta >= system->params.timeline_clear_margin);
    }
    anchor->sightline_clear = final_clear || (new_radial - system->params.timeline_radius >= system->params.timeline_clear_margin);
    anchor->occlusion_score = difference;

    anchor->width = 0.48f * (1.0f + anchor->focus_weight * 0.35f);
    anchor->height = anchor->width * 0.62f;
    anchor->angle = ip_wrap_angle(atan2f(anchor->position[2], anchor->position[0]));
}

void image_panel_interaction(ImagePanelSystem *system, float delta_seconds, size_t focus_index, bool focus_lock)
{
    if (!system || !system->initialized || system->count == 0U)
    {
        return;
    }

    for (size_t index = 0; index < system->count; ++index)
    {
        ip_update_anchor(system, index, delta_seconds, focus_index, focus_lock);
    }
}

size_t image_panel_count(const ImagePanelSystem *system)
{
    return system ? system->count : 0U;
}

bool image_panel_get_anchor(const ImagePanelSystem *system, size_t index, ImagePanelAnchor *out_anchor)
{
    if (!system || !out_anchor)
    {
        return false;
    }
    if (index >= system->count)
    {
        return false;
    }
    *out_anchor = system->anchors[index];
    return true;
}

bool image_panel_is_focus_active(const ImagePanelSystem *system, size_t index)
{
    if (!system || index >= system->count)
    {
        return false;
    }
    return system->focus_weights[index] > 0.55f;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

/*
 * Manual validation checklist:
 * 1. Enter the detail view and confirm cone-of-light beams align with floating panels.
 * 2. Rotate around the room; ensure panels glide without intersecting the circular timeline.
 * 3. Hover near a panel and observe the focus swell and inward drift while neighbouring panels keep their spacing.
 */

static void ip_draw_panel(const ImagePanelAnchor *anchor, const float origin[3], float room_radius, float activation)
{
    Vector3 normal = {anchor->normal[0], anchor->normal[1], anchor->normal[2]};
    Vector3 up = {anchor->up[0], anchor->up[1], anchor->up[2]};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, normal));
    up = Vector3Normalize(Vector3CrossProduct(normal, right));

    float width = anchor->width * room_radius;
    float height = anchor->height * room_radius;
    Vector3 half_right = Vector3Scale(right, width * 0.5f);
    Vector3 half_up = Vector3Scale(up, height * 0.5f);

    Vector3 center = {
        origin[0] + anchor->position[0] * room_radius,
        origin[1] + anchor->position[1] * room_radius,
        origin[2] + anchor->position[2] * room_radius};

    Vector3 p0 = Vector3Add(Vector3Subtract(center, half_right), half_up);
    Vector3 p1 = Vector3Add(Vector3Add(center, half_right), half_up);
    Vector3 p2 = Vector3Subtract(Vector3Add(center, half_right), half_up);
    Vector3 p3 = Vector3Subtract(Vector3Subtract(center, half_right), half_up);

    Color face_color = {200, 232, 255, (unsigned char)(80 + (int)(activation * 140.0f))};
    Color rim_color = {90, 165, 255, (unsigned char)(70 + (int)(activation * 150.0f))};
    Color highlight = {255, 240, 180, (unsigned char)(60 + (int)(anchor->focus_weight * 120.0f))};

    rlDisableTexture();
    rlBegin(RL_QUADS);
    rlColor4ub(face_color.r, face_color.g, face_color.b, face_color.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlVertex3f(p0.x, p0.y, p0.z);
    rlVertex3f(p1.x, p1.y, p1.z);
    rlVertex3f(p2.x, p2.y, p2.z);
    rlVertex3f(p3.x, p3.y, p3.z);
    rlEnd();

    rlBegin(RL_LINES);
    rlColor4ub(rim_color.r, rim_color.g, rim_color.b, rim_color.a);
    rlVertex3f(p0.x, p0.y, p0.z);
    rlVertex3f(p1.x, p1.y, p1.z);
    rlVertex3f(p1.x, p1.y, p1.z);
    rlVertex3f(p2.x, p2.y, p2.z);
    rlVertex3f(p2.x, p2.y, p2.z);
    rlVertex3f(p3.x, p3.y, p3.z);
    rlVertex3f(p3.x, p3.y, p3.z);
    rlVertex3f(p0.x, p0.y, p0.z);
    rlEnd();

    if (anchor->focus_weight > 0.01f)
    {
        rlBegin(RL_LINES);
        rlColor4ub(highlight.r, highlight.g, highlight.b, highlight.a);
        rlVertex3f(center.x, center.y + height * 0.55f, center.z);
        rlVertex3f(center.x, center.y + height * 0.85f, center.z);
        rlEnd();
    }

    Vector3 cone_base = {center.x, origin[1] - room_radius * 0.45f, center.z};
    float cone_height = Vector3Distance(cone_base, center);
    float cone_radius = width * 0.18f + anchor->focus_weight * width * 0.12f;
    Color cone_color = {120, 210, 255, (unsigned char)(55 + (int)(activation * 130.0f))};
    DrawCylinder(cone_base, cone_radius, 0.0f, cone_height, 16, cone_color);
}

void image_panel_render(const ImagePanelSystem *system, float activation, const float origin[3], float room_radius)
{
    if (!system || system->count == 0U)
    {
        return;
    }

    float clamped_activation = activation;
    if (clamped_activation < 0.0f)
    {
        clamped_activation = 0.0f;
    }
    if (clamped_activation > 1.0f)
    {
        clamped_activation = 1.0f;
    }

    for (size_t index = 0; index < system->count; ++index)
    {
        ip_draw_panel(&system->anchors[index], origin, room_radius, clamped_activation);
    }
}

#else

void image_panel_render(const ImagePanelSystem *system, float activation, const float origin[3], float room_radius)
{
    (void)system;
    (void)activation;
    (void)origin;
    (void)room_radius;
}

#endif
