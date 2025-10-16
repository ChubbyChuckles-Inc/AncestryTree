#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

#include <stdbool.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct CameraControllerConfig
    {
        float target[3];
        float up[3];
        float default_radius;
        float min_radius;
        float max_radius;
        float default_yaw;
        float default_pitch;
        float rotation_speed;
        float zoom_speed;
        float pan_speed;
        float smoothing_half_life_seconds;
    } CameraControllerConfig;

    typedef struct CameraControllerInput
    {
        float yaw_delta;
        float pitch_delta;
        float zoom_delta;
        float pan_right;
        float pan_up;
    } CameraControllerInput;

    typedef struct CameraController
    {
        CameraControllerConfig config;
        float target[3];
        float up[3];
        float position[3];
        float view_target[3];
        float view_position[3];
        float yaw;
        float pitch;
        float radius;
        bool initialized;
        bool view_valid;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        struct Camera3D camera;
#endif
    } CameraController;

    void camera_controller_config_default(CameraControllerConfig *config);
    void camera_controller_input_clear(CameraControllerInput *input);
    bool camera_controller_init(CameraController *controller, const CameraControllerConfig *config);
    void camera_controller_reset(CameraController *controller);
    void camera_controller_focus(CameraController *controller, const float target[3], float radius);
    void camera_controller_update(CameraController *controller, const CameraControllerInput *input, float delta_seconds);

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    const struct Camera3D *camera_controller_get_camera(const CameraController *controller);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_CONTROLLER_H */
