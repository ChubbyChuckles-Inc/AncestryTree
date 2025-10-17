#ifndef EXPANSION_H
#define EXPANSION_H

#include <stdbool.h>

struct LayoutResult;
struct Person;
struct CameraController;

typedef struct ExpansionState
{
    bool active;
    bool animating;
    bool reversing;
    bool detail_mode;
    const struct Person *person;
    float elapsed;
    float duration;
    float origin_position[3];
    float detail_position[3];
    float current_position[3];
    float camera_target_start[3];
    float camera_target_detail[3];
    float camera_radius_start;
    float camera_radius_detail;
    float radius_source;
    float radius_target;
    float radius_current;
    float inactive_source;
    float inactive_target;
    float inactive_current;
    float fade_source;
    float fade_target;
    float fade_current;
} ExpansionState;

void expansion_state_reset(ExpansionState *state);
bool expansion_start(ExpansionState *state, const struct LayoutResult *layout, const struct Person *person,
                     struct CameraController *camera);
bool expansion_update(ExpansionState *state, float delta_seconds, struct CameraController *camera);
void expansion_complete(ExpansionState *state);
void expansion_reverse(ExpansionState *state, struct CameraController *camera);
bool expansion_is_active(const ExpansionState *state);
bool expansion_is_reversing(const ExpansionState *state);
bool expansion_is_in_detail_mode(const ExpansionState *state);
float expansion_primary_scale(const ExpansionState *state);
float expansion_inactive_scale(const ExpansionState *state);
float expansion_inactive_opacity(const ExpansionState *state);
void expansion_current_position(const ExpansionState *state, float out_position[3]);

#endif /* EXPANSION_H */
