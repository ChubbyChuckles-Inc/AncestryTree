#include "ui_scaling.h"

#include <math.h>

static float ui_clamp(float value, float minimum, float maximum)
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

float ui_scaling_default_font_size(int surface_height)
{
    if (surface_height <= 0)
    {
        return 18.0f;
    }
    const float reference_height = 1080.0f;
    const float minimum_scale = 0.9f;
    const float maximum_scale = 2.0f;
    float scale = (float)surface_height / reference_height;
    scale = ui_clamp(scale, minimum_scale, maximum_scale);
    /* Square-root dampens the growth so 4K displays do not overshoot. */
    float dampened = sqrtf(scale);
    const float base_size = 20.0f;
    return ui_clamp(base_size * dampened, 16.0f, 38.0f);
}
