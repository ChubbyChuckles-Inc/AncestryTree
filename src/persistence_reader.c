#include "persistence.h"

#include "json_parser.h"
#include "persistence_internal.h"

#include "at_memory.h"
#include "at_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LoadContext
{
    char *error_buffer;
    size_t error_buffer_size;
    FamilyTree *tree;
} LoadContext;

static bool assign_string(char **target, const char *value)
{
    if (!target)
    {
        return false;
    }
    char *copy = NULL;
    if (value && value[0] != '\0')
    {
        copy = at_string_dup(value);
        if (!copy)
        {
            return false;
        }
    }
    AT_FREE(*target);
    *target = copy;
    return true;
}

static bool ctx_set_error(LoadContext *ctx, const char *message)
{
    return persistence_set_error_message(ctx ? ctx->error_buffer : NULL,
                                         ctx ? ctx->error_buffer_size : 0U, message);
}

static bool validate_string_field(const char *value, const char *field, LoadContext *ctx)
{
    if (!value)
    {
        char message[128];
        (void)snprintf(message, sizeof(message), "%s is required", field);
        return ctx_set_error(ctx, message);
    }
    if (!persistence_utf8_validate(value))
    {
        char message[128];
        (void)snprintf(message, sizeof(message), "%s contains invalid UTF-8", field);
        return ctx_set_error(ctx, message);
    }
    return true;
}

static bool load_person_dates(Person *person, const JsonValue *dates, LoadContext *ctx)
{
    const char *birth_date = json_value_get_string(json_value_object_get(dates, "birth_date"));
    if (!validate_string_field(birth_date, "birth_date", ctx))
    {
        return false;
    }
    const char *birth_location =
        json_value_get_string(json_value_object_get(dates, "birth_location"));
    if (birth_location && !persistence_utf8_validate(birth_location))
    {
        return ctx_set_error(ctx, "birth location contains invalid UTF-8");
    }
    if (!person_set_birth(person, birth_date,
                          birth_location && birth_location[0] != '\0' ? birth_location : NULL))
    {
        return ctx_set_error(ctx, "failed to assign birth location");
    }
    const JsonValue *death_date_value = json_value_object_get(dates, "death_date");
    if (death_date_value && json_value_type(death_date_value) == JSON_VALUE_STRING)
    {
        const char *death_date = json_value_get_string(death_date_value);
        if (!persistence_utf8_validate(death_date))
        {
            return ctx_set_error(ctx, "death date contains invalid UTF-8");
        }
        const char *death_location =
            json_value_get_string(json_value_object_get(dates, "death_location"));
        if (death_location && !persistence_utf8_validate(death_location))
        {
            return ctx_set_error(ctx, "death location contains invalid UTF-8");
        }
        if (!person_set_death(person, death_date, death_location))
        {
            return ctx_set_error(ctx, "invalid death information");
        }
    }
    else
    {
        if (!person_set_death(person, NULL, NULL))
        {
            return ctx_set_error(ctx, "failed to clear death information");
        }
    }
    return true;
}

static bool load_person_name(Person *person, const JsonValue *name_object, LoadContext *ctx)
{
    const char *first  = json_value_get_string(json_value_object_get(name_object, "first"));
    const char *last   = json_value_get_string(json_value_object_get(name_object, "last"));
    const char *middle = json_value_get_string(json_value_object_get(name_object, "middle"));
    if (!validate_string_field(first, "first name", ctx) ||
        !validate_string_field(last, "last name", ctx))
    {
        return false;
    }
    if (middle && !persistence_utf8_validate(middle))
    {
        return ctx_set_error(ctx, "middle name contains invalid UTF-8");
    }
    if (!person_set_name(person, first, middle && middle[0] != '\0' ? middle : NULL, last))
    {
        return ctx_set_error(ctx, "failed to assign person name");
    }
    return true;
}

static bool load_person_metadata(Person *person, const JsonValue *metadata, LoadContext *ctx)
{
    if (!metadata)
    {
        return true;
    }
    if (json_value_type(metadata) != JSON_VALUE_OBJECT)
    {
        return ctx_set_error(ctx, "metadata must be object");
    }
    size_t count = json_value_object_size(metadata);
    for (size_t index = 0; index < count; ++index)
    {
        const char *key  = json_value_object_key(metadata, index);
        JsonValue *value = json_value_object_value(metadata, index);
        if (!key || !value || json_value_type(value) != JSON_VALUE_STRING)
        {
            return ctx_set_error(ctx, "metadata entries must map to strings");
        }
        const char *text = json_value_get_string(value);
        if (!persistence_utf8_validate(key) || !persistence_utf8_validate(text))
        {
            return ctx_set_error(ctx, "metadata contains invalid UTF-8");
        }
        if (!person_metadata_set(person, key, text))
        {
            return ctx_set_error(ctx, "failed to assign metadata entry");
        }
    }
    return true;
}

static bool load_person_timeline(Person *person, const JsonValue *timeline_array, LoadContext *ctx)
{
    if (json_value_type(timeline_array) != JSON_VALUE_ARRAY)
    {
        return ctx_set_error(ctx, "timeline must be array");
    }
    size_t count = json_value_array_size(timeline_array);
    for (size_t index = 0; index < count; ++index)
    {
        const JsonValue *entry_value = json_value_array_get(timeline_array, index);
        if (!entry_value || json_value_type(entry_value) != JSON_VALUE_OBJECT)
        {
            return ctx_set_error(ctx, "timeline entry must be object");
        }
        const char *type_string = json_value_get_string(json_value_object_get(entry_value, "type"));
        const char *date        = json_value_get_string(json_value_object_get(entry_value, "date"));
        const char *description =
            json_value_get_string(json_value_object_get(entry_value, "description"));
        const char *location =
            json_value_get_string(json_value_object_get(entry_value, "location"));
        if (!validate_string_field(type_string, "timeline type", ctx) ||
            !validate_string_field(date, "timeline date", ctx) ||
            !validate_string_field(description, "timeline description", ctx))
        {
            return false;
        }
        if ((location && !persistence_utf8_validate(location)) ||
            !persistence_utf8_validate(date) || !persistence_utf8_validate(description))
        {
            return ctx_set_error(ctx, "timeline entry contains invalid UTF-8");
        }
        TimelineEntry entry;
        timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
        if (strcmp(type_string, "birth") == 0)
        {
            entry.type = TIMELINE_EVENT_BIRTH;
        }
        else if (strcmp(type_string, "marriage") == 0)
        {
            entry.type = TIMELINE_EVENT_MARRIAGE;
        }
        else if (strcmp(type_string, "death") == 0)
        {
            entry.type = TIMELINE_EVENT_DEATH;
        }
        if (!timeline_entry_set_date(&entry, date) ||
            !timeline_entry_set_description(&entry, description) ||
            !timeline_entry_set_location(&entry, location))
        {
            timeline_entry_reset(&entry);
            return ctx_set_error(ctx, "invalid timeline entry");
        }
        const JsonValue *media_array = json_value_object_get(entry_value, "media");
        if (media_array && json_value_type(media_array) == JSON_VALUE_ARRAY)
        {
            size_t media_count = json_value_array_size(media_array);
            for (size_t media_index = 0; media_index < media_count; ++media_index)
            {
                const char *media_path =
                    json_value_get_string(json_value_array_get(media_array, media_index));
                if (!media_path || !persistence_utf8_validate(media_path) ||
                    !timeline_entry_add_media(&entry, media_path))
                {
                    timeline_entry_reset(&entry);
                    return ctx_set_error(ctx, "invalid timeline media entry");
                }
            }
        }
        if (!timeline_entry_validate(&entry, NULL, 0U) ||
            !person_add_timeline_entry(person, &entry))
        {
            timeline_entry_reset(&entry);
            return ctx_set_error(ctx, "failed to append timeline entry");
        }
        timeline_entry_reset(&entry);
    }
    return true;
}

static bool populate_person_relationships(const JsonValue *person_object, Person *person,
                                          FamilyTree *tree, LoadContext *ctx)
{
    const JsonValue *children_array = json_value_object_get(person_object, "children");
    if (children_array && json_value_type(children_array) == JSON_VALUE_ARRAY)
    {
        size_t count = json_value_array_size(children_array);
        for (size_t index = 0; index < count; ++index)
        {
            const JsonValue *child_value = json_value_array_get(children_array, index);
            double child_id              = 0.0;
            if (!child_value || !json_value_get_number(child_value, &child_id))
            {
                return ctx_set_error(ctx, "child ID must be numeric");
            }
            Person *child = family_tree_find_person(tree, (uint32_t)child_id);
            if (!child || !person_add_child(person, child))
            {
                return ctx_set_error(ctx, "invalid child reference");
            }
        }
    }

    const JsonValue *parents_array = json_value_object_get(person_object, "parents");
    if (parents_array && json_value_type(parents_array) == JSON_VALUE_ARRAY)
    {
        size_t count = json_value_array_size(parents_array);
        if (count > 2U)
        {
            return ctx_set_error(ctx, "parents array must contain at most two entries");
        }
        for (size_t index = 0; index < count; ++index)
        {
            const JsonValue *parent_value = json_value_array_get(parents_array, index);
            if (!parent_value)
            {
                continue;
            }
            if (json_value_type(parent_value) == JSON_VALUE_NULL)
            {
                person->parents[index] = NULL;
                continue;
            }
            double parent_id = 0.0;
            if (!json_value_get_number(parent_value, &parent_id))
            {
                return ctx_set_error(ctx, "parent ID must be numeric");
            }
            Person *parent = family_tree_find_person(tree, (uint32_t)parent_id);
            if (!parent || !person_set_parent(person, parent, (PersonParentSlot)index))
            {
                return ctx_set_error(ctx, "invalid parent reference");
            }
        }
    }

    const JsonValue *spouses_array = json_value_object_get(person_object, "spouses");
    if (spouses_array && json_value_type(spouses_array) == JSON_VALUE_ARRAY)
    {
        size_t count = json_value_array_size(spouses_array);
        for (size_t index = 0; index < count; ++index)
        {
            const JsonValue *spouse_entry = json_value_array_get(spouses_array, index);
            if (!spouse_entry || json_value_type(spouse_entry) != JSON_VALUE_OBJECT)
            {
                return ctx_set_error(ctx, "spouse entry must be object");
            }
            double spouse_id = 0.0;
            if (!json_value_get_number(json_value_object_get(spouse_entry, "id"), &spouse_id))
            {
                return ctx_set_error(ctx, "spouse ID must be numeric");
            }
            Person *spouse = family_tree_find_person(tree, (uint32_t)spouse_id);
            if (!spouse || !person_add_spouse(person, spouse))
            {
                return ctx_set_error(ctx, "invalid spouse reference");
            }
            const char *marriage_date =
                json_value_get_string(json_value_object_get(spouse_entry, "marriage_date"));
            const char *marriage_location =
                json_value_get_string(json_value_object_get(spouse_entry, "marriage_location"));
            if ((marriage_date && !persistence_utf8_validate(marriage_date)) ||
                (marriage_location && !persistence_utf8_validate(marriage_location)))
            {
                return ctx_set_error(ctx, "spouse metadata contains invalid UTF-8");
            }
            if (!person_set_marriage(person, spouse, marriage_date, marriage_location))
            {
                return ctx_set_error(ctx, "failed to assign marriage metadata");
            }
        }
    }
    return true;
}

static bool populate_person_asset_lists(Person *person, const JsonValue *person_object,
                                        LoadContext *ctx)
{
    const JsonValue *certificates_array = json_value_object_get(person_object, "certificates");
    if (certificates_array && json_value_type(certificates_array) == JSON_VALUE_ARRAY)
    {
        size_t count = json_value_array_size(certificates_array);
        for (size_t index = 0; index < count; ++index)
        {
            const char *certificate_path =
                json_value_get_string(json_value_array_get(certificates_array, index));
            if (!certificate_path || !persistence_utf8_validate(certificate_path) ||
                !person_add_certificate(person, certificate_path))
            {
                return ctx_set_error(ctx, "invalid certificate entry");
            }
        }
    }

    const JsonValue *profile_image_value = json_value_object_get(person_object, "profile_image");
    if (profile_image_value && json_value_type(profile_image_value) == JSON_VALUE_STRING)
    {
        const char *profile_image = json_value_get_string(profile_image_value);
        if (!persistence_utf8_validate(profile_image))
        {
            return ctx_set_error(ctx, "profile image contains invalid UTF-8");
        }
        if (!assign_string(&person->profile_image_path, profile_image))
        {
            return ctx_set_error(ctx, "failed to assign profile image");
        }
    }
    return true;
}

static bool populate_person(const JsonValue *person_object, LoadContext *ctx)
{
    double identifier = 0.0;
    if (!json_value_get_number(json_value_object_get(person_object, "id"), &identifier))
    {
        return ctx_set_error(ctx, "person id must be numeric");
    }
    Person *person = person_create((uint32_t)identifier);
    if (!person)
    {
        return ctx_set_error(ctx, "failed to allocate person");
    }

    const JsonValue *name_object = json_value_object_get(person_object, "name");
    if (!name_object || json_value_type(name_object) != JSON_VALUE_OBJECT ||
        !load_person_name(person, name_object, ctx))
    {
        person_destroy(person);
        return false;
    }

    const JsonValue *dates_object = json_value_object_get(person_object, "dates");
    if (!dates_object || json_value_type(dates_object) != JSON_VALUE_OBJECT ||
        !load_person_dates(person, dates_object, ctx))
    {
        person_destroy(person);
        return false;
    }

    const JsonValue *timeline_array = json_value_object_get(person_object, "timeline");
    if (timeline_array && !load_person_timeline(person, timeline_array, ctx))
    {
        person_destroy(person);
        return false;
    }

    const JsonValue *metadata_object = json_value_object_get(person_object, "metadata");
    if (metadata_object && !load_person_metadata(person, metadata_object, ctx))
    {
        person_destroy(person);
        return false;
    }

    if (!populate_person_asset_lists(person, person_object, ctx))
    {
        person_destroy(person);
        return false;
    }

    const JsonValue *is_alive_value = json_value_object_get(person_object, "is_alive");
    bool is_alive                   = true;
    if (is_alive_value && json_value_get_bool(is_alive_value, &is_alive))
    {
        if (!is_alive &&
            !person_set_death(person, person->dates.death_date, person->dates.death_location))
        {
            person_destroy(person);
            return ctx_set_error(ctx, "invalid death information");
        }
    }
    if (!family_tree_add_person(ctx->tree, person))
    {
        person_destroy(person);
        return ctx_set_error(ctx, "failed to add person to tree");
    }
    return true;
}

static bool load_tree_metadata(const JsonValue *metadata_object, LoadContext *ctx)
{
    if (!metadata_object || json_value_type(metadata_object) != JSON_VALUE_OBJECT)
    {
        return ctx_set_error(ctx, "metadata section is required");
    }
    const char *version = json_value_get_string(json_value_object_get(metadata_object, "version"));
    if (!version || strcmp(version, PERSISTENCE_SCHEMA_VERSION) != 0)
    {
        return ctx_set_error(ctx, "unsupported schema version");
    }
    const char *name = json_value_get_string(json_value_object_get(metadata_object, "name"));
    const char *creation_date =
        json_value_get_string(json_value_object_get(metadata_object, "creation_date"));
    if (name && !assign_string(&ctx->tree->name, name))
    {
        return ctx_set_error(ctx, "failed to duplicate tree name");
    }
    if (creation_date && !family_tree_set_creation_date(ctx->tree, creation_date))
    {
        return ctx_set_error(ctx, "failed to assign creation date");
    }
    return true;
}

FamilyTree *persistence_tree_load(const char *path, char *error_buffer, size_t error_buffer_size)
{
    FILE *stream = NULL;
    if (persistence_portable_fopen(&stream, path, "rb") != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to open", path);
        return NULL;
    }
    fprintf(stderr, "[debug] load: opening %s\n", path);
    if (fseek(stream, 0L, SEEK_END) != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to seek", path);
        fclose(stream);
        return NULL;
    }
    long size = ftell(stream);
    if (size < 0L)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to tell", path);
        fclose(stream);
        return NULL;
    }
    rewind(stream);
    char *contents = malloc((size_t)size + 1U);
    if (!contents)
    {
        persistence_set_error_message(error_buffer, error_buffer_size, "failed to allocate buffer");
        fclose(stream);
        return NULL;
    }
    size_t read = fread(contents, 1U, (size_t)size, stream);
    fclose(stream);
    if (read != (size_t)size)
    {
        free(contents);
        persistence_set_error_message(error_buffer, error_buffer_size, "failed to read file");
        return NULL;
    }
    contents[size] = '\0';

    int error_line   = 0;
    int error_column = 0;
    JsonValue *root =
        json_parse(contents, error_buffer, error_buffer_size, &error_line, &error_column);
    free(contents);
    if (!root)
    {
        return NULL;
    }

    fprintf(stderr, "[debug] load: parsing json\n");
    LoadContext ctx;
    ctx.error_buffer      = error_buffer;
    ctx.error_buffer_size = error_buffer_size;
    ctx.tree              = family_tree_create(NULL);
    if (!ctx.tree)
    {
        json_value_destroy(root);
        persistence_set_error_message(error_buffer, error_buffer_size, "failed to allocate tree");
        return NULL;
    }

    const JsonValue *metadata_object = json_value_object_get(root, "metadata");
    if (!load_tree_metadata(metadata_object, &ctx))
    {
        json_value_destroy(root);
        family_tree_destroy(ctx.tree);
        return NULL;
    }

    const JsonValue *persons_array = json_value_object_get(root, "persons");
    if (!persons_array || json_value_type(persons_array) != JSON_VALUE_ARRAY)
    {
        json_value_destroy(root);
        family_tree_destroy(ctx.tree);
        persistence_set_error_message(error_buffer, error_buffer_size, "persons section missing");
        return NULL;
    }

    size_t person_count = json_value_array_size(persons_array);
    for (size_t index = 0; index < person_count; ++index)
    {
        const JsonValue *person_object = json_value_array_get(persons_array, index);
        if (!person_object || json_value_type(person_object) != JSON_VALUE_OBJECT ||
            !populate_person(person_object, &ctx))
        {
            json_value_destroy(root);
            family_tree_destroy(ctx.tree);
            return NULL;
        }
    }

    for (size_t index = 0; index < person_count; ++index)
    {
        const JsonValue *person_object = json_value_array_get(persons_array, index);
        Person *person                 = ctx.tree->persons[index];
        if (!populate_person_relationships(person_object, person, ctx.tree, &ctx))
        {
            json_value_destroy(root);
            family_tree_destroy(ctx.tree);
            return NULL;
        }
    }

    json_value_destroy(root);

    if (!family_tree_validate(ctx.tree, error_buffer, error_buffer_size))
    {
        family_tree_destroy(ctx.tree);
        return NULL;
    }

    fprintf(stderr, "[debug] load: done\n");
    return ctx.tree;
}
