#ifndef PERSON_H
#define PERSON_H

#include "timeline.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct PersonMetadataEntry
{
    char *key;
    char *value;
} PersonMetadataEntry;

typedef struct PersonName
{
    char *first;
    char *middle;
    char *last;
} PersonName;

typedef struct PersonDates
{
    char *birth_date;
    char *birth_location;
    char *death_date;
    char *death_location;
} PersonDates;

typedef enum PersonParentSlot
{
    PERSON_PARENT_FATHER = 0,
    PERSON_PARENT_MOTHER = 1
} PersonParentSlot;

struct Person;

typedef struct PersonSpouseRecord
{
    struct Person *partner;
    char *marriage_date;
    char *marriage_location;
} PersonSpouseRecord;

typedef struct Person
{
    uint32_t id;
    PersonName name;
    PersonDates dates;
    bool is_alive;
    struct Person *parents[2];
    struct Person **children;
    size_t children_count;
    size_t children_capacity;
    PersonSpouseRecord *spouses;
    size_t spouses_count;
    size_t spouses_capacity;
    char *profile_image_path;
    char **certificate_paths;
    size_t certificate_count;
    size_t certificate_capacity;
    TimelineEntry *timeline_entries;
    size_t timeline_count;
    size_t timeline_capacity;
    PersonMetadataEntry *metadata;
    size_t metadata_count;
    size_t metadata_capacity;
} Person;

Person *person_create(uint32_t id);
void person_destroy(Person *person);

bool person_set_name(Person *person, const char *first, const char *middle, const char *last);
bool person_set_birth(Person *person, const char *date, const char *location);
bool person_set_death(Person *person, const char *date, const char *location);

bool person_set_parent(Person *child, Person *parent, PersonParentSlot slot);
bool person_add_child(Person *parent, Person *child);
bool person_add_spouse(Person *person, Person *spouse);
bool person_remove_child(Person *parent, Person *child);
bool person_remove_spouse(Person *person, Person *spouse);
bool person_clear_parent(Person *child, PersonParentSlot slot);
bool person_add_certificate(Person *person, const char *path);
bool person_add_timeline_entry(Person *person, const TimelineEntry *entry);
bool person_metadata_set(Person *person, const char *key, const char *value);
bool person_set_marriage(Person *person, Person *spouse, const char *date, const char *location);

bool person_validate(const Person *person, char *error_buffer, size_t error_buffer_size);
bool person_format_display_name(const Person *person, char *buffer, size_t capacity);

#endif /* PERSON_H */
