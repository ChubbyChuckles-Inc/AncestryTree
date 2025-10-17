#include "certificate_render.h"

#include "detail_view.h"

#include <math.h>
#include <stddef.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#include <raymath.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void certificate_reset(CertificateRenderState *state)
{
    if (!state)
    {
        return;
    }
    state->orbit_angle = 0.0f;
    state->zoom_target = -1;
    state->active_zoom = -1;
    state->zoom_progress = 0.0f;
    state->highlight_timer = 0.0f;
}

void certificate_update(CertificateRenderState *state, float delta_seconds, float activation, size_t certificate_count,
                        size_t focus_index)
{
    if (!state)
    {
        return;
    }

    float safe_delta = (delta_seconds > 0.0f) ? delta_seconds : 0.0f;
    state->orbit_angle += safe_delta * (0.35f + activation * 0.6f);
    if (state->orbit_angle > (float)(M_PI * 2.0))
    {
        state->orbit_angle = fmodf(state->orbit_angle, (float)(M_PI * 2.0));
    }

    state->highlight_timer += safe_delta;
    if (state->highlight_timer > 4.0f)
    {
        state->highlight_timer = fmodf(state->highlight_timer, 4.0f);
    }

    if (state->zoom_target < 0 && certificate_count > 0U)
    {
        state->zoom_target = (int)focus_index;
    }

    const float approach_speed = 2.4f;
    if (state->zoom_target >= 0)
    {
        state->zoom_progress += safe_delta * approach_speed;
        if (state->zoom_progress > 1.0f)
        {
            state->zoom_progress = 1.0f;
        }
        state->active_zoom = state->zoom_target;
    }
    else
    {
        if (state->zoom_progress > 0.0f)
        {
            state->zoom_progress -= safe_delta * approach_speed;
            if (state->zoom_progress < 0.0f)
            {
                state->zoom_progress = 0.0f;
            }
        }
        if (state->zoom_progress <= 0.0001f)
        {
            state->active_zoom = -1;
        }
    }
}

void certificate_zoom(CertificateRenderState *state, size_t index, bool enable)
{
    if (!state)
    {
        return;
    }
    if (!enable)
    {
        state->zoom_target = -1;
        return;
    }
    state->zoom_target = (int)index;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
static Color certificate_colour(DetailViewCertificateType type)
{
    switch (type)
    {
    case DETAIL_CERTIFICATE_BIRTH:
        return (Color){120, 220, 255, 200};
    case DETAIL_CERTIFICATE_MARRIAGE:
        return (Color){255, 160, 220, 200};
    case DETAIL_CERTIFICATE_DEATH:
        return (Color){200, 140, 255, 200};
    case DETAIL_CERTIFICATE_ARCHIVE:
    default:
        return (Color){150, 210, 190, 200};
    }
}
#endif

void certificate_render(const CertificateRenderState *state, const DetailViewContent *content, float activation,
                        const float origin[3], float room_radius, const Camera3D *camera)
{
    (void)camera;
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
    if (!state || !content || activation <= 0.01f)
    {
        return;
    }
    if (content->certificate_count == 0U)
    {
        return;
    }

    const float base_radius = room_radius * 0.62f;
    const float base_height = room_radius * 0.38f;
    const float angle_step = (float)(M_PI * 2.0) / (float)content->certificate_count;

    for (size_t index = 0U; index < content->certificate_count; ++index)
    {
        float angle = state->orbit_angle + angle_step * (float)index;
        float spread = base_radius + sinf(angle * 2.0f) * room_radius * 0.04f;

        Vector3 center = {
            origin[0] + cosf(angle) * spread,
            origin[1] - room_radius * 0.12f + sinf(state->highlight_timer + angle) * room_radius * 0.02f,
            origin[2] + sinf(angle) * spread};

        float activation_scale = 0.6f + activation * 0.4f;
        float height = base_height * activation_scale;
        float radius = room_radius * 0.08f * activation_scale;

        if ((int)index == state->active_zoom)
        {
            float zoom = 1.0f + state->zoom_progress * 0.8f;
            height *= zoom;
            radius *= zoom;
        }

        Color colour = certificate_colour(content->certificates[index].type);
        colour.a = (unsigned char)((float)colour.a * activation);

        Vector3 start = {center.x, center.y - height * 0.5f, center.z};
        Vector3 end = {center.x, center.y + height * 0.5f, center.z};
        DrawCylinderEx(start, end, radius * 0.85f, radius, 8, colour);

        Color cap = colour;
        cap.a = (unsigned char)(colour.a * 0.7f);
        DrawSphere(end, radius * 1.05f, cap);
    }
#else
    (void)state;
    (void)content;
    (void)activation;
    (void)origin;
    (void)room_radius;
#endif
}
