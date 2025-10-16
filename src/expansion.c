#include "expansion.h"

#include "camera_controller.h"
#include "layout.h"

#include <math.h>
#include <string.h>
#include <stddef.h>

static float clamp01(float value)
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

static float ease_out_cubic(float t)
{
    float inv = 1.0f - t;
    return 1.0f - (inv * inv * inv);
}

static void vector_copy(float destination[3], const float source[3])
{
    if (!destination)
    {
        return;
    }
    if (!source)
    {
        destination[0] = 0.0f;
        destination[1] = 0.0f;
        destination[2] = 0.0f;
        return;
    }
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

static void vector_subtract(const float a[3], const float b[3], float out[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static float vector_length(const float v[3])
{
    return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static void vector_scale(float v[3], float scale)
{
    v[0] *= scale;
    v[1] *= scale;
    v[2] *= scale;
}

static void vector_add(float v[3], const float addition[3])
{
    v[0] += addition[0];
    v[1] += addition[1];
    v[2] += addition[2];
}

static void vector_lerp(const float a[3], const float b[3], float t, float out[3])
{
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

void expansion_state_reset(ExpansionState *state)
{
    if (!state)
    {
        return;
    }
    state->active = false;
    state->animating = false;
    state->reversing = false;
    state->detail_mode = false;
    state->person = NULL;
    state->elapsed = 0.0f;
    state->duration = 0.6f;
    memset(state->origin_position, 0, sizeof(state->origin_position));
    memset(state->detail_position, 0, sizeof(state->detail_position));
    memset(state->current_position, 0, sizeof(state->current_position));
    memset(state->camera_target_start, 0, sizeof(state->camera_target_start));
    memset(state->camera_target_detail, 0, sizeof(state->camera_target_detail));
    state->camera_radius_start = 0.0f;
    state->camera_radius_detail = 0.0f;
    state->radius_source = 1.0f;
    state->radius_target = 1.0f;
    state->radius_current = 1.0f;
    state->inactive_source = 1.0f;
    state->inactive_target = 1.0f;
    state->inactive_current = 1.0f;
    state->fade_source = 1.0f;
    state->fade_target = 1.0f;
    state->fade_current = 1.0f;
}

static bool find_person_position(const LayoutResult *layout, const struct Person *person, float out_position[3])
{
    if (!layout || !person || !out_position)
    {
        return false;
    }
    for (size_t index = 0; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        if (node->person == person)
        {
            out_position[0] = node->position[0];
            out_position[1] = node->position[1];
            out_position[2] = node->position[2];
            return true;
        }
    }
    return false;
}

bool expansion_start(ExpansionState *state, const LayoutResult *layout, const struct Person *person,
                     CameraController *camera)
{
    if (!state || !layout || !person || layout->count == 0U)
    {
        return false;
    }

    float target_position[3];
    if (!find_person_position(layout, person, target_position))
    {
        return false;
    }

    vector_copy(state->origin_position, target_position);
    vector_copy(state->detail_position, target_position);
    vector_copy(state->current_position, target_position);

    state->active = true;
    state->animating = true;
    state->reversing = false;
    state->detail_mode = false;
    state->person = person;
    state->elapsed = 0.0f;
    state->duration = 0.65f;
    state->radius_source = 1.0f;
    state->radius_target = 3.4f;
    state->radius_current = 1.0f;
    state->inactive_source = 1.0f;
    state->inactive_target = 0.35f;
    state->inactive_current = 1.0f;
    state->fade_source = 1.0f;
    state->fade_target = 0.15f;
    state->fade_current = 1.0f;
    state->camera_radius_start = 0.0f;
    state->camera_radius_detail = 0.0f;

    if (camera)
    {
        vector_copy(state->camera_target_start, camera->target);
        float focus_radius = camera->radius * 0.55f;
        if (!(focus_radius > 0.0f))
        {
            focus_radius = camera->config.default_radius * 0.6f;
        }

        vector_copy(state->camera_target_detail, target_position);
        state->camera_radius_start = camera->radius;
        state->camera_radius_detail = focus_radius;

        float to_camera[3];
        vector_subtract(camera->position, camera->target, to_camera);
        float distance = vector_length(to_camera);
        if (distance > 0.0001f)
        {
            float advance = distance * 0.3f;
            vector_scale(to_camera, 1.0f / distance);
            vector_scale(to_camera, advance);
            float offset_target[3];
            vector_copy(offset_target, target_position);
            vector_add(offset_target, to_camera);
            vector_copy(state->detail_position, offset_target);
            vector_copy(state->camera_target_detail, offset_target);
        }

        camera_controller_focus(camera, state->camera_target_detail, focus_radius);
    }

    return true;
}

bool expansion_update(ExpansionState *state, float delta_seconds, CameraController *camera)
{
    if (!state || !state->active)
    {
        return false;
    }

    if (!state->animating)
    {
        return false;
    }

    if (!(delta_seconds > 0.0f))
    {
        delta_seconds = 0.0f;
    }

    state->elapsed += delta_seconds;
    float t = clamp01(state->elapsed / fmaxf(state->duration, 0.0001f));
    float eased = ease_out_cubic(t);
    float blend = state->reversing ? (1.0f - eased) : eased;

    state->radius_current = state->radius_source + (state->radius_target - state->radius_source) * blend;
    state->inactive_current = state->inactive_source + (state->inactive_target - state->inactive_source) * blend;
    state->fade_current = state->fade_source + (state->fade_target - state->fade_source) * blend;
    vector_lerp(state->origin_position, state->detail_position, blend, state->current_position);

    if (camera && state->person)
    {
        float target_radius = state->reversing ? state->camera_radius_start : state->camera_radius_detail;
        if (!(target_radius > 0.0f))
        {
            target_radius = camera->config.default_radius;
        }
        float focus_radius = camera->radius + (target_radius - camera->radius) * clamp01(delta_seconds * 5.0f);

        float desired_target[3];
        const float *target_a = state->reversing ? state->camera_target_detail : state->camera_target_start;
        const float *target_b = state->reversing ? state->camera_target_start : state->camera_target_detail;
        vector_lerp(target_a, target_b, eased, desired_target);

        camera_controller_focus(camera, desired_target, focus_radius);
    }

    if (t >= 1.0f)
    {
        if (state->reversing)
        {
            expansion_state_reset(state);
        }
        else
        {
            expansion_complete(state);
        }
        return true;
    }
    return false;
}

void expansion_complete(ExpansionState *state)
{
    if (!state)
    {
        return;
    }
    state->animating = false;
    state->reversing = false;
    state->detail_mode = true;
    state->elapsed = state->duration;
    state->radius_current = state->radius_target;
    state->inactive_current = state->inactive_target;
    state->fade_current = state->fade_target;
    vector_copy(state->current_position, state->detail_position);
}

void expansion_reverse(ExpansionState *state, CameraController *camera)
{
    if (!state)
    {
        return;
    }
    if (!state->active)
    {
        return;
    }
    state->animating = true;
    state->reversing = true;
    state->detail_mode = false;
    state->elapsed = 0.0f;
    if (camera)
    {
        float focus_radius = state->camera_radius_start > 0.0f ? state->camera_radius_start : camera->config.default_radius;
        camera_controller_focus(camera, state->camera_target_start, focus_radius);
    }
}

bool expansion_is_active(const ExpansionState *state)
{
    return state && state->active;
}

bool expansion_is_reversing(const ExpansionState *state)
{
    return state && state->reversing;
}

bool expansion_is_in_detail_mode(const ExpansionState *state)
{
    return state && state->detail_mode;
}

float expansion_primary_scale(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 1.0f;
    }
    return state->radius_current;
}

float expansion_inactive_scale(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 1.0f;
    }
    return state->inactive_current;
}

float expansion_inactive_opacity(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 1.0f;
    }
    if (state->fade_current < 0.0f)
    {
        return 0.0f;
    }
    if (state->fade_current > 1.0f)
    {
        return 1.0f;
    }
    return state->fade_current;
}

void expansion_current_position(const ExpansionState *state, float out_position[3])
{
    if (!out_position)
    {
        return;
    }
    if (!state || !state->active)
    {
        out_position[0] = 0.0f;
        out_position[1] = 0.0f;
        out_position[2] = 0.0f;
        return;
    }
    vector_copy(out_position, state->current_position);
}
