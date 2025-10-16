#include "ui.h"

#include "camera_controller.h"
#include "graphics.h"
#include "layout.h"
#include "render.h"
#include "settings.h"
#include "tree.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
#include "ui_nuklear_config.h"
#include "external/nuklear.h"
#endif

typedef struct UIInternal
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    struct nk_context ctx;
    struct nk_user_font font;
    Font rl_font;
    float font_size;
    bool scissor_active;
    bool auto_orbit;
    bool show_about_window;
    bool show_help_window;
    bool show_settings_window;
    bool show_exit_prompt;
    char status_message[128];
    float status_timer;
#else
    int unused;
#endif
} UIInternal;

static void ui_event_queue_reset(UIEventQueue *queue)
{
    if (!queue)
    {
        return;
    }
    queue->head = 0U;
    queue->count = 0U;
}

static bool ui_event_queue_push(UIEventQueue *queue, UIEventType type)
{
    if (!queue || type == UI_EVENT_NONE)
    {
        return false;
    }
    if (queue->count >= UI_EVENT_QUEUE_CAPACITY)
    {
        return false;
    }
    size_t index = (queue->head + queue->count) % UI_EVENT_QUEUE_CAPACITY;
    queue->events[index].type = type;
    queue->count += 1U;
    return true;
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
static UIInternal *ui_internal_cast(UIContext *ui)
{
    if (!ui)
    {
        return NULL;
    }
    return (UIInternal *)ui->impl;
}

static const UIInternal *ui_internal_const_cast(const UIContext *ui)
{
    if (!ui)
    {
        return NULL;
    }
    return (const UIInternal *)ui->impl;
}
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)

static const char *ui_connection_style_label(RenderConnectionStyle style)
{
    switch (style)
    {
    case RENDER_CONNECTION_STYLE_STRAIGHT:
        return "Straight";
    case RENDER_CONNECTION_STYLE_BEZIER:
        return "Bezier";
    default:
        return "Unknown";
    }
}

static const char *ui_settings_graphics_quality_label(SettingsGraphicsQuality quality)
{
    switch (quality)
    {
    case SETTINGS_GRAPHICS_QUALITY_PERFORMANCE:
        return "Performance";
    case SETTINGS_GRAPHICS_QUALITY_QUALITY:
    default:
        return "Quality";
    }
}

static const char *ui_settings_layout_label(SettingsLayoutAlgorithm algorithm)
{
    switch (algorithm)
    {
    case SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED:
        return "Force-directed";
    case SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL:
    default:
        return "Hierarchical";
    }
}

static const char *ui_settings_color_scheme_label(SettingsColorScheme scheme)
{
    switch (scheme)
    {
    case SETTINGS_COLOR_SCHEME_SOLAR_ORCHID:
        return "Solar Orchid";
    case SETTINGS_COLOR_SCHEME_CYAN_GRAPH:
    default:
        return "Cyan Graph";
    }
}

static const char *ui_settings_language_label(SettingsLanguage language)
{
    switch (language)
    {
    case SETTINGS_LANGUAGE_FUTURE:
        return "Future Tongue";
    case SETTINGS_LANGUAGE_ENGLISH:
    default:
        return "English";
    }
}

static float nk_raylib_text_width(nk_handle handle, float height, const char *text, int len)
{
    const Font *font = (const Font *)handle.ptr;
    if (!font)
    {
        return (float)len * height * 0.5f;
    }
    if (len < 0)
    {
        len = (int)strlen(text);
    }
    if (len <= 0)
    {
        return 0.0f;
    }
    char stack_buffer[512];
    char *buffer = stack_buffer;
    if (len >= (int)sizeof(stack_buffer))
    {
        buffer = (char *)malloc((size_t)len + 1U);
        if (!buffer)
        {
            return 0.0f;
        }
    }
    memcpy(buffer, text, (size_t)len);
    buffer[len] = '\0';
    Vector2 size = MeasureTextEx(*font, buffer, height, 0.0f);
    if (buffer != stack_buffer)
    {
        free(buffer);
    }
    return size.x;
}

static void nk_raylib_query_font(nk_handle handle, float height, struct nk_user_font_glyph *glyph, nk_rune codepoint,
                                 nk_rune next_codepoint)
{
    (void)next_codepoint;
    Font *font = (Font *)handle.ptr;
    char buffer[8];
    int byte_count = 0;
    const char *utf8 = CodepointToUTF8((int)codepoint, &byte_count);
    if (!utf8 || byte_count <= 0 || byte_count >= (int)sizeof(buffer))
    {
        buffer[0] = (char)codepoint;
        byte_count = 1;
    }
    else
    {
        memcpy(buffer, utf8, (size_t)byte_count);
    }
    buffer[byte_count] = '\0';
    Vector2 size = font ? MeasureTextEx(*font, buffer, height, 0.0f) : (Vector2){height, height};
    glyph->width = size.x;
    glyph->height = height;
    glyph->offset = nk_vec2(0.0f, 0.0f);
    glyph->xadvance = size.x;
    glyph->uv[0] = nk_vec2(0.0f, 0.0f);
    glyph->uv[1] = nk_vec2(0.0f, 0.0f);
}

static void ui_apply_default_style(struct nk_context *ctx)
{
    if (!ctx)
    {
        return;
    }
    struct nk_color table[NK_COLOR_COUNT];
    table[NK_COLOR_TEXT] = nk_rgba(236, 240, 241, 255);
    table[NK_COLOR_WINDOW] = nk_rgba(12, 16, 24, 230);
    table[NK_COLOR_HEADER] = nk_rgba(34, 44, 68, 255);
    table[NK_COLOR_BORDER] = nk_rgba(76, 86, 106, 200);
    table[NK_COLOR_BUTTON] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(66, 82, 110, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(90, 110, 150, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(66, 82, 110, 255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(236, 240, 241, 255);
    table[NK_COLOR_SELECT] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(90, 110, 150, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_SLIDER_CURSOR] = nk_rgba(236, 240, 241, 255);
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = nk_rgba(255, 255, 255, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = nk_rgba(255, 255, 255, 255);
    table[NK_COLOR_PROPERTY] = nk_rgba(34, 44, 68, 255);
    table[NK_COLOR_EDIT] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(236, 240, 241, 255);
    table[NK_COLOR_COMBO] = nk_rgba(34, 44, 68, 255);
    table[NK_COLOR_CHART] = nk_rgba(45, 55, 80, 255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(0, 190, 255, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(0, 200, 255, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(34, 44, 68, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = nk_rgba(66, 82, 110, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = nk_rgba(90, 110, 150, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = nk_rgba(90, 110, 150, 255);
    table[NK_COLOR_TAB_HEADER] = nk_rgba(34, 44, 68, 255);
    nk_style_from_table(ctx, table);
}

static void ui_clipboard_copy(nk_handle usr, const char *text, int len)
{
    (void)usr;
    if (!text || len <= 0)
    {
        return;
    }
    char *buffer = (char *)malloc((size_t)len + 1U);
    if (!buffer)
    {
        return;
    }
    memcpy(buffer, text, (size_t)len);
    buffer[len] = '\0';
    SetClipboardText(buffer);
    free(buffer);
}

static void ui_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    (void)usr;
    const char *text = GetClipboardText();
    if (text)
    {
        nk_textedit_paste(edit, text, nk_strlen(text));
    }
}

static Color nk_color_to_raylib(struct nk_color color)
{
    Color result;
    result.r = color.r;
    result.g = color.g;
    result.b = color.b;
    result.a = color.a;
    return result;
}

static void ui_internal_render(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    const struct nk_command *cmd;
    nk_foreach(cmd, ctx)
    {
        switch (cmd->type)
        {
        case NK_COMMAND_SCISSOR:
        {
            const struct nk_command_scissor *scissor = (const struct nk_command_scissor *)cmd;
            if (internal->scissor_active)
            {
                EndScissorMode();
            }
            BeginScissorMode(scissor->x, scissor->y, scissor->w, scissor->h);
            internal->scissor_active = true;
        }
        break;
        case NK_COMMAND_LINE:
        {
            const struct nk_command_line *line = (const struct nk_command_line *)cmd;
            Vector2 start = {(float)line->begin.x, (float)line->begin.y};
            Vector2 end = {(float)line->end.x, (float)line->end.y};
            DrawLineEx(start, end, (float)line->line_thickness, nk_color_to_raylib(line->color));
        }
        break;
        case NK_COMMAND_RECT:
        {
            const struct nk_command_rect *rect = (const struct nk_command_rect *)cmd;
            Rectangle bounds = {(float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h};
            if (rect->rounding > 0)
            {
                DrawRectangleRoundedLines(bounds, rect->rounding / bounds.width, 6, rect->line_thickness,
                                          nk_color_to_raylib(rect->color));
            }
            else
            {
                DrawRectangleLinesEx(bounds, rect->line_thickness, nk_color_to_raylib(rect->color));
            }
        }
        break;
        case NK_COMMAND_RECT_FILLED:
        {
            const struct nk_command_rect_filled *rect = (const struct nk_command_rect_filled *)cmd;
            Rectangle bounds = {(float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h};
            if (rect->rounding > 0)
            {
                DrawRectangleRounded(bounds, rect->rounding / bounds.width, 6, nk_color_to_raylib(rect->color));
            }
            else
            {
                DrawRectangleRec(bounds, nk_color_to_raylib(rect->color));
            }
        }
        break;
        case NK_COMMAND_TEXT:
        {
            const struct nk_command_text *text = (const struct nk_command_text *)cmd;
            const struct nk_user_font *font = text->font;
            float height = font ? font->height : internal->font_size;
            Font draw_font = internal->rl_font;
            if (font && font->userdata.ptr)
            {
                draw_font = *(Font *)font->userdata.ptr;
            }
            int length = text->length;
            if (length <= 0)
            {
                break;
            }
            char stack_buffer[512];
            char *buffer = stack_buffer;
            if (length >= (int)sizeof(stack_buffer))
            {
                buffer = (char *)malloc((size_t)length + 1U);
                if (!buffer)
                {
                    break;
                }
            }
            memcpy(buffer, text->string, (size_t)length);
            buffer[length] = '\0';
            DrawTextEx(draw_font, buffer, (Vector2){(float)text->x, (float)text->y}, height, 0.0f,
                       nk_color_to_raylib(text->foreground));
            if (buffer != stack_buffer)
            {
                free(buffer);
            }
        }
        break;
        default:
            break;
        }
    }
    if (internal->scissor_active)
    {
        EndScissorMode();
        internal->scissor_active = false;
    }
    nk_clear(ctx);
}

static void ui_internal_set_status(UIInternal *internal, const char *message)
{
    if (!internal)
    {
        return;
    }
    if (!message || message[0] == '\0')
    {
        internal->status_message[0] = '\0';
        internal->status_timer = 0.0f;
        return;
    }
    (void)snprintf(internal->status_message, sizeof(internal->status_message), "%s", message);
    internal->status_timer = 4.0f;
}

static void ui_internal_tick(UIInternal *internal, float delta_seconds)
{
    if (!internal)
    {
        return;
    }
    if (internal->status_timer > 0.0f)
    {
        internal->status_timer -= delta_seconds;
        if (internal->status_timer <= 0.0f)
        {
            internal->status_timer = 0.0f;
            internal->status_message[0] = '\0';
        }
    }
}

static bool ui_compute_layout_center(const LayoutResult *layout, float out_center[3])
{
    if (!layout || !out_center || layout->count == 0U)
    {
        return false;
    }
    out_center[0] = 0.0f;
    out_center[1] = 0.0f;
    out_center[2] = 0.0f;
    for (size_t index = 0U; index < layout->count; ++index)
    {
        out_center[0] += layout->nodes[index].position[0];
        out_center[1] += layout->nodes[index].position[1];
        out_center[2] += layout->nodes[index].position[2];
    }
    float count = (float)layout->count;
    out_center[0] /= count;
    out_center[1] /= count;
    out_center[2] /= count;
    return true;
}

static void ui_draw_about_window(UIInternal *internal, const UIContext *ui)
{
    if (!internal || !ui || !internal->show_about_window)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float width = fminf(360.0f, (float)ui->width * 0.6f);
    float height = 240.0f;
    float x = ((float)ui->width - width) * 0.5f;
    struct nk_rect bounds = nk_rect(x, 64.0f, width, height);
    if (nk_begin(ctx, "About##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "AncestryTree Prototype", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Futuristic holographic ancestry explorer.", NK_TEXT_LEFT);
        nk_label(ctx, "Current focus: rendering pipeline + UI overlays.", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Raylib + Nuklear integration in progress.", NK_TEXT_LEFT);
        nk_label(ctx, "Credits: ChubbyChuckles Labs & open-source collaborators.", NK_TEXT_LEFT);
        nk_label(ctx, "License: MIT (see bundled LICENSE file).", NK_TEXT_LEFT);
        nk_label(ctx, "Documentation: README.md and docs/ directory.", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (nk_button_label(ctx, "Close"))
        {
            internal->show_about_window = false;
        }
        if (nk_button_label(ctx, "Project Page"))
        {
            ui_internal_set_status(internal, "Documentation pending public release.");
        }
    }
    else
    {
        internal->show_about_window = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "About##AncestryTree"))
    {
        internal->show_about_window = false;
    }
}

static void ui_draw_help_window(UIInternal *internal, const UIContext *ui)
{
    if (!internal || !ui || !internal->show_help_window)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float width = fminf(420.0f, (float)ui->width * 0.75f);
    float height = fminf(320.0f, (float)ui->height * 0.75f);
    float x = ((float)ui->width - width) * 0.5f;
    struct nk_rect bounds = nk_rect(x, 80.0f, width, height);
    if (nk_begin(ctx, "Quick Help##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Keyboard Shortcuts", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Ctrl+N: New placeholder tree", NK_TEXT_LEFT);
        nk_label(ctx, "Ctrl+O: Load sample tree", NK_TEXT_LEFT);
        nk_label(ctx, "Ctrl+S: Save current tree", NK_TEXT_LEFT);
        nk_label(ctx, "Ctrl+Shift+S: Save tree as timestamped copy", NK_TEXT_LEFT);
        nk_label(ctx, "Ctrl+Z / Ctrl+Y: Undo / Redo (pending command stack)", NK_TEXT_LEFT);
        nk_label(ctx, "Space: Reset orbital camera", NK_TEXT_LEFT);
        nk_label(ctx, "Escape: Clear selection & close dialogs", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Navigation", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Right mouse drag: Orbit", NK_TEXT_LEFT);
        nk_label(ctx, "Middle mouse drag / WASD: Pan", NK_TEXT_LEFT);
        nk_label(ctx, "Mouse wheel: Zoom", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Troubleshooting", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "If overlays disappear, ensure View > Show holographic overlay remains enabled.",
                 NK_TEXT_LEFT);
        nk_label(ctx, "Use Focus Roots after loading large trees to re-center the camera instantly.",
                 NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 30.0f, 1);
        if (nk_button_label(ctx, "Close"))
        {
            internal->show_help_window = false;
        }
    }
    else
    {
        internal->show_help_window = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Quick Help##AncestryTree"))
    {
        internal->show_help_window = false;
    }
}

static void ui_draw_settings_window(UIInternal *internal, UIContext *ui, Settings *settings, bool settings_dirty)
{
    if (!internal || !ui || !settings || !internal->show_settings_window)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float width = fminf(420.0f, (float)ui->width - 40.0f);
    if (width < 280.0f)
    {
        width = 280.0f;
    }
    struct nk_rect bounds = nk_rect((float)ui->width - width - 20.0f, 60.0f, width, 460.0f);
    if (nk_begin(ctx, "Settings##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_CLOSABLE))
    {
        if (settings_dirty)
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            nk_label(ctx, "Unsaved changes pending.", NK_TEXT_LEFT);
        }
        else
        {
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, "Settings match saved configuration.", NK_TEXT_LEFT);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Graphics", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (nk_combo_begin_label(ctx, ui_settings_graphics_quality_label(settings->graphics_quality),
                                 nk_vec2(220.0f, 80.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (nk_combo_item_label(ctx, "Quality", NK_TEXT_LEFT) &&
                settings->graphics_quality != SETTINGS_GRAPHICS_QUALITY_QUALITY)
            {
                settings->graphics_quality = SETTINGS_GRAPHICS_QUALITY_QUALITY;
                settings_mark_dirty(settings);
            }
            if (nk_combo_item_label(ctx, "Performance", NK_TEXT_LEFT) &&
                settings->graphics_quality != SETTINGS_GRAPHICS_QUALITY_PERFORMANCE)
            {
                settings->graphics_quality = SETTINGS_GRAPHICS_QUALITY_PERFORMANCE;
                settings_mark_dirty(settings);
            }
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Color Scheme", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (nk_combo_begin_label(ctx, ui_settings_color_scheme_label(settings->color_scheme), nk_vec2(220.0f, 80.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (nk_combo_item_label(ctx, "Cyan Graph", NK_TEXT_LEFT) &&
                settings->color_scheme != SETTINGS_COLOR_SCHEME_CYAN_GRAPH)
            {
                settings->color_scheme = SETTINGS_COLOR_SCHEME_CYAN_GRAPH;
                settings_mark_dirty(settings);
            }
            if (nk_combo_item_label(ctx, "Solar Orchid", NK_TEXT_LEFT) &&
                settings->color_scheme != SETTINGS_COLOR_SCHEME_SOLAR_ORCHID)
            {
                settings->color_scheme = SETTINGS_COLOR_SCHEME_SOLAR_ORCHID;
                settings_mark_dirty(settings);
            }
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Camera Sensitivity", NK_TEXT_LEFT);

        float rotation = settings->camera_rotation_sensitivity;
        nk_property_float(ctx, "Orbit", 0.05f, &rotation, 5.0f, 0.05f, 0.01f);
        if (fabsf(rotation - settings->camera_rotation_sensitivity) > 0.0001f)
        {
            settings->camera_rotation_sensitivity = rotation;
            settings_mark_dirty(settings);
        }

        float pan_mouse = settings->camera_pan_sensitivity;
        nk_property_float(ctx, "Pan (mouse)", 0.05f, &pan_mouse, 10.0f, 0.05f, 0.01f);
        if (fabsf(pan_mouse - settings->camera_pan_sensitivity) > 0.0001f)
        {
            settings->camera_pan_sensitivity = pan_mouse;
            settings_mark_dirty(settings);
        }

        float pan_keyboard = settings->camera_keyboard_pan_sensitivity;
        nk_property_float(ctx, "Pan (keys)", 0.05f, &pan_keyboard, 10.0f, 0.05f, 0.01f);
        if (fabsf(pan_keyboard - settings->camera_keyboard_pan_sensitivity) > 0.0001f)
        {
            settings->camera_keyboard_pan_sensitivity = pan_keyboard;
            settings_mark_dirty(settings);
        }

        float zoom = settings->camera_zoom_sensitivity;
        nk_property_float(ctx, "Zoom", 0.05f, &zoom, 5.0f, 0.05f, 0.01f);
        if (fabsf(zoom - settings->camera_zoom_sensitivity) > 0.0001f)
        {
            settings->camera_zoom_sensitivity = zoom;
            settings_mark_dirty(settings);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Layout", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (nk_combo_begin_label(ctx, ui_settings_layout_label(settings->default_layout_algorithm),
                                 nk_vec2(220.0f, 70.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (nk_combo_item_label(ctx, "Hierarchical", NK_TEXT_LEFT) &&
                settings->default_layout_algorithm != SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL)
            {
                settings->default_layout_algorithm = SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL;
                settings_mark_dirty(settings);
            }
            if (nk_combo_item_label(ctx, "Force-directed (prototype)", NK_TEXT_LEFT) &&
                settings->default_layout_algorithm != SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED)
            {
                settings->default_layout_algorithm = SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED;
                settings_mark_dirty(settings);
            }
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Language", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (nk_combo_begin_label(ctx, ui_settings_language_label(settings->language), nk_vec2(220.0f, 70.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (nk_combo_item_label(ctx, "English", NK_TEXT_LEFT) && settings->language != SETTINGS_LANGUAGE_ENGLISH)
            {
                settings->language = SETTINGS_LANGUAGE_ENGLISH;
                settings_mark_dirty(settings);
            }
            if (nk_combo_item_label(ctx, "Future Tongue", NK_TEXT_LEFT) &&
                settings->language != SETTINGS_LANGUAGE_FUTURE)
            {
                settings->language = SETTINGS_LANGUAGE_FUTURE;
                settings_mark_dirty(settings);
            }
            nk_combo_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Auto-save", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_bool auto_save = settings->auto_save_enabled ? nk_true : nk_false;
        auto_save = nk_check_label(ctx, "Enable auto-save", auto_save);
        bool new_auto_save = (auto_save == nk_true);
        if (new_auto_save != settings->auto_save_enabled)
        {
            settings->auto_save_enabled = new_auto_save;
            settings_mark_dirty(settings);
        }

        int interval = (int)settings->auto_save_interval_seconds;
        nk_property_int(ctx, "Interval (sec)", 15, &interval, 3600, 15, 15);
        if (interval < 15)
        {
            interval = 15;
        }
        if ((unsigned int)interval != settings->auto_save_interval_seconds)
        {
            settings->auto_save_interval_seconds = (unsigned int)interval;
            settings_mark_dirty(settings);
        }

        nk_layout_row_dynamic(ctx, 12.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (nk_button_label(ctx, "Save to Disk"))
        {
            if (ui_event_enqueue(ui, UI_EVENT_SAVE_SETTINGS))
            {
                ui_internal_set_status(internal, "Settings save requested.");
            }
            else
            {
                ui_internal_set_status(internal, "Settings save aborted; event queue full.");
            }
        }
        if (nk_button_label(ctx, "Reload from Disk"))
        {
            if (ui_event_enqueue(ui, UI_EVENT_RELOAD_SETTINGS))
            {
                ui_internal_set_status(internal, "Settings reload requested.");
            }
            else
            {
                ui_internal_set_status(internal, "Settings reload aborted; event queue full.");
            }
        }

        /* Manual validation checklist:
         * 1. Adjust orbit/pan/zoom sliders and verify camera responsiveness changes after closing the window.
         * 2. Toggle graphics quality and confirm connection smoothing/portraits update per selection.
         * 3. Change auto-save interval and observe log output indicating the new cadence.
         */
    }
    else
    {
        internal->show_settings_window = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Settings##AncestryTree"))
    {
        internal->show_settings_window = false;
    }
}

static void ui_draw_exit_prompt(UIInternal *internal, UIContext *ui)
{
    if (!internal || !ui || !internal->show_exit_prompt)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float width = 320.0f;
    float height = 150.0f;
    float x = ((float)ui->width - width) * 0.5f;
    float y = ((float)ui->height - height) * 0.5f;
    struct nk_rect bounds = nk_rect(x, y, width, height);
    if (nk_begin(ctx, "Exit##Prompt", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Are you sure you want to exit?", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (nk_button_label(ctx, "Cancel"))
        {
            internal->show_exit_prompt = false;
        }
        if (nk_button_label(ctx, "Exit"))
        {
            internal->show_exit_prompt = false;
            if (ui_event_enqueue(ui, UI_EVENT_REQUEST_EXIT))
            {
                ui_internal_set_status(internal, "Exit requested.");
            }
            else
            {
                ui_internal_set_status(internal, "Event queue full; exit request failed.");
            }
        }
    }
    else
    {
        internal->show_exit_prompt = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Exit##Prompt"))
    {
        internal->show_exit_prompt = false;
    }
}

static void ui_draw_menu_bar(UIInternal *internal, UIContext *ui, const FamilyTree *tree, const LayoutResult *layout,
                             CameraController *camera, RenderConfig *render_config, bool settings_dirty)
{
    if (!internal || !ui)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    struct nk_rect bar_rect = nk_rect(0.0f, 0.0f, (float)ui->width, 30.0f);
    if (nk_begin(ctx, "MenuBar", bar_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND))
    {
        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 6);

        nk_layout_row_push(ctx, 60.0f);
        if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(200.0f, 220.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (nk_menu_item_label(ctx, "New Tree", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_NEW_TREE))
                {
                    ui_internal_set_status(internal, "New tree request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; new tree aborted.");
                }
            }
            if (nk_menu_item_label(ctx, "Open...", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_OPEN_TREE))
                {
                    ui_internal_set_status(internal, "Open request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; open aborted.");
                }
            }
            if (nk_menu_item_label(ctx, "Save", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_SAVE_TREE))
                {
                    ui_internal_set_status(internal, "Save request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; save aborted.");
                }
            }
            if (nk_menu_item_label(ctx, "Save As...", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_SAVE_TREE_AS))
                {
                    ui_internal_set_status(internal, "Save As request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; Save As aborted.");
                }
            }
            if (nk_menu_item_label(ctx, "Exit", NK_TEXT_LEFT))
            {
                internal->show_exit_prompt = true;
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 60.0f);
        if (nk_menu_begin_label(ctx, "Edit", NK_TEXT_LEFT, nk_vec2(160.0f, 180.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            nk_label(ctx, "Undo/Redo pipeline planned.", NK_TEXT_LEFT);
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 70.0f);
        if (nk_menu_begin_label(ctx, "View", NK_TEXT_LEFT, nk_vec2(220.0f, 200.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            const char *settings_label = settings_dirty ? "Settings... *" : "Settings...";
            if (nk_menu_item_label(ctx, settings_label, NK_TEXT_LEFT))
            {
                internal->show_settings_window = true;
            }
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (nk_menu_item_label(ctx, "Reset Camera", NK_TEXT_LEFT))
            {
                camera_controller_reset(camera);
                ui_internal_set_status(internal, "Camera reset to default orbit.");
            }
            if (nk_menu_item_label(ctx, "Focus Roots", NK_TEXT_LEFT))
            {
                float center[3];
                if (ui_compute_layout_center(layout, center))
                {
                    camera_controller_focus(camera, center, 14.0f);
                    ui_internal_set_status(internal, "Camera centered on root generation.");
                }
                else
                {
                    ui_internal_set_status(internal, "Layout center unavailable.");
                }
            }
            if (render_config)
            {
                nk_layout_row_dynamic(ctx, 20.0f, 1);
                nk_bool show_grid = render_config->show_grid ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Show grid", &show_grid);
                bool new_grid = (show_grid == nk_true);
                if (new_grid != render_config->show_grid)
                {
                    render_config->show_grid = new_grid;
                    ui_internal_set_status(internal, new_grid ? "Grid lines enabled." : "Grid lines hidden.");
                }

                nk_bool show_connections = render_config->show_connections ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Show connections", &show_connections);
                bool new_connections = (show_connections == nk_true);
                if (new_connections != render_config->show_connections)
                {
                    render_config->show_connections = new_connections;
                    ui_internal_set_status(internal,
                                           new_connections ? "Relationship lines enabled." : "Relationship lines hidden.");
                }

                nk_bool show_overlay = render_config->show_overlay ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Show holographic overlay", &show_overlay);
                bool new_overlay = (show_overlay == nk_true);
                if (new_overlay != render_config->show_overlay)
                {
                    render_config->show_overlay = new_overlay;
                    ui_internal_set_status(internal,
                                           new_overlay ? "Overlay enabled." : "Overlay hidden.");
                }

                nk_bool show_labels = render_config->show_name_panels ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Show name panels", &show_labels);
                bool new_labels = (show_labels == nk_true);
                if (new_labels != render_config->show_name_panels)
                {
                    render_config->show_name_panels = new_labels;
                    ui_internal_set_status(internal,
                                           new_labels ? "Holographic name panels enabled." : "Holographic name panels hidden.");
                }

                nk_bool show_profiles = render_config->show_profile_images ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Show profile images", &show_profiles);
                bool new_profiles = (show_profiles == nk_true);
                if (new_profiles != render_config->show_profile_images)
                {
                    render_config->show_profile_images = new_profiles;
                    ui_internal_set_status(internal,
                                           new_profiles ? "Portraits enabled in name panels." : "Portraits hidden from name panels.");
                }

                /* Manual validation checklist:
                 * 1. Toggle "Smooth connection lines" and confirm relationship links switch between crisp and
                 *    antialiased rendering without artifacts.
                 * 2. Switch parent/spouse connection styles and ensure the renderer updates immediately (straight vs.
                 *    bezier curves) while hover/selection highlights remain correct.
                 */
                nk_bool smooth_connections = render_config->connection_antialiasing ? nk_true : nk_false;
                nk_checkbox_label(ctx, "Smooth connection lines", &smooth_connections);
                bool new_smoothing = (smooth_connections == nk_true);
                if (new_smoothing != render_config->connection_antialiasing)
                {
                    render_config->connection_antialiasing = new_smoothing;
                    ui_internal_set_status(internal, new_smoothing ? "Smooth line rendering enabled." : "Smooth line rendering disabled.");
                }

                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "Parent connection style", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (nk_combo_begin_label(ctx, ui_connection_style_label(render_config->connection_style_parent_child),
                                         nk_vec2(180.0f, 70.0f)))
                {
                    nk_layout_row_dynamic(ctx, 20.0f, 1);
                    if (nk_combo_item_label(ctx, "Straight", NK_TEXT_LEFT) &&
                        render_config->connection_style_parent_child != RENDER_CONNECTION_STYLE_STRAIGHT)
                    {
                        render_config->connection_style_parent_child = RENDER_CONNECTION_STYLE_STRAIGHT;
                        ui_internal_set_status(internal, "Parent connections set to straight segments.");
                    }
                    if (nk_combo_item_label(ctx, "Bezier", NK_TEXT_LEFT) &&
                        render_config->connection_style_parent_child != RENDER_CONNECTION_STYLE_BEZIER)
                    {
                        render_config->connection_style_parent_child = RENDER_CONNECTION_STYLE_BEZIER;
                        ui_internal_set_status(internal, "Parent connections set to bezier curves.");
                    }
                    nk_combo_end(ctx);
                }

                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "Spouse connection style", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (nk_combo_begin_label(ctx, ui_connection_style_label(render_config->connection_style_spouse),
                                         nk_vec2(180.0f, 70.0f)))
                {
                    nk_layout_row_dynamic(ctx, 20.0f, 1);
                    if (nk_combo_item_label(ctx, "Straight", NK_TEXT_LEFT) &&
                        render_config->connection_style_spouse != RENDER_CONNECTION_STYLE_STRAIGHT)
                    {
                        render_config->connection_style_spouse = RENDER_CONNECTION_STYLE_STRAIGHT;
                        ui_internal_set_status(internal, "Spouse connections set to straight segments.");
                    }
                    if (nk_combo_item_label(ctx, "Bezier", NK_TEXT_LEFT) &&
                        render_config->connection_style_spouse != RENDER_CONNECTION_STYLE_BEZIER)
                    {
                        render_config->connection_style_spouse = RENDER_CONNECTION_STYLE_BEZIER;
                        ui_internal_set_status(internal, "Spouse connections set to bezier curves.");
                    }
                    nk_combo_end(ctx);
                }

                float font_control = render_config->name_panel_font_size;
                nk_property_float(ctx, "Name panel font", 16.0f, &font_control, 64.0f, 1.0f, 0.2f);
                if (fabsf(font_control - render_config->name_panel_font_size) > 0.05f)
                {
                    render_config->name_panel_font_size = font_control;
                    ui_internal_set_status(internal, "Name panel font size updated.");
                }
            }
            if (nk_menu_item_label(ctx, internal->auto_orbit ? "Disable Auto Orbit" : "Enable Auto Orbit",
                                   NK_TEXT_LEFT))
            {
                internal->auto_orbit = !internal->auto_orbit;
                ui_internal_set_status(internal, internal->auto_orbit ? "Auto orbit enabled." : "Auto orbit disabled.");
            }
            if (nk_menu_item_label(ctx, "Layout Options", NK_TEXT_LEFT))
            {
                ui_internal_set_status(internal, "Layout options UI under development.");
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 70.0f);
        if (nk_menu_begin_label(ctx, "Help", NK_TEXT_LEFT, nk_vec2(180.0f, 160.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (nk_menu_item_label(ctx, "About", NK_TEXT_LEFT))
            {
                internal->show_about_window = true;
            }
            if (nk_menu_item_label(ctx, "Quick Help", NK_TEXT_LEFT))
            {
                internal->show_help_window = true;
            }
            if (nk_menu_item_label(ctx, "Documentation", NK_TEXT_LEFT))
            {
                ui_internal_set_status(internal, "Documentation menu loads roadmap in future milestone.");
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_end(ctx);

        nk_menubar_end(ctx);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        if (tree && tree->name)
        {
            char label[128];
            (void)snprintf(label, sizeof(label), "Tree: %s", tree->name);
            nk_label(ctx, label, NK_TEXT_RIGHT);
        }
        else
        {
            nk_label(ctx, "Tree: (unnamed)", NK_TEXT_RIGHT);
        }
    }
    nk_end(ctx);
}

static void ui_process_input(UIInternal *internal, UIContext *ui, float delta_seconds)
{
    if (!internal)
    {
        return;
    }
    ui_internal_tick(internal, delta_seconds);
    struct nk_context *ctx = &internal->ctx;
    nk_input_begin(ctx);
    (void)ui;
    Vector2 mouse = GetMousePosition();
    nk_input_motion(ctx, (int)mouse.x, (int)mouse.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)mouse.x, (int)mouse.y, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)mouse.x, (int)mouse.y, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)mouse.x, (int)mouse.y, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    nk_input_scroll(ctx, nk_vec2(0.0f, GetMouseWheelMove()));

    nk_input_key(ctx, NK_KEY_DEL, IsKeyDown(KEY_DELETE));
    nk_input_key(ctx, NK_KEY_ENTER, IsKeyDown(KEY_ENTER));
    nk_input_key(ctx, NK_KEY_TAB, IsKeyDown(KEY_TAB));
    nk_input_key(ctx, NK_KEY_BACKSPACE, IsKeyDown(KEY_BACKSPACE));
    nk_input_key(ctx, NK_KEY_LEFT, IsKeyDown(KEY_LEFT));
    nk_input_key(ctx, NK_KEY_RIGHT, IsKeyDown(KEY_RIGHT));
    nk_input_key(ctx, NK_KEY_UP, IsKeyDown(KEY_UP));
    nk_input_key(ctx, NK_KEY_DOWN, IsKeyDown(KEY_DOWN));
    nk_input_key(ctx, NK_KEY_COPY, IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_C));
    nk_input_key(ctx, NK_KEY_PASTE, IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_V));
    nk_input_key(ctx, NK_KEY_CUT, IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_X));
    nk_input_key(ctx, NK_KEY_SHIFT, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    nk_input_key(ctx, NK_KEY_CTRL, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));

    int codepoint = GetCharPressed();
    while (codepoint > 0)
    {
        nk_input_unicode(ctx, codepoint);
        codepoint = GetCharPressed();
    }

    nk_input_end(ctx);
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB && ANCESTRYTREE_HAVE_NUKLEAR */

bool ui_init(UIContext *ui, int width, int height)
{
    if (!ui)
    {
        return false;
    }
    ui_event_queue_reset(&ui->event_queue);
    ui->available = false;
    ui->impl = NULL;
    ui->width = width;
    ui->height = height;

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = (UIInternal *)calloc(1, sizeof(UIInternal));
    if (!internal)
    {
        return false;
    }
    internal->rl_font = GetFontDefault();
    internal->font_size = (float)internal->rl_font.baseSize;

    internal->font.userdata = nk_handle_ptr(&internal->rl_font);
    internal->font.height = internal->font_size;
    internal->font.width = nk_raylib_text_width;
    internal->font.query = nk_raylib_query_font;
    internal->font.texture = nk_handle_ptr(NULL);
    internal->auto_orbit = false;

    if (!nk_init_default(&internal->ctx, &internal->font))
    {
        free(internal);
        return false;
    }
    nk_style_set_font(&internal->ctx, &internal->font);
    ui_apply_default_style(&internal->ctx);
    internal->ctx.clip.copy = ui_clipboard_copy;
    internal->ctx.clip.paste = ui_clipboard_paste;

    ui->impl = internal;
    ui->available = true;
    return true;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

void ui_resize(UIContext *ui, int width, int height)
{
    if (!ui)
    {
        return;
    }
    ui->width = width;
    ui->height = height;
}

void ui_cleanup(UIContext *ui)
{
    if (!ui)
    {
        return;
    }
    ui_event_queue_reset(&ui->event_queue);
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (internal)
    {
        nk_free(&internal->ctx);
        free(internal);
    }
#endif
    ui->impl = NULL;
    ui->available = false;
}

void ui_shutdown(UIContext *ui)
{
    ui_cleanup(ui);
}

bool ui_begin_frame(UIContext *ui, float delta_seconds)
{
    if (!ui)
    {
        return false;
    }
    if (!ui->available)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return false;
    }
    ui_process_input(internal, ui, delta_seconds);
    return true;
#else
    return false;
#endif
}

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)

static void ui_compose_person_name(const Person *person, char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    buffer[0] = '\0';
    if (!person)
    {
        (void)snprintf(buffer, capacity, "(none)");
        return;
    }

    size_t written = 0U;
    const char *parts[3] = {person->name.first, person->name.middle, person->name.last};
    for (size_t index = 0U; index < 3U; ++index)
    {
        const char *part = parts[index];
        if (!part || part[0] == '\0')
        {
            continue;
        }
        if (written > 0U)
        {
            int result = snprintf(buffer + written, capacity - written, " %s", part);
            if (result < 0)
            {
                buffer[written] = '\0';
                break;
            }
            written += (size_t)result;
        }
        else
        {
            int result = snprintf(buffer, capacity, "%s", part);
            if (result < 0)
            {
                buffer[0] = '\0';
                break;
            }
            written = (size_t)result;
        }
        if (written >= capacity)
        {
            buffer[capacity - 1U] = '\0';
            return;
        }
    }

    if (written == 0U)
    {
        (void)snprintf(buffer, capacity, "Person %u", person->id);
    }
}

static void ui_draw_hover_tooltip(UIInternal *internal, UIContext *ui, const Person *hovered)
{
    if (!internal || !ui || !hovered)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    Vector2 mouse = GetMousePosition();
    float width = 260.0f;
    float height = 120.0f;
    float x = mouse.x + 18.0f;
    float y = mouse.y + 18.0f;
    if (x + width > (float)ui->width)
    {
        x = (float)ui->width - width - 10.0f;
    }
    if (y + height > (float)ui->height)
    {
        y = (float)ui->height - height - 10.0f;
    }

    struct nk_rect bounds = nk_rect(x, y, width, height);
    if (nk_begin(ctx, "Tooltip##Person", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND))
    {
        char name[128];
        ui_compose_person_name(hovered, name, sizeof(name));
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, name, NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "ID: %u", hovered->id);
        const char *birth_date = hovered->dates.birth_date ? hovered->dates.birth_date : "Unknown";
        const char *birth_location = hovered->dates.birth_location ? hovered->dates.birth_location : "Unknown";
        nk_labelf(ctx, NK_TEXT_LEFT, "Born: %s", birth_date);
        nk_labelf(ctx, NK_TEXT_LEFT, "Birthplace: %s", birth_location);
        if (hovered->is_alive)
        {
            nk_label(ctx, "Status: Alive", NK_TEXT_LEFT);
        }
        else
        {
            const char *death_date = hovered->dates.death_date ? hovered->dates.death_date : "Unknown";
            const char *death_location = hovered->dates.death_location ? hovered->dates.death_location : "Unknown";
            nk_labelf(ctx, NK_TEXT_LEFT, "Died: %s", death_date);
            nk_labelf(ctx, NK_TEXT_LEFT, "Place of death: %s", death_location);
        }
    }
    nk_end(ctx);
}

static void ui_draw_tree_panel(UIInternal *internal, const FamilyTree *tree, const LayoutResult *layout,
                               CameraController *camera, float fps, const Person *selected_person,
                               const Person *hovered_person)
{
    struct nk_context *ctx = &internal->ctx;
    if (nk_begin(ctx, "HUD", nk_rect(18.0f, 20.0f, 320.0f, 220.0f),
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE))
    {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "AncestryTree Prototype", NK_TEXT_LEFT);
        nk_labelf(ctx, NK_TEXT_LEFT, "FPS: %.1f", fps);
        unsigned int person_count = tree ? (unsigned int)tree->person_count : 0U;
        nk_labelf(ctx, NK_TEXT_LEFT, "Persons: %u", person_count);

        char name_buffer[128];
        ui_compose_person_name(selected_person, name_buffer, sizeof(name_buffer));
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Selected: %s", selected_person ? name_buffer : "(none)");
        ui_compose_person_name(hovered_person, name_buffer, sizeof(name_buffer));
        nk_labelf(ctx, NK_TEXT_LEFT, "Hovered: %s", hovered_person ? name_buffer : "(none)");

        if (internal->status_timer > 0.0f && internal->status_message[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "Status: %s", internal->status_message);
        }

        nk_layout_row_dynamic(ctx, 24.0f, 2);
        if (nk_button_label(ctx, "Reset Camera"))
        {
            camera_controller_reset(camera);
            ui_internal_set_status(internal, "Camera reset to default orbit.");
        }
        if (nk_button_label(ctx, "Focus Roots"))
        {
            float center[3];
            if (ui_compute_layout_center(layout, center))
            {
                camera_controller_focus(camera, center, 14.0f);
                ui_internal_set_status(internal, "Camera centered on root generation.");
            }
            else
            {
                ui_internal_set_status(internal, "Layout center unavailable.");
            }
        }

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Camera Controls", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Right mouse: Orbit", NK_TEXT_LEFT);
        nk_label(ctx, "Middle mouse / WASD: Pan", NK_TEXT_LEFT);
        nk_label(ctx, "Mouse wheel: Zoom", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_bool orbit = internal->auto_orbit ? nk_true : nk_false;
        orbit = nk_check_label(ctx, "Auto orbit when idle", orbit);
        bool new_auto_orbit = (orbit == nk_true);
        if (internal->auto_orbit != new_auto_orbit)
        {
            internal->auto_orbit = new_auto_orbit;
            ui_internal_set_status(internal, new_auto_orbit ? "Auto orbit enabled." : "Auto orbit disabled.");
        }
    }
    nk_end(ctx);
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB && ANCESTRYTREE_HAVE_NUKLEAR */

void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout, CameraController *camera,
                     float fps, const Person *selected_person, const Person *hovered_person,
                     RenderConfig *render_config, Settings *settings, bool settings_dirty)
{
    if (!ui || !ui->available)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return;
    }
    ui_draw_menu_bar(internal, ui, tree, layout, camera, render_config, settings_dirty);
    ui_draw_tree_panel(internal, tree, layout, camera, fps, selected_person, hovered_person);
    ui_draw_about_window(internal, ui);
    ui_draw_help_window(internal, ui);
    ui_draw_settings_window(internal, ui, settings, settings_dirty);
    ui_draw_exit_prompt(internal, ui);
    if (hovered_person)
    {
        ui_draw_hover_tooltip(internal, ui, hovered_person);
    }
#else
    (void)tree;
    (void)layout;
    (void)camera;
    (void)fps;
    (void)selected_person;
    (void)hovered_person;
    (void)render_config;
    (void)settings;
    (void)settings_dirty;
#endif
}

void ui_end_frame(UIContext *ui)
{
    if (!ui || !ui->available)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (internal)
    {
        ui_internal_render(internal);
    }
#endif
}

bool ui_is_available(const UIContext *ui)
{
    return ui ? ui->available : false;
}

bool ui_auto_orbit_enabled(const UIContext *ui)
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    const UIInternal *internal = ui_internal_const_cast(ui);
    if (internal)
    {
        return internal->auto_orbit;
    }
#else
    (void)ui;
#endif
    return false;
}

bool ui_event_enqueue(UIContext *ui, UIEventType type)
{
    if (!ui)
    {
        return false;
    }
    return ui_event_queue_push(&ui->event_queue, type);
}

size_t ui_poll_events(UIContext *ui, UIEvent *events, size_t capacity)
{
    if (!ui || !events || capacity == 0U)
    {
        return 0U;
    }
    UIEventQueue *queue = &ui->event_queue;
    size_t count = queue->count;
    if (count == 0U)
    {
        return 0U;
    }
    if (count > capacity)
    {
        count = capacity;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        size_t event_index = (queue->head + index) % UI_EVENT_QUEUE_CAPACITY;
        events[index] = queue->events[event_index];
    }
    queue->head = (queue->head + count) % UI_EVENT_QUEUE_CAPACITY;
    queue->count -= count;
    return count;
}

bool ui_notify_status(UIContext *ui, const char *message)
{
    if (!ui)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (internal)
    {
        ui_internal_set_status(internal, message);
        return true;
    }
#else
    (void)message;
#endif
    return false;
}

bool ui_handle_escape(UIContext *ui)
{
    if (!ui || !ui->available)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return false;
    }
    bool dismissed = false;
    if (internal->show_exit_prompt)
    {
        internal->show_exit_prompt = false;
        dismissed = true;
    }
    if (internal->show_about_window)
    {
        internal->show_about_window = false;
        dismissed = true;
    }
    if (internal->show_help_window)
    {
        internal->show_help_window = false;
        dismissed = true;
    }
    if (internal->show_settings_window)
    {
        internal->show_settings_window = false;
        dismissed = true;
    }
    if (dismissed)
    {
        ui_internal_set_status(internal, "Dialogs dismissed.");
    }
    return dismissed;
#else
    return false;
#endif
}

/* Manual validation checklist (requires raylib + Nuklear):
 * 1. Launch the application and confirm the menu bar renders with File/View/Help entries.
 * 2. Trigger "Reset Camera" and "Focus Roots" from both the menu and HUD buttons; verify camera motion.
 * 3. Toggle auto orbit via the checkbox or menu and observe status banner updates.
 * 4. Open the About dialog from Help and close it via the button and window close control.
 * 5. Launch the Quick Help overlay, review shortcut descriptions, and close it with both the Close button and the Escape key.
 * 6. Select File->Exit and confirm the confirmation dialog responds and closes the application when accepted.
 * 7. Open View->Settings, tweak sensitivities and graphics quality, then save and reload to confirm persistence and runtime application.
 */
