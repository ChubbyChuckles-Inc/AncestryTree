#include "test_framework.h"
#include "texture_atlas.h"

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

TEST(test_texture_atlas_packs_regions)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    InitWindow(160, 120, "atlas_test");
    TextureAtlas atlas;
    ASSERT_TRUE(texture_atlas_init(&atlas, 128, 128, 1));

    Image first = GenImageColor(16, 16, (Color){200, 80, 120, 255});
    TextureAtlasRegion region_first;
    ASSERT_TRUE(texture_atlas_pack_image(&atlas, &first, &region_first));
    UnloadImage(first);

    Image second = GenImageColor(24, 12, (Color){80, 160, 220, 255});
    TextureAtlasRegion region_second;
    ASSERT_TRUE(texture_atlas_pack_image(&atlas, &second, &region_second));
    UnloadImage(second);

    ASSERT_TRUE(texture_atlas_finalize(&atlas));
    ASSERT_TRUE(texture_atlas_ready(&atlas));

    Texture2D texture = texture_atlas_texture(&atlas);
    ASSERT_TRUE(texture.id != 0U);
    ASSERT_TRUE(region_first.width == 16);
    ASSERT_TRUE(region_second.width == 24);
    ASSERT_TRUE(region_first.x != region_second.x || region_first.y != region_second.y);

    texture_atlas_shutdown(&atlas);
    CloseWindow();
#else
    ASSERT_TRUE(true);
#endif
}

void register_texture_atlas_tests(TestRegistry *registry)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    REGISTER_TEST(registry, test_texture_atlas_packs_regions);
#else
    (void)registry;
#endif
}
