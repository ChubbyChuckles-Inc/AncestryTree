#include "render_labels.h"

#include "at_memory.h"
#include "person.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raymath.h>
#endif

static bool render_labels_reset(RenderLabelSystem *system)
{
    if (!system)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    for (size_t index = 0U; index < system->count; ++index)
    {
        RenderLabelEntry *entry = &system->entries[index];
        if (entry->texture.id != 0)
        {
            UnloadTexture(entry->texture);
            entry->texture.id = 0;
        }
        entry->in_use = false;
        entry->signature[0] = '\0';
        entry->width_pixels = 0.0f;
        entry->height_pixels = 0.0f;
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
    system->font_size = 24.0f;
    system->text_color = (Color){236, 248, 255, 255};
    system->background_color_top = (Color){20, 32, 52, 228};
    system->background_color_bottom = (Color){6, 12, 24, 228};
    system->frame_color = (Color){0, 210, 255, 200};
    system->entries = NULL;
    system->count = 0U;
    system->capacity = 0U;
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
    RenderLabelEntry *entries = at_secure_realloc(system->entries, new_capacity, sizeof(RenderLabelEntry));
    if (!entries)
    {
        return false;
    }
    for (size_t index = system->capacity; index < new_capacity; ++index)
    {
        entries[index].person_id = 0U;
        entries[index].signature[0] = '\0';
        entries[index].texture.id = 0;
        entries[index].texture.width = 0;
        entries[index].texture.height = 0;
        entries[index].width_pixels = 0.0f;
        entries[index].height_pixels = 0.0f;
        entries[index].in_use = false;
    }
    system->entries = entries;
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
    render_labels_reset(system);
    free(system->entries);
    system->entries = NULL;
    system->count = 0U;
    system->capacity = 0U;
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
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    for (size_t index = 0U; index < system->count; ++index)
    {
        RenderLabelEntry *entry = &system->entries[index];
        if (!entry->in_use && entry->texture.id != 0)
        {
            UnloadTexture(entry->texture);
            entry->texture.id = 0;
            entry->width_pixels = 0.0f;
            entry->height_pixels = 0.0f;
            entry->signature[0] = '\0';
            entry->person_id = 0U;
        }
    }
#endif
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
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

static void render_labels_draw_gradient(Image *image, int width, int height, Color top, Color bottom)
{
    if (!image || width <= 0 || height <= 0)
    {
        return;
    }
    for (int y = 0; y < height; ++y)
    {
        float t = (height > 1) ? ((float)y / (float)(height - 1)) : 0.0f;
        Color row_color = render_labels_lerp_color(top, bottom, t);
        ImageDrawLine(image, 0, y, width - 1, y, row_color);
    }
}

static bool render_labels_build_texture(RenderLabelSystem *system, RenderLabelEntry *entry, const Person *person,
                                        bool include_profile)
{
    char name_buffer[192];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person ? person->id : 0U);
    }

    Font font = GetFontDefault();
    float spacing = system->font_size * 0.08f;
    Vector2 text_size = MeasureTextEx(font, name_buffer, system->font_size, spacing);
    int padding = (int)(system->font_size * 0.6f);
    int portrait_pixels = 0;
    Image profile_image = {0};
    bool profile_loaded = false;

    if (include_profile && person && person->profile_image_path && person->profile_image_path[0] != '\0')
    {
        if (FileExists(person->profile_image_path))
        {
            profile_image = LoadImage(person->profile_image_path);
            if (profile_image.data != NULL)
            {
                portrait_pixels = (int)(system->font_size * 2.0f);
                if (portrait_pixels < 48)
                {
                    portrait_pixels = 48;
                }
                ImageResize(&profile_image, portrait_pixels, portrait_pixels);
                profile_loaded = true;
            }
        }
    }

    int width = (int)ceilf(text_size.x) + padding * 2 + portrait_pixels;
    int text_height = (int)ceilf(text_size.y);
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

    Image canvas = GenImageColor(width, height, (Color){0, 0, 0, 0});
    render_labels_draw_gradient(&canvas, width, height, system->background_color_top, system->background_color_bottom);
    ImageDrawRectangleLines(&canvas, (Rectangle){1.0f, 1.0f, (float)(width - 2), (float)(height - 2)}, 2,
                            system->frame_color);

    if (profile_loaded)
    {
        Rectangle src_rect = {0.0f, 0.0f, (float)profile_image.width, (float)profile_image.height};
        Rectangle dst_rect = {(float)padding, (float)(padding + (content_height - portrait_pixels) / 2),
                              (float)portrait_pixels, (float)portrait_pixels};
        ImageDraw(&canvas, profile_image, src_rect, dst_rect, WHITE);
        UnloadImage(profile_image);
    }

    Vector2 text_position = {(float)(padding + portrait_pixels + (portrait_pixels > 0 ? padding / 2 : 0)),
                             (float)(padding + (content_height - text_height) / 2)};
    ImageDrawTextEx(&canvas, font, name_buffer, text_position, system->font_size, spacing, system->text_color);

    Texture2D texture = LoadTextureFromImage(canvas);
    UnloadImage(canvas);
    if (texture.id == 0)
    {
        return false;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    entry->texture = texture;
    entry->width_pixels = (float)texture.width;
    entry->height_pixels = (float)texture.height;
    return true;
}
#endif

bool render_labels_acquire(RenderLabelSystem *system, const Person *person, bool include_profile,
                           RenderLabelInfo *out_info)
{
    if (!system || !person || !out_info)
    {
        return false;
    }
    memset(out_info, 0, sizeof(*out_info));
#if !defined(ANCESTRYTREE_HAVE_RAYLIB)
    (void)include_profile;
    return false;
#else
    char signature[256];
    char name_buffer[192];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }
    const char *profile_path = (include_profile && person->profile_image_path) ? person->profile_image_path : "";
    (void)snprintf(signature, sizeof(signature), "%s|%s", name_buffer, profile_path);

    RenderLabelEntry *available_entry = NULL;
    for (size_t index = 0U; index < system->count; ++index)
    {
        RenderLabelEntry *entry = &system->entries[index];
        if (entry->person_id == person->id && strcmp(entry->signature, signature) == 0 && entry->texture.id != 0)
        {
            entry->in_use = true;
            out_info->texture = entry->texture;
            out_info->width_pixels = entry->width_pixels;
            out_info->height_pixels = entry->height_pixels;
            out_info->valid = true;
            return true;
        }
        if (!available_entry && entry->texture.id == 0)
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

    if (!render_labels_build_texture(system, available_entry, person, include_profile))
    {
        available_entry->person_id = 0U;
        available_entry->signature[0] = '\0';
        available_entry->texture.id = 0;
        available_entry->width_pixels = 0.0f;
        available_entry->height_pixels = 0.0f;
        available_entry->in_use = false;
        return false;
    }

    available_entry->person_id = person->id;
    (void)snprintf(available_entry->signature, sizeof(available_entry->signature), "%s", signature);
    available_entry->in_use = true;

    out_info->texture = available_entry->texture;
    out_info->width_pixels = available_entry->width_pixels;
    out_info->height_pixels = available_entry->height_pixels;
    out_info->valid = true;
    return true;
#endif
}
