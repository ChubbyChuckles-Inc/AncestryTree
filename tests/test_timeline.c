#include "timeline.h"
#include "test_framework.h"

#include <stdlib.h>

TEST(test_timeline_accepts_year_or_full_date)
{
    TimelineEntry entry;
    timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
    ASSERT_TRUE(timeline_entry_set_date(&entry, "1999"));
    ASSERT_TRUE(timeline_entry_set_description(&entry, "Event"));
    ASSERT_TRUE(timeline_entry_set_location(&entry, "Location"));
    ASSERT_TRUE(timeline_entry_validate(&entry, NULL, 0U));
    ASSERT_TRUE(timeline_entry_set_date(&entry, "2000-01-01"));
    ASSERT_TRUE(timeline_entry_validate(&entry, NULL, 0U));
    timeline_entry_reset(&entry);
}

TEST(test_timeline_rejects_invalid_date)
{
    TimelineEntry entry;
    timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
    ASSERT_FALSE(timeline_entry_set_date(&entry, "20-01-01"));
    ASSERT_TRUE(timeline_entry_set_description(&entry, "Desc"));
    ASSERT_TRUE(timeline_entry_set_location(&entry, "Loc"));
    ASSERT_FALSE(timeline_entry_validate(&entry, NULL, 0U));
    timeline_entry_reset(&entry);
}

TEST(test_timeline_clone_preserves_media)
{
    TimelineEntry source;
    timeline_entry_init(&source, TIMELINE_EVENT_CUSTOM);
    ASSERT_TRUE(timeline_entry_set_date(&source, "2020-05-20"));
    ASSERT_TRUE(timeline_entry_set_description(&source, "Launch"));
    ASSERT_TRUE(timeline_entry_add_media(&source, "media/path.png"));

    TimelineEntry clone;
    timeline_entry_init(&clone, TIMELINE_EVENT_BIRTH);
    ASSERT_TRUE(timeline_entry_clone(&source, &clone));
    ASSERT_TRUE(timeline_entry_validate(&clone, NULL, 0U));
    ASSERT_EQ(clone.media_count, 1U);

    timeline_entry_reset(&source);
    timeline_entry_reset(&clone);
}

void register_timeline_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_timeline_accepts_year_or_full_date);
    REGISTER_TEST(registry, test_timeline_rejects_invalid_date);
    REGISTER_TEST(registry, test_timeline_clone_preserves_media);
}
