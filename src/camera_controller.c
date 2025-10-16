#include "camera_controller.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float clampf(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static void copy3(float destination[3], const float source[3])
{
    destination[0] = source[0];
    destination[1] = source[1];
    destination[2] = source[2];
}

static void add_scaled3(float vector[3], const float direction[3], float scale)
{
    vector[0] += direction[0] * scale;
    vector[1] += direction[1] * scale;
    vector[2] += direction[2] * scale;
}

static void normalise3(const float input[3], float output[3])
{
    float length_sq = (input[0] * input[0]) + (input[1] * input[1]) + (input[2] * input[2]);
    if (length_sq <= FLT_EPSILON)
    {
        output[0] = 0.0f;
        output[1] = 1.0f;
        output[2] = 0.0f;
        return;
    }
    float inv_length = 1.0f / sqrtf(length_sq);
    output[0] = input[0] * inv_length;
    output[1] = input[1] * inv_length;
    output[2] = input[2] * inv_length;
}

static void cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = (a[1] * b[2]) - (a[2] * b[1]);
    out[1] = (a[2] * b[0]) - (a[0] * b[2]);
    out[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

static void commit_view(CameraController *controller)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    controller->camera.position =
        (Vector3){controller->view_position[0], controller->view_position[1], controller->view_position[2]};
    controller->camera.target =
        (Vector3){controller->view_target[0], controller->view_target[1], controller->view_target[2]};
    controller->camera.up = (Vector3){controller->up[0], controller->up[1], controller->up[2]};
#else
    (void)controller;
#endif
}

static void sync_view(CameraController *controller)
{
    copy3(controller->view_position, controller->position);
    copy3(controller->view_target, controller->target);
    controller->view_valid = true;
    commit_view(controller);
}

static float smoothing_blend(const CameraControllerConfig *config, float delta_seconds)
{
    if (!config || config->smoothing_half_life_seconds <= 0.0f || delta_seconds <= 0.0f)
    {
        return 1.0f;
    }
    /* Exponential smoothing expressed via half-life for intuitive tuning. */
    float decay = powf(0.5f, delta_seconds / config->smoothing_half_life_seconds);
    float blend = 1.0f - decay;
    if (blend < 0.0f)
    {
        blend = 0.0f;
    }
    if (blend > 1.0f)
    {
        blend = 1.0f;
    }
    return blend;
}

static void apply_smoothing(CameraController *controller, float delta_seconds)
{
    if (!controller)
    {
        return;
    }
    if (!controller->view_valid || controller->config.smoothing_half_life_seconds <= 0.0f || delta_seconds <= 0.0f)
    {
        sync_view(controller);
        return;
    }

    float blend = smoothing_blend(&controller->config, delta_seconds);
    const float retain = 1.0f - blend;
    for (int index = 0; index < 3; ++index)
    {
        controller->view_position[index] = controller->view_position[index] * retain + controller->position[index] * blend;
        controller->view_target[index] = controller->view_target[index] * retain + controller->target[index] * blend;
    }
    commit_view(controller);
}

static void recalc_position(CameraController *controller)
{
    const float yaw = controller->yaw;
    const float pitch = controller->pitch;
    const float radius = controller->radius;
    const float cos_pitch = cosf(pitch);
    const float sin_pitch = sinf(pitch);
    const float cos_yaw = cosf(yaw);
    const float sin_yaw = sinf(yaw);

    float forward[3];
    forward[0] = cos_pitch * sin_yaw;
    forward[1] = sin_pitch;
    forward[2] = cos_pitch * cos_yaw;

    controller->position[0] = controller->target[0] - forward[0] * radius;
    controller->position[1] = controller->target[1] - forward[1] * radius;
    controller->position[2] = controller->target[2] - forward[2] * radius;
}

static void apply_pan(CameraController *controller, float right_amount, float up_amount, float delta_seconds)
{
    if (right_amount == 0.0f && up_amount == 0.0f)
    {
        return;
    }
    float yaw = controller->yaw;
    float pitch = controller->pitch;
    const float cos_pitch = cosf(pitch);
    const float sin_pitch = sinf(pitch);
    const float cos_yaw = cosf(yaw);
    const float sin_yaw = sinf(yaw);

    float forward[3];
    forward[0] = cos_pitch * sin_yaw;
    forward[1] = sin_pitch;
    forward[2] = cos_pitch * cos_yaw;

    float up[3];
    normalise3(controller->up, up);
    float right[3];

    cross3(forward, up, right);
    normalise3(right, right);

    float shift[3];
    shift[0] = (right[0] * right_amount) + (up[0] * up_amount);
    shift[1] = (right[1] * right_amount) + (up[1] * up_amount);
    shift[2] = (right[2] * right_amount) + (up[2] * up_amount);

    float scale = controller->config.pan_speed * delta_seconds;
    add_scaled3(controller->target, shift, scale);
}

void camera_controller_config_default(CameraControllerConfig *config)
{
    if (!config)
    {
        return;
    }
    config->target[0] = 0.0f;
    config->target[1] = 1.5f;
    config->target[2] = 0.0f;
    config->up[0] = 0.0f;
    config->up[1] = 1.0f;
    config->up[2] = 0.0f;
    config->default_radius = 12.0f;
    config->min_radius = 4.0f;
    config->max_radius = 32.0f;
    config->default_yaw = (float)(M_PI * 0.25);
    config->default_pitch = (float)(M_PI * 0.15);
    config->rotation_speed = 1.5f;
    config->zoom_speed = 15.0f;
    config->pan_speed = 10.0f;
    config->smoothing_half_life_seconds = 0.18f;
}

void camera_controller_input_clear(CameraControllerInput *input)
{
    if (!input)
    {
        return;
    }
    input->yaw_delta = 0.0f;
    input->pitch_delta = 0.0f;
    input->zoom_delta = 0.0f;
    input->pan_right = 0.0f;
    input->pan_up = 0.0f;
}

static void apply_config(CameraController *controller, const CameraControllerConfig *config)
{
    controller->config = *config;
    copy3(controller->target, config->target);
    copy3(controller->up, config->up);
    controller->radius = clampf(config->default_radius, config->min_radius, config->max_radius);
    controller->yaw = config->default_yaw;
    const float pitch_limit = (float)(M_PI * 0.5) - 0.05f;
    controller->pitch = clampf(config->default_pitch, -pitch_limit, pitch_limit);
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    controller->camera.fovy = 45.0f;
    controller->camera.projection = CAMERA_PERSPECTIVE;
#endif
    recalc_position(controller);
    sync_view(controller);
    controller->initialized = true;
}

bool camera_controller_init(CameraController *controller, const CameraControllerConfig *config)
{
    if (!controller || !config)
    {
        return false;
    }
    memset(controller, 0, sizeof(*controller));
    apply_config(controller, config);
    return true;
}

void camera_controller_reset(CameraController *controller)
{
    if (!controller || !controller->initialized)
    {
        return;
    }
    apply_config(controller, &controller->config);
}

void camera_controller_focus(CameraController *controller, const float target[3], float radius)
{
    if (!controller || !controller->initialized)
    {
        return;
    }
    if (target)
    {
        copy3(controller->target, target);
    }
    if (radius > 0.0f)
    {
        controller->radius = clampf(radius, controller->config.min_radius, controller->config.max_radius);
    }
    recalc_position(controller);
}

void camera_controller_update(CameraController *controller, const CameraControllerInput *input, float delta_seconds)
{
    if (!controller || !controller->initialized || !input)
    {
        return;
    }
    const float rotation_scale = controller->config.rotation_speed * delta_seconds;
    controller->yaw += input->yaw_delta * rotation_scale;
    controller->pitch += input->pitch_delta * rotation_scale;

    const float pitch_limit = (float)(M_PI * 0.5) - 0.05f;
    controller->pitch = clampf(controller->pitch, -pitch_limit, pitch_limit);

    const float zoom_scale = controller->config.zoom_speed * delta_seconds;
    controller->radius -= input->zoom_delta * zoom_scale;
    controller->radius = clampf(controller->radius, controller->config.min_radius, controller->config.max_radius);

    apply_pan(controller, input->pan_right, input->pan_up, delta_seconds);
    recalc_position(controller);
    apply_smoothing(controller, delta_seconds);
}

/*
 * Manual validation checklist:
 * 1. Orbit the camera rapidly and confirm motion eases into place without stutter.
 * 2. Trigger "Focus Roots" UI action and observe a smooth glide toward the new anchor point.
 * 3. Toggle smoothing (set half-life to zero in config) to verify immediate snapping remains available.
 */

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
const Camera3D *camera_controller_get_camera(const CameraController *controller)
{
    if (!controller || !controller->initialized)
    {
        return NULL;
    }
    return &controller->camera;
}
#endif
