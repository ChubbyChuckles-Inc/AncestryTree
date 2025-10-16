#ifndef RENDER_LABELS_H
#define RENDER_LABELS_H

#include <stdbool.h>
#include <stddef.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

struct Person;

typedef struct RenderLabelInfo
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    Texture2D texture;
    float width_pixels;
    float height_pixels;
    float font_size;
#else
    int placeholder_texture_id;
    float width_pixels;
    float height_pixels;
    float font_size;
#endif
    bool valid;
} RenderLabelInfo;

typedef struct RenderLabelEntry
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    unsigned int person_id;
    char signature[256];
    Texture2D texture;
    float width_pixels;
    float height_pixels;
    float font_size;
    bool in_use;
#else
    unsigned int person_id;
    float font_size;
    bool in_use;
#endif
} RenderLabelEntry;

typedef struct RenderLabelSystem
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    RenderLabelEntry *entries;
    size_t count;
    size_t capacity;
    float base_font_size;
    Color text_color;
    Color background_color_top;
    Color background_color_bottom;
    Color frame_color;
#else
    RenderLabelEntry *entries;
    size_t count;
    size_t capacity;
    float base_font_size;
#endif
} RenderLabelSystem;

bool render_labels_init(RenderLabelSystem *system);
void render_labels_shutdown(RenderLabelSystem *system);
void render_labels_begin_frame(RenderLabelSystem *system);
void render_labels_end_frame(RenderLabelSystem *system);
void render_labels_set_base_font_size(RenderLabelSystem *system, float font_size);
bool render_labels_acquire(RenderLabelSystem *system, const struct Person *person, bool include_profile,
                           float font_size, RenderLabelInfo *out_info);

#endif /* RENDER_LABELS_H */
