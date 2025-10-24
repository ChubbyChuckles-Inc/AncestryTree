#include "ui_theme.h"

#include <math.h>
#include <string.h>

#if !defined(ANCESTRYTREE_HAVE_NUKLEAR) || !defined(ANCESTRYTREE_HAVE_RAYLIB)

void ui_theme_apply_holographic(struct nk_context *ctx, UIThemePalette *palette)
{
    (void)ctx;
    if (palette)
    {
        memset(palette, 0, sizeof(*palette));
        palette->panel_min_alpha  = 0.1f;
        palette->panel_show_speed = 4.0f;
        palette->panel_hide_speed = 3.0f;
    }
}

void ui_theme_apply_high_contrast(struct nk_context *ctx, UIThemePalette *palette)
{
    (void)ctx;
    if (palette)
    {
        memset(palette, 0, sizeof(*palette));
        palette->text_color        = (nk_color){255, 255, 255, 255};
        palette->window_background = (nk_color){16, 16, 16, 240};
        palette->window_border     = (nk_color){255, 255, 255, 255};
        palette->header_background = (nk_color){64, 64, 64, 255};
        palette->accent_color      = (nk_color){255, 200, 0, 255};
        palette->accent_hover      = (nk_color){255, 220, 0, 255};
        palette->accent_active     = (nk_color){255, 240, 0, 255};
        palette->warning_color     = (nk_color){255, 80, 80, 255};
        palette->panel_min_alpha   = 0.18f;
        palette->panel_show_speed  = 5.5f;
        palette->panel_hide_speed  = 4.2f;
    }
}

void ui_theme_push_panel_alpha(struct nk_context *ctx, const UIThemePalette *palette, float alpha,
                               UIThemePanelStyleToken *token)
{
    (void)ctx;
    (void)palette;
    if (token)
    {
        memset(token, 0, sizeof(*token));
    }
}

void ui_theme_pop_panel_alpha(struct nk_context *ctx, UIThemePanelStyleToken *token)
{
    (void)ctx;
    (void)token;
}

void ui_animated_panel_reset(UIAnimatedPanel *panel, bool visible)
{
    if (!panel)
    {
        return;
    }
    panel->progress   = visible ? 1.0f : 0.0f;
    panel->show_speed = 4.0f;
    panel->hide_speed = 3.0f;
    panel->min_alpha  = 0.1f;
}

void ui_animated_panel_update(UIAnimatedPanel *panel, bool target_visible, float delta_seconds)
{
    if (!panel || delta_seconds <= 0.0f)
    {
        return;
    }
    float speed     = target_visible ? panel->show_speed : panel->hide_speed;
    float direction = target_visible ? 1.0f : -1.0f;
    panel->progress += direction * speed * delta_seconds;
    if (panel->progress < 0.0f)
    {
        panel->progress = 0.0f;
    }
    if (panel->progress > 1.0f)
    {
        panel->progress = 1.0f;
    }
}

bool ui_animated_panel_visible(const UIAnimatedPanel *panel)
{
    return panel ? panel->progress > 0.001f : false;
}

float ui_animated_panel_alpha(const UIAnimatedPanel *panel)
{
    if (!panel)
    {
        return 0.0f;
    }
    float t = panel->progress;
    if (t <= 0.0f)
    {
        return 0.0f;
    }
    if (t >= 1.0f)
    {
        return 1.0f;
    }
    float eased = t * t * (3.0f - 2.0f * t);
    float base  = panel->min_alpha;
    if (base < 0.0f)
    {
        base = 0.0f;
    }
    if (base > 0.95f)
    {
        base = 0.95f;
    }
    float alpha = base + (1.0f - base) * eased;
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }
    return alpha;
}

#else

static struct nk_color ui_theme_color_with_alpha(struct nk_color color, float alpha,
                                                 float min_alpha)
{
    if (alpha < min_alpha)
    {
        alpha = min_alpha;
    }
    if (alpha < 0.0f)
    {
        alpha = 0.0f;
    }
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }
    struct nk_color result = color;
    float new_alpha        = (float)color.a * alpha;
    if (new_alpha < 0.0f)
    {
        new_alpha = 0.0f;
    }
    if (new_alpha > 255.0f)
    {
        new_alpha = 255.0f;
    }
    result.a = (nk_byte)(new_alpha + 0.5f);
    return result;
}

void ui_theme_apply_holographic(struct nk_context *ctx, UIThemePalette *palette)
{
    if (!ctx)
    {
        if (palette)
        {
            memset(palette, 0, sizeof(*palette));
        }
        return;
    }

    struct nk_color table[NK_COLOR_COUNT];
    memset(table, 0, sizeof(table));

    struct nk_color text          = nk_rgba(210, 240, 255, 255);
    struct nk_color window        = nk_rgba(8, 18, 34, 228);
    struct nk_color header        = nk_rgba(18, 60, 112, 255);
    struct nk_color header_hover  = nk_rgba(28, 82, 148, 255);
    struct nk_color header_active = nk_rgba(36, 104, 176, 255);
    struct nk_color border        = nk_rgba(0, 200, 255, 220);
    struct nk_color button        = nk_rgba(0, 170, 255, 230);
    struct nk_color button_hover  = nk_rgba(26, 190, 255, 240);
    struct nk_color button_active = nk_rgba(72, 210, 255, 255);
    struct nk_color warning       = nk_rgba(255, 120, 120, 255);

    table[NK_COLOR_TEXT]                    = text;
    table[NK_COLOR_WINDOW]                  = window;
    table[NK_COLOR_HEADER]                  = header;
    table[NK_COLOR_BORDER]                  = border;
    table[NK_COLOR_BUTTON]                  = button;
    table[NK_COLOR_BUTTON_HOVER]            = button_hover;
    table[NK_COLOR_BUTTON_ACTIVE]           = button_active;
    table[NK_COLOR_TOGGLE]                  = button;
    table[NK_COLOR_TOGGLE_HOVER]            = button_hover;
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(240, 248, 255, 255);
    table[NK_COLOR_SELECT]                  = button;
    table[NK_COLOR_SELECT_ACTIVE]           = button_active;
    table[NK_COLOR_SLIDER]                  = nk_rgba(24, 60, 102, 235);
    table[NK_COLOR_SLIDER_CURSOR]           = button_active;
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(120, 230, 255, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(160, 245, 255, 255);
    table[NK_COLOR_PROPERTY]                = nk_rgba(12, 34, 62, 235);
    table[NK_COLOR_EDIT]                    = nk_rgba(24, 52, 88, 235);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(0, 245, 255, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(12, 34, 62, 235);
    table[NK_COLOR_CHART]                   = button;
    table[NK_COLOR_CHART_COLOR]             = button_active;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(120, 240, 255, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(14, 38, 70, 200);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(32, 84, 128, 220);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = button_hover;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = button_active;
    table[NK_COLOR_TAB_HEADER]              = header;

    nk_style_from_table(ctx, table);

    ctx->style.window.background          = window;
    ctx->style.window.fixed_background    = nk_style_item_color(window);
    ctx->style.window.border_color        = border;
    ctx->style.window.header.normal       = nk_style_item_color(header);
    ctx->style.window.header.hover        = nk_style_item_color(header_hover);
    ctx->style.window.header.active       = nk_style_item_color(header_active);
    ctx->style.window.header.label_normal = text;
    ctx->style.window.header.label_hover  = text;
    ctx->style.window.header.label_active = text;
    ctx->style.window.padding             = nk_vec2(16.0f, 16.0f);
    ctx->style.window.spacing             = nk_vec2(10.0f, 8.0f);
    ctx->style.window.border              = 2.0f;
    ctx->style.window.rounding            = 10.0f;
    ctx->style.window.header.align        = NK_HEADER_RIGHT;
    ctx->style.window.header.padding      = nk_vec2(10.0f, 10.0f);

    ctx->style.button.rounding     = 6.0f;
    ctx->style.button.border       = 1.0f;
    ctx->style.button.padding      = nk_vec2(10.0f, 6.0f);
    ctx->style.button.normal       = nk_style_item_color(button);
    ctx->style.button.hover        = nk_style_item_color(button_hover);
    ctx->style.button.active       = nk_style_item_color(button_active);
    ctx->style.button.border_color = border;
    ctx->style.button.text_normal  = text;
    ctx->style.button.text_hover   = text;
    ctx->style.button.text_active  = text;

    ctx->style.tab.rounding = 6.0f;
    ctx->style.tab.border   = 1.0f;
    ctx->style.tab.padding  = nk_vec2(8.0f, 6.0f);

    ctx->style.checkbox.padding   = nk_vec2(4.0f, 4.0f);
    ctx->style.option.text_active = text;

    if (palette)
    {
        palette->text_color        = text;
        palette->window_background = window;
        palette->window_border     = border;
        palette->header_background = header;
        palette->accent_color      = button;
        palette->accent_hover      = button_hover;
        palette->accent_active     = button_active;
        palette->warning_color     = warning;
        palette->panel_min_alpha   = 0.12f;
        palette->panel_show_speed  = 6.0f;
        palette->panel_hide_speed  = 5.0f;
    }
}

void ui_theme_apply_high_contrast(struct nk_context *ctx, UIThemePalette *palette)
{
    if (!ctx)
    {
        if (palette)
        {
            memset(palette, 0, sizeof(*palette));
            palette->text_color        = nk_rgba(255, 255, 255, 255);
            palette->window_background = nk_rgba(16, 16, 16, 240);
            palette->window_border     = nk_rgba(255, 255, 255, 255);
            palette->header_background = nk_rgba(64, 64, 64, 255);
            palette->accent_color      = nk_rgba(255, 200, 0, 255);
            palette->accent_hover      = nk_rgba(255, 220, 0, 255);
            palette->accent_active     = nk_rgba(255, 240, 0, 255);
            palette->warning_color     = nk_rgba(255, 96, 96, 255);
            palette->panel_min_alpha   = 0.18f;
            palette->panel_show_speed  = 7.0f;
            palette->panel_hide_speed  = 5.5f;
        }
        return;
    }

    struct nk_color table[NK_COLOR_COUNT];
    memset(table, 0, sizeof(table));

    struct nk_color text          = nk_rgba(255, 255, 255, 255);
    struct nk_color window        = nk_rgba(16, 16, 16, 240);
    struct nk_color header        = nk_rgba(64, 64, 64, 255);
    struct nk_color header_hover  = nk_rgba(96, 96, 96, 255);
    struct nk_color header_active = nk_rgba(128, 128, 128, 255);
    struct nk_color border        = nk_rgba(255, 255, 255, 255);
    struct nk_color button        = nk_rgba(255, 200, 0, 255);
    struct nk_color button_hover  = nk_rgba(255, 220, 40, 255);
    struct nk_color button_active = nk_rgba(255, 240, 80, 255);
    struct nk_color warning       = nk_rgba(255, 96, 96, 255);

    table[NK_COLOR_TEXT]                    = text;
    table[NK_COLOR_WINDOW]                  = window;
    table[NK_COLOR_HEADER]                  = header;
    table[NK_COLOR_BORDER]                  = border;
    table[NK_COLOR_BUTTON]                  = button;
    table[NK_COLOR_BUTTON_HOVER]            = button_hover;
    table[NK_COLOR_BUTTON_ACTIVE]           = button_active;
    table[NK_COLOR_TOGGLE]                  = button;
    table[NK_COLOR_TOGGLE_HOVER]            = button_hover;
    table[NK_COLOR_TOGGLE_CURSOR]           = nk_rgba(0, 0, 0, 255);
    table[NK_COLOR_SELECT]                  = button;
    table[NK_COLOR_SELECT_ACTIVE]           = button_active;
    table[NK_COLOR_SLIDER]                  = nk_rgba(48, 48, 48, 255);
    table[NK_COLOR_SLIDER_CURSOR]           = button_active;
    table[NK_COLOR_SLIDER_CURSOR_HOVER]     = nk_rgba(255, 232, 96, 255);
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE]    = nk_rgba(255, 240, 128, 255);
    table[NK_COLOR_PROPERTY]                = nk_rgba(32, 32, 32, 240);
    table[NK_COLOR_EDIT]                    = nk_rgba(32, 32, 32, 240);
    table[NK_COLOR_EDIT_CURSOR]             = nk_rgba(255, 255, 255, 255);
    table[NK_COLOR_COMBO]                   = nk_rgba(32, 32, 32, 240);
    table[NK_COLOR_CHART]                   = button;
    table[NK_COLOR_CHART_COLOR]             = button_active;
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT]   = nk_rgba(255, 240, 80, 255);
    table[NK_COLOR_SCROLLBAR]               = nk_rgba(40, 40, 40, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR]        = nk_rgba(80, 80, 80, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]  = button_hover;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = button_active;
    table[NK_COLOR_TAB_HEADER]              = header;

    nk_style_from_table(ctx, table);

    ctx->style.window.background          = window;
    ctx->style.window.fixed_background    = nk_style_item_color(window);
    ctx->style.window.border_color        = border;
    ctx->style.window.header.normal       = nk_style_item_color(header);
    ctx->style.window.header.hover        = nk_style_item_color(header_hover);
    ctx->style.window.header.active       = nk_style_item_color(header_active);
    ctx->style.window.header.label_normal = text;
    ctx->style.window.header.label_hover  = text;
    ctx->style.window.header.label_active = text;
    ctx->style.window.padding             = nk_vec2(18.0f, 18.0f);
    ctx->style.window.spacing             = nk_vec2(12.0f, 10.0f);
    ctx->style.window.border              = 3.0f;
    ctx->style.window.rounding            = 8.0f;
    ctx->style.window.header.align        = NK_HEADER_LEFT;
    ctx->style.window.header.padding      = nk_vec2(12.0f, 12.0f);

    ctx->style.button.rounding     = 2.0f;
    ctx->style.button.border       = 2.0f;
    ctx->style.button.padding      = nk_vec2(10.0f, 8.0f);
    ctx->style.button.normal       = nk_style_item_color(button);
    ctx->style.button.hover        = nk_style_item_color(button_hover);
    ctx->style.button.active       = nk_style_item_color(button_active);
    ctx->style.button.border_color = border;
    ctx->style.button.text_normal  = nk_rgba(0, 0, 0, 255);
    ctx->style.button.text_hover   = nk_rgba(0, 0, 0, 255);
    ctx->style.button.text_active  = nk_rgba(0, 0, 0, 255);

    ctx->style.tab.rounding = 2.0f;
    ctx->style.tab.border   = 2.0f;
    ctx->style.tab.padding  = nk_vec2(10.0f, 8.0f);

    ctx->style.checkbox.padding   = nk_vec2(6.0f, 6.0f);
    ctx->style.option.text_active = text;

    if (palette)
    {
        palette->text_color        = text;
        palette->window_background = window;
        palette->window_border     = border;
        palette->header_background = header;
        palette->accent_color      = button;
        palette->accent_hover      = button_hover;
        palette->accent_active     = button_active;
        palette->warning_color     = warning;
        palette->panel_min_alpha   = 0.18f;
        palette->panel_show_speed  = 7.0f;
        palette->panel_hide_speed  = 5.5f;
    }
}

void ui_theme_push_panel_alpha(struct nk_context *ctx, const UIThemePalette *palette, float alpha,
                               UIThemePanelStyleToken *token)
{
    if (token)
    {
        memset(token, 0, sizeof(*token));
    }
    if (!ctx || !palette)
    {
        return;
    }
    float min_alpha = palette->panel_min_alpha;
    struct nk_color window =
        ui_theme_color_with_alpha(palette->window_background, alpha, min_alpha);
    struct nk_color border = ui_theme_color_with_alpha(palette->window_border, alpha, min_alpha);
    struct nk_color header =
        ui_theme_color_with_alpha(palette->header_background, alpha, min_alpha);

    if (nk_style_push_color(ctx, &ctx->style.window.background, window))
    {
        if (token)
        {
            token->window_background_pushed = true;
        }
    }
    if (ctx->style.window.fixed_background.type == NK_STYLE_ITEM_COLOR &&
        nk_style_push_color(ctx, &ctx->style.window.fixed_background.data.color, window))
    {
        if (token)
        {
            token->window_fixed_background_pushed = true;
        }
    }
    if (nk_style_push_color(ctx, &ctx->style.window.border_color, border))
    {
        if (token)
        {
            token->border_color_pushed = true;
        }
    }
    if (ctx->style.window.header.normal.type == NK_STYLE_ITEM_COLOR &&
        nk_style_push_color(ctx, &ctx->style.window.header.normal.data.color, header))
    {
        if (token)
        {
            token->header_normal_pushed = true;
        }
    }
    if (ctx->style.window.header.hover.type == NK_STYLE_ITEM_COLOR &&
        nk_style_push_color(ctx, &ctx->style.window.header.hover.data.color, header))
    {
        if (token)
        {
            token->header_hover_pushed = true;
        }
    }
    if (ctx->style.window.header.active.type == NK_STYLE_ITEM_COLOR &&
        nk_style_push_color(ctx, &ctx->style.window.header.active.data.color, header))
    {
        if (token)
        {
            token->header_active_pushed = true;
        }
    }
}

void ui_theme_pop_panel_alpha(struct nk_context *ctx, UIThemePanelStyleToken *token)
{
    if (!ctx || !token)
    {
        return;
    }
    if (token->header_active_pushed)
    {
        nk_style_pop_color(ctx);
    }
    if (token->header_hover_pushed)
    {
        nk_style_pop_color(ctx);
    }
    if (token->header_normal_pushed)
    {
        nk_style_pop_color(ctx);
    }
    if (token->border_color_pushed)
    {
        nk_style_pop_color(ctx);
    }
    if (token->window_fixed_background_pushed)
    {
        nk_style_pop_color(ctx);
    }
    if (token->window_background_pushed)
    {
        nk_style_pop_color(ctx);
    }
}

void ui_animated_panel_reset(UIAnimatedPanel *panel, bool visible)
{
    if (!panel)
    {
        return;
    }
    panel->progress   = visible ? 1.0f : 0.0f;
    panel->show_speed = 6.0f;
    panel->hide_speed = 5.0f;
    panel->min_alpha  = 0.12f;
}

void ui_animated_panel_update(UIAnimatedPanel *panel, bool target_visible, float delta_seconds)
{
    if (!panel || delta_seconds <= 0.0f)
    {
        return;
    }
    float speed     = target_visible ? panel->show_speed : panel->hide_speed;
    float direction = target_visible ? 1.0f : -1.0f;
    panel->progress += direction * speed * delta_seconds;
    if (panel->progress < 0.0f)
    {
        panel->progress = 0.0f;
    }
    if (panel->progress > 1.0f)
    {
        panel->progress = 1.0f;
    }
}

bool ui_animated_panel_visible(const UIAnimatedPanel *panel)
{
    return panel ? panel->progress > 0.001f : false;
}

float ui_animated_panel_alpha(const UIAnimatedPanel *panel)
{
    if (!panel)
    {
        return 0.0f;
    }
    float t = panel->progress;
    if (t <= 0.0f)
    {
        return 0.0f;
    }
    if (t >= 1.0f)
    {
        return 1.0f;
    }
    float eased = t * t * (3.0f - 2.0f * t);
    float base  = panel->min_alpha;
    if (base < 0.0f)
    {
        base = 0.0f;
    }
    if (base > 0.95f)
    {
        base = 0.95f;
    }
    float alpha = base + (1.0f - base) * eased;
    if (alpha > 1.0f)
    {
        alpha = 1.0f;
    }
    return alpha;
}

#endif
