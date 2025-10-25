#include "ui_icons.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define UI_ICON_DIMENSION 32
#define UI_ICON_PI 3.14159265358979323846f

static const char *UI_ICON_NAMES[UI_ICON_COUNT] = {
    "add", "search", "settings", "help", "info", "save", "cancel", "delete", "exit",
};

static void ui_icon_clamp(float *value)
{
    if (!value)
    {
        return;
    }
    if (*value < 0.0f)
    {
        *value = 0.0f;
    }
    if (*value > 1.0f)
    {
        *value = 1.0f;
    }
}

static void ui_icon_generate_add(float *buffer)
{
    float thickness = 0.14f;
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx    = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny    = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float value = 0.0f;
            if (fabsf(nx) <= thickness)
            {
                value = 1.0f - (fabsf(nx) / thickness);
            }
            if (fabsf(ny) <= thickness)
            {
                float alt = 1.0f - (fabsf(ny) / thickness);
                if (alt > value)
                {
                    value = alt;
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_search(float *buffer)
{
    float radius       = 0.48f;
    float handle_angle = 0.75f;
    float handle_half  = 0.14f;
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx       = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny       = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float distance = sqrtf(nx * nx + ny * ny);
            float ring     = 0.0f;
            float band     = 0.08f;
            if (distance >= radius - band && distance <= radius + band)
            {
                ring = 1.0f - fabsf(distance - radius) / band;
            }
            float hx     = nx * cosf(handle_angle) - ny * sinf(handle_angle);
            float hy     = nx * sinf(handle_angle) + ny * cosf(handle_angle);
            float handle = 0.0f;
            if (hx > 0.2f && fabsf(hy) <= handle_half)
            {
                handle = 1.0f - (fabsf(hy) / handle_half);
            }
            float value                       = (ring > handle) ? ring : handle;
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_settings(float *buffer)
{
    float inner = 0.28f;
    float outer = 0.6f;
    int teeth   = 6;
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx       = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny       = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float distance = sqrtf(nx * nx + ny * ny);
            float value    = 0.0f;
            if (distance >= inner && distance <= outer)
            {
                float spoke      = fabsf(atan2f(ny, nx)) * (float)teeth / UI_ICON_PI;
                float spoke_mod  = spoke - floorf(spoke);
                float spoke_edge = fabsf(spoke_mod - 0.5f) * 2.0f;
                float spike      = 1.0f - spoke_edge;
                if (spike < 0.3f)
                {
                    spike = 0.3f;
                }
                float radial  = (outer - distance) / (outer - inner);
                float blended = radial * spike;
                if (blended > value)
                {
                    value = blended;
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_help(float *buffer)
{
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx     = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny     = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float value  = 0.0f;
            float radius = sqrtf(nx * nx + ny * ny);
            if (radius <= 1.0f)
            {
                float outline_band = 0.08f;
                if (radius >= 0.9f - outline_band)
                {
                    value = 1.0f - fabsf(radius - 0.9f) / outline_band;
                }
                else if (ny > -0.2f)
                {
                    float question_core = fabsf(nx);
                    if (question_core <= 0.15f)
                    {
                        float top  = 1.0f - fabsf(ny - 0.4f) / 0.5f;
                        float body = 1.0f - question_core / 0.15f;
                        float head = (top > 0.0f && body > 0.0f) ? (top * body) : 0.0f;
                        if (head > value)
                        {
                            value = head;
                        }
                    }
                }
                else if (ny <= -0.2f && ny >= -0.6f && fabsf(nx) <= 0.12f)
                {
                    float stem = 1.0f - fabsf(nx) / 0.12f;
                    if (stem > value)
                    {
                        value = stem;
                    }
                }
                else if (ny < -0.6f && fabsf(nx) <= 0.12f)
                {
                    float dot = 1.0f - fabsf(ny + 0.75f) / 0.1f;
                    if (dot > value)
                    {
                        value = dot;
                    }
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_info(float *buffer)
{
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx     = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny     = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float radius = sqrtf(nx * nx + ny * ny);
            float value  = 0.0f;
            if (radius <= 1.0f)
            {
                float outline_band = 0.08f;
                if (radius >= 0.92f - outline_band)
                {
                    value = 1.0f - fabsf(radius - 0.92f) / outline_band;
                }
                else if (fabsf(nx) <= 0.12f && ny >= -0.6f)
                {
                    float stem     = 1.0f - fabsf(nx) / 0.12f;
                    float vertical = 1.0f - fabsf(ny) / 0.6f;
                    float blend    = stem * fmaxf(vertical, 0.0f);
                    if (blend > value)
                    {
                        value = blend;
                    }
                }
                else if (ny < -0.6f && fabsf(nx) <= 0.14f)
                {
                    float dot = 1.0f - fabsf(ny + 0.75f) / 0.12f;
                    if (dot > value)
                    {
                        value = dot;
                    }
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_save(float *buffer)
{
    float notch_height = 0.35f;
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx    = ((float)x / (float)(UI_ICON_DIMENSION - 1U));
            float ny    = ((float)y / (float)(UI_ICON_DIMENSION - 1U));
            float value = 0.0f;
            if (nx >= 0.08f && nx <= 0.92f && ny >= 0.08f && ny <= 0.92f)
            {
                value = 1.0f;
                if (ny <= 0.25f)
                {
                    float slot = (ny - 0.08f) / 0.17f;
                    if (slot < 0.0f)
                    {
                        slot = 0.0f;
                    }
                    if (slot > 1.0f)
                    {
                        slot = 1.0f;
                    }
                    value = slot;
                }
                else if (ny >= 0.55f)
                {
                    float inset = fminf((nx - 0.2f) / 0.6f, (0.8f - nx) / 0.6f);
                    if (inset < value)
                    {
                        value = fmaxf(inset, 0.4f);
                    }
                }
            }
            if (ny >= 0.55f && ny <= 0.55f + notch_height && nx >= 0.25f && nx <= 0.75f)
            {
                value = 1.0f;
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_cancel(float *buffer)
{
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx     = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny     = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float radius = sqrtf(nx * nx + ny * ny);
            float value  = 0.0f;
            float band   = 0.08f;
            if (radius >= 0.85f - band && radius <= 0.85f + band)
            {
                value = 1.0f - fabsf(radius - 0.85f) / band;
            }
            float diagonal = fabsf(nx + ny) / sqrtf(2.0f);
            if (diagonal <= 0.18f)
            {
                float cross = 1.0f - diagonal / 0.18f;
                if (cross > value)
                {
                    value = cross;
                }
            }
            float anti = fabsf(nx - ny) / sqrtf(2.0f);
            if (anti <= 0.18f)
            {
                float cross = 1.0f - anti / 0.18f;
                if (cross > value)
                {
                    value = cross;
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_delete(float *buffer)
{
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx    = ((float)x / (float)(UI_ICON_DIMENSION - 1U));
            float ny    = ((float)y / (float)(UI_ICON_DIMENSION - 1U));
            float value = 0.0f;
            if (ny >= 0.25f && ny <= 0.92f && nx >= 0.25f && nx <= 0.75f)
            {
                float horizontal = 1.0f - fmaxf(fabsf(nx - 0.5f) / 0.25f, 0.0f);
                float vertical   = 1.0f - (ny - 0.25f) / 0.67f;
                float blend      = fmaxf(fminf(horizontal, 1.0f), 0.0f) * fmaxf(vertical, 0.0f);
                if (blend > value)
                {
                    value = blend;
                }
            }
            if (ny >= 0.15f && ny <= 0.25f && nx >= 0.2f && nx <= 0.8f)
            {
                float lid = 1.0f - (ny - 0.15f) / 0.1f;
                if (lid > value)
                {
                    value = lid;
                }
            }
            if (ny >= 0.08f && ny <= 0.15f && nx >= 0.4f && nx <= 0.6f)
            {
                float handle = 1.0f - fabsf(nx - 0.5f) / 0.1f;
                if (handle > value)
                {
                    value = handle;
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate_exit(float *buffer)
{
    for (int y = 0; y < UI_ICON_DIMENSION; ++y)
    {
        for (int x = 0; x < UI_ICON_DIMENSION; ++x)
        {
            float nx    = ((float)x / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float ny    = ((float)y / (float)(UI_ICON_DIMENSION - 1U)) * 2.0f - 1.0f;
            float value = 0.0f;
            if (nx <= -0.1f && fabsf(ny) <= 0.8f)
            {
                float door = 1.0f - fabsf(ny) / 0.8f;
                if (door > value)
                {
                    value = door * 0.6f;
                }
            }
            if (nx >= -0.4f && nx <= 0.9f && fabsf(ny) <= 0.18f)
            {
                float shaft = 1.0f - fabsf(ny) / 0.18f;
                if (shaft > value)
                {
                    value = shaft;
                }
            }
            float arrow_y = ny;
            float arrow_x = nx - 0.3f;
            if (arrow_x >= -0.1f)
            {
                float arrow = 1.0f - fabsf(arrow_y) / (fabsf(arrow_x) + 0.2f);
                if (arrow > value && arrow_x <= 0.7f)
                {
                    value = arrow;
                }
            }
            buffer[y * UI_ICON_DIMENSION + x] = value;
        }
    }
}

static void ui_icon_generate(UIIconType icon, float *buffer)
{
    memset(buffer, 0, sizeof(float) * UI_ICON_DIMENSION * UI_ICON_DIMENSION);
    switch (icon)
    {
    case UI_ICON_ADD:
        ui_icon_generate_add(buffer);
        break;
    case UI_ICON_SEARCH:
        ui_icon_generate_search(buffer);
        break;
    case UI_ICON_SETTINGS:
        ui_icon_generate_settings(buffer);
        break;
    case UI_ICON_HELP:
        ui_icon_generate_help(buffer);
        break;
    case UI_ICON_INFO:
        ui_icon_generate_info(buffer);
        break;
    case UI_ICON_SAVE:
        ui_icon_generate_save(buffer);
        break;
    case UI_ICON_CANCEL:
        ui_icon_generate_cancel(buffer);
        break;
    case UI_ICON_DELETE:
        ui_icon_generate_delete(buffer);
        break;
    case UI_ICON_EXIT:
        ui_icon_generate_exit(buffer);
        break;
    case UI_ICON_COUNT:
    default:
        break;
    }
}

static float ui_icon_calculate_density(const float *buffer)
{
    float sum = 0.0f;
    for (int i = 0; i < UI_ICON_DIMENSION * UI_ICON_DIMENSION; ++i)
    {
        float value = buffer[i];
        if (value < 0.0f)
        {
            value = 0.0f;
        }
        if (value > 1.0f)
        {
            value = 1.0f;
        }
        sum += value;
    }
    float total = (float)(UI_ICON_DIMENSION * UI_ICON_DIMENSION);
    if (!(total > 0.0f))
    {
        return 0.0f;
    }
    return sum / total;
}

int ui_icons_dimension(void) { return UI_ICON_DIMENSION; }

bool ui_icons_density(UIIconType icon, float *out_density)
{
    if (!out_density || icon < 0 || icon >= UI_ICON_COUNT)
    {
        return false;
    }
    float buffer[UI_ICON_DIMENSION * UI_ICON_DIMENSION];
    ui_icon_generate(icon, buffer);
    *out_density = ui_icon_calculate_density(buffer);
    ui_icon_clamp(out_density);
    return true;
}

const char *ui_icons_name(UIIconType icon)
{
    if (icon < 0 || icon >= UI_ICON_COUNT)
    {
        return "unknown";
    }
    return UI_ICON_NAMES[icon];
}

bool ui_icons_init(UIIconLibrary *library)
{
    if (!library)
    {
        return false;
    }
    memset(library, 0, sizeof(*library));
    library->dimension = UI_ICON_DIMENSION;
    for (int icon = 0; icon < UI_ICON_COUNT; ++icon)
    {
        float density;
        ui_icons_density((UIIconType)icon, &density);
        library->density[icon] = density;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    Image atlas =
        GenImageColor(UI_ICON_DIMENSION * UI_ICON_COUNT, UI_ICON_DIMENSION, (Color){0, 0, 0, 0});
    if (!atlas.data)
    {
        library->ready = false;
        return false;
    }
    float buffer[UI_ICON_DIMENSION * UI_ICON_DIMENSION];
    for (int icon = 0; icon < UI_ICON_COUNT; ++icon)
    {
        ui_icon_generate((UIIconType)icon, buffer);
        for (int y = 0; y < UI_ICON_DIMENSION; ++y)
        {
            for (int x = 0; x < UI_ICON_DIMENSION; ++x)
            {
                float value = buffer[y * UI_ICON_DIMENSION + x];
                if (value < 0.0f)
                {
                    value = 0.0f;
                }
                if (value > 1.0f)
                {
                    value = 1.0f;
                }
                unsigned char alpha = (unsigned char)(value * 255.0f);
                Color pixel         = {255, 255, 255, alpha};
                ImageDrawPixel(&atlas, icon * UI_ICON_DIMENSION + x, y, pixel);
            }
        }
    }
    Texture2D texture = LoadTextureFromImage(atlas);
    UnloadImage(atlas);
    if (texture.id == 0)
    {
        library->ready = false;
        return false;
    }
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    library->atlas = texture;
    for (int icon = 0; icon < UI_ICON_COUNT; ++icon)
    {
        struct nk_image image;
        memset(&image, 0, sizeof(image));
        image.handle.ptr      = &library->atlas;
        image.w               = (unsigned short)library->atlas.width;
        image.h               = (unsigned short)library->atlas.height;
        image.region[0]       = (unsigned short)(icon * UI_ICON_DIMENSION);
        image.region[1]       = 0;
        image.region[2]       = (unsigned short)UI_ICON_DIMENSION;
        image.region[3]       = (unsigned short)UI_ICON_DIMENSION;
        library->glyphs[icon] = image;
    }
    library->ready = true;
#else
    library->ready = false;
#endif
    return library->ready;
}

void ui_icons_shutdown(UIIconLibrary *library)
{
    if (!library)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    if (library->ready && library->atlas.id != 0)
    {
        UnloadTexture(library->atlas);
        library->atlas.id = 0;
    }
#endif
    library->ready = false;
}

bool ui_icons_ready(const UIIconLibrary *library)
{
    if (!library)
    {
        return false;
    }
    return library->ready;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)

static struct nk_color ui_icons_apply_intensity(struct nk_color color, float intensity)
{
    if (intensity < 0.0f)
    {
        intensity = 0.0f;
    }
    if (intensity > 1.5f)
    {
        intensity = 1.5f;
    }
    float scale = intensity;
    struct nk_color result;
    result.r = (unsigned char)fminf((float)color.r * scale, 255.0f);
    result.g = (unsigned char)fminf((float)color.g * scale, 255.0f);
    result.b = (unsigned char)fminf((float)color.b * scale, 255.0f);
    result.a = color.a;
    return result;
}

bool ui_icons_draw(const UIIconLibrary *library, struct nk_context *ctx, UIIconType icon,
                   struct nk_rect bounds, struct nk_color color, float intensity)
{
    if (!library || !ctx || icon < 0 || icon >= UI_ICON_COUNT)
    {
        return false;
    }
    if (!library->ready)
    {
        return false;
    }
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    if (!canvas)
    {
        return false;
    }
    struct nk_image image  = library->glyphs[icon];
    struct nk_color tinted = ui_icons_apply_intensity(color, intensity);
    nk_draw_image(canvas, bounds, &image, tinted);
    return true;
}

#endif
