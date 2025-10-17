#include "detail_content_builder.h"

#include "detail_view.h"
#include "person.h"
#include "timeline.h"

#include <ctype.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int string_compare_insensitive(const char *left, const char *right)
{
    if (left == right)
    {
        return 0;
    }
    if (!left)
    {
        return right ? -1 : 0;
    }
    if (!right)
    {
        return 1;
    }
    while (*left != '\0' && *right != '\0')
    {
        unsigned char a = (unsigned char)tolower((unsigned char)*left);
        unsigned char b = (unsigned char)tolower((unsigned char)*right);
        if (a != b)
        {
            return (int)a - (int)b;
        }
        ++left;
        ++right;
    }
    unsigned char a = (unsigned char)tolower((unsigned char)*left);
    unsigned char b = (unsigned char)tolower((unsigned char)*right);
    return (int)a - (int)b;
}

static void copy_string(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0U)
    {
        return;
    }
    if (!source)
    {
        destination[0] = '\0';
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(destination, capacity, source, _TRUNCATE);
#else
    (void)snprintf(destination, capacity, "%s", source);
#endif
}

static void append_text(char *destination, size_t capacity, const char *text)
{
    if (!destination || capacity == 0U || !text || text[0] == '\0')
    {
        return;
    }
    size_t length = strlen(destination);
    if (length >= capacity - 1U)
    {
        return;
    }
#if defined(_MSC_VER)
    (void)strncat_s(destination, capacity, text, _TRUNCATE);
#else
    (void)snprintf(destination + length, capacity - length, "%s", text);
#endif
}

static void build_default_content(struct DetailViewContent *content)
{
    if (!content)
    {
        return;
    }
    memset(content, 0, sizeof(*content));
    copy_string(content->name, sizeof(content->name), "No Profile Selected");
    copy_string(content->lifespan, sizeof(content->lifespan), "Timeline unavailable");
}

static bool string_has_value(const char *text)
{
    return text && text[0] != '\0';
}

static void append_fact(struct DetailViewContent *content, const char *format, const char *value)
{
    if (!content || !format || !value)
    {
        return;
    }
    if (!string_has_value(value) || content->fact_count >= DETAIL_VIEW_MAX_FACTS)
    {
        return;
    }
    char *destination = content->facts[content->fact_count];
    size_t capacity = sizeof(content->facts[0]);
    (void)snprintf(destination, capacity, format, value);
    content->fact_count += 1U;
}

static void extract_filename(const char *path, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    if (!string_has_value(path))
    {
        buffer[0] = '\0';
        return;
    }

    const char *last_forward = strrchr(path, '/');
    const char *last_backward = strrchr(path, '\\');
    const char *cursor = path;
    if (last_forward && last_backward)
    {
        cursor = (last_forward > last_backward) ? (last_forward + 1) : (last_backward + 1);
    }
    else if (last_forward)
    {
        cursor = last_forward + 1;
    }
    else if (last_backward)
    {
        cursor = last_backward + 1;
    }

    copy_string(buffer, capacity, cursor);
    if (buffer[0] == '\0')
    {
        copy_string(buffer, capacity, path);
    }
}

static void push_certificate(struct DetailViewContent *content, DetailViewCertificateType type, const char *heading,
                             const char *summary, const char *media_path)
{
    if (!content || content->certificate_count >= DETAIL_VIEW_MAX_CERTIFICATES)
    {
        return;
    }

    DetailViewCertificate *certificate = &content->certificates[content->certificate_count];
    memset(certificate, 0, sizeof(*certificate));
    certificate->type = type;
    if (string_has_value(heading))
    {
        copy_string(certificate->heading, sizeof(certificate->heading), heading);
    }
    if (string_has_value(summary))
    {
        copy_string(certificate->summary, sizeof(certificate->summary), summary);
    }
    if (string_has_value(media_path))
    {
        copy_string(certificate->media_path, sizeof(certificate->media_path), media_path);
        certificate->has_media_asset = true;
        size_t length = strlen(media_path);
        if (length >= 4U && string_compare_insensitive(media_path + (length - 4U), ".pdf") == 0)
        {
            certificate->is_pdf = true;
        }
    }

    content->certificate_count += 1U;
}

static int parse_year(const char *value)
{
    if (!string_has_value(value))
    {
        return INT_MAX;
    }

    size_t index = 0U;
    int sign = 1;
    if (value[index] == '-')
    {
        sign = -1;
        index += 1U;
    }

    int year = 0;
    size_t digits = 0U;
    while (value[index] >= '0' && value[index] <= '9')
    {
        year = (year * 10) + (value[index] - '0');
        index += 1U;
        digits += 1U;
    }

    if (digits == 0U)
    {
        return INT_MAX;
    }

    return year * sign;
}

static void populate_core_facts(const struct Person *person, struct DetailViewContent *content)
{
    if (!person || !content)
    {
        return;
    }

    char name_buffer[sizeof(content->name)];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }
    copy_string(content->name, sizeof(content->name), name_buffer);

    const char *birth = person->dates.birth_date;
    const char *death = person->dates.death_date;
    if (string_has_value(birth) && string_has_value(death))
    {
        (void)snprintf(content->lifespan, sizeof(content->lifespan), "%s - %s", birth, death);
    }
    else if (string_has_value(birth))
    {
        if (person->is_alive)
        {
            (void)snprintf(content->lifespan, sizeof(content->lifespan), "%s - Present", birth);
        }
        else
        {
            (void)snprintf(content->lifespan, sizeof(content->lifespan), "Born %s", birth);
        }
    }
    else if (string_has_value(death))
    {
        (void)snprintf(content->lifespan, sizeof(content->lifespan), "Died %s", death);
    }
    else
    {
        copy_string(content->lifespan, sizeof(content->lifespan), "Timeline unavailable");
    }

    append_fact(content, "Birthplace: %s", person->dates.birth_location);
    append_fact(content, "Resting Place: %s", person->dates.death_location);
    append_fact(content, "Status: %s", person->is_alive ? "Living" : "Deceased");
}

static bool certificate_path_used(const struct DetailViewContent *content, const char *path)
{
    if (!content || !string_has_value(path))
    {
        return false;
    }
    for (size_t index = 0U; index < content->certificate_count; ++index)
    {
        if (content->certificates[index].has_media_asset &&
            string_compare_insensitive(content->certificates[index].media_path, path) == 0)
        {
            return true;
        }
    }
    return false;
}

static void populate_certificates(const struct Person *person, struct DetailViewContent *content)
{
    if (!person || !content)
    {
        return;
    }

    char summary[160];
    summary[0] = '\0';
    if (string_has_value(person->dates.birth_date) && string_has_value(person->dates.birth_location))
    {
        (void)snprintf(summary, sizeof(summary), "%s - %s", person->dates.birth_date,
                       person->dates.birth_location);
    }
    else if (string_has_value(person->dates.birth_date))
    {
        copy_string(summary, sizeof(summary), person->dates.birth_date);
    }
    else if (string_has_value(person->dates.birth_location))
    {
        copy_string(summary, sizeof(summary), person->dates.birth_location);
    }
    if (summary[0] != '\0')
    {
        const char *media_path = (person->certificate_count > 0U) ? person->certificate_paths[0] : NULL;
        push_certificate(content, DETAIL_CERTIFICATE_BIRTH, "Birth Certificate", summary, media_path);
    }

    for (size_t index = 0U; index < person->spouses_count; ++index)
    {
        const PersonSpouseRecord *record = &person->spouses[index];
        if (!record)
        {
            continue;
        }

        char heading[64];
        heading[0] = '\0';
        if (record->partner)
        {
            char partner_name[64];
            if (!person_format_display_name(record->partner, partner_name, sizeof(partner_name)))
            {
                (void)snprintf(partner_name, sizeof(partner_name), "Person %u", record->partner->id);
            }
            (void)snprintf(heading, sizeof(heading), "Marriage Certificate: %s", partner_name);
        }
        else
        {
            copy_string(heading, sizeof(heading), "Marriage Certificate");
        }

        summary[0] = '\0';
        if (string_has_value(record->marriage_date) && string_has_value(record->marriage_location))
        {
            (void)snprintf(summary, sizeof(summary), "%s - %s", record->marriage_date, record->marriage_location);
        }
        else if (string_has_value(record->marriage_date))
        {
            copy_string(summary, sizeof(summary), record->marriage_date);
        }
        else if (string_has_value(record->marriage_location))
        {
            copy_string(summary, sizeof(summary), record->marriage_location);
        }

        push_certificate(content, DETAIL_CERTIFICATE_MARRIAGE, heading, summary[0] != '\0' ? summary : NULL, NULL);
    }

    summary[0] = '\0';
    if (string_has_value(person->dates.death_date) && string_has_value(person->dates.death_location))
    {
        (void)snprintf(summary, sizeof(summary), "%s - %s", person->dates.death_date,
                       person->dates.death_location);
    }
    else if (string_has_value(person->dates.death_date))
    {
        copy_string(summary, sizeof(summary), person->dates.death_date);
    }
    else if (string_has_value(person->dates.death_location))
    {
        copy_string(summary, sizeof(summary), person->dates.death_location);
    }
    if (summary[0] != '\0')
    {
        const char *media_path = (person->certificate_count > 1U) ? person->certificate_paths[1] : NULL;
        push_certificate(content, DETAIL_CERTIFICATE_DEATH, "Death Certificate", summary, media_path);
    }

    for (size_t index = 0U; index < person->certificate_count; ++index)
    {
        const char *path = person->certificate_paths[index];
        if (!string_has_value(path) || certificate_path_used(content, path))
        {
            continue;
        }
        char title[64];
        extract_filename(path, title, sizeof(title));
        if (title[0] == '\0')
        {
            copy_string(title, sizeof(title), "Archival Document");
        }
        push_certificate(content, DETAIL_CERTIFICATE_ARCHIVE, title, path, path);
    }

    if (content->certificate_count == 0U)
    {
        push_certificate(content, DETAIL_CERTIFICATE_ARCHIVE, "No Certificates Located",
                         "Upload records to visualize lineage documents.", NULL);
    }
}

static void assign_even_timeline_spacing(struct DetailViewContent *content)
{
    if (!content)
    {
        return;
    }
    size_t count = content->timeline_event_count;
    if (count == 0U)
    {
        return;
    }
    if (count == 1U)
    {
        content->timeline_events[0].normalized_time = 0.5f;
        return;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        content->timeline_events[index].normalized_time = (float)index / (float)(count - 1U);
    }
}

static void populate_timeline(const struct Person *person, struct DetailViewContent *content)
{
    if (!person || !content)
    {
        return;
    }

    content->timeline_event_count = 0U;
    int years[DETAIL_VIEW_MAX_TIMELINE_EVENTS];

    for (size_t index = 0U; index < person->timeline_count &&
                           content->timeline_event_count < DETAIL_VIEW_MAX_TIMELINE_EVENTS;
         ++index)
    {
        const TimelineEntry *entry = &person->timeline_entries[index];
        if (!entry)
        {
            continue;
        }

        DetailViewTimelineEvent *event = &content->timeline_events[content->timeline_event_count];
        memset(event, 0, sizeof(*event));

        switch (entry->type)
        {
        case TIMELINE_EVENT_BIRTH:
            event->type = DETAIL_TIMELINE_BIRTH;
            copy_string(event->title, sizeof(event->title), "Birth");
            break;
        case TIMELINE_EVENT_MARRIAGE:
            event->type = DETAIL_TIMELINE_MARRIAGE;
            copy_string(event->title, sizeof(event->title), "Marriage");
            break;
        case TIMELINE_EVENT_DEATH:
            event->type = DETAIL_TIMELINE_DEATH;
            copy_string(event->title, sizeof(event->title), "Death");
            break;
        case TIMELINE_EVENT_CUSTOM:
        default:
            event->type = DETAIL_TIMELINE_CUSTOM;
            if (string_has_value(entry->description))
            {
                copy_string(event->title, sizeof(event->title), entry->description);
            }
            else
            {
                copy_string(event->title, sizeof(event->title), "Event");
            }
            break;
        }

        if (string_has_value(entry->date))
        {
            copy_string(event->date, sizeof(event->date), entry->date);
        }
        if (string_has_value(entry->description))
        {
            copy_string(event->description, sizeof(event->description), entry->description);
        }
        if (string_has_value(entry->location))
        {
            if (event->description[0] != '\0')
            {
                append_text(event->description, sizeof(event->description), " - ");
            }
            append_text(event->description, sizeof(event->description), entry->location);
        }

        if (entry->media_count > 0U && entry->media_paths)
        {
            const char *primary_media = entry->media_paths[0];
            if (string_has_value(primary_media))
            {
                copy_string(event->media_path, sizeof(event->media_path), primary_media);
                extract_filename(primary_media, event->media_label, sizeof(event->media_label));
                if (event->media_label[0] == '\0')
                {
                    copy_string(event->media_label, sizeof(event->media_label), "Attached media");
                }
                if (entry->media_count > 1U)
                {
                    char count_buffer[32];
                    (void)snprintf(count_buffer, sizeof(count_buffer), " (+%zu)", entry->media_count - 1U);
                    append_text(event->media_label, sizeof(event->media_label), count_buffer);
                    event->multiple_media_assets = true;
                }
                event->has_media_asset = true;
                size_t length = strlen(primary_media);
                if (length >= 4U && string_compare_insensitive(primary_media + (length - 4U), ".pdf") == 0)
                {
                    event->media_is_pdf = true;
                }
            }
        }

        years[content->timeline_event_count] = parse_year(entry->date);
        content->timeline_event_count += 1U;
    }

    if (content->timeline_event_count == 0U)
    {
        DetailViewTimelineEvent *event = &content->timeline_events[0];
        memset(event, 0, sizeof(*event));
        event->type = DETAIL_TIMELINE_CUSTOM;
        copy_string(event->title, sizeof(event->title), "Records Pending");
        copy_string(event->description, sizeof(event->description), "Add timeline entries to visualize milestones.");
        event->normalized_time = 0.5f;
        content->timeline_event_count = 1U;
        return;
    }

    for (size_t i = 0U; i + 1U < content->timeline_event_count; ++i)
    {
        for (size_t j = i + 1U; j < content->timeline_event_count; ++j)
        {
            int year_i = years[i];
            int year_j = years[j];
            if (year_j < year_i)
            {
                DetailViewTimelineEvent temp_event = content->timeline_events[i];
                content->timeline_events[i] = content->timeline_events[j];
                content->timeline_events[j] = temp_event;

                int temp_year = years[i];
                years[i] = years[j];
                years[j] = temp_year;
            }
        }
    }

    int min_year = INT_MAX;
    int max_year = INT_MIN;
    for (size_t index = 0U; index < content->timeline_event_count; ++index)
    {
        int year = years[index];
        if (year == INT_MAX)
        {
            continue;
        }
        if (year < min_year)
        {
            min_year = year;
        }
        if (year > max_year)
        {
            max_year = year;
        }
    }

    if (min_year == INT_MAX || max_year == INT_MIN || min_year == max_year)
    {
        assign_even_timeline_spacing(content);
        return;
    }

    float range = (float)(max_year - min_year);
    if (range <= 0.0f)
    {
        assign_even_timeline_spacing(content);
        return;
    }

    size_t count = content->timeline_event_count;
    if (count == 1U)
    {
        content->timeline_events[0].normalized_time = 0.5f;
        return;
    }

    for (size_t index = 0U; index < count; ++index)
    {
        int year = years[index];
        if (year == INT_MAX)
        {
            content->timeline_events[index].normalized_time = (float)index / (float)(count - 1U);
            continue;
        }
        float normalized = (float)(year - min_year) / range;
        if (normalized < 0.0f)
        {
            normalized = 0.0f;
        }
        if (normalized > 1.0f)
        {
            normalized = 1.0f;
        }
        content->timeline_events[index].normalized_time = normalized;
    }
}

bool detail_view_content_build(const struct Person *person, struct DetailViewContent *out_content)
{
    if (!out_content)
    {
        return false;
    }

    build_default_content(out_content);
    if (!person)
    {
        return false;
    }

    populate_core_facts(person, out_content);
    populate_certificates(person, out_content);
    populate_timeline(person, out_content);
    return true;
}
