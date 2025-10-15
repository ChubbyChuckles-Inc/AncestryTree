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
#else
    int placeholder_texture_id;
    float width_pixels;
    float height_pixels;
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
    bool in_use;
#else
    unsigned int person_id;
    bool in_use;
#endif
} RenderLabelEntry;

typedef struct RenderLabelSystem
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    RenderLabelEntry *entries;
    size_t count;
    size_t capacity;
    float font_size;
    Color text_color;
    Color background_color_top;
    Color background_color_bottom;
    Color frame_color;
#else
    RenderLabelEntry *entries;
    size_t count;
    size_t capacity;
#endif
} RenderLabelSystem;

bool render_labels_init(RenderLabelSystem *system);
void render_labels_shutdown(RenderLabelSystem *system);
void render_labels_begin_frame(RenderLabelSystem *system);
void render_labels_end_frame(RenderLabelSystem *system);
bool render_labels_acquire(RenderLabelSystem *system, const struct Person *person, bool include_profile,
                           RenderLabelInfo *out_info);

#endif /* RENDER_LABELS_H */
