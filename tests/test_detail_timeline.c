#include "detail_timeline.h"
#include "person.h"
#include "test_framework.h"
#include "timeline.h"

static void add_timeline_entry(Person *person, TimelineEventType type, const char *date)
{
    TimelineEntry entry;
    timeline_entry_init(&entry, type);
    (void)timeline_entry_set_date(&entry, date);
    (void)person_add_timeline_entry(person, &entry);
    timeline_entry_reset(&entry);
}

TEST(test_detail_timeline_normalizes_years)
{
    DetailTimeline timeline;
    ASSERT_TRUE(detail_timeline_init(&timeline));

    Person *person = person_create(200U);
    ASSERT_NOT_NULL(person);
    add_timeline_entry(person, TIMELINE_EVENT_BIRTH, "1900-01-01");
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "1950-06-10");
    add_timeline_entry(person, TIMELINE_EVENT_DEATH, "2000-12-31");

    ASSERT_TRUE(detail_timeline_build(&timeline, person));
    ASSERT_EQ(timeline.count, 3U);
    ASSERT_TRUE(timeline.has_year_data);
    ASSERT_EQ(timeline.min_year, 1900);
    ASSERT_EQ(timeline.max_year, 2000);

    const DetailTimelineEvent *first = detail_timeline_get_event(&timeline, 0U);
    const DetailTimelineEvent *second = detail_timeline_get_event(&timeline, 1U);
    const DetailTimelineEvent *third = detail_timeline_get_event(&timeline, 2U);
    ASSERT_NOT_NULL(first);
    ASSERT_NOT_NULL(second);
    ASSERT_NOT_NULL(third);
    ASSERT_FLOAT_NEAR(first->position, 0.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(second->position, 0.5f, 0.05f);
    ASSERT_FLOAT_NEAR(third->position, 1.0f, 0.0001f);

    detail_timeline_shutdown(&timeline);
    person_destroy(person);
}

TEST(test_detail_timeline_uniform_spacing_without_years)
{
    DetailTimeline timeline;
    ASSERT_TRUE(detail_timeline_init(&timeline));

    Person *person = person_create(201U);
    ASSERT_NOT_NULL(person);
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "Undated Event A");
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "Undated Event B");
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "Undated Event C");

    ASSERT_TRUE(detail_timeline_build(&timeline, person));
    ASSERT_EQ(timeline.count, 3U);
    ASSERT_FALSE(timeline.has_year_data);

    const DetailTimelineEvent *first = detail_timeline_get_event(&timeline, 0U);
    const DetailTimelineEvent *third = detail_timeline_get_event(&timeline, 2U);
    ASSERT_NOT_NULL(first);
    ASSERT_NOT_NULL(third);
    ASSERT_FLOAT_NEAR(first->position, 0.0f, 0.0001f);
    ASSERT_FLOAT_NEAR(third->position, 1.0f, 0.0001f);

    detail_timeline_shutdown(&timeline);
    person_destroy(person);
}

TEST(test_detail_timeline_hit_test_locates_marker)
{
    DetailTimeline timeline;
    ASSERT_TRUE(detail_timeline_init(&timeline));

    Person *person = person_create(202U);
    ASSERT_NOT_NULL(person);
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "1900-01-01");
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "1950-01-01");
    add_timeline_entry(person, TIMELINE_EVENT_CUSTOM, "2000-01-01");

    ASSERT_TRUE(detail_timeline_build(&timeline, person));

    size_t index = 99U;
    ASSERT_TRUE(detail_timeline_hit_test(&timeline, 0.0f, 100.0f, 50.0f, 6.0f, &index));
    ASSERT_EQ(index, 1U);
    ASSERT_FALSE(detail_timeline_hit_test(&timeline, 0.0f, 100.0f, 5.0f, 2.0f, &index));

    detail_timeline_shutdown(&timeline);
    person_destroy(person);
}

void register_detail_timeline_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_detail_timeline_normalizes_years);
    REGISTER_TEST(registry, test_detail_timeline_uniform_spacing_without_years);
    REGISTER_TEST(registry, test_detail_timeline_hit_test_locates_marker);
}
