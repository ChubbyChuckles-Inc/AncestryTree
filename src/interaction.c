#include "interaction.h"

#include "camera_controller.h"
#include "layout.h"
#include "person.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

void interaction_state_init(InteractionState *state)
{
    if (!state)
    {
        return;
    }
    state->sphere_pick_radius = 0.6f;
    state->hovered = NULL;
    state->selected = NULL;
}

void interaction_state_set_pick_radius(InteractionState *state, float radius)
{
    if (!state)
    {
        return;
    }
    if (radius > 0.0f)
    {
        state->sphere_pick_radius = radius;
    }
}

const Person *interaction_get_hovered(const InteractionState *state)
{
    return state ? state->hovered : NULL;
}

const Person *interaction_get_selected(const InteractionState *state)
{
    return state ? state->selected : NULL;
}

const Person *interaction_get_visual_focus(const InteractionState *state)
{
    if (!state)
    {
        return NULL;
    }
    return state->selected ? state->selected : state->hovered;
}

static bool vector_normalize(float vector[3])
{
    if (!vector)
    {
        return false;
    }
    float length_sq = vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2];
    if (length_sq <= 0.0f)
    {
        return false;
    }
    float length = sqrtf(length_sq);
    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return true;
}

bool interaction_ray_sphere_intersection(const float ray_origin[3], const float ray_direction[3],
                                         const float sphere_center[3], float radius, float *out_distance)
{
    if (!ray_origin || !ray_direction || !sphere_center || !(radius > 0.0f))
    {
        return false;
    }
    float direction[3] = {ray_direction[0], ray_direction[1], ray_direction[2]};
    if (!vector_normalize(direction))
    {
        return false;
    }

    float m[3];
    m[0] = ray_origin[0] - sphere_center[0];
    m[1] = ray_origin[1] - sphere_center[1];
    m[2] = ray_origin[2] - sphere_center[2];
    float b = m[0] * direction[0] + m[1] * direction[1] + m[2] * direction[2];
    float c = m[0] * m[0] + m[1] * m[1] + m[2] * m[2] - radius * radius;

    if (c > 0.0f && b > 0.0f)
    {
        return false;
    }

    float discriminant = b * b - c;
    if (discriminant < 0.0f)
    {
        return false;
    }

    float distance = -b - sqrtf(discriminant);
    if (distance < 0.0f)
    {
        distance = 0.0f;
    }
    if (out_distance)
    {
        *out_distance = distance;
    }
    return true;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static float interaction_pick_radius(const InteractionState *state)
{
    const float default_radius = 0.6f;
    if (!state)
    {
        return default_radius;
    }
    return (state->sphere_pick_radius > 0.0f) ? state->sphere_pick_radius : default_radius;
}
#endif

bool interaction_ray_cast(const InteractionState *state, const LayoutResult *layout, const CameraController *camera,
                          float mouse_x, float mouse_y, InteractionHit *out_hit)
{
    (void)mouse_x;
    (void)mouse_y;
    if (!state || !layout || !camera)
    {
        return false;
    }
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)out_hit;
    return false;
#else
    const Camera3D *camera_data = camera_controller_get_camera(camera);
    if (!camera_data)
    {
        return false;
    }
    Vector2 mouse = {mouse_x, mouse_y};
    Ray ray = GetMouseRay(mouse, *camera_data);

    float ray_origin[3] = {ray.position.x, ray.position.y, ray.position.z};
    float ray_direction[3] = {ray.direction.x, ray.direction.y, ray.direction.z};

    const Person *closest_person = NULL;
    float closest_distance = FLT_MAX;
    float hit_position[3] = {0.0f, 0.0f, 0.0f};
    float radius = interaction_pick_radius(state) * 1.1f;

    for (size_t index = 0U; index < layout->count; ++index)
    {
        const LayoutNode *node = &layout->nodes[index];
        const Person *person = node->person;
        if (!person)
        {
            continue;
        }
        float center[3] = {node->position[0], node->position[1], node->position[2]};
        float distance = 0.0f;
        if (interaction_ray_sphere_intersection(ray_origin, ray_direction, center, radius, &distance))
        {
            if (distance < closest_distance)
            {
                closest_distance = distance;
                closest_person = person;
                hit_position[0] = center[0];
                hit_position[1] = center[1];
                hit_position[2] = center[2];
            }
        }
    }

    if (!closest_person)
    {
        return false;
    }
    if (out_hit)
    {
        out_hit->person = closest_person;
        out_hit->distance = closest_distance;
        out_hit->position[0] = hit_position[0];
        out_hit->position[1] = hit_position[1];
        out_hit->position[2] = hit_position[2];
    }
    return true;
#endif
}

bool interaction_update_hover(InteractionState *state, const LayoutResult *layout, const CameraController *camera,
                              float mouse_x, float mouse_y)
{
    if (!state)
    {
        return false;
    }
    InteractionHit hit;
    const Person *previous = state->hovered;
    if (interaction_ray_cast(state, layout, camera, mouse_x, mouse_y, &hit))
    {
        state->hovered = hit.person;
    }
    else
    {
        state->hovered = NULL;
    }
    return previous != state->hovered;
}

bool interaction_select_at_cursor(InteractionState *state, const LayoutResult *layout, const CameraController *camera,
                                  float mouse_x, float mouse_y, bool allow_deselect_empty)
{
    if (!state)
    {
        return false;
    }
    InteractionHit hit;
    if (interaction_ray_cast(state, layout, camera, mouse_x, mouse_y, &hit))
    {
        if (state->selected == hit.person)
        {
            return false;
        }
        state->selected = hit.person;
        return true;
    }
    if (allow_deselect_empty)
    {
        bool changed = state->selected != NULL;
        state->selected = NULL;
        return changed;
    }
    return false;
}

void interaction_clear_selection(InteractionState *state)
{
    if (!state)
    {
        return;
    }
    state->selected = NULL;
}

/* Manual validation checklist (requires raylib runtime):
 * 1. Hover spheres in the scene and verify the hover outline follows the cursor.
 * 2. Click different spheres and ensure the selection outline updates accordingly.
 * 3. Click in empty space while holding Shift to keep the current selection.
 */
