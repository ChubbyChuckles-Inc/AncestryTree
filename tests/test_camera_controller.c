#include "camera_controller.h"
#include "test_framework.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float distance_between(const float a[3], const float b[3])
{
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    const float dz = a[2] - b[2];
    return sqrtf((dx * dx) + (dy * dy) + (dz * dz));
}

TEST(test_camera_controller_init_uses_defaults)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));
    ASSERT_TRUE(controller.initialized);
    ASSERT_FLOAT_NEAR(controller.radius, config.default_radius, 1e-5f);

    float expected_pos[3];
    const float cos_pitch = cosf(config.default_pitch);
    const float sin_pitch = sinf(config.default_pitch);
    const float cos_yaw = cosf(config.default_yaw);
    const float sin_yaw = sinf(config.default_yaw);
    float forward[3];
    forward[0] = cos_pitch * sin_yaw;
    forward[1] = sin_pitch;
    forward[2] = cos_pitch * cos_yaw;
    expected_pos[0] = config.target[0] - forward[0] * config.default_radius;
    expected_pos[1] = config.target[1] - forward[1] * config.default_radius;
    expected_pos[2] = config.target[2] - forward[2] * config.default_radius;

    ASSERT_TRUE(distance_between(expected_pos, controller.position) < 1e-4f);
}

TEST(test_camera_controller_update_respects_pitch_limits)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));

    CameraControllerInput input;
    camera_controller_input_clear(&input);
    input.pitch_delta = 10.0f;

    camera_controller_update(&controller, &input, 1.0f);
    const float half_pi = (float)(M_PI * 0.5);
    ASSERT_TRUE(controller.pitch <= half_pi);

    input.pitch_delta = -10.0f;
    camera_controller_update(&controller, &input, 1.0f);
    ASSERT_TRUE(controller.pitch >= -half_pi);
}

TEST(test_camera_controller_zoom_clamped_to_bounds)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    config.min_radius = 5.0f;
    config.max_radius = 6.0f;
    config.default_radius = 5.5f;

    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));

    CameraControllerInput input;
    camera_controller_input_clear(&input);
    input.zoom_delta = 10.0f;
    camera_controller_update(&controller, &input, 0.5f);
    ASSERT_FLOAT_NEAR(controller.radius, config.min_radius, 1e-4f);

    camera_controller_input_clear(&input);
    input.zoom_delta = -10.0f;
    camera_controller_update(&controller, &input, 0.5f);
    ASSERT_FLOAT_NEAR(controller.radius, config.max_radius, 1e-4f);
}

TEST(test_camera_controller_focus_updates_target_and_radius)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));

    float new_target[3] = {5.0f, 7.0f, -3.0f};
    camera_controller_focus(&controller, new_target, 8.0f);
    ASSERT_FLOAT_NEAR(controller.target[0], new_target[0], 1e-5f);
    ASSERT_FLOAT_NEAR(controller.target[1], new_target[1], 1e-5f);
    ASSERT_FLOAT_NEAR(controller.target[2], new_target[2], 1e-5f);
    ASSERT_FLOAT_NEAR(controller.radius, 8.0f, 1e-5f);
}

TEST(test_camera_controller_pan_shifts_target)
{
    CameraControllerConfig config;
    camera_controller_config_default(&config);
    CameraController controller;
    ASSERT_TRUE(camera_controller_init(&controller, &config));

    CameraControllerInput input;
    camera_controller_input_clear(&input);
    input.pan_right = 1.0f;
    input.pan_up = 0.5f;
    camera_controller_update(&controller, &input, 1.0f);

    ASSERT_TRUE(distance_between(config.target, controller.target) > 0.1f);
}

void register_camera_controller_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_camera_controller_init_uses_defaults);
    REGISTER_TEST(registry, test_camera_controller_update_respects_pitch_limits);
    REGISTER_TEST(registry, test_camera_controller_zoom_clamped_to_bounds);
    REGISTER_TEST(registry, test_camera_controller_focus_updates_target_and_radius);
    REGISTER_TEST(registry, test_camera_controller_pan_shifts_target);
}
