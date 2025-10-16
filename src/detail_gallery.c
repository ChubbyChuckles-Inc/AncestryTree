#include "detail_gallery.h"

#include "at_memory.h"
#include "at_string.h"
#include "person.h"
#include "timeline.h"

#include <stdio.h>
#include <string.h>

#define DETAIL_GALLERY_MIN_ZOOM 0.35f
#define DETAIL_GALLERY_MAX_ZOOM 3.0f
#define DETAIL_GALLERY_DEFAULT_ZOOM 1.0f

static bool detail_gallery_ensure_capacity(DetailGallery *gallery, size_t capacity)
{
    if (!gallery)
    {
        return false;
    }
    if (gallery->capacity >= capacity)
    {
        return true;
    }
    size_t new_capacity = (gallery->capacity == 0U) ? capacity : gallery->capacity;
    while (new_capacity < capacity)
    {
        new_capacity = (new_capacity == 0U) ? 4U : new_capacity * 2U;
    }
    DetailGalleryEntry *entries =
        at_secure_realloc(gallery->entries, new_capacity, sizeof(DetailGalleryEntry));
    if (!entries)
    {
        return false;
    }
    for (size_t index = gallery->capacity; index < new_capacity; ++index)
    {
        entries[index].path = NULL;
        entries[index].file_available = false;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        entries[index].texture_loaded = false;
        entries[index].texture.id = 0;
#endif
    }
    gallery->entries = entries;
    gallery->capacity = new_capacity;
    return true;
}

static void detail_gallery_entry_reset(DetailGalleryEntry *entry)
{
    if (!entry)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (entry->texture_loaded)
    {
        UnloadTexture(entry->texture);
        entry->texture_loaded = false;
        entry->texture.id = 0;
    }
#endif
    AT_FREE(entry->path);
    entry->path = NULL;
    entry->file_available = false;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
void detail_gallery_release_textures(DetailGallery *gallery)
{
    if (!gallery)
    {
        return;
    }
    for (size_t index = 0U; index < gallery->count; ++index)
    {
        DetailGalleryEntry *entry = &gallery->entries[index];
        if (entry->texture_loaded)
        {
            UnloadTexture(entry->texture);
            entry->texture_loaded = false;
            entry->texture.id = 0;
        }
    }
}
#endif

void detail_gallery_reset(DetailGallery *gallery)
{
    if (!gallery)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    detail_gallery_release_textures(gallery);
#endif
    for (size_t index = 0U; index < gallery->count; ++index)
    {
        detail_gallery_entry_reset(&gallery->entries[index]);
    }
    gallery->count = 0U;
    gallery->selected_index = -1;
    gallery->zoom = DETAIL_GALLERY_DEFAULT_ZOOM;
}

bool detail_gallery_init(DetailGallery *gallery)
{
    if (!gallery)
    {
        return false;
    }
    memset(gallery, 0, sizeof(*gallery));
    gallery->selected_index = -1;
    gallery->zoom = DETAIL_GALLERY_DEFAULT_ZOOM;
    return true;
}

void detail_gallery_shutdown(DetailGallery *gallery)
{
    if (!gallery)
    {
        return;
    }
    detail_gallery_reset(gallery);
    AT_FREE(gallery->entries);
    gallery->entries = NULL;
    gallery->capacity = 0U;
}

static bool detail_gallery_path_exists(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return false;
    }
    FILE *file = fopen(path, "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}

static bool detail_gallery_contains(const DetailGallery *gallery, const char *path)
{
    if (!gallery || !path)
    {
        return false;
    }
    for (size_t index = 0U; index < gallery->count; ++index)
    {
        if (gallery->entries[index].path && strcmp(gallery->entries[index].path, path) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool detail_gallery_add_path(DetailGallery *gallery, const char *path)
{
    if (!gallery || !path || path[0] == '\0')
    {
        return false;
    }
    if (detail_gallery_contains(gallery, path))
    {
        return true;
    }
    if (!detail_gallery_ensure_capacity(gallery, gallery->count + 1U))
    {
        return false;
    }
    char *copy = at_string_dup(path);
    if (!copy)
    {
        return false;
    }
    DetailGalleryEntry *entry = &gallery->entries[gallery->count];
    entry->path = copy;
    entry->file_available = detail_gallery_path_exists(path);
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    entry->texture_loaded = false;
    entry->texture.id = 0;
#endif
    gallery->count += 1U;
    if (gallery->selected_index < 0)
    {
        gallery->selected_index = (int)(gallery->count - 1U);
    }
    return true;
}

static bool detail_gallery_collect_person_media(DetailGallery *gallery, const Person *person)
{
    if (!gallery || !person)
    {
        return false;
    }
    if (person->profile_image_path && person->profile_image_path[0] != '\0')
    {
        if (!detail_gallery_add_path(gallery, person->profile_image_path))
        {
            return false;
        }
    }
    for (size_t cert = 0U; cert < person->certificate_count; ++cert)
    {
        const char *path = person->certificate_paths[cert];
        if (path && path[0] != '\0')
        {
            if (!detail_gallery_add_path(gallery, path))
            {
                return false;
            }
        }
    }
    for (size_t entry_index = 0U; entry_index < person->timeline_count; ++entry_index)
    {
        const TimelineEntry *entry = &person->timeline_entries[entry_index];
        for (size_t media_index = 0U; media_index < entry->media_count; ++media_index)
        {
            const char *path = entry->media_paths[media_index];
            if (path && path[0] != '\0')
            {
                if (!detail_gallery_add_path(gallery, path))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool detail_gallery_populate_from_person(DetailGallery *gallery, const Person *person)
{
    if (!gallery)
    {
        return false;
    }
    detail_gallery_reset(gallery);
    if (!person)
    {
        return true;
    }
    if (!detail_gallery_collect_person_media(gallery, person))
    {
        detail_gallery_reset(gallery);
        return false;
    }
    if (gallery->count == 0U)
    {
        gallery->selected_index = -1;
    }
    return true;
}

bool detail_gallery_has_media(const DetailGallery *gallery)
{
    return gallery ? (gallery->count > 0U) : false;
}

const DetailGalleryEntry *detail_gallery_get_entry(const DetailGallery *gallery, size_t index)
{
    if (!gallery || index >= gallery->count)
    {
        return NULL;
    }
    return &gallery->entries[index];
}

bool detail_gallery_select(DetailGallery *gallery, size_t index)
{
    if (!gallery || index >= gallery->count)
    {
        return false;
    }
    gallery->selected_index = (int)index;
    return true;
}

bool detail_gallery_select_next(DetailGallery *gallery)
{
    if (!gallery || gallery->count == 0U)
    {
        return false;
    }
    size_t next = (gallery->selected_index < 0) ? 0U : ((size_t)gallery->selected_index + 1U) % gallery->count;
    gallery->selected_index = (int)next;
    return true;
}

bool detail_gallery_select_previous(DetailGallery *gallery)
{
    if (!gallery || gallery->count == 0U)
    {
        return false;
    }
    size_t prev;
    if (gallery->selected_index < 0)
    {
        prev = gallery->count - 1U;
    }
    else if (gallery->selected_index == 0)
    {
        prev = gallery->count - 1U;
    }
    else
    {
        prev = (size_t)gallery->selected_index - 1U;
    }
    gallery->selected_index = (int)prev;
    return true;
}

float detail_gallery_get_zoom(const DetailGallery *gallery)
{
    return gallery ? gallery->zoom : DETAIL_GALLERY_DEFAULT_ZOOM;
}

static float detail_gallery_clamp_zoom(float zoom)
{
    if (zoom < DETAIL_GALLERY_MIN_ZOOM)
    {
        return DETAIL_GALLERY_MIN_ZOOM;
    }
    if (zoom > DETAIL_GALLERY_MAX_ZOOM)
    {
        return DETAIL_GALLERY_MAX_ZOOM;
    }
    return zoom;
}

void detail_gallery_set_zoom(DetailGallery *gallery, float zoom)
{
    if (!gallery)
    {
        return;
    }
    gallery->zoom = detail_gallery_clamp_zoom(zoom);
}

float detail_gallery_min_zoom(void)
{
    return DETAIL_GALLERY_MIN_ZOOM;
}

float detail_gallery_max_zoom(void)
{
    return DETAIL_GALLERY_MAX_ZOOM;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
unsigned int detail_gallery_acquire_texture(DetailGallery *gallery, size_t index)
{
    if (!gallery || index >= gallery->count)
    {
        return 0U;
    }
    DetailGalleryEntry *entry = &gallery->entries[index];
    if (!entry->file_available)
    {
        return 0U;
    }
    if (!entry->texture_loaded)
    {
        Texture2D texture = LoadTexture(entry->path);
        if (texture.id == 0U)
        {
            entry->file_available = false;
            return 0U;
        }
        entry->texture = texture;
        entry->texture_loaded = true;
    }
    return entry->texture.id;
}
#endif
