#ifndef RENDER_H
#define RENDER_H

#include <stdbool.h>
#include <stddef.h>

#include "life_particles.h"
#include "selection_particles.h"

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

struct CameraController;
struct LayoutResult;
struct LayoutNode;
struct Person;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
struct RenderLabelSystem;
#endif

typedef struct RenderColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} RenderColor;

typedef enum RenderConnectionStyle
{
    RENDER_CONNECTION_STYLE_STRAIGHT = 0,
    RENDER_CONNECTION_STYLE_BEZIER   = 1
} RenderConnectionStyle;

typedef struct RenderConfig
{
    float sphere_radius;
    float glow_intensity;
    float glow_min_strength;
    float glow_pulse_speed;
    float connection_radius;
    bool connection_antialiasing;
    RenderConnectionStyle connection_style_parent_child;
    RenderConnectionStyle connection_style_spouse;
    RenderColor alive_color;
    RenderColor deceased_color;
    RenderColor selected_outline_color;
    RenderColor connection_color_parent_child;
    RenderColor connection_color_spouse;
    bool show_connections;
    bool show_grid;
    bool show_overlay;
    bool show_name_panels;
    bool show_profile_images;
    float name_panel_font_size;
    float name_panel_width_scale;
    float name_panel_height_scale;
    float name_panel_offset_x;
    float name_panel_offset_y;
    float name_panel_offset_z;
    bool enable_frustum_culling;
    bool enable_lod;
    float lod_near_distance;
    float lod_far_distance;
    float culling_margin;
    bool show_background_gradient;
    RenderColor background_gradient_top;
    RenderColor background_gradient_bottom;
    bool enable_fog;
    float fog_start_distance;
    float fog_end_distance;
    RenderColor fog_color;
    bool enable_rim_lighting;
    float rim_intensity;
    float rim_power;
    RenderColor rim_color;
    bool enable_shadows;
    float shadow_radius_scale;
    float shadow_max_opacity;
    float shadow_height_bias;
    float shadow_softness;
    bool enable_selection_particles;
    size_t selection_particle_capacity;
    float selection_particle_lifetime;
    float selection_particle_speed_min;
    float selection_particle_speed_max;
    float selection_particle_repeat_delay;
    bool enable_life_particles;
    size_t birth_particle_capacity;
    size_t death_particle_capacity;
    float birth_particle_lifetime;
    float death_particle_lifetime;
    float birth_particle_speed_min;
    float birth_particle_speed_max;
    float death_particle_speed_min;
    float death_particle_speed_max;
    float birth_particle_spawn_rate;
    float death_particle_spawn_rate;
    float birth_particle_vertical_bias;
    float death_particle_vertical_bias;
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
    const struct LayoutNode **batch_alive_nodes;
    const struct LayoutNode **batch_deceased_nodes;
    size_t batch_capacity;
    unsigned char *visible_nodes;
    size_t visibility_capacity;
    size_t visible_node_count;
    bool visibility_mask_ready;
    SelectionParticleSystem selection_particles;
    const struct Person *selection_particles_last_person;
    float selection_particles_last_origin[3];
    float selection_particles_timer;
    LifeParticleSystem life_particles;
    float birth_particle_spawn_accumulator;
    float death_particle_spawn_accumulator;
    size_t birth_particle_spawn_cursor;
    size_t death_particle_spawn_cursor;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    struct Shader glow_shader;
    int glow_intensity_loc;
    int glow_time_loc;
    int glow_min_strength_loc;
    int glow_pulse_speed_loc;
    int glow_rim_intensity_loc;
    int glow_rim_power_loc;
    int glow_rim_color_loc;
    int glow_rim_enable_loc;
    bool glow_shader_ready;
    float ambient_strength;
    float light_direction[3];
    RenderTexture2D scene_target;
    struct RenderLabelSystem *label_system;
    bool label_system_ready;
    float label_system_font_size_applied;
    const Camera3D *active_camera;
#endif
} RenderState;

void render_state_init(RenderState *state);
RenderConfig render_config_default(void);
bool render_config_validate(const RenderConfig *config);

bool render_init(RenderState *state, const RenderConfig *config, char *error_buffer,
                 size_t error_buffer_size);
void render_cleanup(RenderState *state);
bool render_resize(RenderState *state, int width, int height, char *error_buffer,
                   size_t error_buffer_size);
bool render_has_render_target(const RenderState *state);

bool render_scene(RenderState *state, const struct LayoutResult *layout,
                  const struct CameraController *camera, const struct Person *selected_person,
                  const struct Person *hovered_person);
bool render_connections_render(RenderState *state, const struct LayoutResult *layout);

bool render_find_person_position(const struct LayoutResult *layout, const struct Person *person,
                                 float out_position[3]);
size_t render_collect_parent_child_segments(const struct LayoutResult *layout,
                                            RenderConnectionSegment *segments,
                                            size_t segment_capacity);
size_t render_collect_spouse_segments(const struct LayoutResult *layout,
                                      RenderConnectionSegment *segments, size_t segment_capacity);

#endif /* RENDER_H */
