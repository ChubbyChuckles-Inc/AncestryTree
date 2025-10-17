#ifndef IMAGE_PANEL_H
#define IMAGE_PANEL_H

#include <stdbool.h>
#include <stddef.h>

#define IMAGE_PANEL_MAX_COUNT 12U

typedef struct ImagePanelAnchor
{
    float position[3];
    float normal[3];
    float up[3];
    float width;
    float height;
    float occlusion_score;
    bool sightline_clear;
    float focus_weight;
    float angle;
} ImagePanelAnchor;

typedef struct ImagePanelLayoutParams
{
    size_t desired_count;
    float cone_inner_radius;
    float cone_outer_radius;
    float min_height;
    float max_height;
    float min_angle_radians;
    float timeline_radius;
    float timeline_height;
    float timeline_clear_margin;
} ImagePanelLayoutParams;

typedef struct ImagePanelSystem ImagePanelSystem;

ImagePanelSystem *image_panel_create(void);
void image_panel_destroy(ImagePanelSystem *system);
void image_panel_reset(ImagePanelSystem *system);
bool image_panel_layout_init(ImagePanelSystem *system, const ImagePanelLayoutParams *params);
void image_panel_interaction(ImagePanelSystem *system, float delta_seconds, size_t focus_index, bool focus_lock);
void image_panel_render(const ImagePanelSystem *system, float activation, const float origin[3], float room_radius);

size_t image_panel_count(const ImagePanelSystem *system);
bool image_panel_get_anchor(const ImagePanelSystem *system, size_t index, ImagePanelAnchor *out_anchor);
bool image_panel_is_focus_active(const ImagePanelSystem *system, size_t index);

#endif /* IMAGE_PANEL_H */
