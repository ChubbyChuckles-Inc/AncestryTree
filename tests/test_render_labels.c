#include "render_labels.h"

#include "person.h"
#include "test_framework.h"

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <math.h>
#include <raylib.h>
#endif

static void test_render_labels_setup_person(Person **out_person)
{
    Person *person = person_create(512U);
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(person_set_name(person, "Avery", "Quinn", "Hologram"));
    *out_person = person;
}

TEST(test_render_labels_cache_reuses_texture)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    bool created_window = false;
    if (!IsWindowReady())
    {
        SetTraceLogLevel(LOG_WARNING);
        InitWindow(320, 180, "render_labels_cache_test");
        created_window = true;
    }

    RenderLabelSystem system;
    ASSERT_TRUE(render_labels_init(&system));

    Person *person = NULL;
    test_render_labels_setup_person(&person);

    render_labels_begin_frame(&system);
    RenderLabelInfo info_first;
    ASSERT_TRUE(render_labels_acquire(&system, person, false, 26.0f, &info_first));
    ASSERT_TRUE(info_first.valid);
    unsigned int first_texture_id = info_first.texture.id;
    Rectangle first_region        = info_first.region;
    render_labels_end_frame(&system);

    render_labels_begin_frame(&system);
    RenderLabelInfo info_second;
    ASSERT_TRUE(render_labels_acquire(&system, person, false, 26.0f, &info_second));
    ASSERT_TRUE(info_second.valid);
    ASSERT_EQ(first_texture_id, info_second.texture.id);
    ASSERT_FLOAT_NEAR(first_region.x, info_second.region.x, 0.01f);
    ASSERT_FLOAT_NEAR(first_region.y, info_second.region.y, 0.01f);
    ASSERT_FLOAT_NEAR(first_region.width, info_second.region.width, 0.01f);
    ASSERT_FLOAT_NEAR(first_region.height, info_second.region.height, 0.01f);
    render_labels_end_frame(&system);

    render_labels_shutdown(&system);
    person_destroy(person);

    if (created_window)
    {
        CloseWindow();
        SetTraceLogLevel(LOG_INFO);
    }
#else
    ASSERT_TRUE(true);
#endif
}

TEST(test_render_labels_distinct_font_sizes_generate_unique_textures)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    bool created_window = false;
    if (!IsWindowReady())
    {
        SetTraceLogLevel(LOG_WARNING);
        InitWindow(320, 180, "render_labels_font_test");
        created_window = true;
    }

    RenderLabelSystem system;
    ASSERT_TRUE(render_labels_init(&system));

    Person *person = NULL;
    test_render_labels_setup_person(&person);

    render_labels_begin_frame(&system);
    RenderLabelInfo info_small;
    ASSERT_TRUE(render_labels_acquire(&system, person, false, 24.0f, &info_small));
    ASSERT_TRUE(info_small.valid);

    RenderLabelInfo info_large;
    ASSERT_TRUE(render_labels_acquire(&system, person, false, 36.0f, &info_large));
    ASSERT_TRUE(info_large.valid);
    ASSERT_TRUE(info_large.width_pixels > info_small.width_pixels);
    ASSERT_TRUE(info_large.region.width >= info_large.width_pixels - 0.5f);
    ASSERT_FLOAT_NEAR(info_small.font_size, 24.0f, 1.0f);
    ASSERT_FLOAT_NEAR(info_large.font_size, 36.0f, 1.0f);
    ASSERT_FALSE(fabsf(info_small.region.x - info_large.region.x) < 0.5f &&
                 fabsf(info_small.region.y - info_large.region.y) < 0.5f);
    render_labels_end_frame(&system);

    render_labels_shutdown(&system);
    person_destroy(person);

    if (created_window)
    {
        CloseWindow();
        SetTraceLogLevel(LOG_INFO);
    }
#else
    ASSERT_TRUE(true);
#endif
}

void register_render_labels_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_render_labels_cache_reuses_texture);
    REGISTER_TEST(registry, test_render_labels_distinct_font_sizes_generate_unique_textures);
}
