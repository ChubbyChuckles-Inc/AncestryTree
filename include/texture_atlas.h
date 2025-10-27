#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <stdbool.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

typedef struct TextureAtlasRegion
{
    int x;
    int y;
    int width;
    int height;
} TextureAtlasRegion;

typedef struct TextureAtlas
{
    int width;
    int height;
    int padding;
    int cursor_x;
    int cursor_y;
    int shelf_height;
    bool finalized;
    bool dirty;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    Image canvas;
    Texture2D texture;
#else
    void *reserved0;
    void *reserved1;
#endif
} TextureAtlas;

bool texture_atlas_init(TextureAtlas *atlas, int width, int height, int padding);
void texture_atlas_shutdown(TextureAtlas *atlas);
bool texture_atlas_ready(const TextureAtlas *atlas);

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
bool texture_atlas_pack_image(TextureAtlas *atlas, const Image *image,
                              TextureAtlasRegion *out_region);
bool texture_atlas_pack_pixels(TextureAtlas *atlas, int width, int height, const Color *pixels,
                               TextureAtlasRegion *out_region);
bool texture_atlas_finalize(TextureAtlas *atlas);
Texture2D texture_atlas_texture(const TextureAtlas *atlas);
#endif

#endif /* TEXTURE_ATLAS_H */
