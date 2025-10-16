#include "detail_view.h"

#include "person.h"
#include "timeline.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_NUKLEAR)
#include "ui_nuklear_config.h"
#include "external/nuklear.h"
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#define DETAIL_VIEW_CERTIFICATE_COLS 3
#define DETAIL_VIEW_MIN_PANEL_WIDTH 480.0f
#define DETAIL_VIEW_MIN_PANEL_HEIGHT 460.0f

static void detail_view_unload_profile(DetailViewState *state)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state)
    {
        return;
    }
    if (state->profile_texture_loaded)
    {
        UnloadTexture(state->profile_texture);
        state->profile_texture_loaded = false;
        state->profile_texture.id = 0;
    }
#else
    (void)state;
#endif
}

static void detail_view_unload_certificate(DetailViewState *state)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state)
    {
        return;
    }
    if (state->certificate_preview_loaded)
    {
        UnloadTexture(state->certificate_preview_texture);
        state->certificate_preview_loaded = false;
        state->certificate_preview_texture.id = 0;
        state->certificate_preview_path[0] = '\0';
    }
#else
    (void)state;
#endif
}

void detail_view_state_reset(DetailViewState *state)
{
    if (!state)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    detail_view_unload_profile(state);
    detail_view_unload_certificate(state);
#endif
    state->cached_person_id = 0U;
    state->selected_certificate_index = -1;
    state->gallery_hover_index = -1;
    if (state->initialized)
    {
        detail_gallery_reset(&state->gallery);
        detail_timeline_reset(&state->timeline);
    }
}

bool detail_view_init(DetailViewState *state)
{
    if (!state)
    {
        return false;
    }
    memset(state, 0, sizeof(*state));
    state->selected_certificate_index = -1;
    state->gallery_hover_index = -1;
    if (!detail_gallery_init(&state->gallery))
    {
        return false;
    }
    if (!detail_timeline_init(&state->timeline))
    {
        detail_gallery_shutdown(&state->gallery);
        return false;
    }
    state->initialized = true;
    return true;
}

void detail_view_cleanup(DetailViewState *state)
{
    if (!state)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    detail_view_unload_profile(state);
    detail_view_unload_certificate(state);
#endif
    detail_gallery_shutdown(&state->gallery);
    detail_timeline_shutdown(&state->timeline);
    state->initialized = false;
    state->cached_person_id = 0U;
    state->selected_certificate_index = -1;
    state->gallery_hover_index = -1;
}

static const char *detail_view_event_type_label(TimelineEventType type)
{
    switch (type)
    {
    case TIMELINE_EVENT_BIRTH:
        return "Birth";
    case TIMELINE_EVENT_MARRIAGE:
        return "Marriage";
    case TIMELINE_EVENT_DEATH:
        return "Death";
    case TIMELINE_EVENT_CUSTOM:
    default:
        return "Event";
    }
}

static void detail_view_format_dates(const Person *person, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    buffer[0] = '\0';
    if (!person)
    {
        return;
    }
    const char *birth = (person->dates.birth_date && person->dates.birth_date[0] != '\0') ? person->dates.birth_date
                                                                                          : "Unknown";
    const char *death = (person->dates.death_date && person->dates.death_date[0] != '\0') ? person->dates.death_date
                                                                                          : (person->is_alive ? "Present"
                                                                                                              : "Unknown");
    (void)snprintf(buffer, capacity, "%s — %s", birth, death);
}

static void detail_view_format_location(const Person *person, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    buffer[0] = '\0';
    if (!person)
    {
        return;
    }
    const char *birth_place =
        (person->dates.birth_location && person->dates.birth_location[0] != '\0') ? person->dates.birth_location : "Unknown";
    const char *death_place =
        (person->dates.death_location && person->dates.death_location[0] != '\0') ? person->dates.death_location : "Unknown";
    (void)snprintf(buffer, capacity, "Born: %s\nDied: %s", birth_place, death_place);
}

static void detail_view_extract_filename(const char *path, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    buffer[0] = '\0';
    if (!path)
    {
        (void)snprintf(buffer, capacity, "(unknown)");
        return;
    }
    const char *last_slash = strrchr(path, '/');
#if defined(_WIN32)
    const char *last_backslash = strrchr(path, '\\');
    if (!last_slash || (last_backslash && last_backslash > last_slash))
    {
        last_slash = last_backslash;
    }
#endif
    const char *filename = last_slash ? last_slash + 1 : path;
    if (filename[0] == '\0')
    {
        filename = path;
    }
    (void)snprintf(buffer, capacity, "%s", filename);
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static void detail_view_refresh_profile_texture(DetailViewState *state, const Person *person)
{
    if (!state)
    {
        return;
    }
    detail_view_unload_profile(state);
    if (!person || !person->profile_image_path || person->profile_image_path[0] == '\0')
    {
        return;
    }
    if (!FileExists(person->profile_image_path))
    {
        return;
    }
    Texture2D texture = LoadTexture(person->profile_image_path);
    if (texture.id == 0)
    {
        return;
    }
    state->profile_texture = texture;
    state->profile_texture_loaded = true;
}

static void detail_view_ensure_profile_loaded(DetailViewState *state, const Person *person)
{
    if (!state || !person)
    {
        return;
    }
    if (!state->profile_texture_loaded)
    {
        detail_view_refresh_profile_texture(state, person);
    }
}

static void detail_view_load_certificate_preview(DetailViewState *state, const char *path)
{
    if (!state)
    {
        return;
    }
    if (!path || path[0] == '\0')
    {
        detail_view_unload_certificate(state);
        return;
    }
    if (state->certificate_preview_loaded && strncmp(state->certificate_preview_path, path,
                                                     sizeof(state->certificate_preview_path)) == 0)
    {
        return;
    }
    detail_view_unload_certificate(state);
    if (!FileExists(path))
    {
        return;
    }
    Texture2D texture = LoadTexture(path);
    if (texture.id == 0)
    {
        return;
    }
    state->certificate_preview_texture = texture;
    state->certificate_preview_loaded = true;
    (void)snprintf(state->certificate_preview_path, sizeof(state->certificate_preview_path), "%s", path);
}
#endif

static void detail_view_render_profile_section(DetailViewState *state, struct nk_context *ctx, const Person *person,
                                               float width)
{
    if (!ctx || !person)
    {
        return;
    }
    nk_layout_row_dynamic(ctx, 26.0f, 1);
    nk_label(ctx, "Profile", NK_TEXT_LEFT);

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    detail_view_ensure_profile_loaded(state, person);
    if (state && state->profile_texture_loaded)
    {
        float image_size = fminf(width, 200.0f);
        nk_layout_row_static(ctx, image_size, (int)image_size, 1);
        struct nk_image nk_tex = nk_image_id(state->profile_texture.id);
        nk_image(ctx, nk_tex);
    }
    else
#endif
    {
        nk_layout_row_dynamic(ctx, 80.0f, 1);
        nk_label_wrap(ctx, "No profile image available.");
    }

    char dates[128];
    detail_view_format_dates(person, dates, sizeof(dates));
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_labelf(ctx, NK_TEXT_LEFT, "Life Span: %s", dates);

    char locations[160];
    detail_view_format_location(person, locations, sizeof(locations));
    nk_layout_row_dynamic(ctx, 40.0f, 1);
    nk_label_wrap(ctx, locations);

    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_labelf(ctx, NK_TEXT_LEFT, "Alive: %s", person->is_alive ? "Yes" : "No");
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    nk_labelf(ctx, NK_TEXT_LEFT, "Person ID: %u", person->id);
}

static void detail_view_render_metadata(struct nk_context *ctx, const Person *person)
{
    if (!ctx || !person)
    {
        return;
    }
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_label(ctx, "Metadata", NK_TEXT_LEFT);
    if (person->metadata_count == 0U)
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "No metadata entries.", NK_TEXT_LEFT);
        return;
    }
    nk_layout_row_dynamic(ctx, 20.0f, 1);
    for (size_t index = 0U; index < person->metadata_count; ++index)
    {
        const PersonMetadataEntry *entry = &person->metadata[index];
        const char *key = (entry->key && entry->key[0] != '\0') ? entry->key : "(key)";
        const char *value = (entry->value && entry->value[0] != '\0') ? entry->value : "(value)";
        nk_labelf(ctx, NK_TEXT_LEFT, "%s: %s", key, value);
    }
}

static void detail_view_render_certificates(DetailViewState *state, struct nk_context *ctx, const Person *person)
{
    if (!ctx || !state || !person)
    {
        return;
    }
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_label(ctx, "Certificates", NK_TEXT_LEFT);
    if (person->certificate_count == 0U)
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "No certificates on record.", NK_TEXT_LEFT);
        return;
    }

    int columns = DETAIL_VIEW_CERTIFICATE_COLS;
    if (person->certificate_count < (size_t)columns)
    {
        columns = (int)person->certificate_count;
    }
    if (columns <= 0)
    {
        columns = 1;
    }

    nk_layout_row_begin(ctx, NK_STATIC, 68.0f, columns);
    for (size_t index = 0U; index < person->certificate_count; ++index)
    {
        nk_layout_row_push(ctx, 120.0f);
        const char *path = person->certificate_paths[index];
        char display[96];
        detail_view_extract_filename(path, display, sizeof(display));
        nk_bool selected = (state->selected_certificate_index == (int)index) ? nk_true : nk_false;
        nk_selectable_label(ctx, display, NK_TEXT_CENTERED, &selected);
        if (selected && state->selected_certificate_index != (int)index)
        {
            state->selected_certificate_index = (int)index;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
            detail_view_load_certificate_preview(state, path);
#else
            (void)path;
#endif
        }
        else if (!selected && state->selected_certificate_index == (int)index)
        {
            state->selected_certificate_index = -1;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
            detail_view_unload_certificate(state);
#else
            (void)path;
#endif
        }
    }
    nk_layout_row_end(ctx);

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    if (state->selected_certificate_index >= 0 && state->certificate_preview_loaded)
    {
        float preview_height = 160.0f;
        nk_layout_row_static(ctx, preview_height, 180.0f, 1);
        struct nk_image image = nk_image_id(state->certificate_preview_texture.id);
        nk_image(ctx, image);
    }
    else if (state->selected_certificate_index >= 0)
#endif
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Preview unavailable for selected certificate.", NK_TEXT_LEFT);
    }
}

static void detail_view_render_gallery(DetailViewState *state, struct nk_context *ctx, float available_width)
{
    if (!state || !ctx)
    {
        return;
    }
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_label(ctx, "Image Gallery", NK_TEXT_LEFT);
    if (!detail_gallery_has_media(&state->gallery))
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "No images or media attached to this person.", NK_TEXT_LEFT);
        return;
    }

    float zoom = detail_gallery_get_zoom(&state->gallery);
    float min_zoom = detail_gallery_min_zoom();
    float max_zoom = detail_gallery_max_zoom();
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_labelf(ctx, NK_TEXT_LEFT, "Zoom: %.2fx", zoom);
    nk_layout_row_dynamic(ctx, 16.0f, 1);
    if (nk_slider_float(ctx, min_zoom, &zoom, max_zoom, 0.05f))
    {
        detail_gallery_set_zoom(&state->gallery, zoom);
    }

    nk_layout_row_dynamic(ctx, 26.0f, 3);
    if (nk_button_label(ctx, "Previous"))
    {
        detail_gallery_select_previous(&state->gallery);
    }
    nk_labelf(ctx, NK_TEXT_CENTERED, "%d / %zu",
              (state->gallery.selected_index >= 0) ? (state->gallery.selected_index + 1) : 0,
              state->gallery.count);
    if (nk_button_label(ctx, "Next"))
    {
        detail_gallery_select_next(&state->gallery);
    }

    const float cell_base = 96.0f;
    float cell_size = cell_base * detail_gallery_get_zoom(&state->gallery);
    if (cell_size < 48.0f)
    {
        cell_size = 48.0f;
    }
    float cell_spacing = 12.0f;
    if (available_width <= 0.0f)
    {
        available_width = cell_size * 2.0f;
    }
    int columns = (int)fmaxf(1.0f, floorf((available_width - cell_spacing) / (cell_size + cell_spacing)));
    if (columns <= 0)
    {
        columns = 1;
    }
    int rows = (int)((state->gallery.count + (size_t)columns - 1U) / (size_t)columns);

    for (int row = 0; row < rows; ++row)
    {
        nk_layout_row_begin(ctx, NK_STATIC, cell_size, columns);
        for (int col = 0; col < columns; ++col)
        {
            size_t index = (size_t)row * (size_t)columns + (size_t)col;
            nk_layout_row_push(ctx, cell_size);
            if (index >= state->gallery.count)
            {
                nk_spacing(ctx, 1);
                continue;
            }
            const DetailGalleryEntry *entry = detail_gallery_get_entry(&state->gallery, index);
            bool selected = (state->gallery.selected_index == (int)index);
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
            unsigned int texture_id = detail_gallery_acquire_texture(&state->gallery, index);
            if (texture_id != 0U)
            {
                struct nk_image image = nk_image_id((int)texture_id);
                if (nk_button_image(ctx, image))
                {
                    detail_gallery_select(&state->gallery, index);
                    selected = true;
                }
                struct nk_rect thumb_bounds = nk_widget_bounds(ctx);
                if (state->gallery.selected_index == (int)index)
                {
                    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
                    if (canvas)
                    {
                        struct nk_color highlight = nk_rgb(248, 220, 92);
                        nk_stroke_rect(canvas, thumb_bounds, 2.0f, 2.0f, highlight);
                    }
                    selected = true;
                }
                else
                {
                    selected = false;
                }
            }
            else
#endif
            {
                nk_bool selectable = selected ? nk_true : nk_false;
                char display[96];
                detail_view_extract_filename(entry ? entry->path : NULL, display, sizeof(display));
                if (nk_selectable_label(ctx, display, NK_TEXT_CENTERED, &selectable))
                {
                    if (selectable)
                    {
                        detail_gallery_select(&state->gallery, index);
                    }
                    selected = (selectable == nk_true);
                }
                else
                {
                    selected = (state->gallery.selected_index == (int)index);
                }
            }
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 20.0f, columns);
        for (int col = 0; col < columns; ++col)
        {
            size_t index = (size_t)row * (size_t)columns + (size_t)col;
            nk_layout_row_push(ctx, cell_size);
            if (index >= state->gallery.count)
            {
                nk_spacing(ctx, 1);
                continue;
            }
            const DetailGalleryEntry *entry = detail_gallery_get_entry(&state->gallery, index);
            char display[96];
            detail_view_extract_filename(entry ? entry->path : NULL, display, sizeof(display));
            nk_label(ctx, display, NK_TEXT_CENTERED);
        }
        nk_layout_row_end(ctx);
    }

    const DetailGalleryEntry *selected_entry = NULL;
    if (state->gallery.selected_index >= 0)
    {
        selected_entry = detail_gallery_get_entry(&state->gallery, (size_t)state->gallery.selected_index);
    }

    if (selected_entry)
    {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, selected_entry->path ? selected_entry->path : "(no path)", NK_TEXT_LEFT);
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
        unsigned int preview_id = detail_gallery_acquire_texture(&state->gallery, (size_t)state->gallery.selected_index);
        if (preview_id != 0U)
        {
            float preview_width = available_width - 12.0f;
            if (preview_width < 100.0f)
            {
                preview_width = 100.0f;
            }
            float preview_height = preview_width * 0.6f * detail_gallery_get_zoom(&state->gallery);
            if (preview_height > 320.0f)
            {
                preview_height = 320.0f;
            }
            nk_layout_row_static(ctx, preview_height, (int)preview_width, 1);
            struct nk_image image = nk_image_id((int)preview_id);
            nk_image(ctx, image);
        }
        else if (selected_entry->file_available)
        {
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, "Preview unavailable; texture load failed.", NK_TEXT_LEFT);
        }
        else
#endif
        {
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, "Media file missing or offline; preview skipped.", NK_TEXT_LEFT);
        }
    }
}

static void detail_view_render_timeline(DetailViewState *state, struct nk_context *ctx, const Person *person)
{
    if (!state || !ctx || !person)
    {
        return;
    }
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    nk_label(ctx, "Timeline", NK_TEXT_LEFT);
    if (state->timeline.count == 0U)
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "No timeline entries available.", NK_TEXT_LEFT);
        return;
    }

    nk_layout_row_dynamic(ctx, 140.0f, 1);
    struct nk_rect axis_bounds;
    (void)nk_widget(&axis_bounds, ctx);
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    if (canvas)
    {
        const float axis_padding = 28.0f;
        float axis_start = axis_bounds.x + axis_padding;
        float axis_end = axis_bounds.x + axis_bounds.w - axis_padding;
        float axis_y = axis_bounds.y + axis_bounds.h * 0.5f;
        struct nk_color axis_color = nk_rgb(92, 132, 180);
        nk_stroke_line(canvas, axis_start, axis_y, axis_end, axis_y, 2.0f, axis_color);

        size_t hovered_index = (size_t)(-1);
        const struct nk_input *input = &ctx->input;
        float cursor_x = input->mouse.pos.x;
        float cursor_y = input->mouse.pos.y;
        if (detail_timeline_hit_test(&state->timeline, axis_start, axis_end, cursor_x, 12.0f, &hovered_index) &&
            cursor_y >= axis_bounds.y - 20.0f && cursor_y <= axis_bounds.y + axis_bounds.h + 20.0f)
        {
            const DetailTimelineEvent *hovered = detail_timeline_get_event(&state->timeline, hovered_index);
            if (hovered && hovered->entry)
            {
                const TimelineEntry *entry = hovered->entry;
                const char *type_label = detail_view_event_type_label(entry->type);
                const char *date = (entry->date && entry->date[0] != '\0') ? entry->date : "Unknown";
                if (entry->description && entry->description[0] != '\0')
                {
                    nk_tooltipf(ctx, "%s — %s\n%s", type_label, date, entry->description);
                }
                else
                {
                    nk_tooltipf(ctx, "%s — %s", type_label, date);
                }
            }
        }

        struct nk_color marker_color = nk_rgb(180, 220, 250);
        struct nk_color marker_highlight = nk_rgb(255, 255, 255);
        struct nk_color text_color = nk_rgb(220, 230, 240);
        const struct nk_user_font *font = ctx->style.font;
        float axis_width = axis_end - axis_start;
        for (size_t index = 0U; index < state->timeline.count; ++index)
        {
            const DetailTimelineEvent *event = detail_timeline_get_event(&state->timeline, index);
            if (!event || !event->entry)
            {
                continue;
            }
            float marker_x = axis_start + axis_width * event->position;
            struct nk_rect marker = nk_rect(marker_x - 5.0f, axis_y - 5.0f, 10.0f, 10.0f);
            struct nk_color color = (hovered_index == index) ? marker_highlight : marker_color;
            nk_fill_circle(canvas, marker, color);

            char label_buffer[32];
            if (event->year != 0 && state->timeline.has_year_data)
            {
                (void)snprintf(label_buffer, sizeof(label_buffer), "%d", event->year);
            }
            else if (event->entry->date && event->entry->date[0] != '\0')
            {
                (void)snprintf(label_buffer, sizeof(label_buffer), "%s", event->entry->date);
            }
            else
            {
                (void)snprintf(label_buffer, sizeof(label_buffer), "%zu", index + 1U);
            }
            struct nk_rect label_bounds = nk_rect(marker_x - 40.0f, axis_y + 10.0f, 80.0f, 18.0f);
            nk_draw_text(canvas, label_bounds, label_buffer, (int)strlen(label_buffer), font, nk_rgba(0, 0, 0, 0),
                         text_color);
        }
    }

    nk_layout_row_dynamic(ctx, 172.0f, 1);
    if (nk_group_begin(ctx, "TimelineList", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR))
    {
        for (size_t index = 0U; index < person->timeline_count; ++index)
        {
            const TimelineEntry *entry = &person->timeline_entries[index];
            const char *type_label = detail_view_event_type_label(entry->type);
            const char *date = (entry->date && entry->date[0] != '\0') ? entry->date : "Unknown";
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "%s — %s", type_label, date);

            if (entry->description && entry->description[0] != '\0')
            {
                nk_layout_row_dynamic(ctx, 40.0f, 1);
                nk_label_wrap(ctx, entry->description);
            }
            if (entry->location && entry->location[0] != '\0')
            {
                nk_layout_row_dynamic(ctx, 20.0f, 1);
                nk_labelf(ctx, NK_TEXT_LEFT, "Location: %s", entry->location);
            }
            if (entry->media_count > 0U)
            {
                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_labelf(ctx, NK_TEXT_LEFT, "Media items: %zu", entry->media_count);
            }
            nk_layout_row_dynamic(ctx, 8.0f, 1);
            nk_spacing(ctx, 1);
        }
        nk_group_end(ctx);
    }
}

bool detail_view_render(DetailViewState *state, struct nk_context *ctx, const struct Person *person,
                        int viewport_width, int viewport_height, bool *out_exit_requested)
{
    (void)viewport_width;
    (void)viewport_height;
    if (out_exit_requested)
    {
        *out_exit_requested = false;
    }
    if (!state || !ctx || !person)
    {
        return false;
    }
#if !defined(ANCESTRYTREE_HAVE_NUKLEAR)
    (void)state;
    (void)ctx;
    (void)person;
    (void)viewport_width;
    (void)viewport_height;
    return false;
#else
    if (!state->initialized)
    {
        if (!detail_view_init(state))
        {
            return false;
        }
    }
    if (state->cached_person_id != person->id)
    {
        detail_view_state_reset(state);
        state->cached_person_id = person->id;
        if (!detail_gallery_populate_from_person(&state->gallery, person))
        {
            detail_gallery_reset(&state->gallery);
        }
        if (!detail_timeline_build(&state->timeline, person))
        {
            detail_timeline_reset(&state->timeline);
        }
    }

    float width = (float)viewport_width;
    float height = (float)viewport_height;
    if (!(width > 0.0f) || !(height > 0.0f))
    {
        return false;
    }

    float panel_width = fmaxf(width * 0.66f, DETAIL_VIEW_MIN_PANEL_WIDTH);
    if (panel_width > width - 40.0f)
    {
        panel_width = width - 40.0f;
    }
    float panel_height = fmaxf(height * 0.78f, DETAIL_VIEW_MIN_PANEL_HEIGHT);
    if (panel_height > height - 40.0f)
    {
        panel_height = height - 40.0f;
    }

    float panel_x = (width - panel_width) * 0.5f;
    if (panel_x < 20.0f)
    {
        panel_x = 20.0f;
    }
    float panel_y = (height - panel_height) * 0.5f;
    if (panel_y < 20.0f)
    {
        panel_y = 20.0f;
    }

    struct nk_rect bounds = nk_rect(panel_x, panel_y, panel_width, panel_height);
    if (!nk_begin(ctx, "DetailView", bounds,
                  NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR))
    {
        nk_end(ctx);
        return true;
    }

    char name_buffer[192];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }

    nk_layout_row_dynamic(ctx, 34.0f, 1);
    nk_label(ctx, name_buffer, NK_TEXT_CENTERED);

    nk_layout_row_dynamic(ctx, 30.0f, 1);
    if (nk_button_label(ctx, "Return to Tree View"))
    {
        if (out_exit_requested)
        {
            *out_exit_requested = true;
        }
    }

    nk_layout_row_begin(ctx, NK_DYNAMIC, panel_height - 120.0f, 2);
    nk_layout_row_push(ctx, 0.38f);
    if (nk_group_begin(ctx, "DetailLeft", NK_WINDOW_BORDER))
    {
        detail_view_render_profile_section(state, ctx, person, panel_width * 0.32f);
        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);
        detail_view_render_metadata(ctx, person);
        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);
        detail_view_render_certificates(state, ctx, person);
        nk_group_end(ctx);
    }

    nk_layout_row_push(ctx, 0.62f);
    if (nk_group_begin(ctx, "DetailRight", NK_WINDOW_BORDER))
    {
        float group_width = panel_width * 0.62f;
        detail_view_render_timeline(state, ctx, person);
        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);
        detail_view_render_gallery(state, ctx, group_width - 16.0f);
        nk_group_end(ctx);
    }
    nk_layout_row_end(ctx);

    nk_end(ctx);

    return true;
#endif
}

/* Manual validation checklist:
 * 1. Enter detail view and confirm the overlay displays the person's name, lifespan, and metadata.
 * 2. Verify profile images appear when the referenced asset exists and a placeholder message shows otherwise.
 * 3. Select certificates and confirm the preview updates (or reports when the asset is missing).
 * 4. Scroll through lengthy timeline entries to ensure the content remains readable and scrollable.
 * 5. Click "Return to Tree View" and confirm the camera animates back to the tree layout.
 */
