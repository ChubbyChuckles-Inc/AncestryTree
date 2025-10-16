#ifndef INTERACTION_H
#define INTERACTION_H

#include <stdbool.h>

struct LayoutResult;
struct CameraController;
struct Person;

typedef struct InteractionHit
{
    const struct Person *person;
    float distance;
    float position[3];
} InteractionHit;

typedef struct InteractionState
{
    float sphere_pick_radius;
    const struct Person *hovered;
    const struct Person *selected;
} InteractionState;

void interaction_state_init(InteractionState *state);
void interaction_state_set_pick_radius(InteractionState *state, float radius);
const struct Person *interaction_get_hovered(const InteractionState *state);
const struct Person *interaction_get_selected(const InteractionState *state);
const struct Person *interaction_get_visual_focus(const InteractionState *state);

bool interaction_ray_sphere_intersection(const float ray_origin[3], const float ray_direction[3],
                                         const float sphere_center[3], float radius, float *out_distance);

bool interaction_ray_cast(const InteractionState *state, const struct LayoutResult *layout,
                          const struct CameraController *camera, float mouse_x, float mouse_y,
                          InteractionHit *out_hit);

bool interaction_update_hover(InteractionState *state, const struct LayoutResult *layout,
                              const struct CameraController *camera, float mouse_x, float mouse_y);

bool interaction_select_at_cursor(InteractionState *state, const struct LayoutResult *layout,
                                  const struct CameraController *camera, float mouse_x, float mouse_y,
                                  bool allow_deselect_empty);

void interaction_clear_selection(InteractionState *state);
bool interaction_select_person(InteractionState *state, const struct Person *person);

#endif /* INTERACTION_H */
