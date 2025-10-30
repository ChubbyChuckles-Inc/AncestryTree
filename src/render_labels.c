#include "render_labels.h"

#include "at_memory.h"
#include "path_utils.h"
#include "person.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raymath.h>

#define RENDER_LABEL_ATLAS_INITIAL_DIMENSION 2048
#define RENDER_LABEL_ATLAS_MAX_DIMENSION 4096
#define RENDER_LABEL_ATLAS_PADDING 4
#define RENDER_LABEL_CANVAS_POOL_SIZE 8

static void render_labels_destroy_canvas_slot(RenderLabelCanvasSlot *slot)
{
    if (!slot)
    {
        return;
    }
    if (slot->image.data)
    {
        UnloadImage(slot->image);
        slot->image.data = NULL;
    }
    slot->image.width   = 0;
    slot->image.height  = 0;
    slot->image.mipmaps = 1;
    slot->image.format  = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    slot->in_use        = false;
}

static bool render_labels_canvas_prepare(RenderLabelCanvasSlot *slot, int width, int height,
                                         RenderLabelSystem *system)
{
    if (!slot || width <= 0 || height <= 0)
    {
        return false;
    }
    if (!slot->image.data)
    {
        slot->image = GenImageColor(width, height, (Color){0, 0, 0, 0});
        if (!slot->image.data)
        {
            return false;
        }
        if (system)
        {
            system->stats.canvas_creations++;
        }
        return true;
    }
    if (slot->image.width == width && slot->image.height == height)
    {
        return true;
    }
    ImageResizeCanvas(&slot->image, width, height, 0, 0, (Color){0, 0, 0, 0});
    if (slot->image.width != width || slot->image.height != height)
    {
        UnloadImage(slot->image);
        slot->image = GenImageColor(width, height, (Color){0, 0, 0, 0});
        if (!slot->image.data)
        {
            return false;
        }
        if (system)
        {
            system->stats.canvas_creations++;
        }
    }
    return true;
}

static RenderLabelCanvasSlot *render_labels_canvas_acquire(RenderLabelSystem *system, int width,
                                                           int height)
{
    if (!system || width <= 0 || height <= 0)
    {
        return NULL;
    }
    RenderLabelCanvasSlot *best_slot  = NULL;
    RenderLabelCanvasSlot *empty_slot = NULL;
    for (size_t index = 0U; index < RENDER_LABEL_CANVAS_POOL_SIZE; ++index)
    {
        RenderLabelCanvasSlot *slot = &system->canvas_pool[index];
        if (slot->in_use)
        {
            continue;
        }
        if (!slot->image.data)
        {
            if (!empty_slot)
            {
                empty_slot = slot;
            }
            continue;
        }
        if (slot->image.width >= width && slot->image.height >= height)
        {
            best_slot = slot;
            break;
        }
        if (!best_slot)
        {
            best_slot = slot;
        }
    }

    RenderLabelCanvasSlot *chosen = best_slot ? best_slot : empty_slot;
    if (!chosen)
    {
        return NULL;
    }
    if (!render_labels_canvas_prepare(chosen, width, height, system))
    {
        return NULL;
    }
    ImageClearBackground(&chosen->image, (Color){0, 0, 0, 0});
    chosen->in_use = true;
    return chosen;
}

static void render_labels_canvas_release(RenderLabelCanvasSlot *slot)
{
    if (!slot)
    {
        return;
    }
    slot->in_use = false;
}

static bool render_labels_atlas_reinitialize(RenderLabelSystem *system, int dimension,
                                             bool count_as_reset)
{
    if (!system)
    {
        return false;
    }
    texture_atlas_shutdown(&system->atlas);
    if (!texture_atlas_init(&system->atlas, dimension, dimension, RENDER_LABEL_ATLAS_PADDING))
    {
        system->atlas_initialized = false;
        return false;
    }
    system->atlas_initialized = true;
    system->atlas_dirty       = false;
    system->atlas_compressed  = false;
    system->atlas_dimension   = dimension;
    system->count             = 0U;
    for (size_t index = 0U; index < system->capacity; ++index)
    {
        system->entries[index].person_id     = 0U;
        system->entries[index].signature[0]  = '\0';
        system->entries[index].region.x      = 0;
        system->entries[index].region.y      = 0;
        system->entries[index].region.width  = 0;
        system->entries[index].region.height = 0;
        system->entries[index].width_pixels  = 0.0f;
        system->entries[index].height_pixels = 0.0f;
        system->entries[index].font_size     = 0.0f;
        system->entries[index].in_use        = false;
    }
    if (count_as_reset)
    {
        system->stats.atlas_resets++;
    }
    return true;
}

static bool render_labels_finalize_atlas(RenderLabelSystem *system)
{
    if (!system || !system->atlas_initialized)
    {
        return false;
    }
    if (system->atlas_dirty)
    {
        if (!texture_atlas_finalize(&system->atlas))
        {
            return false;
        }
        system->atlas_dirty      = false;
        system->atlas_compressed = false;
    }
    return texture_atlas_ready(&system->atlas);
}

static bool render_labels_grow_atlas(RenderLabelSystem *system)
{
    if (!system)
    {
        return false;
    }
    int current = system->atlas_dimension;
    if (current <= 0)
    {
        current = RENDER_LABEL_ATLAS_INITIAL_DIMENSION;
    }
    int next_dimension = current * 2;
    if (next_dimension > RENDER_LABEL_ATLAS_MAX_DIMENSION)
    {
        return false;
    }
    if (!render_labels_atlas_reinitialize(system, next_dimension, true))
    {
        return false;
    }
    system->stats.atlas_grows++;
    return true;
}

static bool render_labels_pack_canvas(RenderLabelSystem *system, RenderLabelEntry *entry,
                                      const Image *canvas)
{
    if (!system || !entry || !canvas)
    {
        return false;
    }
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (!system->atlas_initialized)
        {
            if (!render_labels_atlas_reinitialize(system, RENDER_LABEL_ATLAS_INITIAL_DIMENSION,
                                                  false))
            {
                return false;
            }
        }
        TextureAtlasRegion region;
        if (texture_atlas_pack_image(&system->atlas, canvas, &region))
        {
            entry->region            = region;
            entry->width_pixels      = (float)region.width;
            entry->height_pixels     = (float)region.height;
            system->atlas_dirty      = true;
            system->atlas_compressed = false;
            return true;
        }
        if (!render_labels_grow_atlas(system))
        {
            break;
        }
    }
    return false;
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB */

static bool render_labels_reset(RenderLabelSystem *system)
{
    if (!system)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    for (size_t index = 0U; index < RENDER_LABEL_CANVAS_POOL_SIZE; ++index)
    {
        system->canvas_pool[index].in_use = false;
    }
    int dimension = system->atlas_dimension > 0 ? system->atlas_dimension
                                                : RENDER_LABEL_ATLAS_INITIAL_DIMENSION;
    if (!render_labels_atlas_reinitialize(system, dimension, true))
    {
        return false;
    }
#endif
    return true;
}

bool render_labels_init(RenderLabelSystem *system)
{
    if (!system)
    {
        return false;
    }
    memset(system, 0, sizeof(*system));
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    system->base_font_size          = 26.0f;
    system->text_color              = (Color){236, 248, 255, 255};
    system->background_color_top    = (Color){20, 32, 52, 228};
    system->background_color_bottom = (Color){6, 12, 24, 228};
    system->frame_color             = (Color){0, 210, 255, 200};
    system->entries                 = NULL;
    system->count                   = 0U;
    system->capacity                = 0U;
    system->atlas_initialized       = false;
    system->atlas_dirty             = false;
    system->atlas_compressed        = false;
    system->atlas_dimension         = RENDER_LABEL_ATLAS_INITIAL_DIMENSION;
    for (size_t index = 0U; index < RENDER_LABEL_CANVAS_POOL_SIZE; ++index)
    {
        system->canvas_pool[index].image.data = NULL;
        system->canvas_pool[index].in_use     = false;
    }
    if (!render_labels_atlas_reinitialize(system, system->atlas_dimension, false))
    {
        return false;
    }
#endif
    return true;
}

static bool render_labels_ensure_capacity(RenderLabelSystem *system)
{
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)system;
    return false;
#else
    if (system->count < system->capacity)
    {
        return true;
    }
    size_t new_capacity = (system->capacity == 0U) ? 8U : system->capacity * 2U;
    RenderLabelEntry *entries =
        at_secure_realloc(system->entries, new_capacity, sizeof(RenderLabelEntry));
    if (!entries)
    {
        return false;
    }
    for (size_t index = system->capacity; index < new_capacity; ++index)
    {
        entries[index].person_id     = 0U;
        entries[index].signature[0]  = '\0';
        entries[index].region.x      = 0;
        entries[index].region.y      = 0;
        entries[index].region.width  = 0;
        entries[index].region.height = 0;
        entries[index].width_pixels  = 0.0f;
        entries[index].height_pixels = 0.0f;
        entries[index].font_size     = 0.0f;
        entries[index].in_use        = false;
    }
    system->entries  = entries;
    system->capacity = new_capacity;
    return true;
#endif
}

void render_labels_shutdown(RenderLabelSystem *system)
{
    if (!system)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    for (size_t index = 0U; index < RENDER_LABEL_CANVAS_POOL_SIZE; ++index)
    {
        render_labels_destroy_canvas_slot(&system->canvas_pool[index]);
    }
    texture_atlas_shutdown(&system->atlas);
    system->atlas_initialized = false;
    system->atlas_dirty       = false;
    system->atlas_compressed  = false;
    system->atlas_dimension   = 0;
    AT_FREE(system->entries);
    system->entries        = NULL;
    system->count          = 0U;
    system->capacity       = 0U;
    system->base_font_size = 26.0f;
#endif
}

void render_labels_begin_frame(RenderLabelSystem *system)
{
    if (!system)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    for (size_t index = 0U; index < system->count; ++index)
    {
        system->entries[index].in_use = false;
    }
#endif
}

void render_labels_end_frame(RenderLabelSystem *system)
{
    if (!system)
    {
        return;
    }
    (void)system;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
void render_labels_get_stats(const RenderLabelSystem *system, RenderLabelStats *out_stats)
{
    if (!out_stats)
    {
        return;
    }
    if (!system)
    {
        memset(out_stats, 0, sizeof(*out_stats));
        return;
    }
    *out_stats = system->stats;
}
#else
void render_labels_get_stats(const RenderLabelSystem *system, RenderLabelStats *out_stats)
{
    (void)system;
    if (!out_stats)
    {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
}
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static float render_labels_clamp_font_size(float value)
{
    float clamped = value;
    if (clamped < 16.0f)
    {
        clamped = 16.0f;
    }
    if (clamped > 72.0f)
    {
        clamped = 72.0f;
    }
    return clamped;
}

static Color render_labels_lerp_color(Color a, Color b, float t)
{
    if (t < 0.0f)
    {
        t = 0.0f;
    }
    if (t > 1.0f)
    {
        t = 1.0f;
    }
    Color result;
    result.r = (unsigned char)((1.0f - t) * a.r + t * b.r);
    result.g = (unsigned char)((1.0f - t) * a.g + t * b.g);
    result.b = (unsigned char)((1.0f - t) * a.b + t * b.b);
    result.a = (unsigned char)((1.0f - t) * a.a + t * b.a);
    return result;
}

static void render_labels_draw_gradient(Image *image, int width, int height, Color top,
                                        Color bottom)
{
    if (!image || width <= 0 || height <= 0)
    {
        return;
    }
    for (int y = 0; y < height; ++y)
    {
        float t         = (height > 1) ? ((float)y / (float)(height - 1)) : 0.0f;
        Color row_color = render_labels_lerp_color(top, bottom, t);
        ImageDrawLine(image, 0, y, width - 1, y, row_color);
    }
}

static void render_labels_normalize_slashes(char *path)
{
    if (!path)
    {
        return;
    }
    for (size_t index = 0U; path[index] != '\0'; ++index)
    {
        if (path[index] == '\\')
        {
            path[index] = '/';
        }
    }
}

static bool render_labels_path_has_prefix_ci(const char *value, const char *prefix)
{
    if (!value || !prefix)
    {
        return false;
    }
    for (size_t index = 0U; prefix[index] != '\0'; ++index)
    {
        char lhs = value[index];
        char rhs = prefix[index];
        if (lhs == '\0')
        {
            return false;
        }
        lhs = (char)tolower((unsigned char)lhs);
        rhs = (char)tolower((unsigned char)rhs);
        if (lhs != rhs)
        {
            return false;
        }
    }
    return true;
}

static bool render_labels_try_resolve_candidate(const char *candidate, char *resolved,
                                                size_t capacity)
{
    if (!candidate || candidate[0] == '\0' || !resolved || capacity == 0U)
    {
        return false;
    }

    char normalized[512];
    size_t length = 0U;
    while (candidate[length] != '\0' && length + 1U < sizeof(normalized))
    {
        char c = candidate[length];
        if (c == '\\')
        {
            c = '/';
        }
        normalized[length++] = c;
    }
    if (candidate[length] != '\0')
    {
        return false;
    }
    normalized[length] = '\0';

    if (FileExists(normalized))
    {
        if (length + 1U > capacity)
        {
            return false;
        }
        memcpy(resolved, normalized, length + 1U);
        return true;
    }

    const unsigned int level_candidates[] = {0U, 1U, 2U, 3U};
    const char *application_dir           = GetApplicationDirectory();
    for (size_t level = 0U; level < sizeof(level_candidates) / sizeof(level_candidates[0]); ++level)
    {
        char joined[512];
        if (!path_join_relative(application_dir, level_candidates[level], normalized, joined,
                                sizeof(joined)))
        {
            continue;
        }
        if (FileExists(joined))
        {
            size_t joined_length = strlen(joined);
            if (joined_length + 1U > capacity)
            {
                return false;
            }
            memcpy(resolved, joined, joined_length + 1U);
            return true;
        }
    }

    return false;
}

static bool render_labels_resolve_profile_image_path(const char *input_path, char *resolved,
                                                     size_t capacity)
{
    if (!input_path || input_path[0] == '\0' || !resolved || capacity == 0U)
    {
        return false;
    }

    char assets_candidate[512];
    const char *candidates[3];
    size_t candidate_count = 0U;

    candidates[candidate_count++] = input_path;

    if (!render_labels_path_has_prefix_ci(input_path, "assets/"))
    {
        if (snprintf(assets_candidate, sizeof(assets_candidate), "assets/%s", input_path) <
            (int)sizeof(assets_candidate))
        {
            render_labels_normalize_slashes(assets_candidate);
            candidates[candidate_count++] = assets_candidate;
        }
    }

    for (size_t index = 0U; index < candidate_count; ++index)
    {
        if (render_labels_try_resolve_candidate(candidates[index], resolved, capacity))
        {
            return true;
        }
    }

    return false;
}

static bool render_labels_build_texture(RenderLabelSystem *system, RenderLabelEntry *entry,
                                        const Person *person, bool include_profile,
                                        float requested_font_size)
{
    char name_buffer[192];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person ? person->id : 0U);
    }

    float font_size     = render_labels_clamp_font_size(requested_font_size);
    Font font           = GetFontDefault();
    float spacing       = font_size * 0.08f;
    Vector2 text_size   = MeasureTextEx(font, name_buffer, font_size, spacing);
    int padding         = (int)(font_size * 0.6f);
    int portrait_pixels = 0;
    Image profile_image = (Image){0};
    bool profile_loaded = false;

    if (include_profile && person && person->profile_image_path &&
        person->profile_image_path[0] != '\0')
    {
        char resolved_path[512];
        if (render_labels_resolve_profile_image_path(person->profile_image_path, resolved_path,
                                                     sizeof(resolved_path)))
        {
            profile_image = LoadImage(resolved_path);
            if (profile_image.data != NULL)
            {
                portrait_pixels = (int)(font_size * 2.0f);
                if (portrait_pixels < 48)
                {
                    portrait_pixels = 48;
                }
                ImageResize(&profile_image, portrait_pixels, portrait_pixels);
                profile_loaded = true;
            }
        }
    }

    int width          = (int)ceilf(text_size.x) + padding * 2 + portrait_pixels;
    int text_height    = (int)ceilf(text_size.y);
    int content_height = text_height;
    if (portrait_pixels > 0 && portrait_pixels > content_height)
    {
        content_height = portrait_pixels;
    }
    int height = content_height + padding * 2;
    if (width <= 0)
    {
        width = 64;
    }
    if (height <= 0)
    {
        height = 32;
    }

    RenderLabelCanvasSlot *slot = render_labels_canvas_acquire(system, width, height);
    if (!slot)
    {
        if (profile_loaded)
        {
            UnloadImage(profile_image);
        }
        return false;
    }
    Image *canvas = &slot->image;
    render_labels_draw_gradient(canvas, width, height, system->background_color_top,
                                system->background_color_bottom);
    ImageDrawRectangleLines(canvas,
                            (Rectangle){1.0f, 1.0f, (float)(width - 2), (float)(height - 2)}, 2,
                            system->frame_color);

    if (profile_loaded)
    {
        Rectangle src_rect = {0.0f, 0.0f, (float)profile_image.width, (float)profile_image.height};
        Rectangle dst_rect = {(float)padding,
                              (float)(padding + (content_height - portrait_pixels) / 2),
                              (float)portrait_pixels, (float)portrait_pixels};
        ImageDraw(canvas, profile_image, src_rect, dst_rect, WHITE);
        UnloadImage(profile_image);
    }

    Vector2 text_position = {
        (float)(padding + portrait_pixels + (portrait_pixels > 0 ? padding / 2 : 0)),
        (float)(padding + (content_height - text_height) / 2)};
    ImageDrawTextEx(canvas, font, name_buffer, text_position, font_size, spacing,
                    system->text_color);

    bool packed = render_labels_pack_canvas(system, entry, canvas);
    render_labels_canvas_release(slot);
    if (!packed)
    {
        return false;
    }

    entry->font_size = font_size;
    return true;
}
#endif

bool render_labels_acquire(RenderLabelSystem *system, const Person *person, bool include_profile,
                           float requested_font_size, RenderLabelInfo *out_info)
{
    if (!system || !person || !out_info)
    {
        return false;
    }
    memset(out_info, 0, sizeof(*out_info));
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)include_profile;
    (void)requested_font_size;
    return false;
#else
    float font_size = (requested_font_size > 0.0f) ? requested_font_size : system->base_font_size;
    font_size       = render_labels_clamp_font_size(font_size);

    char signature[256];
    char name_buffer[192];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }
    const char *profile_path =
        (include_profile && person->profile_image_path) ? person->profile_image_path : "";
    (void)snprintf(signature, sizeof(signature), "%0.2f|%s|%s", font_size, name_buffer,
                   profile_path);

    RenderLabelEntry *available_entry = NULL;
    for (size_t index = 0U; index < system->count; ++index)
    {
        RenderLabelEntry *entry = &system->entries[index];
        bool signature_matches  = (entry->person_id == person->id) &&
                                 (entry->region.width > 0 && entry->region.height > 0) &&
                                 (strcmp(entry->signature, signature) == 0) &&
                                 (fabsf(entry->font_size - font_size) < 0.05f);
        if (signature_matches)
        {
            entry->in_use = true;
            if (!render_labels_finalize_atlas(system))
            {
                return false;
            }
            size_t entry_index = (size_t)(entry - system->entries);
            if (system->count <= entry_index)
            {
                system->count = entry_index + 1U;
            }
            Texture2D atlas_texture = texture_atlas_texture(&system->atlas);
            out_info->texture       = atlas_texture;
            out_info->region        = (Rectangle){(float)entry->region.x, (float)entry->region.y,
                                                  (float)entry->region.width, (float)entry->region.height};
            out_info->width_pixels  = entry->width_pixels;
            out_info->height_pixels = entry->height_pixels;
            out_info->font_size     = entry->font_size;
            out_info->valid         = true;
            return true;
        }
        if (!available_entry && entry->person_id == 0U)
        {
            available_entry = entry;
        }
    }

    if (!available_entry)
    {
        if (!render_labels_ensure_capacity(system))
        {
            return false;
        }
        available_entry = &system->entries[system->count++];
    }

    if (!render_labels_build_texture(system, available_entry, person, include_profile, font_size))
    {
        available_entry->person_id     = 0U;
        available_entry->signature[0]  = '\0';
        available_entry->region.x      = 0;
        available_entry->region.y      = 0;
        available_entry->region.width  = 0;
        available_entry->region.height = 0;
        available_entry->width_pixels  = 0.0f;
        available_entry->height_pixels = 0.0f;
        available_entry->font_size     = 0.0f;
        available_entry->in_use        = false;
        return false;
    }

    available_entry->person_id = person->id;
    (void)snprintf(available_entry->signature, sizeof(available_entry->signature), "%s", signature);
    available_entry->in_use = true;

    if (!render_labels_finalize_atlas(system))
    {
        available_entry->person_id     = 0U;
        available_entry->signature[0]  = '\0';
        available_entry->region.x      = 0;
        available_entry->region.y      = 0;
        available_entry->region.width  = 0;
        available_entry->region.height = 0;
        available_entry->width_pixels  = 0.0f;
        available_entry->height_pixels = 0.0f;
        available_entry->font_size     = 0.0f;
        available_entry->in_use        = false;
        return false;
    }

    size_t entry_index = (size_t)(available_entry - system->entries);
    if (system->count <= entry_index)
    {
        system->count = entry_index + 1U;
    }

    Texture2D atlas_texture = texture_atlas_texture(&system->atlas);
    out_info->texture       = atlas_texture;
    out_info->region =
        (Rectangle){(float)available_entry->region.x, (float)available_entry->region.y,
                    (float)available_entry->region.width, (float)available_entry->region.height};
    out_info->width_pixels  = available_entry->width_pixels;
    out_info->height_pixels = available_entry->height_pixels;
    out_info->font_size     = available_entry->font_size;
    out_info->valid         = true;
    return true;
#endif
}

void render_labels_set_base_font_size(RenderLabelSystem *system, float font_size)
{
    if (!system)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    float clamped =
        render_labels_clamp_font_size((font_size > 0.0f) ? font_size : system->base_font_size);
    if (fabsf(system->base_font_size - clamped) < 0.05f)
    {
        return;
    }
    system->base_font_size = clamped;
    (void)render_labels_reset(system);
#else
    (void)font_size;
#endif
}
