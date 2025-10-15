#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stddef.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

struct CameraController;
struct LayoutResult;
struct Person;

typedef struct RenderColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} RenderColor;

typedef struct RenderConfig
{
    float sphere_radius;
    float glow_intensity;
    float connection_radius;
    RenderColor alive_color;
    RenderColor deceased_color;
    RenderColor selected_outline_color;
    RenderColor connection_color_parent_child;
    RenderColor connection_color_spouse;
} RenderConfig;

typedef struct RenderConnectionSegment
{
    float start[3];
    float end[3];
} RenderConnectionSegment;

typedef struct RenderState
{
    bool initialized;
    RenderConfig config;
    int render_width;
    int render_height;
    bool render_target_ready;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    struct Shader glow_shader;
    int glow_intensity_loc;
    bool glow_shader_ready;
    float ambient_strength;
    float light_direction[3];
    RenderTexture2D scene_target;
#endif
} RenderState;

void render_state_init(RenderState *state);
RenderConfig render_config_default(void);
bool render_config_validate(const RenderConfig *config);

bool render_init(RenderState *state, const RenderConfig *config, char *error_buffer, size_t error_buffer_size);
void render_cleanup(RenderState *state);
bool render_resize(RenderState *state, int width, int height, char *error_buffer, size_t error_buffer_size);
bool render_has_render_target(const RenderState *state);

bool render_scene(RenderState *state, const struct LayoutResult *layout, const struct CameraController *camera,
                  const struct Person *selected_person, const struct Person *hovered_person);
bool render_connections_render(RenderState *state, const struct LayoutResult *layout);

bool render_find_person_position(const struct LayoutResult *layout, const struct Person *person, float out_position[3]);
size_t render_collect_parent_child_segments(const struct LayoutResult *layout, RenderConnectionSegment *segments,
                                            size_t segment_capacity);
size_t render_collect_spouse_segments(const struct LayoutResult *layout, RenderConnectionSegment *segments,
                                      size_t segment_capacity);

#endif /* RENDER_H */
