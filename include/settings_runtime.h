#ifndef SETTINGS_RUNTIME_H
#define SETTINGS_RUNTIME_H

#include <stdbool.h>

#include "settings.h"
#include "camera_controller.h"
#include "render.h"

#ifdef __cplusplus
extern "C"
{
#endif

    bool settings_runtime_apply_camera(const Settings *settings, CameraController *controller);

    bool settings_runtime_apply_render(const Settings *settings, RenderConfig *config);

    void settings_runtime_compute_input_sensitivity(const Settings *settings, float *orbit_sensitivity,
                                                    float *pan_mouse_sensitivity, float *pan_keyboard_sensitivity,
                                                    float *zoom_sensitivity);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_RUNTIME_H */
