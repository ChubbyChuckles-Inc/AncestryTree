#include "person.h"
#include "test_framework.h"

#include <stdlib.h>
#include <string.h>

static TimelineEntry make_timeline_entry(const char *date, const char *description)
{
    TimelineEntry entry;
    timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
    (void)timeline_entry_set_date(&entry, date);
    (void)timeline_entry_set_description(&entry, description);
    (void)timeline_entry_set_location(&entry, "Test City");
    (void)timeline_entry_add_media(&entry, "media/test.png");
    return entry;
}

static void dispose_timeline_entry(TimelineEntry *entry)
{
    timeline_entry_reset(entry);
}

TEST(test_person_create_and_destroy)
{
    Person *person = person_create(1U);
    ASSERT_NOT_NULL(person);
    ASSERT_TRUE(person_set_name(person, "Ada", "", "Lovelace"));
    ASSERT_TRUE(person_set_birth(person, "1815-12-10", "London"));
    ASSERT_TRUE(person_validate(person, NULL, 0U));
    person_destroy(person);
}

TEST(test_person_add_child_links_parents)
{
    Person *parent = person_create(2U);
    Person *child = person_create(3U);
    ASSERT_TRUE(person_set_name(parent, "Grace", NULL, "Hopper"));
    ASSERT_TRUE(person_set_birth(parent, "1906-12-09", "New York"));
    ASSERT_TRUE(person_set_name(child, "Howard", NULL, "Aiken"));
    ASSERT_TRUE(person_set_birth(child, "1900-03-08", "Hoboken"));

    ASSERT_TRUE(person_add_child(parent, child));
    ASSERT_EQ(parent->children_count, 1U);
    ASSERT_EQ(child->parents[0], parent);

    person_destroy(parent);
    person_destroy(child);
}

TEST(test_person_add_spouse_is_reciprocal)
{
    Person *a = person_create(4U);
    Person *b = person_create(5U);
    ASSERT_TRUE(person_set_name(a, "Linus", NULL, "Torvalds"));
    ASSERT_TRUE(person_set_birth(a, "1969-12-28", "Helsinki"));
    ASSERT_TRUE(person_set_name(b, "Tove", NULL, "Torvalds"));
    ASSERT_TRUE(person_set_birth(b, "1970-04-10", "Helsinki"));

    ASSERT_TRUE(person_add_spouse(a, b));
    ASSERT_EQ(a->spouses_count, 1U);
    ASSERT_EQ(b->spouses_count, 1U);
    ASSERT_EQ(a->spouses[0], b);
    ASSERT_EQ(b->spouses[0], a);

    person_destroy(a);
    person_destroy(b);
}

TEST(test_person_timeline_and_metadata)
{
    Person *person = person_create(6U);
    ASSERT_TRUE(person_set_name(person, "Katherine", NULL, "Johnson"));
    ASSERT_TRUE(person_set_birth(person, "1918-08-26", "White Sulphur Springs"));

    TimelineEntry entry = make_timeline_entry("1962-02-20", "Computed orbital mechanics");
    ASSERT_TRUE(person_add_timeline_entry(person, &entry));
    dispose_timeline_entry(&entry);
    ASSERT_EQ(person->timeline_count, 1U);

    ASSERT_TRUE(person_metadata_set(person, "occupation", "Mathematician"));
    ASSERT_TRUE(person_metadata_set(person, "occupation", "NASA mathematician"));
    ASSERT_EQ(person->metadata_count, 1U);
    ASSERT_STREQ(person->metadata[0].value, "NASA mathematician");

    person_destroy(person);
}

TEST(test_person_validation_rules)
{
    Person *person = person_create(7U);
    char buffer[128];

    ASSERT_FALSE(person_validate(person, buffer, sizeof(buffer)));
    ASSERT_TRUE(person_set_name(person, "Alan", NULL, "Turing"));
    ASSERT_FALSE(person_validate(person, buffer, sizeof(buffer)));
    ASSERT_TRUE(person_set_birth(person, "1912-06-23", "London"));
    ASSERT_TRUE(person_validate(person, buffer, sizeof(buffer)));

    ASSERT_TRUE(person_set_death(person, "1954-06-07", "Wilmslow"));
    ASSERT_TRUE(person_validate(person, buffer, sizeof(buffer)));

    ASSERT_TRUE(person_set_death(person, NULL, NULL));
    ASSERT_TRUE(person_validate(person, buffer, sizeof(buffer)));

    person_destroy(person);
}

void register_person_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_person_create_and_destroy);
    REGISTER_TEST(registry, test_person_add_child_links_parents);
    REGISTER_TEST(registry, test_person_add_spouse_is_reciprocal);
    REGISTER_TEST(registry, test_person_timeline_and_metadata);
    REGISTER_TEST(registry, test_person_validation_rules);
}
```
}