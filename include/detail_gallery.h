#ifndef DETAIL_GALLERY_H
#define DETAIL_GALLERY_H

#include <stdbool.h>
#include <stddef.h>

struct Person;

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

typedef struct DetailGalleryEntry
{
    char *path;
    bool file_available;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    Texture2D texture;
    bool texture_loaded;
#endif
} DetailGalleryEntry;

typedef struct DetailGallery
{
    DetailGalleryEntry *entries;
    size_t count;
    size_t capacity;
    int selected_index;
    float zoom;
} DetailGallery;

#ifdef __cplusplus
extern "C"
{
#endif

    bool detail_gallery_init(DetailGallery *gallery);
    void detail_gallery_reset(DetailGallery *gallery);
    void detail_gallery_shutdown(DetailGallery *gallery);
    bool detail_gallery_populate_from_person(DetailGallery *gallery, const struct Person *person);
    bool detail_gallery_has_media(const DetailGallery *gallery);
    const DetailGalleryEntry *detail_gallery_get_entry(const DetailGallery *gallery, size_t index);
    bool detail_gallery_select(DetailGallery *gallery, size_t index);
    bool detail_gallery_select_next(DetailGallery *gallery);
    bool detail_gallery_select_previous(DetailGallery *gallery);
    float detail_gallery_get_zoom(const DetailGallery *gallery);
    void detail_gallery_set_zoom(DetailGallery *gallery, float zoom);
    float detail_gallery_min_zoom(void);
    float detail_gallery_max_zoom(void);

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    unsigned int detail_gallery_acquire_texture(DetailGallery *gallery, size_t index);
    void detail_gallery_release_textures(DetailGallery *gallery);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DETAIL_GALLERY_H */
