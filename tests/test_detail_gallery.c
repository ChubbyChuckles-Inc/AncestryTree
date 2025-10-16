#include "detail_gallery.h"
#include "person.h"
#include "test_framework.h"
#include "timeline.h"

#include "at_string.h"

#include <stddef.h>

static Person *make_person_with_media(void)
{
    Person *person = person_create(100U);
    if (!person)
    {
        return NULL;
    }
    person->profile_image_path = at_string_dup("assets/profile.png");
    (void)person_add_certificate(person, "assets/certificate.png");

    TimelineEntry entry;
    timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
    (void)timeline_entry_set_date(&entry, "1990-01-01");
    (void)timeline_entry_add_media(&entry, "assets/media/photo.png");
    (void)timeline_entry_add_media(&entry, "assets/certificate.png"); /* duplicate should be ignored */
    (void)person_add_timeline_entry(person, &entry);
    timeline_entry_reset(&entry);

    return person;
}

TEST(test_detail_gallery_collects_unique_media)
{
    DetailGallery gallery;
    ASSERT_TRUE(detail_gallery_init(&gallery));

    Person *person = make_person_with_media();
    ASSERT_NOT_NULL(person);

    ASSERT_TRUE(detail_gallery_populate_from_person(&gallery, person));
    ASSERT_TRUE(detail_gallery_has_media(&gallery));
    ASSERT_EQ(gallery.count, 3U); /* profile + certificate + timeline media (duplicate ignored) */
    ASSERT_EQ(gallery.selected_index, 0);

    const DetailGalleryEntry *first = detail_gallery_get_entry(&gallery, 0U);
    ASSERT_NOT_NULL(first);
    ASSERT_STREQ(first->path, "assets/profile.png");

    detail_gallery_shutdown(&gallery);
    person_destroy(person);
}

TEST(test_detail_gallery_selection_wraps)
{
    DetailGallery gallery;
    ASSERT_TRUE(detail_gallery_init(&gallery));

    Person *person = make_person_with_media();
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(detail_gallery_populate_from_person(&gallery, person));

    ASSERT_TRUE(detail_gallery_select_next(&gallery));
    ASSERT_EQ(gallery.selected_index, 1);
    ASSERT_TRUE(detail_gallery_select_previous(&gallery));
    ASSERT_EQ(gallery.selected_index, 0);
    ASSERT_TRUE(detail_gallery_select_previous(&gallery));
    ASSERT_EQ(gallery.selected_index, (int)(gallery.count - 1U));

    detail_gallery_shutdown(&gallery);
    person_destroy(person);
}

TEST(test_detail_gallery_zoom_clamps)
{
    DetailGallery gallery;
    ASSERT_TRUE(detail_gallery_init(&gallery));

    detail_gallery_set_zoom(&gallery, 100.0f);
    ASSERT_FLOAT_NEAR(detail_gallery_get_zoom(&gallery), detail_gallery_max_zoom(), 0.0001f);
    detail_gallery_set_zoom(&gallery, 0.01f);
    ASSERT_FLOAT_NEAR(detail_gallery_get_zoom(&gallery), detail_gallery_min_zoom(), 0.0001f);

    detail_gallery_shutdown(&gallery);
}

void register_detail_gallery_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_detail_gallery_collects_unique_media);
    REGISTER_TEST(registry, test_detail_gallery_selection_wraps);
    REGISTER_TEST(registry, test_detail_gallery_zoom_clamps);
}
