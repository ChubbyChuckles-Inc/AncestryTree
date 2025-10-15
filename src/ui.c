#include "ui.h"

#include "camera_controller.h"
#include "graphics.h"
#include "layout.h"
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
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_IMPLEMENTATION
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
#endif
} UIInternal;

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

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)

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

static void ui_process_input(UIInternal *internal, UIContext *ui, float delta_seconds)
{
    struct nk_context *ctx = &internal->ctx;
    nk_input_begin(ctx);
    (void)delta_seconds;
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
    (void)ui;
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB && ANCESTRYTREE_HAVE_NUKLEAR */

bool ui_init(UIContext *ui, int width, int height)
{
    if (!ui)
    {
        return false;
    }
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

void ui_shutdown(UIContext *ui)
{
    if (!ui)
    {
        return;
    }
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

bool ui_begin_frame(UIContext *ui, float delta_seconds)
{
    (void)delta_seconds;
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

static void ui_draw_tree_panel(UIInternal *internal, const FamilyTree *tree, const LayoutResult *layout,
                               CameraController *camera, float fps)
{
    (void)layout;
    struct nk_context *ctx = &internal->ctx;
    if (nk_begin(ctx, "HUD", nk_rect(18.0f, 20.0f, 320.0f, 220.0f),
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE))
    {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "AncestryTree Prototype", NK_TEXT_LEFT);
        nk_labelf(ctx, NK_TEXT_LEFT, "FPS: %.1f", fps);
        unsigned int person_count = tree ? (unsigned int)tree->person_count : 0U;
        nk_labelf(ctx, NK_TEXT_LEFT, "Persons: %u", person_count);

        nk_layout_row_dynamic(ctx, 24.0f, 2);
        if (nk_button_label(ctx, "Reset Camera"))
        {
            camera_controller_reset(camera);
        }
        if (nk_button_label(ctx, "Focus Roots"))
        {
            if (layout && layout->count > 0)
            {
                float center[3] = {0.0f, 0.0f, 0.0f};
                for (size_t index = 0U; index < layout->count; ++index)
                {
                    center[0] += layout->nodes[index].position[0];
                    center[1] += layout->nodes[index].position[1];
                    center[2] += layout->nodes[index].position[2];
                }
                center[0] /= (float)layout->count;
                center[1] /= (float)layout->count;
                center[2] /= (float)layout->count;
                camera_controller_focus(camera, center, 14.0f);
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
        internal->auto_orbit = orbit == nk_true;
    }
    nk_end(ctx);
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB && ANCESTRYTREE_HAVE_NUKLEAR */

void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout, CameraController *camera,
                     float fps)
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
    ui_draw_tree_panel(internal, tree, layout, camera, fps);
#else
    (void)tree;
    (void)layout;
    (void)camera;
    (void)fps;
#endif
}

void ui_render(UIContext *ui)
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

/* Manual validation checklist (requires raylib + Nuklear):
 * 1. Launch the application and confirm the HUD window appears with stats and controls.
 * 2. Press the "Reset Camera" and "Focus Roots" buttons to ensure the camera recenters correctly.
 * 3. Toggle the auto orbit option and observe the tree rotating when idle.
 */
