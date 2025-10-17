#include "detail_content_builder.h"

#include "detail_view.h"
#include "person.h"
#include "timeline.h"

#include <ctype.h>
#include <math.h>
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
    copy_string(content->name, sizeof(content->name), "Select a person");
    copy_string(content->lifespan, sizeof(content->lifespan), "Awaiting selection");
    copy_string(content->facts[0], sizeof(content->facts[0]),
                "Choose an ancestor to explore their holographic records.");
    content->fact_count = 1U;
    content->certificate_count = 0U;
    content->certificate_focus_index = 0U;
    content->timeline_event_count = 0U;
}

static int parse_year(const char *date_string)
{
    if (!date_string)
    {
        return -1;
    }
    int digits = 0;
    int year = 0;
    for (const char *cursor = date_string; *cursor != '\0'; ++cursor)
    {
        if (!isdigit((unsigned char)*cursor))
        {
            if (digits >= 4)
            {
                break;
            }
            if (digits > 0)
            {
                return year;
            }
            continue;
        }
        year = year * 10 + (*cursor - '0');
        digits += 1;
        if (digits == 4)
        {
            return year;
        }
    }
    return digits >= 4 ? year : -1;
}

static void append_fact(struct DetailViewContent *content, const char *format, const char *value)
{
    if (!content || !value || value[0] == '\0')
    {
        return;
    }
    if (content->fact_count >= DETAIL_VIEW_MAX_FACTS)
    {
        return;
    }
    (void)snprintf(content->facts[content->fact_count], sizeof(content->facts[content->fact_count]), format, value);
    content->fact_count += 1U;
}

static void push_certificate(struct DetailViewContent *content, DetailViewCertificateType type, const char *heading,
                             const char *summary, const char *media_path)
{
    if (!content || !heading)
    {
        return;
    }
    if (content->certificate_count >= DETAIL_VIEW_MAX_CERTIFICATES)
    {
        return;
    }
    DetailViewCertificate *certificate = &content->certificates[content->certificate_count];
    memset(certificate, 0, sizeof(*certificate));
    certificate->type = type;
    copy_string(certificate->heading, sizeof(certificate->heading), heading);
    if (summary)
    {
        copy_string(certificate->summary, sizeof(certificate->summary), summary);
    }
    if (media_path)
    {
        copy_string(certificate->media_path, sizeof(certificate->media_path), media_path);
        certificate->has_media_asset = true;
        size_t length = strlen(media_path);
        certificate->is_pdf = length >= 4 &&
                              string_compare_insensitive(media_path + (length - 4), ".pdf") == 0;
    }
    content->certificate_count += 1U;
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

    const char *birth = (person->dates.birth_date && person->dates.birth_date[0] != '\0') ? person->dates.birth_date
                                                                                          : NULL;
    const char *death = (person->dates.death_date && person->dates.death_date[0] != '\0') ? person->dates.death_date
                                                                                          : NULL;

    if (birth && death)
    {
        (void)snprintf(content->lifespan, sizeof(content->lifespan), "%s - %s", birth, death);
    }
    else if (birth)
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
    else if (death)
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

static void populate_certificates(const struct Person *person, struct DetailViewContent *content)
{
    if (!person || !content)
    {
        return;
    }

    if (person->dates.birth_date || person->dates.birth_location)
    {
        char summary[160];
        summary[0] = '\0';
        if (person->dates.birth_date && person->dates.birth_location)
        {
            (void)snprintf(summary, sizeof(summary), "%s - %s", person->dates.birth_date,
                           person->dates.birth_location);
        }
        else if (person->dates.birth_date)
        {
            copy_string(summary, sizeof(summary), person->dates.birth_date);
        }
        else if (person->dates.birth_location)
        {
            copy_string(summary, sizeof(summary), person->dates.birth_location);
        }
        push_certificate(content, DETAIL_CERTIFICATE_BIRTH, "Birth Certificate", summary,
                         person->certificate_count > 0U ? person->certificate_paths[0] : NULL);
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

        char summary[160];
        summary[0] = '\0';
        if (record->marriage_date && record->marriage_location)
        {
            (void)snprintf(summary, sizeof(summary), "%s - %s", record->marriage_date, record->marriage_location);
        }
        else if (record->marriage_date)
        {
            copy_string(summary, sizeof(summary), record->marriage_date);
        }
        else if (record->marriage_location)
        {
            copy_string(summary, sizeof(summary), record->marriage_location);
        }
        push_certificate(content, DETAIL_CERTIFICATE_MARRIAGE, heading, summary, NULL);
    }

    if (person->dates.death_date || person->dates.death_location)
    {
        char summary[160];
        summary[0] = '\0';
        if (person->dates.death_date && person->dates.death_location)
        {
            (void)snprintf(summary, sizeof(summary), "%s - %s", person->dates.death_date,
                           person->dates.death_location);
        }
        else if (person->dates.death_date)
        {
            copy_string(summary, sizeof(summary), person->dates.death_date);
        }
        else if (person->dates.death_location)
        {
            copy_string(summary, sizeof(summary), person->dates.death_location);
        }
        push_certificate(content, DETAIL_CERTIFICATE_DEATH, "Death Certificate", summary,
                         person->certificate_count > 1U ? person->certificate_paths[1] : NULL);
    }

    for (size_t index = 0U; index < person->certificate_count; ++index)
    {
        const char *path = person->certificate_paths[index];
        if (!path)
        {
            continue;
        }
        bool already_used = false;
        for (size_t existing = 0U; existing < content->certificate_count; ++existing)
        {
            if (content->certificates[existing].has_media_asset &&
                string_compare_insensitive(content->certificates[existing].media_path, path) == 0)
            {
                already_used = true;
                break;
            }
        }
        if (already_used)
        {
            continue;
        }
        push_certificate(content, DETAIL_CERTIFICATE_ARCHIVE, "Archival Document", path, path);
    }

    if (content->certificate_count == 0U)
    {
        push_certificate(content, DETAIL_CERTIFICATE_ARCHIVE, "No Certificates Located",
                         "Upload records to visualize lineage documents.", NULL);
    }
}

static void populate_timeline(const struct Person *person, struct DetailViewContent *content)
{
    if (!person || !content)
    {
        return;
    }

    int min_year = 99999;
    int max_year = -99999;
    int years[DETAIL_VIEW_MAX_TIMELINE_EVENTS];
    size_t year_count = 0U;

    content->timeline_event_count = 0U;
    for (size_t index = 0U; index < person->timeline_count && content->timeline_event_count < DETAIL_VIEW_MAX_TIMELINE_EVENTS;
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
            copy_string(event->title, sizeof(event->title), entry->description ? entry->description : "Event");
            break;
        }

        if (entry->date)
        {
            copy_string(event->date, sizeof(event->date), entry->date);
        }
        if (entry->description)
        {
            copy_string(event->description, sizeof(event->description), entry->description);
        }
        if (entry->location)
        {
            if (event->description[0] != '\0')
            {
                append_text(event->description, sizeof(event->description), " - ");
            }
            append_text(event->description, sizeof(event->description), entry->location);
        }

        int year = parse_year(entry->date);
        years[year_count < DETAIL_VIEW_MAX_TIMELINE_EVENTS ? year_count : DETAIL_VIEW_MAX_TIMELINE_EVENTS - 1U] = year;
        if (year >= 0)
        {
            if (year < min_year)
            {
                min_year = year;
            }
            if (year > max_year)
            {
                max_year = year;
            }
        }
        year_count += 1U;

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

    if (min_year > max_year || min_year == 99999 || max_year == -99999)
    {
        /* Fall back to positional spacing when no year data is available. */
        size_t count = content->timeline_event_count;
        for (size_t index = 0U; index < count; ++index)
        {
            content->timeline_events[index].normalized_time = (float)index / (float)(count > 1U ? (count - 1U) : 1U);
        }
        return;
    }

    const float range = (float)(max_year - min_year);
    for (size_t index = 0U; index < content->timeline_event_count; ++index)
    {
        int year = parse_year(content->timeline_events[index].date);
        if (year < 0 || range <= 0.0f)
        {
            content->timeline_events[index].normalized_time = (float)index /
                                                              (float)(content->timeline_event_count > 1U
                                                                          ? (content->timeline_event_count - 1U)
                                                                          : 1U);
        }
        else
        {
            content->timeline_events[index].normalized_time = (float)(year - min_year) / range;
        }
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
