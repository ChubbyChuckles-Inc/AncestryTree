#include "timeline_viewer.h"

#include "detail_view.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

void timeline_reset(TimelineViewState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->hover_index = -1;
}

void timeline_init(TimelineViewState *state, const DetailViewContent *content)
{
    if (!state)
    {
        return;
    }
    timeline_reset(state);
    if (!content)
    {
        return;
    }
    state->event_count = content->timeline_event_count;
    for (size_t index = 0U; index < state->event_count && index < DETAIL_VIEW_MAX_TIMELINE_EVENTS; ++index)
    {
        const DetailViewTimelineEvent *event = &content->timeline_events[index];
        TimelineViewEventVisual *visual = &state->events[index];
        visual->normalized_time = event->normalized_time;
        visual->eased_height = 0.0f;
    }
}

void timeline_update(TimelineViewState *state, float delta_seconds, float activation)
{
    if (!state)
    {
        return;
    }
    float safe_delta = (delta_seconds > 0.0f) ? delta_seconds : 0.0f;
    for (size_t index = 0U; index < state->event_count && index < DETAIL_VIEW_MAX_TIMELINE_EVENTS; ++index)
    {
        float target = activation;
        if ((int)index == state->hover_index)
        {
            target = fminf(1.0f, activation + 0.25f);
        }
        float blend = fminf(1.0f, safe_delta * 4.5f);
        state->events[index].eased_height = state->events[index].eased_height * (1.0f - blend) + target * blend;
    }
    float hover_target = (state->hover_index >= 0) ? 1.0f : 0.0f;
    float hover_blend = fminf(1.0f, safe_delta * 6.0f);
    state->hover_blend = state->hover_blend * (1.0f - hover_blend) + hover_target * hover_blend;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static Color timeline_colour(DetailViewTimelineType type)
{
    switch (type)
    {
    case DETAIL_TIMELINE_BIRTH:
        return (Color){120, 220, 255, 230};
    case DETAIL_TIMELINE_MARRIAGE:
        return (Color){255, 180, 140, 230};
    case DETAIL_TIMELINE_DEATH:
        return (Color){200, 140, 255, 230};
    case DETAIL_TIMELINE_CUSTOM:
    default:
        return (Color){160, 200, 180, 230};
    }
}
#endif

void timeline_render(const TimelineViewState *state, float activation, int screen_width, int screen_height,
                     const DetailViewContent *content)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state || !content || activation <= 0.05f)
    {
        return;
    }
    int margin = screen_width < 1280 ? 36 : 64;
    int area_height = screen_height < 720 ? 120 : 160;
    int base_y = screen_height - margin - area_height;
    Rectangle area = {(float)margin, (float)base_y, (float)(screen_width - margin * 2), (float)area_height};

    Color background = {18, 28, 46, (unsigned char)(activation * 90.0f)};
    DrawRectangleRounded(area, 0.08f, 12, background);

    float line_y = area.y + area.height * 0.6f;
    Color line_colour = {80, 140, 220, (unsigned char)(activation * 180.0f)};
    DrawLineEx((Vector2){area.x + 18.0f, line_y}, (Vector2){area.x + area.width - 18.0f, line_y}, 3.0f, line_colour);

    for (size_t index = 0U; index < state->event_count && index < content->timeline_event_count; ++index)
    {
        const DetailViewTimelineEvent *event = &content->timeline_events[index];
        const TimelineViewEventVisual *visual = &state->events[index];
        float x = area.x + 24.0f + visual->normalized_time * (area.width - 48.0f);
        float height = area.height * 0.28f * fmaxf(0.3f, visual->eased_height);
        float radius = 12.0f + 18.0f * visual->eased_height;
        Color colour = timeline_colour(event->type);
        colour.a = (unsigned char)(colour.a * fminf(1.0f, activation + 0.15f));

        Vector2 marker = {x, line_y - height};
        DrawCircleGradient((int)marker.x, (int)marker.y, radius, colour, (Color){10, 18, 32, 0});
        DrawCircleLines((int)marker.x, (int)marker.y, radius, (Color){colour.r, colour.g, colour.b, (unsigned char)255});

        if (radius > 16.0f)
        {
            int font_size = 16;
            const char *label = event->title;
            int text_width = MeasureText(label, font_size);
            DrawText(label, (int)(marker.x - text_width / 2), (int)(marker.y - radius - 20.0f), font_size,
                     (Color){215, 240, 255, (unsigned char)(activation * 200.0f)});
        }
    }
#else
    (void)state;
    (void)activation;
    (void)screen_width;
    (void)screen_height;
    (void)content;
#endif
}

int timeline_event_hover(TimelineViewState *state, int cursor_x, int cursor_y, int screen_width, int screen_height,
                         const DetailViewContent *content)
{
    if (!state || !content)
    {
        return -1;
    }
    int hover = -1;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    int margin = screen_width < 1280 ? 36 : 64;
    int area_height = screen_height < 720 ? 120 : 160;
    int base_y = screen_height - margin - area_height;
    Rectangle area = {(float)margin, (float)base_y, (float)(screen_width - margin * 2), (float)area_height};
    Vector2 cursor = {(float)cursor_x, (float)cursor_y};
    if (CheckCollisionPointRec(cursor, area))
    {
        for (size_t index = 0U; index < state->event_count && index < content->timeline_event_count; ++index)
        {
            const TimelineViewEventVisual *visual = &state->events[index];
            float x = area.x + 24.0f + visual->normalized_time * (area.width - 48.0f);
            float radius = 16.0f + 18.0f * visual->eased_height;
            float dx = cursor.x - x;
            float dy = cursor.y - (area.y + area.height * 0.6f - area.height * 0.28f * visual->eased_height);
            float distance_sq = dx * dx + dy * dy;
            if (distance_sq <= radius * radius)
            {
                hover = (int)index;
                break;
            }
        }
    }
#else
    (void)cursor_x;
    (void)cursor_y;
    (void)screen_width;
    (void)screen_height;
#endif
    state->hover_index = hover;
    return hover;
}
