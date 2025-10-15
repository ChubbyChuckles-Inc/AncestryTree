#include "persistence.h"

#include "persistence_internal.h"

#include "person.h"
#include "timeline.h"
#include "tree.h"

#include <stdlib.h>
#include <string.h>

typedef struct WriteContext
{
    FILE *stream;
    char *error_buffer;
    size_t error_buffer_size;
} WriteContext;

static bool ctx_set_error(WriteContext *ctx, const char *message)
{
    return persistence_set_error_message(ctx ? ctx->error_buffer : NULL,
                                         ctx ? ctx->error_buffer_size : 0U,
                                         message);
}

static bool write_indent(WriteContext *ctx, size_t indent)
{
    if (!ctx || !ctx->stream)
    {
        return false;
    }
    for (size_t index = 0; index < indent; ++index)
    {
        if (fputc(' ', ctx->stream) == EOF)
        {
            return ctx_set_error(ctx, "failed to write indent");
        }
    }
    return true;
}

static bool write_raw(WriteContext *ctx, const char *text)
{
    if (!ctx || !ctx->stream)
    {
        return false;
    }
    if (fputs(text, ctx->stream) == EOF)
    {
        return ctx_set_error(ctx, "failed to write output");
    }
    return true;
}

static bool write_escaped_string(WriteContext *ctx, const char *value)
{
    if (!write_raw(ctx, "\""))
    {
        return false;
    }
    if (value)
    {
        if (!persistence_utf8_validate(value))
        {
            return ctx_set_error(ctx, "invalid UTF-8 string");
        }
        for (const unsigned char *cursor = (const unsigned char *)value; *cursor != '\0'; ++cursor)
        {
            unsigned char character = *cursor;
            switch (character)
            {
            case '"':
                if (!write_raw(ctx, "\\\""))
                {
                    return false;
                }
                break;
            case '\\':
                if (!write_raw(ctx, "\\\\"))
                {
                    return false;
                }
                break;
            case '\b':
                if (!write_raw(ctx, "\\b"))
                {
                    return false;
                }
                break;
            case '\f':
                if (!write_raw(ctx, "\\f"))
                {
                    return false;
                }
                break;
            case '\n':
                if (!write_raw(ctx, "\\n"))
                {
                    return false;
                }
                break;
            case '\r':
                if (!write_raw(ctx, "\\r"))
                {
                    return false;
                }
                break;
            case '\t':
                if (!write_raw(ctx, "\\t"))
                {
                    return false;
                }
                break;
            default:
                if (character < 0x20U)
                {
                    char buffer[7];
                    (void)snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned int)character);
                    if (!write_raw(ctx, buffer))
                    {
                        return false;
                    }
                }
                else if (fputc(character, ctx->stream) == EOF)
                {
                    return ctx_set_error(ctx, "failed to write string");
                }
                break;
            }
        }
    }
    if (fputc('"', ctx->stream) == EOF)
    {
        return ctx_set_error(ctx, "failed to close string");
    }
    return true;
}

static const char *timeline_event_type_to_string(TimelineEventType type)
{
    switch (type)
    {
    case TIMELINE_EVENT_BIRTH:
        return "birth";
    case TIMELINE_EVENT_MARRIAGE:
        return "marriage";
    case TIMELINE_EVENT_DEATH:
        return "death";
    case TIMELINE_EVENT_CUSTOM:
    default:
        return "custom";
    }
}

static bool write_metadata_entries(WriteContext *ctx, const Person *person, size_t indent)
{
    if (!write_raw(ctx, "{\n"))
    {
        return false;
    }
    for (size_t index = 0; index < person->metadata_count; ++index)
    {
        const PersonMetadataEntry *entry = &person->metadata[index];
        if (!persistence_utf8_validate(entry->key) || !persistence_utf8_validate(entry->value))
        {
            return ctx_set_error(ctx, "invalid UTF-8 metadata entry");
        }
        if (!write_indent(ctx, indent + 2U) || !write_escaped_string(ctx, entry->key) || !write_raw(ctx, ": ") ||
            !write_escaped_string(ctx, entry->value ? entry->value : ""))
        {
            return false;
        }
        if (index + 1U < person->metadata_count)
        {
            if (!write_raw(ctx, ","))
            {
                return false;
            }
        }
        if (!write_raw(ctx, "\n"))
        {
            return false;
        }
    }
    if (!write_indent(ctx, indent) || !write_raw(ctx, "}"))
    {
        return false;
    }
    return true;
}

static bool write_string_array(WriteContext *ctx, char **values, size_t count)
{
    if (!write_raw(ctx, "["))
    {
        return false;
    }
    for (size_t index = 0; index < count; ++index)
    {
        if (!write_escaped_string(ctx, values[index]))
        {
            return false;
        }
        if (index + 1U < count)
        {
            if (!write_raw(ctx, ", "))
            {
                return false;
            }
        }
    }
    return write_raw(ctx, "]");
}

static bool write_timeline_entries(WriteContext *ctx, const Person *person, size_t indent)
{
    if (!write_raw(ctx, "[\n"))
    {
        return false;
    }
    for (size_t index = 0; index < person->timeline_count; ++index)
    {
        const TimelineEntry *entry = &person->timeline_entries[index];
        if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "{\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"type\": ") ||
            !write_escaped_string(ctx, timeline_event_type_to_string(entry->type)) ||
            !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"date\": ") ||
            !write_escaped_string(ctx, entry->date ? entry->date : "") || !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"description\": ") ||
            !write_escaped_string(ctx, entry->description ? entry->description : "") ||
            !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"location\": ") ||
            !write_escaped_string(ctx, entry->location ? entry->location : "") || !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"media\": "))
        {
            return false;
        }
        if (!write_string_array(ctx, entry->media_paths, entry->media_count) || !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"valid\": ") ||
            !write_raw(ctx, (timeline_entry_validate(entry, NULL, 0U) ? "true" : "false")) ||
            !write_raw(ctx, "\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "}"))
        {
            return false;
        }
        if (index + 1U < person->timeline_count)
        {
            if (!write_raw(ctx, ","))
            {
                return false;
            }
        }
        if (!write_raw(ctx, "\n"))
        {
            return false;
        }
    }
    if (!write_indent(ctx, indent) || !write_raw(ctx, "]"))
    {
        return false;
    }
    return true;
}

static bool write_spouses(WriteContext *ctx, const Person *person, size_t indent)
{
    if (!write_raw(ctx, "[\n"))
    {
        return false;
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        const PersonSpouseRecord *record = &person->spouses[index];
        if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "{\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"id\": "))
        {
            return false;
        }
        if (record->partner)
        {
            char id_buffer[32];
            (void)snprintf(id_buffer, sizeof(id_buffer), "%u", record->partner->id);
            if (!write_raw(ctx, id_buffer))
            {
                return false;
            }
        }
        else if (!write_raw(ctx, "null"))
        {
            return false;
        }
        if (!write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"marriage_date\": ") ||
            !write_escaped_string(ctx, record->marriage_date ? record->marriage_date : "") ||
            !write_raw(ctx, ",\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"marriage_location\": ") ||
            !write_escaped_string(ctx, record->marriage_location ? record->marriage_location : "") ||
            !write_raw(ctx, "\n"))
        {
            return false;
        }
        if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "}"))
        {
            return false;
        }
        if (index + 1U < person->spouses_count)
        {
            if (!write_raw(ctx, ","))
            {
                return false;
            }
        }
        if (!write_raw(ctx, "\n"))
        {
            return false;
        }
    }
    if (!write_indent(ctx, indent) || !write_raw(ctx, "]"))
    {
        return false;
    }
    return true;
}

static bool write_person(WriteContext *ctx, const Person *person, size_t indent)
{
    if (!write_indent(ctx, indent) || !write_raw(ctx, "{\n"))
    {
        return false;
    }
    char buffer[64];
    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"id\": "))
    {
        return false;
    }
    (void)snprintf(buffer, sizeof(buffer), "%u", person->id);
    if (!write_raw(ctx, buffer) || !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"name\": {\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"first\": ") ||
        !write_escaped_string(ctx, person->name.first ? person->name.first : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"middle\": ") ||
        !write_escaped_string(ctx, person->name.middle ? person->name.middle : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"last\": ") ||
        !write_escaped_string(ctx, person->name.last ? person->name.last : "") ||
        !write_raw(ctx, "\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "},\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"dates\": {\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"birth_date\": ") ||
        !write_escaped_string(ctx, person->dates.birth_date ? person->dates.birth_date : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"birth_location\": ") ||
        !write_escaped_string(ctx, person->dates.birth_location ? person->dates.birth_location : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"death_date\": ") ||
        !write_escaped_string(ctx, person->dates.death_date ? person->dates.death_date : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"death_location\": ") ||
        !write_escaped_string(ctx, person->dates.death_location ? person->dates.death_location : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 4U) || !write_raw(ctx, "\"is_alive\": ") ||
        !write_raw(ctx, person->is_alive ? "true" : "false") || !write_raw(ctx, "\n"))
    {
        return false;
    }
    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "},\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"parents\": ["))
    {
        return false;
    }
    for (size_t index = 0; index < 2U; ++index)
    {
        const Person *parent = person->parents[index];
        if (parent)
        {
            (void)snprintf(buffer, sizeof(buffer), "%u", parent->id);
        }
        else
        {
            (void)snprintf(buffer, sizeof(buffer), "null");
        }
        if (!write_raw(ctx, buffer))
        {
            return false;
        }
        if (index + 1U < 2U)
        {
            if (!write_raw(ctx, ", "))
            {
                return false;
            }
        }
    }
    if (!write_raw(ctx, "],\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"children\": ["))
    {
        return false;
    }
    for (size_t index = 0; index < person->children_count; ++index)
    {
        (void)snprintf(buffer, sizeof(buffer), "%u", person->children[index]->id);
        if (!write_raw(ctx, buffer))
        {
            return false;
        }
        if (index + 1U < person->children_count)
        {
            if (!write_raw(ctx, ", "))
            {
                return false;
            }
        }
    }
    if (!write_raw(ctx, "],\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"spouses\": "))
    {
        return false;
    }
    if (!write_spouses(ctx, person, indent + 2U) || !write_raw(ctx, ",\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"certificates\": "))
    {
        return false;
    }
    if (!write_string_array(ctx, person->certificate_paths, person->certificate_count) ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"profile_image\": ") ||
        !write_escaped_string(ctx, person->profile_image_path ? person->profile_image_path : "") ||
        !write_raw(ctx, ",\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"timeline\": "))
    {
        return false;
    }
    if (!write_timeline_entries(ctx, person, indent + 2U) || !write_raw(ctx, ",\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent + 2U) || !write_raw(ctx, "\"metadata\": "))
    {
        return false;
    }
    if (!write_metadata_entries(ctx, person, indent + 2U) || !write_raw(ctx, "\n"))
    {
        return false;
    }

    if (!write_indent(ctx, indent) || !write_raw(ctx, "}"))
    {
        return false;
    }
    return true;
}

static bool write_persons_array(WriteContext *ctx, const FamilyTree *tree, size_t indent)
{
    if (!write_raw(ctx, "[\n"))
    {
        return false;
    }
    for (size_t index = 0; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!write_person(ctx, person, indent + 2U))
        {
            return false;
        }
        if (index + 1U < tree->person_count)
        {
            if (!write_raw(ctx, ","))
            {
                return false;
            }
        }
        if (!write_raw(ctx, "\n"))
        {
            return false;
        }
    }
    if (!write_indent(ctx, indent) || !write_raw(ctx, "]"))
    {
        return false;
    }
    return true;
}

static bool write_tree_roots(WriteContext *ctx, const FamilyTree *tree)
{
    if (!write_raw(ctx, "["))
    {
        return false;
    }
    size_t root_count = family_tree_get_roots(tree, NULL, 0U);
    Person **roots = NULL;
    if (root_count > 0U)
    {
        roots = calloc(root_count, sizeof(Person *));
        if (!roots)
        {
            return ctx_set_error(ctx, "failed to allocate roots buffer");
        }
        (void)family_tree_get_roots(tree, roots, root_count);
    }
    for (size_t index = 0; index < root_count; ++index)
    {
        char buffer[32];
        (void)snprintf(buffer, sizeof(buffer), "%u", roots[index]->id);
        if (!write_raw(ctx, buffer))
        {
            free(roots);
            return false;
        }
        if (index + 1U < root_count)
        {
            if (!write_raw(ctx, ", "))
            {
                free(roots);
                return false;
            }
        }
    }
    free(roots);
    return write_raw(ctx, "]");
}

bool persistence_tree_save(const FamilyTree *tree, const char *path, char *error_buffer, size_t error_buffer_size)
{
    WriteContext ctx;
    ctx.stream = NULL;
    ctx.error_buffer = error_buffer;
    ctx.error_buffer_size = error_buffer_size;

    if (!tree)
    {
        return ctx_set_error(&ctx, "tree pointer is NULL");
    }
    if (!path)
    {
        return ctx_set_error(&ctx, "path pointer is NULL");
    }

    char validation_error[256];
    if (!family_tree_validate(tree, validation_error, sizeof(validation_error)))
    {
        return ctx_set_error(&ctx, validation_error);
    }

    if (!persistence_create_backup_if_needed(path, error_buffer, error_buffer_size))
    {
        return false;
    }

    if (persistence_portable_fopen(&ctx.stream, path, "wb") != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to open", path);
        return false;
    }

    bool result = false;
    do
    {
        if (!write_raw(&ctx, "{\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 2U) || !write_raw(&ctx, "\"metadata\": {\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 4U) || !write_raw(&ctx, "\"version\": ") ||
            !write_escaped_string(&ctx, PERSISTENCE_SCHEMA_VERSION) || !write_raw(&ctx, ",\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 4U) || !write_raw(&ctx, "\"name\": ") ||
            !write_escaped_string(&ctx, tree->name ? tree->name : "") || !write_raw(&ctx, ",\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 4U) || !write_raw(&ctx, "\"creation_date\": ") ||
            !write_escaped_string(&ctx, tree->creation_date ? tree->creation_date : "") ||
            !write_raw(&ctx, ",\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 4U) || !write_raw(&ctx, "\"root_ids\": "))
        {
            break;
        }
        if (!write_tree_roots(&ctx, tree) || !write_raw(&ctx, "\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 2U) || !write_raw(&ctx, "},\n"))
        {
            break;
        }
        if (!write_indent(&ctx, 2U) || !write_raw(&ctx, "\"persons\": "))
        {
            break;
        }
        if (!write_persons_array(&ctx, tree, 2U) || !write_raw(&ctx, "\n"))
        {
            break;
        }
        if (!write_raw(&ctx, "}\n"))
        {
            break;
        }
        result = true;
    } while (0);

    if (fclose(ctx.stream) != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to close", path);
        result = false;
    }

    return result;
}
