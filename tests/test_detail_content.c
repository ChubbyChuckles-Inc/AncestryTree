#include "detail_content_builder.h"
#include "detail_view.h"
#include "person.h"
#include "test_framework.h"
#include "timeline.h"

#include <string.h>

TEST(test_detail_content_builder_populates_certificates_and_timeline)
{
    Person *primary = person_create(101U);
    ASSERT_NOT_NULL(primary);
    ASSERT_TRUE(person_set_name(primary, "Ada", "", "Lovelace"));
    ASSERT_TRUE(person_set_birth(primary, "1815-12-10", "London"));
    ASSERT_TRUE(person_set_death(primary, "1852-11-27", "Marylebone"));

    Person *spouse = person_create(102U);
    ASSERT_NOT_NULL(spouse);
    ASSERT_TRUE(person_set_name(spouse, "William", "King", "Noel"));
    ASSERT_TRUE(person_add_spouse(primary, spouse));
    ASSERT_TRUE(person_set_marriage(primary, spouse, "1835-07-08", "London"));

    TimelineEntry birth;
    timeline_entry_init(&birth, TIMELINE_EVENT_BIRTH);
    ASSERT_TRUE(timeline_entry_set_date(&birth, "1815-12-10"));
    ASSERT_TRUE(timeline_entry_set_description(&birth, "Birth"));
    ASSERT_TRUE(person_add_timeline_entry(primary, &birth));
    timeline_entry_reset(&birth);

    TimelineEntry marriage;
    timeline_entry_init(&marriage, TIMELINE_EVENT_MARRIAGE);
    ASSERT_TRUE(timeline_entry_set_date(&marriage, "1835-07-08"));
    ASSERT_TRUE(timeline_entry_set_description(&marriage, "Marriage to William"));
    ASSERT_TRUE(timeline_entry_add_media(&marriage, "assets/docs/marriage.pdf"));
    ASSERT_TRUE(timeline_entry_add_media(&marriage, "assets/docs/marriage-photo.png"));
    ASSERT_TRUE(person_add_timeline_entry(primary, &marriage));
    timeline_entry_reset(&marriage);

    TimelineEntry death;
    timeline_entry_init(&death, TIMELINE_EVENT_DEATH);
    ASSERT_TRUE(timeline_entry_set_date(&death, "1852-11-27"));
    ASSERT_TRUE(timeline_entry_set_description(&death, "Passing"));
    ASSERT_TRUE(person_add_timeline_entry(primary, &death));
    timeline_entry_reset(&death);

    DetailViewContent content;
    ASSERT_TRUE(detail_view_content_build(primary, &content));
    ASSERT_TRUE(content.fact_count >= 3U);
    ASSERT_TRUE(content.certificate_count >= 3U);
    ASSERT_TRUE(content.timeline_event_count >= 3U);

    size_t birth_certificates = 0U;
    for (size_t index = 0; index < content.certificate_count; ++index)
    {
        if (content.certificates[index].type == DETAIL_CERTIFICATE_BIRTH)
        {
            birth_certificates += 1U;
        }
    }
    ASSERT_TRUE(birth_certificates >= 1U);

    float last_time = 0.0f;
    for (size_t index = 0; index < content.timeline_event_count; ++index)
    {
        const DetailViewTimelineEvent *event = &content.timeline_events[index];
        ASSERT_TRUE(event->normalized_time >= 0.0f);
        ASSERT_TRUE(event->normalized_time <= 1.0f);
        ASSERT_TRUE(event->normalized_time + 0.001f >= last_time);
        last_time = event->normalized_time;

        if (event->type == DETAIL_TIMELINE_MARRIAGE)
        {
            ASSERT_TRUE(event->has_media_asset);
            ASSERT_TRUE(event->multiple_media_assets);
            ASSERT_TRUE(event->media_is_pdf);
            ASSERT_STREQ("marriage.pdf (+1)", event->media_label);
            ASSERT_STREQ("assets/docs/marriage.pdf", event->media_path);
        }
    }

    person_destroy(spouse);
    person_destroy(primary);
}

void register_detail_content_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_detail_content_builder_populates_certificates_and_timeline);
}
