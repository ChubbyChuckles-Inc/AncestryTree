#ifndef UI_SCALING_H
#define UI_SCALING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * Computes the default UI font size for a given render surface height.
     * The value is clamped to ensure legibility on both low and high DPI panels.
     */
    float ui_scaling_default_font_size(int surface_height);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCALING_H */
