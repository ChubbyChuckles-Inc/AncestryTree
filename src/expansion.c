/**
 * @file expansion.c
 * Implements the immersive sphere expansion animation and associated lighting state transitions.
 */

#include "expansion.h"

#include "camera_controller.h"
#include "layout.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define EXPANSION_DEFAULT_DURATION 0.75f
#define EXPANSION_RETURN_DURATION 0.65f
#define EXPANSION_MIN_RADIUS 0.6f

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

static void vector_lerp(const float a[3], const float b[3], float t, float out[3])
{
    out[0] = a[0] + (b[0] - a[0]) * t;
    out[1] = a[1] + (b[1] - a[1]) * t;
    out[2] = a[2] + (b[2] - a[2]) * t;
}

static void expansion_configure_targets(ExpansionState *state, bool entering_detail)
{
    if (!state)
    {
        return;
    }

    if (entering_detail)
    {
        state->radius_source     = state->radius_current;
        state->radius_target     = state->detail_radius;
        state->inactive_source   = state->inactive_current;
        state->inactive_target   = 0.32f;
        state->fade_source       = state->fade_current;
        state->fade_target       = 0.12f;
        state->tree_light_source = state->tree_light_current;
        state->tree_light_target = 0.28f;
        state->room_light_source = state->room_light_current;
        state->room_light_target = 1.0f;
    }
    else
    {
        state->radius_source     = state->radius_current;
        state->radius_target     = state->baseline_radius;
        state->inactive_source   = state->inactive_current;
        state->inactive_target   = 1.0f;
        state->fade_source       = state->fade_current;
        state->fade_target       = 1.0f;
        state->tree_light_source = state->tree_light_current;
        state->tree_light_target = 1.0f;
        state->room_light_source = state->room_light_current;
        state->room_light_target = 0.0f;
    }
}

void expansion_state_reset(ExpansionState *state)
{
    if (!state)
    {
        return;
    }
    state->active           = false;
    state->animating        = false;
    state->reversing        = false;
    state->detail_mode      = false;
    state->person           = NULL;
    state->elapsed          = 0.0f;
    state->duration         = EXPANSION_DEFAULT_DURATION;
    state->progress_current = 0.0f;
    state->baseline_radius  = 1.0f;
    state->detail_radius    = 3.2f;
    memset(state->origin_position, 0, sizeof(state->origin_position));
    memset(state->detail_position, 0, sizeof(state->detail_position));
    memset(state->current_position, 0, sizeof(state->current_position));
    memset(state->camera_target_start, 0, sizeof(state->camera_target_start));
    memset(state->camera_target_detail, 0, sizeof(state->camera_target_detail));
    state->camera_radius_start  = 0.0f;
    state->camera_radius_detail = 0.0f;
    state->radius_source        = state->baseline_radius;
    state->radius_target        = state->baseline_radius;
    state->radius_current       = state->baseline_radius;
    state->inactive_source      = 1.0f;
    state->inactive_target      = 1.0f;
    state->inactive_current     = 1.0f;
    state->fade_source          = 1.0f;
    state->fade_target          = 1.0f;
    state->fade_current         = 1.0f;
    state->tree_light_source    = 1.0f;
    state->tree_light_target    = 1.0f;
    state->tree_light_current   = 1.0f;
    state->room_light_source    = 0.0f;
    state->room_light_target    = 0.0f;
    state->room_light_current   = 0.0f;
}

static bool find_person_position(const LayoutResult *layout, const struct Person *person,
                                 float out_position[3])
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

static float expansion_compute_detail_radius(const ExpansionState *state)
{
    float base   = (state && state->baseline_radius > 0.0f) ? state->baseline_radius : 1.0f;
    float scaled = base * 3.25f;
    if (scaled < base + 1.5f)
    {
        scaled = base + 1.5f;
    }
    if (scaled < EXPANSION_MIN_RADIUS * 2.5f)
    {
        scaled = EXPANSION_MIN_RADIUS * 2.5f;
    }
    return scaled;
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

    if (state->active)
    {
        expansion_state_reset(state);
    }

    vector_copy(state->origin_position, target_position);
    vector_copy(state->current_position, target_position);

    state->active           = true;
    state->animating        = true;
    state->reversing        = false;
    state->detail_mode      = false;
    state->person           = person;
    state->elapsed          = 0.0f;
    state->duration         = EXPANSION_DEFAULT_DURATION;
    state->progress_current = 0.0f;
    state->baseline_radius =
        state->radius_current > 0.0f ? state->radius_current : state->baseline_radius;
    state->detail_radius  = expansion_compute_detail_radius(state);
    state->radius_current = state->baseline_radius;
    expansion_configure_targets(state, true);

    if (camera)
    {
        const float *viewer_position =
            camera->view_valid ? camera->view_position : camera->position;
        vector_copy(state->detail_position, viewer_position);
        vector_copy(state->camera_target_start, camera->target);
        vector_copy(state->camera_target_detail, camera->target);
        state->camera_radius_start  = camera->radius;
        state->camera_radius_detail = camera->radius;
    }
    else
    {
        vector_copy(state->detail_position, target_position);
        vector_copy(state->camera_target_start, target_position);
        vector_copy(state->camera_target_detail, target_position);
        state->camera_radius_start  = state->baseline_radius;
        state->camera_radius_detail = state->detail_radius * 0.55f;
    }

    return true;
}

bool expansion_retarget(ExpansionState *state, const LayoutResult *layout,
                        const struct Person *person, CameraController *camera)
{
    if (!state || !layout || !person || layout->count == 0U)
    {
        return false;
    }

    if (!state->active || state->reversing || !state->detail_mode)
    {
        return expansion_start(state, layout, person, camera);
    }

    float target_position[3];
    if (!find_person_position(layout, person, target_position))
    {
        return false;
    }

    state->person = person;
    vector_copy(state->origin_position, target_position);
    vector_copy(state->camera_target_start, target_position);

    if (!(state->camera_radius_start > 0.0f) && camera)
    {
        state->camera_radius_start = camera->config.default_radius;
    }

    state->elapsed            = state->duration;
    state->progress_current   = 1.0f;
    state->animating          = false;
    state->reversing          = false;
    state->detail_mode        = true;
    state->radius_current     = state->radius_target;
    state->inactive_current   = state->inactive_target;
    state->fade_current       = state->fade_target;
    state->tree_light_current = state->tree_light_target;
    state->room_light_current = state->room_light_target;
    vector_copy(state->current_position, state->detail_position);

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
    float duration = fmaxf(state->duration, 0.0001f);
    float t        = clamp01(state->elapsed / duration);
    float eased    = ease_out_cubic(t);
    float blend    = state->reversing ? (1.0f - eased) : eased;

    state->radius_current =
        state->radius_source + (state->radius_target - state->radius_source) * blend;
    state->inactive_current =
        state->inactive_source + (state->inactive_target - state->inactive_source) * blend;
    state->fade_current = state->fade_source + (state->fade_target - state->fade_source) * blend;
    state->tree_light_current =
        state->tree_light_source + (state->tree_light_target - state->tree_light_source) * blend;
    state->room_light_current =
        state->room_light_source + (state->room_light_target - state->room_light_source) * blend;
    state->progress_current = blend;
    vector_lerp(state->origin_position, state->detail_position, blend, state->current_position);

    if (camera)
    {
        float desired_radius = state->camera_radius_start +
                               (state->camera_radius_detail - state->camera_radius_start) * blend;
        if (!(desired_radius > 0.0f))
        {
            desired_radius = camera->config.default_radius;
        }

        float desired_target[3];
        vector_lerp(state->camera_target_start, state->camera_target_detail, blend, desired_target);
        camera_controller_focus(camera, desired_target, desired_radius);
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
    state->animating          = false;
    state->reversing          = false;
    state->detail_mode        = true;
    state->elapsed            = state->duration;
    state->progress_current   = 1.0f;
    state->radius_current     = state->radius_target;
    state->inactive_current   = state->inactive_target;
    state->fade_current       = state->fade_target;
    state->tree_light_current = state->tree_light_target;
    state->room_light_current = state->room_light_target;
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

    state->animating        = true;
    state->reversing        = true;
    state->detail_mode      = false;
    state->elapsed          = 0.0f;
    state->duration         = EXPANSION_RETURN_DURATION;
    state->progress_current = 1.0f;
    expansion_configure_targets(state, false);

    if (camera)
    {
        camera_controller_focus(camera, state->camera_target_start,
                                state->camera_radius_start > 0.0f ? state->camera_radius_start
                                                                  : camera->config.default_radius);
    }
}

bool expansion_is_active(const ExpansionState *state) { return state && state->active; }

bool expansion_is_reversing(const ExpansionState *state) { return state && state->reversing; }

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

float expansion_tree_light_factor(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 1.0f;
    }
    return clamp01(state->tree_light_current);
}

float expansion_room_light_factor(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 0.0f;
    }
    return clamp01(state->room_light_current);
}

float expansion_detail_activation(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 0.0f;
    }
    float progress   = clamp01(state->progress_current);
    float room_light = clamp01(state->room_light_current);
    float fade_gain  = 1.0f - clamp01(state->fade_current);
    float combined   = (progress * 0.7f) + (room_light * 0.2f) + (fade_gain * 0.1f);
    return clamp01(combined);
}

float expansion_timeline_activation(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 0.0f;
    }
    float progress = clamp01(state->progress_current);
    float delayed  = (progress - 0.18f) / 0.82f;
    if (delayed < 0.0f)
    {
        delayed = 0.0f;
    }
    if (delayed > 1.0f)
    {
        delayed = 1.0f;
    }
    float tree_dim        = 1.0f - clamp01(state->tree_light_current);
    float normalized_tree = tree_dim / 0.72f;
    if (normalized_tree < 0.0f)
    {
        normalized_tree = 0.0f;
    }
    if (normalized_tree > 1.0f)
    {
        normalized_tree = 1.0f;
    }
    float combined = (delayed * 0.6f) + (normalized_tree * 0.4f);
    return clamp01(combined);
}

float expansion_panel_activation(const ExpansionState *state)
{
    if (!state || !state->active)
    {
        return 0.0f;
    }
    float progress = clamp01(state->progress_current);
    float delayed  = (progress - 0.32f) / 0.68f;
    if (delayed < 0.0f)
    {
        delayed = 0.0f;
    }
    if (delayed > 1.0f)
    {
        delayed = 1.0f;
    }
    float fade_gain       = 1.0f - clamp01(state->fade_current);
    float normalized_fade = fade_gain / 0.88f;
    if (normalized_fade < 0.0f)
    {
        normalized_fade = 0.0f;
    }
    if (normalized_fade > 1.0f)
    {
        normalized_fade = 1.0f;
    }
    float combined = (delayed * 0.7f) + (normalized_fade * 0.3f);
    return clamp01(combined);
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
