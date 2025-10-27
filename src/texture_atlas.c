#include "texture_atlas.h"

#include <stddef.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)

static bool texture_atlas_allocate_slot(TextureAtlas *atlas, int width, int height,
                                        TextureAtlasRegion *out_region)
{
    if (!atlas || width <= 0 || height <= 0)
    {
        return false;
    }

    int padding       = (atlas->padding > 0) ? atlas->padding : 0;
    int padded_width  = width + padding * 2;
    int padded_height = height + padding * 2;

    if (padded_width > atlas->width || padded_height > atlas->height)
    {
        return false;
    }

    if (atlas->cursor_x + padded_width > atlas->width)
    {
        atlas->cursor_x = 0;
        atlas->cursor_y += atlas->shelf_height;
        atlas->shelf_height = 0;
    }

    if (atlas->cursor_y + padded_height > atlas->height)
    {
        return false;
    }

    int draw_x = atlas->cursor_x + padding;
    int draw_y = atlas->cursor_y + padding;

    atlas->cursor_x += padded_width;
    if (padded_height > atlas->shelf_height)
    {
        atlas->shelf_height = padded_height;
    }

    if (out_region)
    {
        out_region->x      = draw_x;
        out_region->y      = draw_y;
        out_region->width  = width;
        out_region->height = height;
    }

    return true;
}

#endif

bool texture_atlas_init(TextureAtlas *atlas, int width, int height, int padding)
{
    if (!atlas || width <= 0 || height <= 0)
    {
        return false;
    }

    memset(atlas, 0, sizeof(*atlas));
    atlas->width        = width;
    atlas->height       = height;
    atlas->padding      = (padding >= 0) ? padding : 0;
    atlas->cursor_x     = 0;
    atlas->cursor_y     = 0;
    atlas->shelf_height = 0;
    atlas->finalized    = false;
    atlas->dirty        = false;

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    atlas->canvas = GenImageColor(width, height, (Color){0, 0, 0, 0});
    if (!atlas->canvas.data)
    {
        memset(atlas, 0, sizeof(*atlas));
        return false;
    }
    atlas->texture        = (Texture2D){0};
    atlas->texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
#else
    (void)padding;
#endif
    return true;
}

void texture_atlas_shutdown(TextureAtlas *atlas)
{
    if (!atlas)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (atlas->texture.id != 0U)
    {
        UnloadTexture(atlas->texture);
        atlas->texture.id = 0U;
    }
    if (atlas->canvas.data)
    {
        UnloadImage(atlas->canvas);
        atlas->canvas.data   = NULL;
        atlas->canvas.width  = 0;
        atlas->canvas.height = 0;
    }
#endif
    memset(atlas, 0, sizeof(*atlas));
}

bool texture_atlas_ready(const TextureAtlas *atlas)
{
    if (!atlas)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    return atlas->finalized && atlas->texture.id != 0U;
#else
    return false;
#endif
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)

bool texture_atlas_pack_image(TextureAtlas *atlas, const Image *image,
                              TextureAtlasRegion *out_region)
{
    if (!atlas || !image || image->width <= 0 || image->height <= 0)
    {
        return false;
    }
    if (!atlas->canvas.data)
    {
        return false;
    }
    TextureAtlasRegion region;
    if (!texture_atlas_allocate_slot(atlas, image->width, image->height, &region))
    {
        return false;
    }

    Rectangle src = {0.0f, 0.0f, (float)image->width, (float)image->height};
    Rectangle dst = {(float)region.x, (float)region.y, (float)region.width, (float)region.height};
    ImageDraw(&atlas->canvas, *image, src, dst, WHITE);

    atlas->finalized = false;
    atlas->dirty     = true;
    if (out_region)
    {
        *out_region = region;
    }
    return true;
}

bool texture_atlas_pack_pixels(TextureAtlas *atlas, int width, int height, const Color *pixels,
                               TextureAtlasRegion *out_region)
{
    if (!atlas || !pixels || width <= 0 || height <= 0)
    {
        return false;
    }
    if (!atlas->canvas.data)
    {
        return false;
    }
    TextureAtlasRegion region;
    if (!texture_atlas_allocate_slot(atlas, width, height, &region))
    {
        return false;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Color color = pixels[y * width + x];
            ImageDrawPixel(&atlas->canvas, region.x + x, region.y + y, color);
        }
    }

    atlas->finalized = false;
    atlas->dirty     = true;
    if (out_region)
    {
        *out_region = region;
    }
    return true;
}

bool texture_atlas_finalize(TextureAtlas *atlas)
{
    if (!atlas || !atlas->canvas.data)
    {
        return false;
    }
    if (!atlas->dirty && atlas->finalized && atlas->texture.id != 0U)
    {
        return true;
    }

    Texture2D texture = LoadTextureFromImage(atlas->canvas);
    if (texture.id == 0U)
    {
        return false;
    }

    if (IsTextureReady(texture))
    {
        GenTextureMipmaps(&texture);
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }

    if (atlas->texture.id != 0U)
    {
        UnloadTexture(atlas->texture);
    }

    atlas->texture   = texture;
    atlas->finalized = true;
    atlas->dirty     = false;
    return true;
}

Texture2D texture_atlas_texture(const TextureAtlas *atlas)
{
    Texture2D empty = {0};
    if (!atlas)
    {
        return empty;
    }
    return atlas->texture;
}

#endif
