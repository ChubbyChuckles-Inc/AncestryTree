#include "expansion.h"

#include "camera_controller.h"
#include "layout.h"

#include <math.h>
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
    state->radius_source = 1.0f;
    state->radius_target = 1.0f;
    state->radius_current = 1.0f;
    state->inactive_source = 1.0f;
    state->inactive_target = 1.0f;
    state->inactive_current = 1.0f;
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

    if (camera)
    {
        float focus_radius = camera->radius * 0.55f;
        if (!(focus_radius > 0.0f))
        {
            focus_radius = camera->config.default_radius * 0.6f;
        }
        camera_controller_focus(camera, target_position, focus_radius);
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

    if (camera && state->person)
    {
        /* Maintain focus radius progression to avoid abrupt pop when reversing. */
        float desired_radius = camera->config.default_radius;
        if (!state->reversing)
        {
            desired_radius *= 0.65f;
        }
        float lerp_radius = camera->radius + (desired_radius - camera->radius) * clamp01(delta_seconds * 4.0f);
        camera->radius = lerp_radius;
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
        camera_controller_focus(camera, camera->config.target, camera->config.default_radius);
    }
}

bool expansion_is_active(const ExpansionState *state)
{
    return state && state->active;
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
