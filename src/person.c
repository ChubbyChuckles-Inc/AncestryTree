#include "person.h"

#include "at_date.h"
#include "at_memory.h"
#include "at_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERSON_VALIDATION_MAX_DEPTH 1024U

static bool ensure_children_capacity(Person *person)
{
    if (!person)
    {
        return false;
    }
    if (person->children_count < person->children_capacity)
    {
        return true;
    }
    size_t new_capacity = (person->children_capacity == 0U) ? 4U : person->children_capacity * 2U;
    Person **new_children = at_secure_realloc(person->children, new_capacity, sizeof(Person *));
    if (!new_children)
    {
        return false;
    }
    person->children = new_children;
    person->children_capacity = new_capacity;
    return true;
}

static bool ensure_spouse_capacity(Person *person)
{
    if (!person)
    {
        return false;
    }
    if (person->spouses_count < person->spouses_capacity)
    {
        return true;
    }
    size_t new_capacity = (person->spouses_capacity == 0U) ? 2U : person->spouses_capacity * 2U;
    PersonSpouseRecord *records = at_secure_realloc(person->spouses, new_capacity, sizeof(PersonSpouseRecord));
    if (!records)
    {
        return false;
    }
    for (size_t index = person->spouses_capacity; index < new_capacity; ++index)
    {
        records[index].partner = NULL;
        records[index].marriage_date = NULL;
        records[index].marriage_location = NULL;
    }
    person->spouses = records;
    person->spouses_capacity = new_capacity;
    return true;
}

static bool ensure_certificate_capacity(Person *person)
{
    if (!person)
    {
        return false;
    }
    if (person->certificate_count < person->certificate_capacity)
    {
        return true;
    }
    size_t new_capacity = (person->certificate_capacity == 0U) ? 2U : person->certificate_capacity * 2U;
    char **new_paths = at_secure_realloc(person->certificate_paths, new_capacity, sizeof(char *));
    if (!new_paths)
    {
        return false;
    }
    person->certificate_paths = new_paths;
    person->certificate_capacity = new_capacity;
    return true;
}

static bool ensure_timeline_capacity(Person *person)
{
    if (!person)
    {
        return false;
    }
    if (person->timeline_count < person->timeline_capacity)
    {
        return true;
    }
    size_t new_capacity = (person->timeline_capacity == 0U) ? 2U : person->timeline_capacity * 2U;
    TimelineEntry *entries = at_secure_realloc(person->timeline_entries, new_capacity, sizeof(TimelineEntry));
    if (!entries)
    {
        return false;
    }
    person->timeline_entries = entries;
    person->timeline_capacity = new_capacity;
    return true;
}

static bool ensure_metadata_capacity(Person *person)
{
    if (!person)
    {
        return false;
    }
    if (person->metadata_count < person->metadata_capacity)
    {
        return true;
    }
    size_t new_capacity = (person->metadata_capacity == 0U) ? 2U : person->metadata_capacity * 2U;
    PersonMetadataEntry *entries =
        at_secure_realloc(person->metadata, new_capacity, sizeof(PersonMetadataEntry));
    if (!entries)
    {
        return false;
    }
    person->metadata = entries;
    person->metadata_capacity = new_capacity;
    return true;
}

static void person_clear_name(Person *person)
{
    if (!person)
    {
        return;
    }
    free(person->name.first);
    free(person->name.middle);
    free(person->name.last);
    person->name.first = NULL;
    person->name.middle = NULL;
    person->name.last = NULL;
}

static void person_clear_dates(Person *person)
{
    if (!person)
    {
        return;
    }
    free(person->dates.birth_date);
    free(person->dates.birth_location);
    free(person->dates.death_date);
    free(person->dates.death_location);
    person->dates.birth_date = NULL;
    person->dates.birth_location = NULL;
    person->dates.death_date = NULL;
    person->dates.death_location = NULL;
}

static void person_clear_certificates(Person *person)
{
    if (!person)
    {
        return;
    }
    for (size_t index = 0; index < person->certificate_count; ++index)
    {
        free(person->certificate_paths[index]);
    }
    free(person->certificate_paths);
    person->certificate_paths = NULL;
    person->certificate_count = 0U;
    person->certificate_capacity = 0U;
}

static void person_clear_timeline(Person *person)
{
    if (!person)
    {
        return;
    }
    for (size_t index = 0; index < person->timeline_count; ++index)
    {
        timeline_entry_reset(&person->timeline_entries[index]);
    }
    free(person->timeline_entries);
    person->timeline_entries = NULL;
    person->timeline_count = 0U;
    person->timeline_capacity = 0U;
}

static void person_clear_metadata(Person *person)
{
    if (!person)
    {
        return;
    }
    for (size_t index = 0; index < person->metadata_count; ++index)
    {
        free(person->metadata[index].key);
        free(person->metadata[index].value);
    }
    free(person->metadata);
    person->metadata = NULL;
    person->metadata_count = 0U;
    person->metadata_capacity = 0U;
}

static void person_clear_spouses(Person *person)
{
    if (!person)
    {
        return;
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        free(person->spouses[index].marriage_date);
        free(person->spouses[index].marriage_location);
    }
    free(person->spouses);
    person->spouses = NULL;
    person->spouses_count = 0U;
    person->spouses_capacity = 0U;
}

Person *person_create(uint32_t id)
{
    Person *person = calloc(1U, sizeof(Person));
    if (!person)
    {
        return NULL;
    }
    person->id = id;
    person->is_alive = true;
    person->parents[0] = NULL;
    person->parents[1] = NULL;
    /* Manual validation: instantiate via unit tests and verify parent/child symmetry. */
    return person;
}

void person_destroy(Person *person)
{
    if (!person)
    {
        return;
    }
    person_clear_name(person);
    person_clear_dates(person);
    free(person->profile_image_path);
    free(person->children);
    person_clear_spouses(person);
    person_clear_certificates(person);
    person_clear_timeline(person);
    person_clear_metadata(person);
    free(person);
}

static bool person_assign_string(char **target, const char *value)
{
    if (!target)
    {
        return false;
    }
    if (!value)
    {
        free(*target);
        *target = NULL;
        return true;
    }
    char *copy = at_string_dup(value);
    if (!copy)
    {
        return false;
    }
    free(*target);
    *target = copy;
    return true;
}

bool person_set_name(Person *person, const char *first, const char *middle, const char *last)
{
    if (!person)
    {
        return false;
    }
    if (!first || !last || first[0] == '\0' || last[0] == '\0')
    {
        return false;
    }
    char *first_copy = at_string_dup(first);
    char *middle_copy = NULL;
    char *last_copy = at_string_dup(last);
    if (middle && middle[0] != '\0')
    {
        middle_copy = at_string_dup(middle);
        if (!middle_copy)
        {
            free(first_copy);
            free(last_copy);
            return false;
        }
    }
    if (!first_copy || !last_copy)
    {
        free(first_copy);
        free(middle_copy);
        free(last_copy);
        return false;
    }
    person_clear_name(person);
    person->name.first = first_copy;
    person->name.middle = middle_copy;
    person->name.last = last_copy;
    return true;
}

bool person_set_birth(Person *person, const char *date, const char *location)
{
    if (!person || !date || date[0] == '\0')
    {
        return false;
    }
    if (!at_date_is_valid_iso8601(date))
    {
        return false;
    }
    char *date_copy = at_string_dup(date);
    if (!date_copy)
    {
        return false;
    }
    char *location_copy = NULL;
    if (location && location[0] != '\0')
    {
        location_copy = at_string_dup(location);
        if (!location_copy)
        {
            free(date_copy);
            return false;
        }
    }
    free(person->dates.birth_date);
    free(person->dates.birth_location);
    person->dates.birth_date = date_copy;
    person->dates.birth_location = location_copy;
    return true;
}

bool person_set_death(Person *person, const char *date, const char *location)
{
    if (!person)
    {
        return false;
    }
    if (!date || date[0] == '\0')
    {
        person->is_alive = true;
        free(person->dates.death_date);
        free(person->dates.death_location);
        person->dates.death_date = NULL;
        person->dates.death_location = NULL;
        return true;
    }
    if (!at_date_is_valid_iso8601(date))
    {
        return false;
    }
    char *date_copy = at_string_dup(date);
    if (!date_copy)
    {
        return false;
    }
    char *location_copy = NULL;
    if (location && location[0] != '\0')
    {
        location_copy = at_string_dup(location);
        if (!location_copy)
        {
            free(date_copy);
            return false;
        }
    }
    free(person->dates.death_date);
    free(person->dates.death_location);
    person->dates.death_date = date_copy;
    person->dates.death_location = location_copy;
    person->is_alive = false;
    return true;
}

bool person_set_parent(Person *child, Person *parent, PersonParentSlot slot)
{
    if (!child || !parent || slot > PERSON_PARENT_MOTHER)
    {
        return false;
    }
    child->parents[slot] = parent;
    return true;
}

static bool person_add_child_reference(Person *parent, Person *child)
{
    if (!ensure_children_capacity(parent))
    {
        return false;
    }
    parent->children[parent->children_count++] = child;
    return true;
}

static bool person_assign_parent_slot(Person *child, Person *parent)
{
    for (size_t index = 0; index < 2U; ++index)
    {
        if (child->parents[index] == parent)
        {
            return true;
        }
        if (child->parents[index] == NULL)
        {
            child->parents[index] = parent;
            return true;
        }
    }
    return false;
}

bool person_add_child(Person *parent, Person *child)
{
    if (!parent || !child || parent == child)
    {
        return false;
    }
    for (size_t index = 0; index < parent->children_count; ++index)
    {
        if (parent->children[index] == child)
        {
            return true;
        }
    }
    if (!person_add_child_reference(parent, child))
    {
        return false;
    }
    if (!person_assign_parent_slot(child, parent))
    {
        parent->children_count--;
        return false;
    }
    return true;
}

static bool person_find_spouse_index(const Person *person, const Person *spouse, size_t *out_index)
{
    if (!person || !spouse)
    {
        return false;
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        if (person->spouses[index].partner == spouse)
        {
            if (out_index)
            {
                *out_index = index;
            }
            return true;
        }
    }
    return false;
}

static bool person_relationship_cycle_detect(const Person *root, const Person *current, const Person **path,
                                             size_t depth)
{
    if (!current)
    {
        return false;
    }
    if (depth >= PERSON_VALIDATION_MAX_DEPTH)
    {
        return true;
    }
    for (size_t index = 0; index < current->children_count; ++index)
    {
        const Person *child = current->children[index];
        if (!child)
        {
            continue;
        }
        if (child == root)
        {
            return true;
        }
        for (size_t path_index = 0; path_index < depth; ++path_index)
        {
            if (path[path_index] == child)
            {
                return true;
            }
        }
        path[depth] = child;
        if (person_relationship_cycle_detect(root, child, path, depth + 1U))
        {
            return true;
        }
    }
    return false;
}

static bool person_add_spouse_internal(Person *person, Person *spouse, bool reciprocal)
{
    if (!person || !spouse || person == spouse)
    {
        return false;
    }
    if (person_find_spouse_index(person, spouse, NULL))
    {
        return true;
    }
    if (!ensure_spouse_capacity(person))
    {
        return false;
    }
    PersonSpouseRecord *record = &person->spouses[person->spouses_count];
    record->partner = spouse;
    record->marriage_date = NULL;
    record->marriage_location = NULL;
    person->spouses_count++;
    if (reciprocal)
    {
        if (!person_add_spouse_internal(spouse, person, false))
        {
            person->spouses_count--;
            record->partner = NULL;
            return false;
        }
    }
    return true;
}

bool person_add_spouse(Person *person, Person *spouse)
{
    return person_add_spouse_internal(person, spouse, true);
}

static bool person_set_marriage_internal(Person *person, Person *spouse, const char *date, const char *location,
                                         bool reciprocal)
{
    if (!person || !spouse)
    {
        return false;
    }

    if (date && date[0] != '\0' && !at_date_is_valid_iso8601(date))
    {
        return false;
    }

    size_t index = 0U;
    if (!person_find_spouse_index(person, spouse, &index))
    {
        return false;
    }

    char *date_copy = NULL;
    char *location_copy = NULL;

    if (date && date[0] != '\0')
    {
        date_copy = at_string_dup(date);
        if (!date_copy)
        {
            return false;
        }
    }
    if (location && location[0] != '\0')
    {
        location_copy = at_string_dup(location);
        if (!location_copy)
        {
            free(date_copy);
            return false;
        }
    }

    if (reciprocal)
    {
        if (!person_set_marriage_internal(spouse, person, date, location, false))
        {
            free(date_copy);
            free(location_copy);
            return false;
        }
    }

    free(person->spouses[index].marriage_date);
    free(person->spouses[index].marriage_location);
    person->spouses[index].marriage_date = date_copy;
    person->spouses[index].marriage_location = location_copy;
    return true;
}

bool person_set_marriage(Person *person, Person *spouse, const char *date, const char *location)
{
    return person_set_marriage_internal(person, spouse, date, location, true);
}

bool person_add_certificate(Person *person, const char *path)
{
    if (!person || !path || path[0] == '\0')
    {
        return false;
    }
    for (size_t index = 0; index < person->certificate_count; ++index)
    {
        if (strcmp(person->certificate_paths[index], path) == 0)
        {
            return true;
        }
    }
    if (!ensure_certificate_capacity(person))
    {
        return false;
    }
    char *copy = at_string_dup(path);
    if (!copy)
    {
        return false;
    }
    person->certificate_paths[person->certificate_count++] = copy;
    return true;
}

bool person_add_timeline_entry(Person *person, const TimelineEntry *entry)
{
    if (!person || !entry)
    {
        return false;
    }
    if (!ensure_timeline_capacity(person))
    {
        return false;
    }
    TimelineEntry *destination = &person->timeline_entries[person->timeline_count];
    if (!timeline_entry_clone(entry, destination))
    {
        return false;
    }
    person->timeline_count++;
    return true;
}

bool person_metadata_set(Person *person, const char *key, const char *value)
{
    if (!person || !key || key[0] == '\0')
    {
        return false;
    }
    for (size_t index = 0; index < person->metadata_count; ++index)
    {
        if (strcmp(person->metadata[index].key, key) == 0)
        {
            return person_assign_string(&person->metadata[index].value, value);
        }
    }
    if (!ensure_metadata_capacity(person))
    {
        return false;
    }
    char *key_copy = at_string_dup(key);
    char *value_copy = value ? at_string_dup(value) : NULL;
    if (!key_copy || (value && !value_copy))
    {
        free(key_copy);
        free(value_copy);
        return false;
    }
    person->metadata[person->metadata_count].key = key_copy;
    person->metadata[person->metadata_count].value = value_copy;
    person->metadata_count++;
    return true;
}

static bool string_is_null_or_empty(const char *value)
{
    return value == NULL || value[0] == '\0';
}

bool person_validate(const Person *person, char *error_buffer, size_t error_buffer_size)
{
    if (!person)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person pointer is NULL");
        }
        return false;
    }
    if (person->id == 0U)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %p has invalid id", (const void *)person);
        }
        return false;
    }
    if (string_is_null_or_empty(person->name.first) || string_is_null_or_empty(person->name.last))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u lacks required name components", person->id);
        }
        return false;
    }
    if (string_is_null_or_empty(person->dates.birth_date))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u missing birth date", person->id);
        }
        return false;
    }
    if (!person->is_alive && string_is_null_or_empty(person->dates.death_date))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u marked deceased without death date", person->id);
        }
        return false;
    }
    if (person->is_alive && !string_is_null_or_empty(person->dates.death_date))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u has death date but is alive", person->id);
        }
        return false;
    }
    for (size_t index = 0; index < person->certificate_count; ++index)
    {
        if (string_is_null_or_empty(person->certificate_paths[index]))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u has empty certificate path", person->id);
            }
            return false;
        }
    }
    const Person *path[PERSON_VALIDATION_MAX_DEPTH];
    if (person_relationship_cycle_detect(person, person, path, 0U))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u participates in a relationship cycle",
                           person->id);
        }
        return false;
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        const PersonSpouseRecord *record = &person->spouses[index];
        if (!record->partner)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u spouse entry lacks partner", person->id);
            }
            return false;
        }
        if (record->partner == person)
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u cannot be their own spouse", person->id);
            }
            return false;
        }
        if (record->marriage_date && record->marriage_date[0] != '\0' &&
            !at_date_is_valid_iso8601(record->marriage_date))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Person %u has invalid marriage date", person->id);
            }
            return false;
        }
    }
    for (size_t index = 0; index < person->timeline_count; ++index)
    {
        if (!timeline_entry_validate(&person->timeline_entries[index], error_buffer, error_buffer_size))
        {
            return false;
        }
    }
    return true;
}

bool person_format_display_name(const Person *person, char *buffer, size_t capacity)
{
    if (!person || !buffer || capacity == 0U)
    {
        return false;
    }
    buffer[0] = '\0';

    const char *segments[3] = {person->name.first, person->name.middle, person->name.last};
    size_t written = 0U;
    for (size_t index = 0U; index < 3U; ++index)
    {
        const char *segment = segments[index];
        if (!segment || segment[0] == '\0')
        {
            continue;
        }
        int required = 0;
        if (written == 0U)
        {
            required = snprintf(buffer, capacity, "%s", segment);
        }
        else
        {
            required = snprintf(buffer + written, capacity - written, " %s", segment);
        }
        if (required < 0)
        {
            buffer[0] = '\0';
            return false;
        }
        written += (size_t)required;
        if (written >= capacity)
        {
            buffer[capacity - 1U] = '\0';
            return false;
        }
    }

    if (written == 0U)
    {
        int required = snprintf(buffer, capacity, "Person %u", person->id);
        if (required < 0)
        {
            buffer[0] = '\0';
            return false;
        }
        if ((size_t)required >= capacity)
        {
            buffer[capacity - 1U] = '\0';
            return false;
        }
    }
    return true;
}
