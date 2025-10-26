#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdbool.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
#include "ui_nuklear_config.h"
#include "external/nuklear.h"
#else
typedef struct nk_color
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} nk_color;
#endif

struct nk_context;

typedef struct UIThemePalette
{
    struct nk_color text_color;
    struct nk_color window_background;
    struct nk_color window_border;
    struct nk_color header_background;
    struct nk_color accent_color;
    struct nk_color accent_hover;
    struct nk_color accent_active;
    struct nk_color warning_color;
    float panel_min_alpha;
    float panel_show_speed;
    float panel_hide_speed;
} UIThemePalette;

typedef struct UIThemePanelStyleToken
{
    bool window_background_pushed;
    bool window_fixed_background_pushed;
    bool border_color_pushed;
    bool header_normal_pushed;
    bool header_hover_pushed;
    bool header_active_pushed;
} UIThemePanelStyleToken;

typedef struct UIAnimatedPanel
{
    float progress;
    float show_speed;
    float hide_speed;
    float min_alpha;
} UIAnimatedPanel;

void ui_theme_apply_holographic(struct nk_context *ctx, UIThemePalette *palette);
void ui_theme_apply_high_contrast(struct nk_context *ctx, UIThemePalette *palette);
void ui_theme_push_panel_alpha(struct nk_context *ctx, const UIThemePalette *palette, float alpha,
                               UIThemePanelStyleToken *token);
void ui_theme_pop_panel_alpha(struct nk_context *ctx, UIThemePanelStyleToken *token);

void ui_animated_panel_reset(UIAnimatedPanel *panel, bool visible);
void ui_animated_panel_update(UIAnimatedPanel *panel, bool target_visible, float delta_seconds);
bool ui_animated_panel_visible(const UIAnimatedPanel *panel);
float ui_animated_panel_alpha(const UIAnimatedPanel *panel);

#endif /* UI_THEME_H */
