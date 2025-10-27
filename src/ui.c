#include "ui.h"

#include "at_date.h"
#include "at_memory.h"
#include "camera_controller.h"
#include "graphics.h"
#include "layout.h"
#include "onboarding.h"
#include "person.h"
#include "progress_tracker.h"
#include "render.h"
#include "screen_reader.h"
#include "search.h"
#include "settings.h"
#include "timeline.h"
#include "tree.h"
#include "tree_statistics.h"
#include "ui_icons.h"
#include "ui_navigation.h"
#include "ui_scaling.h"
#include "ui_theme.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ANCESTRYTREE_HAVE_RAYLIB)
#include <raylib.h>
#endif

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
#include "external/nuklear.h"
#include "ui_nuklear_config.h"
#endif

#define UI_SEARCH_MAX_RESULTS 64U

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
static const char *const UI_FONT_CANDIDATES[] = {"assets/fonts/AncestryTree-UI.ttf",
                                                 "assets/fonts/Roboto-Regular.ttf",
                                                 "assets/fonts/OpenSans-Regular.ttf"};
#endif

static void ui_compose_person_name(const Person *person, char *buffer, size_t capacity);

typedef struct UIScreenReaderState
{
    ScreenReaderChannel channel;
    bool active;
    uint32_t last_person_id;
    char last_status[SCREEN_READER_MAX_MESSAGE_LENGTH];
    char last_progress_label[SCREEN_READER_MAX_MESSAGE_LENGTH];
    bool progress_active;
    float last_progress_value;
} UIScreenReaderState;

typedef struct UIInternal
{
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    struct nk_context ctx;
    struct nk_user_font font;
    Font rl_font;
    bool font_owned;
    float font_size;
    bool scissor_active;
    bool auto_orbit;
    bool show_about_window;
    bool show_help_window;
    bool show_settings_window;
    bool show_search_panel;
    bool show_analytics_panel;
    bool show_add_person_panel;
    bool show_edit_person_panel;
    bool show_exit_prompt;
    bool show_error_dialog;
    UIThemePalette theme;
    UINavigationState nav_state;
    UINavigationInput nav_input;
    bool accessibility_high_contrast;
    float accessibility_font_scale;
    UIIconLibrary icons;
    bool icons_ready;
    UIAnimatedPanel about_panel_anim;
    UIAnimatedPanel help_panel_anim;
    UIAnimatedPanel settings_panel_anim;
    UIAnimatedPanel search_panel_anim;
    UIAnimatedPanel analytics_panel_anim;
    UIAnimatedPanel add_person_panel_anim;
    UIAnimatedPanel edit_person_panel_anim;
    UIAnimatedPanel exit_prompt_panel_anim;
    UIAnimatedPanel error_dialog_panel_anim;
    OnboardingState onboarding;
    ProgressTracker progress;
    float progress_target_value;
    bool progress_target_valid;
    float progress_spinner_phase;
    float frame_delta;
    char status_message[128];
    float status_timer;
    char error_dialog_title[64];
    char error_dialog_message[256];
    TreeStatistics analytics_stats;
    UIScreenReaderState screen_reader;
    char search_query[64];
    bool search_include_alive;
    bool search_include_deceased;
    bool search_use_birth_year_range;
    int search_birth_year_min;
    int search_birth_year_max;
    uint32_t search_results[UI_SEARCH_MAX_RESULTS];
    size_t search_result_count;
    int search_selected_index;
    bool search_dirty;
    const FamilyTree *search_last_tree;
    UIAddPersonRequest add_person_request;
    bool add_person_request_pending;
    struct
    {
        UIAddPersonRequest draft;
        TimelineEventType new_timeline_type;
        char new_timeline_date[32];
        char new_timeline_description[128];
        char new_timeline_location[96];
        char new_certificate_path[128];
        char error_message[192];
    } add_person_form;
    UIEditPersonRequest edit_person_request;
    bool edit_person_request_pending;
    struct
    {
        UIEditPersonRequest draft;
        char error_message[192];
        bool confirm_delete;
    } edit_person_form;
#else
    int unused;
#endif
} UIInternal;

static void ui_internal_set_status(UIInternal *internal, const char *message);
static void ui_accessibility_set_enabled(UIInternal *internal, bool enabled);

static void ui_event_queue_reset(UIEventQueue *queue)
{
    if (!queue)
    {
        return;
    }
    queue->head  = 0U;
    queue->count = 0U;
    memset(queue->events, 0, sizeof(queue->events));
}

static bool ui_event_queue_push(UIEventQueue *queue, const UIEvent *event)
{
    if (!queue || !event || event->type == UI_EVENT_NONE)
    {
        return false;
    }
    if (queue->count >= UI_EVENT_QUEUE_CAPACITY)
    {
        return false;
    }
    size_t index         = (queue->head + queue->count) % UI_EVENT_QUEUE_CAPACITY;
    queue->events[index] = *event;
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

static void ui_navigation_draw_focus_rect(struct nk_context *ctx, struct nk_rect bounds)
{
    if (!ctx)
    {
        return;
    }
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    if (!canvas)
    {
        return;
    }
    const float inset = 2.0f;
    struct nk_rect outline;
    outline.x = bounds.x - inset;
    outline.y = bounds.y - inset;
    outline.w = bounds.w + inset * 2.0f;
    outline.h = bounds.h + inset * 2.0f;
    nk_stroke_rect(canvas, outline, 2.0f, 2.0f, nk_rgba(255, 215, 0, 230));
}

static size_t ui_navigation_register(UIInternal *internal)
{
    if (!internal)
    {
        return SIZE_MAX;
    }
    return ui_navigation_register_item(&internal->nav_state);
}

static OnboardingHint ui_onboarding_hint_for_menu(const char *label)
{
    if (!label)
    {
        return ONBOARDING_HINT_NONE;
    }
    if (strcmp(label, "New Tree") == 0)
    {
        return ONBOARDING_HINT_MENU_NEW_TREE;
    }
    if (strcmp(label, "Open...") == 0)
    {
        return ONBOARDING_HINT_MENU_OPEN_TREE;
    }
    if (strcmp(label, "Save") == 0)
    {
        return ONBOARDING_HINT_MENU_SAVE;
    }
    if (strcmp(label, "Save As...") == 0)
    {
        return ONBOARDING_HINT_MENU_SAVE_AS;
    }
    if (strcmp(label, "Reset Camera") == 0)
    {
        return ONBOARDING_HINT_RESET_CAMERA;
    }
    if (strcmp(label, "Focus Roots") == 0)
    {
        return ONBOARDING_HINT_FOCUS_ROOTS;
    }
    return ONBOARDING_HINT_NONE;
}

static OnboardingHint ui_onboarding_hint_for_button(const char *label)
{
    return ui_onboarding_hint_for_menu(label);
}

static OnboardingHint ui_onboarding_hint_for_icon(const char *tooltip)
{
    if (!tooltip)
    {
        return ONBOARDING_HINT_NONE;
    }
    if (strcmp(tooltip, "Search / Filter") == 0)
    {
        return ONBOARDING_HINT_ICON_SEARCH;
    }
    if (strcmp(tooltip, "Add Person") == 0)
    {
        return ONBOARDING_HINT_ICON_ADD_PERSON;
    }
    if (strcmp(tooltip, "Settings") == 0)
    {
        return ONBOARDING_HINT_ICON_SETTINGS;
    }
    if (strcmp(tooltip, "Quick Help") == 0)
    {
        return ONBOARDING_HINT_ICON_HELP;
    }
    return ONBOARDING_HINT_NONE;
}

static void ui_onboarding_show_hint(UIInternal *internal, struct nk_context *ctx,
                                    OnboardingHint hint, struct nk_rect bounds)
{
    if (!internal || !ctx)
    {
        return;
    }
    if (!onboarding_hint_active(&internal->onboarding, hint))
    {
        return;
    }
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    if (canvas)
    {
        float phase          = internal->onboarding.elapsed_step * 4.5f;
        float waveform       = 0.5f + 0.5f * sinf(phase);
        nk_byte opacity      = (nk_byte)(160.0f + waveform * 80.0f);
        struct nk_color glow = nk_rgba(96, 220, 255, opacity);
        float rounding       = fminf(bounds.w, bounds.h) * 0.1f;
        nk_stroke_rect(canvas, bounds, rounding, 3.0f, glow);
    }
    if (!nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
    {
        return;
    }
    const char *text = onboarding_hint_text(hint);
    if (!text || text[0] == '\0')
    {
        return;
    }
    ui_internal_set_status(internal, text);
}

static void ui_onboarding_handle_action(UIInternal *internal, OnboardingHint hint)
{
    if (!internal)
    {
        return;
    }
    if (hint == ONBOARDING_HINT_NONE)
    {
        return;
    }
    onboarding_track_hint(&internal->onboarding, hint);
}

static void ui_draw_progress_spinner(UIInternal *internal, struct nk_context *ctx,
                                     struct nk_rect area)
{
    (void)internal;
    if (!ctx)
    {
        return;
    }
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    if (!canvas)
    {
        return;
    }
    float radius = fminf(area.w, area.h) * 0.5f - 1.0f;
    if (!(radius > 0.0f))
    {
        return;
    }
    float center_x   = area.x + area.w * 0.5f;
    float center_y   = area.y + area.h * 0.5f;
    const int arcs   = 12;
    const float step = (float)(2.0 * NK_PI) / (float)arcs;
    float phase      = internal ? internal->progress_spinner_phase : 0.0f;
    for (int index = 0; index < arcs; ++index)
    {
        float t                = phase + step * (float)index;
        float intensity        = (float)(arcs - index) / (float)arcs;
        float outer_x          = center_x + cosf(t) * radius;
        float outer_y          = center_y + sinf(t) * radius;
        float inner_x          = center_x + cosf(t) * radius * 0.55f;
        float inner_y          = center_y + sinf(t) * radius * 0.55f;
        struct nk_color colour = nk_rgba(96, 200, 255, (nk_byte)(110 + intensity * 120.0f));
        nk_stroke_line(canvas, inner_x, inner_y, outer_x, outer_y, 2.2f, colour);
    }
}

static bool ui_nav_button_label(UIInternal *internal, struct nk_context *ctx, const char *label)
{
    bool activated        = nk_button_label(ctx, label);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    if (!internal)
    {
        return activated;
    }
    size_t index = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (!activated && ui_navigation_consume_activation(&internal->nav_state))
        {
            activated = true;
        }
    }
    OnboardingHint button_hint = ui_onboarding_hint_for_button(label);
    ui_onboarding_show_hint(internal, ctx, button_hint, bounds);
    if (activated)
    {
        ui_onboarding_handle_action(internal, button_hint);
    }
    return activated;
}

static bool ui_nav_button_icon_label(UIInternal *internal, struct nk_context *ctx, UIIconType icon,
                                     const char *label)
{
    if (!ctx)
    {
        return false;
    }
    if (!internal)
    {
        return nk_button_label(ctx, label ? label : "");
    }
    struct nk_style_button backup    = ctx->style.button;
    ctx->style.button.text_alignment = NK_TEXT_LEFT;
    ctx->style.button.padding.x      = backup.padding.x + 20.0f;
    bool activated                   = nk_button_label(ctx, label ? label : "");
    struct nk_rect bounds            = nk_widget_bounds(ctx);
    bool focused                     = false;
    if (internal)
    {
        size_t index = ui_navigation_register(internal);
        focused      = ui_navigation_is_focused(&internal->nav_state, index);
        if (focused)
        {
            ui_navigation_draw_focus_rect(ctx, bounds);
            if (!activated && ui_navigation_consume_activation(&internal->nav_state))
            {
                activated = true;
            }
        }
    }
    ctx->style.button = backup;

    struct nk_rect icon_area = bounds;
    float icon_size          = fminf(bounds.h * 0.7f, 18.0f);
    icon_area.x += backup.padding.x + 6.0f;
    icon_area.y += (bounds.h - icon_size) * 0.5f;
    icon_area.w = icon_size;
    icon_area.h = icon_size;

    struct nk_color icon_color =
        focused ? internal->theme.accent_hover : internal->theme.accent_color;
    float intensity = activated ? 1.15f : 0.92f;
    bool drawn      = false;
    if (internal->icons_ready)
    {
        drawn = ui_icons_draw(&internal->icons, ctx, icon, icon_area, icon_color, intensity);
    }
    if (!drawn)
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        if (canvas)
        {
            nk_stroke_rect(canvas, icon_area, 3.0f, 2.0f, icon_color);
        }
    }
    OnboardingHint combo_hint = ui_onboarding_hint_for_button(label);
    ui_onboarding_show_hint(internal, ctx, combo_hint, bounds);
    if (activated)
    {
        ui_onboarding_handle_action(internal, combo_hint);
    }
    return activated;
}

static bool ui_nav_icon_button(UIInternal *internal, struct nk_context *ctx, UIIconType icon,
                               const char *tooltip, bool toggled)
{
    if (!internal || !ctx)
    {
        return false;
    }
    struct nk_style_button backup    = ctx->style.button;
    ctx->style.button.padding.x      = 4.0f;
    ctx->style.button.padding.y      = 4.0f;
    ctx->style.button.text_alignment = NK_TEXT_CENTERED;
    if (toggled)
    {
        struct nk_color active        = internal->theme.accent_active;
        struct nk_color hover         = internal->theme.accent_hover;
        ctx->style.button.normal      = nk_style_item_color(active);
        ctx->style.button.hover       = nk_style_item_color(hover);
        ctx->style.button.active      = nk_style_item_color(active);
        ctx->style.button.text_normal = internal->theme.window_background;
        ctx->style.button.text_hover  = internal->theme.window_background;
        ctx->style.button.text_active = internal->theme.window_background;
    }
    bool activated        = nk_button_label(ctx, "");
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    bool focused          = ui_navigation_is_focused(&internal->nav_state, index);
    if (focused)
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (!activated && ui_navigation_consume_activation(&internal->nav_state))
        {
            activated = true;
        }
    }
    ctx->style.button = backup;

    struct nk_color icon_color = internal->theme.accent_color;
    if (toggled)
    {
        icon_color = internal->theme.accent_active;
    }
    if (focused)
    {
        icon_color = internal->theme.accent_hover;
    }
    float intensity = toggled ? 1.12f : 1.0f;
    bool drawn      = false;
    if (internal->icons_ready)
    {
        drawn = ui_icons_draw(&internal->icons, ctx, icon, bounds, icon_color, intensity);
    }
    if (!drawn)
    {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        if (canvas)
        {
            nk_stroke_rect(canvas, bounds, 2.0f, 2.0f, icon_color);
        }
    }

    if (tooltip && tooltip[0] != '\0' && nk_input_is_mouse_hovering_rect(&ctx->input, bounds))
    {
        nk_tooltip(ctx, tooltip);
    }

    OnboardingHint icon_hint = ui_onboarding_hint_for_icon(tooltip);
    ui_onboarding_show_hint(internal, ctx, icon_hint, bounds);
    if (activated)
    {
        ui_onboarding_handle_action(internal, icon_hint);
    }

    return activated;
}

static nk_bool ui_nav_menu_item_label(UIInternal *internal, struct nk_context *ctx,
                                      const char *label, nk_flags alignment)
{
    nk_bool selected      = nk_menu_item_label(ctx, label, alignment);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (!selected && ui_navigation_consume_activation(&internal->nav_state))
        {
            selected = nk_true;
        }
    }
    OnboardingHint menu_hint = ui_onboarding_hint_for_menu(label);
    ui_onboarding_show_hint(internal, ctx, menu_hint, bounds);
    if (selected)
    {
        ui_onboarding_handle_action(internal, menu_hint);
    }
    return selected;
}

static nk_bool ui_nav_checkbox_label(UIInternal *internal, struct nk_context *ctx,
                                     const char *label, nk_bool *value)
{
    nk_bool changed       = nk_checkbox_label(ctx, label, value);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (ui_navigation_consume_activation(&internal->nav_state))
        {
            *value  = (*value == nk_true) ? nk_false : nk_true;
            changed = nk_true;
        }
    }
    return changed;
}

static bool ui_nav_combo_item_label(UIInternal *internal, struct nk_context *ctx, const char *label,
                                    nk_flags alignment)
{
    bool chosen           = nk_combo_item_label(ctx, label, alignment);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (!chosen && ui_navigation_consume_activation(&internal->nav_state))
        {
            chosen = true;
        }
    }
    return chosen;
}

static nk_flags ui_nav_edit_string(UIInternal *internal, struct nk_context *ctx, nk_flags flags,
                                   char *buffer, int capacity, nk_plugin_filter filter)
{
    nk_flags result       = nk_edit_string_zero_terminated(ctx, flags, buffer, capacity, filter);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if ((result & NK_EDIT_ACTIVE) == 0)
        {
            nk_edit_focus(ctx, 0);
        }
    }
    return result;
}

static void ui_nav_property_int(UIInternal *internal, struct nk_context *ctx, const char *label,
                                int min_value, int *value, int max_value, int step,
                                float inc_per_pixel, const char *status_message)
{
    if (!internal || !ctx || !value)
    {
        nk_property_int(ctx, label, min_value, value, max_value, step, inc_per_pixel);
        return;
    }
    int previous = *value;
    nk_property_int(ctx, label, min_value, value, max_value, step, inc_per_pixel);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (internal->nav_input.arrow_up_pressed || internal->nav_input.arrow_right_pressed)
        {
            if (*value < max_value)
            {
                *value += step;
                if (*value > max_value)
                {
                    *value = max_value;
                }
            }
        }
        if (internal->nav_input.arrow_down_pressed || internal->nav_input.arrow_left_pressed)
        {
            if (*value > min_value)
            {
                *value -= step;
                if (*value < min_value)
                {
                    *value = min_value;
                }
            }
        }
    }
    if (*value != previous && status_message)
    {
        ui_internal_set_status(internal, status_message);
    }
}

static void ui_nav_property_float(UIInternal *internal, struct nk_context *ctx, const char *label,
                                  float min_value, float *value, float max_value, float step,
                                  float inc_per_pixel, float keyboard_step,
                                  const char *status_message)
{
    if (!internal || !ctx || !value)
    {
        nk_property_float(ctx, label, min_value, value, max_value, step, inc_per_pixel);
        return;
    }
    float previous = *value;
    nk_property_float(ctx, label, min_value, value, max_value, step, inc_per_pixel);
    struct nk_rect bounds = nk_widget_bounds(ctx);
    size_t index          = ui_navigation_register(internal);
    if (ui_navigation_is_focused(&internal->nav_state, index))
    {
        ui_navigation_draw_focus_rect(ctx, bounds);
        if (internal->nav_input.arrow_up_pressed || internal->nav_input.arrow_right_pressed)
        {
            *value += keyboard_step;
            if (*value > max_value)
            {
                *value = max_value;
            }
        }
        if (internal->nav_input.arrow_down_pressed || internal->nav_input.arrow_left_pressed)
        {
            *value -= keyboard_step;
            if (*value < min_value)
            {
                *value = min_value;
            }
        }
    }
    if ((*value != previous) && status_message)
    {
        ui_internal_set_status(internal, status_message);
    }
}

static nk_bool ui_nav_combo_begin_label(UIInternal *internal, struct nk_context *ctx,
                                        const char *selected, struct nk_vec2 size)
{
    if (!internal || !ctx || !selected)
    {
        return nk_false;
    }

    struct nk_rect predicted = nk_layout_widget_bounds(ctx);
    size_t nav_index         = ui_navigation_register(internal);
    const bool nav_focus     = ui_navigation_is_focused(&internal->nav_state, nav_index);

    bool simulate_click = false;
    if (nav_focus && ui_navigation_consume_activation(&internal->nav_state))
    {
        simulate_click = true;
    }

    struct nk_input saved_input;
    if (simulate_click)
    {
        saved_input = ctx->input;
        int click_x = (int)(predicted.x + (predicted.w * 0.5f));
        int click_y = (int)(predicted.y + (predicted.h * 0.5f));
        nk_input_button(ctx, NK_BUTTON_LEFT, click_x, click_y, nk_true);
        nk_input_button(ctx, NK_BUTTON_LEFT, click_x, click_y, nk_false);
    }

    nk_bool opened        = nk_combo_begin_label(ctx, selected, size);
    struct nk_rect header = nk_widget_bounds(ctx);

    if (simulate_click)
    {
        ctx->input = saved_input;
    }

    if (nav_focus)
    {
        ui_navigation_draw_focus_rect(ctx, header);
    }

    return opened;
}

static void ui_compose_person_name(const Person *person, char *buffer, size_t capacity);

static void ui_add_person_form_reset(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    memset(&internal->add_person_form.draft, 0, sizeof(UIAddPersonRequest));
    internal->add_person_form.draft.is_alive              = true;
    internal->add_person_form.new_timeline_type           = TIMELINE_EVENT_CUSTOM;
    internal->add_person_form.new_timeline_date[0]        = '\0';
    internal->add_person_form.new_timeline_description[0] = '\0';
    internal->add_person_form.new_timeline_location[0]    = '\0';
    internal->add_person_form.new_certificate_path[0]     = '\0';
    internal->add_person_form.error_message[0]            = '\0';
}

static void ui_add_person_set_error(UIInternal *internal, const char *message)
{
    if (!internal)
    {
        return;
    }
    if (message)
    {
        (void)snprintf(internal->add_person_form.error_message,
                       sizeof(internal->add_person_form.error_message), "%s", message);
    }
    else
    {
        internal->add_person_form.error_message[0] = '\0';
    }
}

static void ui_edit_person_form_reset(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    memset(&internal->edit_person_form.draft, 0, sizeof(UIEditPersonRequest));
    internal->edit_person_form.draft.update_father  = true;
    internal->edit_person_form.draft.update_mother  = true;
    internal->edit_person_form.draft.update_spouses = true;
    internal->edit_person_form.draft.has_death      = false;
    internal->edit_person_form.error_message[0]     = '\0';
    internal->edit_person_form.confirm_delete       = false;
}

static void ui_edit_person_set_error(UIInternal *internal, const char *message)
{
    if (!internal)
    {
        return;
    }
    if (message)
    {
        (void)snprintf(internal->edit_person_form.error_message,
                       sizeof(internal->edit_person_form.error_message), "%s", message);
    }
    else
    {
        internal->edit_person_form.error_message[0] = '\0';
    }
}

static void ui_edit_person_remove_spouse(UIEditPersonRequest *request, size_t index)
{
    if (!request || index >= request->spouse_count)
    {
        return;
    }
    for (size_t shift = index + 1U; shift < request->spouse_count; ++shift)
    {
        request->spouse_ids[shift - 1U] = request->spouse_ids[shift];
    }
    request->spouse_count -= 1U;
}

static const char *ui_timeline_type_label(TimelineEventType type)
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
        return "Custom";
    }
}

static void ui_add_person_remove_timeline_entry(UIAddPersonRequest *request, size_t index)
{
    if (!request || index >= request->timeline_count)
    {
        return;
    }
    for (size_t shift = index + 1U; shift < request->timeline_count; ++shift)
    {
        request->timeline_entries[shift - 1U] = request->timeline_entries[shift];
    }
    request->timeline_count -= 1U;
}

static void ui_add_person_remove_certificate(UIAddPersonRequest *request, size_t index)
{
    if (!request || index >= request->certificate_count)
    {
        return;
    }
    for (size_t shift = index + 1U; shift < request->certificate_count; ++shift)
    {
        memcpy(request->certificate_paths[shift - 1U], request->certificate_paths[shift],
               sizeof(request->certificate_paths[shift]));
    }
    request->certificate_count -= 1U;
}

static bool ui_add_person_validate(UIInternal *internal, const FamilyTree *tree, char *message,
                                   size_t message_capacity)
{
    if (!internal)
    {
        return false;
    }
    UIAddPersonRequest *draft = &internal->add_person_form.draft;
    if (draft->first[0] == '\0' || draft->last[0] == '\0')
    {
        (void)snprintf(message, message_capacity, "First and last names are required.");
        return false;
    }
    if (draft->birth_date[0] == '\0' || !at_date_is_valid_iso8601(draft->birth_date))
    {
        (void)snprintf(message, message_capacity, "Enter a valid birth date (YYYY-MM-DD).");
        return false;
    }
    if (draft->is_alive)
    {
        if (draft->death_date[0] != '\0')
        {
            (void)snprintf(message, message_capacity, "Alive persons must not have a death date.");
            return false;
        }
        draft->death_location[0] = '\0';
    }
    else
    {
        if (draft->death_date[0] == '\0' || !at_date_is_valid_iso8601(draft->death_date))
        {
            (void)snprintf(message, message_capacity, "Enter a valid death date (YYYY-MM-DD).");
            return false;
        }
    }
    for (size_t index = 0U; index < draft->timeline_count; ++index)
    {
        UIAddPersonTimelineItem *item = &draft->timeline_entries[index];
        if (item->description[0] == '\0')
        {
            (void)snprintf(message, message_capacity, "Timeline entries require a description.");
            return false;
        }
        if (item->date[0] != '\0' && !at_date_is_valid_iso8601(item->date))
        {
            (void)snprintf(message, message_capacity, "Timeline entry %zu has an invalid date.",
                           index + 1U);
            return false;
        }
    }
    for (size_t index = 0U; index < draft->certificate_count; ++index)
    {
        if (draft->certificate_paths[index][0] == '\0')
        {
            (void)snprintf(message, message_capacity, "Certificate paths cannot be empty.");
            return false;
        }
    }
    if (tree)
    {
        if (draft->father_id != 0U && !family_tree_find_person(tree, draft->father_id))
        {
            (void)snprintf(message, message_capacity, "Father reference is invalid.");
            return false;
        }
        if (draft->mother_id != 0U && !family_tree_find_person(tree, draft->mother_id))
        {
            (void)snprintf(message, message_capacity, "Mother reference is invalid.");
            return false;
        }
        if (draft->spouse_id != 0U && !family_tree_find_person(tree, draft->spouse_id))
        {
            (void)snprintf(message, message_capacity, "Spouse reference is invalid.");
            return false;
        }
    }
    return true;
}

static bool ui_edit_person_validate(UIInternal *internal, const FamilyTree *tree, char *message,
                                    size_t message_capacity)
{
    if (!internal)
    {
        return false;
    }
    UIEditPersonRequest *draft = &internal->edit_person_form.draft;
    if (draft->person_id == 0U)
    {
        (void)snprintf(message, message_capacity, "No person selected for editing.");
        return false;
    }
    if (draft->first[0] == '\0' || draft->last[0] == '\0')
    {
        (void)snprintf(message, message_capacity, "First and last names are required.");
        return false;
    }
    if (draft->birth_date[0] == '\0' || !at_date_is_valid_iso8601(draft->birth_date))
    {
        (void)snprintf(message, message_capacity, "Enter a valid birth date (YYYY-MM-DD).");
        return false;
    }
    if (draft->has_death)
    {
        if (draft->death_date[0] == '\0')
        {
            (void)snprintf(message, message_capacity, "Deceased profiles require a death date.");
            return false;
        }
        if (!at_date_is_valid_iso8601(draft->death_date))
        {
            (void)snprintf(message, message_capacity, "Enter a valid death date (YYYY-MM-DD).");
            return false;
        }
    }
    else
    {
        draft->death_date[0]     = '\0';
        draft->death_location[0] = '\0';
    }

    if (draft->update_father)
    {
        if (draft->father_id == draft->person_id)
        {
            (void)snprintf(message, message_capacity, "A person cannot be their own father.");
            return false;
        }
        if (draft->father_id != 0U && tree && !family_tree_find_person(tree, draft->father_id))
        {
            (void)snprintf(message, message_capacity,
                           "Selected father no longer exists in the tree.");
            return false;
        }
    }
    if (draft->update_mother)
    {
        if (draft->mother_id == draft->person_id)
        {
            (void)snprintf(message, message_capacity, "A person cannot be their own mother.");
            return false;
        }
        if (draft->mother_id != 0U && tree && !family_tree_find_person(tree, draft->mother_id))
        {
            (void)snprintf(message, message_capacity,
                           "Selected mother no longer exists in the tree.");
            return false;
        }
    }

    if (draft->update_spouses)
    {
        if (draft->spouse_count > UI_EDIT_PERSON_MAX_SPOUSES)
        {
            (void)snprintf(message, message_capacity, "Spouse limit exceeded.");
            return false;
        }
        for (size_t outer = 0U; outer < draft->spouse_count; ++outer)
        {
            uint32_t spouse_id = draft->spouse_ids[outer];
            if (spouse_id == 0U)
            {
                continue;
            }
            if (spouse_id == draft->person_id)
            {
                (void)snprintf(message, message_capacity,
                               "A person cannot be listed as their own spouse.");
                return false;
            }
            if (tree && !family_tree_find_person(tree, spouse_id))
            {
                (void)snprintf(message, message_capacity,
                               "Spouse selection %u is not present in the tree.", spouse_id);
                return false;
            }
            for (size_t inner = outer + 1U; inner < draft->spouse_count; ++inner)
            {
                if (spouse_id != 0U && spouse_id == draft->spouse_ids[inner])
                {
                    (void)snprintf(message, message_capacity,
                                   "Duplicate spouse selections detected.");
                    return false;
                }
            }
        }
    }

    return true;
}

static void ui_edit_person_populate(UIInternal *internal, const Person *person)
{
    if (!internal)
    {
        return;
    }
    ui_edit_person_form_reset(internal);
    if (!person)
    {
        ui_edit_person_set_error(internal, "Select a person to edit.");
        internal->show_edit_person_panel = false;
        return;
    }

    UIEditPersonRequest *draft = &internal->edit_person_form.draft;
    draft->person_id           = person->id;

    if (person->name.first)
    {
        (void)snprintf(draft->first, sizeof(draft->first), "%s", person->name.first);
    }
    if (person->name.middle)
    {
        (void)snprintf(draft->middle, sizeof(draft->middle), "%s", person->name.middle);
    }
    if (person->name.last)
    {
        (void)snprintf(draft->last, sizeof(draft->last), "%s", person->name.last);
    }

    if (person->dates.birth_date)
    {
        (void)snprintf(draft->birth_date, sizeof(draft->birth_date), "%s",
                       person->dates.birth_date);
    }
    if (person->dates.birth_location)
    {
        (void)snprintf(draft->birth_location, sizeof(draft->birth_location), "%s",
                       person->dates.birth_location);
    }

    if (!person->is_alive && person->dates.death_date)
    {
        draft->has_death = true;
        (void)snprintf(draft->death_date, sizeof(draft->death_date), "%s",
                       person->dates.death_date);
        if (person->dates.death_location)
        {
            (void)snprintf(draft->death_location, sizeof(draft->death_location), "%s",
                           person->dates.death_location);
        }
    }
    else
    {
        draft->has_death         = false;
        draft->death_date[0]     = '\0';
        draft->death_location[0] = '\0';
    }

    draft->update_father = true;
    draft->father_id     = (person->parents[PERSON_PARENT_FATHER] != NULL)
                               ? person->parents[PERSON_PARENT_FATHER]->id
                               : 0U;
    draft->update_mother = true;
    draft->mother_id     = (person->parents[PERSON_PARENT_MOTHER] != NULL)
                               ? person->parents[PERSON_PARENT_MOTHER]->id
                               : 0U;

    draft->update_spouses = true;
    draft->spouse_count   = 0U;
    size_t limit          = person->spouses_count;
    if (limit > UI_EDIT_PERSON_MAX_SPOUSES)
    {
        limit = UI_EDIT_PERSON_MAX_SPOUSES;
    }
    for (size_t index = 0U; index < limit; ++index)
    {
        PersonSpouseRecord *record = &person->spouses[index];
        if (!record || !record->partner)
        {
            continue;
        }
        draft->spouse_ids[draft->spouse_count++] = record->partner->id;
    }

    ui_edit_person_set_error(internal, NULL);
    internal->show_edit_person_panel = true;
}

static const char *ui_edit_person_relationship_label(const FamilyTree *tree, uint32_t relation_id,
                                                     char *buffer, size_t capacity)
{
    if (relation_id == 0U)
    {
        return "None";
    }
    if (!tree)
    {
        (void)snprintf(buffer, capacity, "(#%u)", relation_id);
        return buffer;
    }
    Person *relation = family_tree_find_person(tree, relation_id);
    if (!relation)
    {
        (void)snprintf(buffer, capacity, "Missing (#%u)", relation_id);
        return buffer;
    }
    char name_buffer[128];
    ui_compose_person_name(relation, name_buffer, sizeof(name_buffer));
    (void)snprintf(buffer, capacity, "%s (#%u)", name_buffer, relation->id);
    return buffer;
}

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
        buffer = (char *)AT_MALLOC((size_t)len + 1U);
        if (!buffer)
        {
            return 0.0f;
        }
    }
    memcpy(buffer, text, (size_t)len);
    buffer[len]  = '\0';
    Vector2 size = MeasureTextEx(*font, buffer, height, 0.0f);
    if (buffer != stack_buffer)
    {
        AT_FREE(buffer);
    }
    return size.x;
}

static void nk_raylib_query_font(nk_handle handle, float height, struct nk_user_font_glyph *glyph,
                                 nk_rune codepoint, nk_rune next_codepoint)
{
    (void)next_codepoint;
    Font *font = (Font *)handle.ptr;
    char buffer[8];
    int byte_count   = 0;
    const char *utf8 = CodepointToUTF8((int)codepoint, &byte_count);
    if (!utf8 || byte_count <= 0 || byte_count >= (int)sizeof(buffer))
    {
        buffer[0]  = (char)codepoint;
        byte_count = 1;
    }
    else
    {
        memcpy(buffer, utf8, (size_t)byte_count);
    }
    buffer[byte_count] = '\0';
    Vector2 size    = font ? MeasureTextEx(*font, buffer, height, 0.0f) : (Vector2){height, height};
    glyph->width    = size.x;
    glyph->height   = height;
    glyph->offset   = nk_vec2(0.0f, 0.0f);
    glyph->xadvance = size.x;
    glyph->uv[0]    = nk_vec2(0.0f, 0.0f);
    glyph->uv[1]    = nk_vec2(0.0f, 0.0f);
}

static void ui_internal_configure_panel(UIInternal *internal, UIAnimatedPanel *panel, bool visible)
{
    if (!internal || !panel)
    {
        return;
    }
    ui_animated_panel_reset(panel, visible);
    panel->show_speed = internal->theme.panel_show_speed;
    panel->hide_speed = internal->theme.panel_hide_speed;
    panel->min_alpha  = internal->theme.panel_min_alpha;
}

static void ui_internal_refresh_panel_speeds(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    UIAnimatedPanel *panels[] = {
        &internal->about_panel_anim,       &internal->help_panel_anim,
        &internal->settings_panel_anim,    &internal->search_panel_anim,
        &internal->analytics_panel_anim,   &internal->add_person_panel_anim,
        &internal->edit_person_panel_anim, &internal->exit_prompt_panel_anim,
        &internal->error_dialog_panel_anim};
    size_t count = sizeof(panels) / sizeof(panels[0]);
    for (size_t index = 0U; index < count; ++index)
    {
        UIAnimatedPanel *panel = panels[index];
        if (!panel)
        {
            continue;
        }
        panel->show_speed = internal->theme.panel_show_speed;
        panel->hide_speed = internal->theme.panel_hide_speed;
        panel->min_alpha  = internal->theme.panel_min_alpha;
    }
}

static void ui_internal_reset_panels(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    UIAnimatedPanel *panels[] = {
        &internal->about_panel_anim,       &internal->help_panel_anim,
        &internal->settings_panel_anim,    &internal->search_panel_anim,
        &internal->analytics_panel_anim,   &internal->add_person_panel_anim,
        &internal->edit_person_panel_anim, &internal->exit_prompt_panel_anim,
        &internal->error_dialog_panel_anim};
    const bool visibility[] = {internal->show_about_window,      internal->show_help_window,
                               internal->show_settings_window,   internal->show_search_panel,
                               internal->show_analytics_panel,   internal->show_add_person_panel,
                               internal->show_edit_person_panel, internal->show_exit_prompt,
                               internal->show_error_dialog};
    size_t count            = sizeof(panels) / sizeof(panels[0]);
    for (size_t index = 0U; index < count; ++index)
    {
        ui_internal_configure_panel(internal, panels[index], visibility[index]);
    }
}

static void ui_internal_apply_theme(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    if (internal->accessibility_high_contrast)
    {
        ui_theme_apply_high_contrast(&internal->ctx, &internal->theme);
    }
    else
    {
        ui_theme_apply_holographic(&internal->ctx, &internal->theme);
    }

    ui_internal_reset_panels(internal);
}

static float ui_internal_compute_scaled_font_size(const UIContext *ui, float scale)
{
    float base_size = 20.0f;
    if (ui)
    {
        base_size = ui_scaling_default_font_size(ui->height);
        if (!(base_size > 0.0f))
        {
            base_size = 20.0f;
        }
    }
    if (!(scale > 0.0f))
    {
        scale = 1.0f;
    }
    if (scale < 0.6f)
    {
        scale = 0.6f;
    }
    if (scale > 1.8f)
    {
        scale = 1.8f;
    }
    float font_size = base_size * scale;
    if (font_size < 16.0f)
    {
        font_size = 16.0f;
    }
    if (font_size > 48.0f)
    {
        font_size = 48.0f;
    }
    return font_size;
}

static void ui_internal_bind_font(UIInternal *internal, bool update_context)
{
    if (!internal)
    {
        return;
    }
    internal->font.userdata = nk_handle_ptr(&internal->rl_font);
    internal->font.height   = internal->font_size;
    internal->font.width    = nk_raylib_text_width;
    internal->font.query    = nk_raylib_query_font;
    internal->font.texture  = nk_handle_ptr(NULL);
    if (update_context)
    {
        nk_style_set_font(&internal->ctx, &internal->font);
    }
}

static void ui_internal_acquire_font(UIInternal *internal, float font_size)
{
    if (!internal)
    {
        return;
    }
    if (!(font_size > 0.0f))
    {
        font_size = 20.0f;
    }

    Font chosen       = GetFontDefault();
    bool chosen_owned = false;

    for (size_t index = 0U; index < sizeof(UI_FONT_CANDIDATES) / sizeof(UI_FONT_CANDIDATES[0]);
         ++index)
    {
        const char *candidate = UI_FONT_CANDIDATES[index];
        if (!candidate || candidate[0] == '\0')
        {
            continue;
        }
        if (!FileExists(candidate))
        {
            continue;
        }
        Font loaded = LoadFontEx(candidate, (int)ceilf(font_size), NULL, 0);
        if (loaded.texture.id != 0)
        {
            chosen       = loaded;
            chosen_owned = true;
            SetTextureFilter(chosen.texture, TEXTURE_FILTER_BILINEAR);
            break;
        }
    }

    if (internal->font_owned)
    {
        UnloadFont(internal->rl_font);
    }

    internal->rl_font    = chosen;
    internal->font_owned = chosen_owned;
    internal->font_size  = font_size;
    ui_internal_bind_font(internal, false);
}

static void ui_internal_sync_font(UIInternal *internal, const UIContext *ui, float scale)
{
    if (!internal)
    {
        return;
    }
    float clamped_scale = scale;
    if (!(clamped_scale > 0.0f))
    {
        clamped_scale = 1.0f;
    }
    if (clamped_scale < 0.6f)
    {
        clamped_scale = 0.6f;
    }
    if (clamped_scale > 1.8f)
    {
        clamped_scale = 1.8f;
    }

    float target_size = ui_internal_compute_scaled_font_size(ui, clamped_scale);
    if (!(target_size > 0.0f))
    {
        target_size = 20.0f;
    }

    bool needs_reload = fabsf(target_size - internal->font_size) >= 0.1f;
    if (needs_reload)
    {
        ui_internal_acquire_font(internal, target_size);
        ui_internal_bind_font(internal, true);
    }
    else
    {
        internal->font_size = target_size;
        ui_internal_bind_font(internal, true);
    }

    internal->accessibility_font_scale = clamped_scale;
}

static void ui_internal_sync_accessibility(UIInternal *internal, UIContext *ui,
                                           const Settings *settings)
{
    if (!internal)
    {
        return;
    }
    bool desired_high_contrast = settings ? settings->high_contrast_mode : false;
    float desired_scale        = settings ? settings->ui_font_scale : 1.0f;

    if (internal->accessibility_high_contrast != desired_high_contrast)
    {
        internal->accessibility_high_contrast = desired_high_contrast;
        ui_internal_apply_theme(internal);
    }

    if (fabsf(desired_scale - internal->accessibility_font_scale) > 0.01f)
    {
        ui_internal_sync_font(internal, ui, desired_scale);
    }

    bool desired_screen_reader = settings ? settings->screen_reader_enabled : false;
    if (internal->screen_reader.active != desired_screen_reader)
    {
        ui_accessibility_set_enabled(internal, desired_screen_reader);
    }
}

static void ui_clipboard_copy(nk_handle usr, const char *text, int len)
{
    (void)usr;
    if (!text || len <= 0)
    {
        return;
    }
    char *buffer = (char *)AT_MALLOC((size_t)len + 1U);
    if (!buffer)
    {
        return;
    }
    memcpy(buffer, text, (size_t)len);
    buffer[len] = '\0';
    SetClipboardText(buffer);
    AT_FREE(buffer);
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
            Vector2 start                      = {(float)line->begin.x, (float)line->begin.y};
            Vector2 end                        = {(float)line->end.x, (float)line->end.y};
            DrawLineEx(start, end, (float)line->line_thickness, nk_color_to_raylib(line->color));
        }
        break;
        case NK_COMMAND_RECT:
        {
            const struct nk_command_rect *rect = (const struct nk_command_rect *)cmd;
            Rectangle bounds = {(float)rect->x, (float)rect->y, (float)rect->w, (float)rect->h};
            if (rect->rounding > 0)
            {
                DrawRectangleRoundedLines(bounds, rect->rounding / bounds.width, 6,
                                          rect->line_thickness, nk_color_to_raylib(rect->color));
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
                DrawRectangleRounded(bounds, rect->rounding / bounds.width, 6,
                                     nk_color_to_raylib(rect->color));
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
            const struct nk_user_font *font    = text->font;
            float height                       = font ? font->height : internal->font_size;
            Font draw_font                     = internal->rl_font;
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
                buffer = (char *)AT_MALLOC((size_t)length + 1U);
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
                AT_FREE(buffer);
            }
        }
        break;
        case NK_COMMAND_IMAGE:
        {
            const struct nk_command_image *img_cmd = (const struct nk_command_image *)cmd;
            if (!img_cmd->img.handle.ptr)
            {
                break;
            }
            Texture2D *texture = (Texture2D *)img_cmd->img.handle.ptr;
            if (!texture || texture->id == 0)
            {
                break;
            }
            Rectangle source;
            source.x      = (float)img_cmd->img.region[0];
            source.y      = (float)img_cmd->img.region[1];
            source.width  = (float)img_cmd->img.region[2];
            source.height = (float)img_cmd->img.region[3];
            if (source.width == 0.0f || source.height == 0.0f)
            {
                source.x      = 0.0f;
                source.y      = 0.0f;
                source.width  = (float)texture->width;
                source.height = (float)texture->height;
            }
            Rectangle dest = {(float)img_cmd->x, (float)img_cmd->y, (float)img_cmd->w,
                              (float)img_cmd->h};
            Color tint     = nk_color_to_raylib(img_cmd->col);
            DrawTexturePro(*texture, source, dest, (Vector2){0.0f, 0.0f}, 0.0f, tint);
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

static void ui_accessibility_reset(UIScreenReaderState *state)
{
    if (!state)
    {
        return;
    }
    screen_reader_init(&state->channel);
    screen_reader_set_log_path(&state->channel, SCREEN_READER_DEFAULT_PATH);
    state->active                 = false;
    state->last_person_id         = 0U;
    state->last_status[0]         = '\0';
    state->last_progress_label[0] = '\0';
    state->progress_active        = false;
    state->last_progress_value    = -1.0f;
}

static void ui_accessibility_set_enabled(UIInternal *internal, bool enabled)
{
    if (!internal)
    {
        return;
    }
    UIScreenReaderState *state = &internal->screen_reader;
    screen_reader_set_enabled(&state->channel, enabled);
    state->active = enabled;
    if (!enabled)
    {
        state->last_person_id         = 0U;
        state->last_status[0]         = '\0';
        state->last_progress_label[0] = '\0';
        state->progress_active        = false;
        state->last_progress_value    = -1.0f;
    }
    else
    {
        screen_reader_announce(&state->channel, "Screen reader narration enabled.");
    }
}

static void ui_accessibility_announce(UIInternal *internal, const char *message)
{
    if (!internal || !message || message[0] == '\0')
    {
        return;
    }
    UIScreenReaderState *state = &internal->screen_reader;
    if (!state->active)
    {
        return;
    }
    screen_reader_announce(&state->channel, message);
}

static void ui_accessibility_emit_status(UIInternal *internal, const char *message)
{
    UIScreenReaderState *state = &internal->screen_reader;
    if (!state->active)
    {
        state->last_status[0] = '\0';
        return;
    }
    if (!message || message[0] == '\0')
    {
        state->last_status[0] = '\0';
        return;
    }
    if (strncmp(state->last_status, message, sizeof(state->last_status)) == 0)
    {
        return;
    }
    screen_reader_announce(&state->channel, message);
#if defined(_MSC_VER)
    (void)strncpy_s(state->last_status, sizeof(state->last_status), message, _TRUNCATE);
#else
    (void)snprintf(state->last_status, sizeof(state->last_status), "%s", message);
#endif
}

static void ui_accessibility_progress_begin(UIInternal *internal, const char *label)
{
    UIScreenReaderState *state = &internal->screen_reader;
    if (!state->active)
    {
        state->progress_active     = false;
        state->last_progress_value = -1.0f;
        return;
    }
    state->progress_active     = true;
    state->last_progress_value = 0.0f;
    const char *spoken         = label && label[0] != '\0' ? label : "Operation in progress.";
    screen_reader_announce(&state->channel, spoken);
#if defined(_MSC_VER)
    (void)strncpy_s(state->last_progress_label, sizeof(state->last_progress_label), spoken,
                    _TRUNCATE);
#else
    (void)snprintf(state->last_progress_label, sizeof(state->last_progress_label), "%s", spoken);
#endif
}

static void ui_accessibility_progress_update(UIInternal *internal, float value)
{
    UIScreenReaderState *state = &internal->screen_reader;
    if (!state->active || !state->progress_active)
    {
        return;
    }
    if (!(value >= 0.0f))
    {
        value = 0.0f;
    }
    if (value > 1.0f)
    {
        value = 1.0f;
    }
    const float announce_step = 0.25f;
    if (value < 0.999f && value < state->last_progress_value + announce_step)
    {
        return;
    }
    state->last_progress_value = value;
    int percent                = (int)(value * 100.0f + 0.5f);
    char buffer[128];
    if (state->last_progress_label[0] != '\0')
    {
        (void)snprintf(buffer, sizeof(buffer), "%s: %d%% complete.", state->last_progress_label,
                       percent);
    }
    else
    {
        (void)snprintf(buffer, sizeof(buffer), "Operation %d%% complete.", percent);
    }
    screen_reader_announce(&state->channel, buffer);
}

static void ui_accessibility_progress_complete(UIInternal *internal, bool success,
                                               const char *label)
{
    UIScreenReaderState *state = &internal->screen_reader;
    if (!state->active)
    {
        state->progress_active     = false;
        state->last_progress_value = -1.0f;
        return;
    }
    char buffer[160];
    if (label && label[0] != '\0')
    {
        (void)snprintf(buffer, sizeof(buffer), "%s", label);
    }
    else
    {
        (void)snprintf(buffer, sizeof(buffer), "Operation %s.", success ? "completed" : "failed");
    }
    screen_reader_announce(&state->channel, buffer);
    state->progress_active        = false;
    state->last_progress_value    = -1.0f;
    state->last_progress_label[0] = '\0';
}

static void ui_accessibility_report_selection(UIInternal *internal, const Person *selected_person)
{
    UIScreenReaderState *state = &internal->screen_reader;
    uint32_t person_id         = selected_person ? selected_person->id : 0U;
    if (!state->active)
    {
        state->last_person_id = person_id;
        return;
    }
    if (state->last_person_id == person_id)
    {
        return;
    }
    state->last_person_id = person_id;
    if (!selected_person)
    {
        screen_reader_announce(&state->channel, "Selection cleared.");
        return;
    }
    char name[128];
    ui_compose_person_name(selected_person, name, sizeof(name));
    const char *life_state = selected_person->is_alive ? "alive" : "deceased";
    char announcement[196];
    size_t timeline_count = selected_person->timeline_count;
    if (timeline_count > 0U)
    {
        (void)snprintf(announcement, sizeof(announcement),
                       "%s selected, %s with %zu timeline events.", name, life_state,
                       timeline_count);
    }
    else
    {
        (void)snprintf(announcement, sizeof(announcement), "%s selected, %s.", name, life_state);
    }
    screen_reader_announce(&state->channel, announcement);
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
        internal->status_timer      = 0.0f;
        ui_accessibility_emit_status(internal, NULL);
        return;
    }
    (void)snprintf(internal->status_message, sizeof(internal->status_message), "%s", message);
    internal->status_timer = 4.0f;
    ui_accessibility_emit_status(internal, message);
}

static void ui_internal_show_error(UIInternal *internal, const char *title, const char *message)
{
    if (!internal)
    {
        return;
    }
    const char *resolved_title = (title && title[0] != '\0') ? title : "Error";
    const char *resolved_message =
        (message && message[0] != '\0') ? message : "An unexpected error occurred.";
    (void)snprintf(internal->error_dialog_title, sizeof(internal->error_dialog_title), "%s",
                   resolved_title);
    (void)snprintf(internal->error_dialog_message, sizeof(internal->error_dialog_message), "%s",
                   resolved_message);
    internal->show_error_dialog = true;
    ui_internal_set_status(internal, resolved_message);
    char spoken[SCREEN_READER_MAX_MESSAGE_LENGTH];
    (void)snprintf(spoken, sizeof(spoken), "Error: %s", resolved_message);
    ui_accessibility_announce(internal, spoken);
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
            internal->status_timer      = 0.0f;
            internal->status_message[0] = '\0';
        }
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    struct PanelUpdate
    {
        UIAnimatedPanel *panel;
        bool visible;
    };
    const struct PanelUpdate updates[] = {
        {&internal->about_panel_anim, internal->show_about_window},
        {&internal->help_panel_anim, internal->show_help_window},
        {&internal->settings_panel_anim, internal->show_settings_window},
        {&internal->search_panel_anim, internal->show_search_panel},
        {&internal->analytics_panel_anim, internal->show_analytics_panel},
        {&internal->add_person_panel_anim, internal->show_add_person_panel},
        {&internal->edit_person_panel_anim, internal->show_edit_person_panel},
        {&internal->exit_prompt_panel_anim, internal->show_exit_prompt},
        {&internal->error_dialog_panel_anim, internal->show_error_dialog}};
    /* Manual QA: Toggle overlays from the menu bar and confirm fade timings feel responsive at 60
     * FPS. */
    ui_internal_refresh_panel_speeds(internal);
    for (size_t index = 0U; index < sizeof(updates) / sizeof(updates[0]); ++index)
    {
        UIAnimatedPanel *panel = updates[index].panel;
        if (!panel)
        {
            continue;
        }
        ui_animated_panel_update(panel, updates[index].visible, delta_seconds);
    }
#endif
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

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
static void ui_search_clear_results(UIInternal *internal)
{
    if (!internal)
    {
        return;
    }
    internal->search_result_count   = 0U;
    internal->search_selected_index = -1;
    memset(internal->search_results, 0, sizeof(internal->search_results));
}

static void ui_search_refresh(UIInternal *internal, const FamilyTree *tree)
{
    if (!internal)
    {
        return;
    }
    if (!tree)
    {
        ui_search_clear_results(internal);
        internal->search_last_tree = NULL;
        internal->search_dirty     = false;
        return;
    }

    const Person *matches[UI_SEARCH_MAX_RESULTS];
    SearchFilter filter;
    filter.name_substring   = (internal->search_query[0] != '\0') ? internal->search_query : NULL;
    filter.include_alive    = internal->search_include_alive;
    filter.include_deceased = internal->search_include_deceased;
    filter.use_birth_year_range = internal->search_use_birth_year_range;
    filter.birth_year_min       = internal->search_birth_year_min;
    filter.birth_year_max       = internal->search_birth_year_max;

    size_t count = search_execute(tree, &filter, matches, UI_SEARCH_MAX_RESULTS);
    if (count > UI_SEARCH_MAX_RESULTS)
    {
        count = UI_SEARCH_MAX_RESULTS;
    }
    for (size_t index = 0U; index < count; ++index)
    {
        const Person *person            = matches[index];
        internal->search_results[index] = person ? person->id : 0U;
    }
    if (count < UI_SEARCH_MAX_RESULTS)
    {
        memset(&internal->search_results[count], 0,
               (UI_SEARCH_MAX_RESULTS - count) * sizeof(internal->search_results[0]));
    }
    internal->search_result_count = count;
    if (internal->search_selected_index >= (int)count)
    {
        internal->search_selected_index = (count == 0U) ? -1 : (int)(count - 1);
    }
    if (count == 0U)
    {
        internal->search_selected_index = -1;
    }
    internal->search_last_tree = tree;
    internal->search_dirty     = false;
}
#endif

static void ui_draw_about_window(UIInternal *internal, const UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    bool active =
        internal->show_about_window || ui_animated_panel_visible(&internal->about_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->about_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width           = fminf(360.0f, (float)ui->width * 0.6f);
    float height          = 240.0f;
    float x               = ((float)ui->width - width) * 0.5f;
    struct nk_rect bounds = nk_rect(x, 64.0f, width, height);
    if (nk_begin(ctx, "About##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Futuristic holographic ancestry explorer.", NK_TEXT_LEFT);
        nk_label(ctx, "Current focus: rendering pipeline + UI overlays.", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Raylib + Nuklear integration in progress.", NK_TEXT_LEFT);
        nk_label(ctx, "Credits: ChubbyChuckles Labs & open-source collaborators.", NK_TEXT_LEFT);
        nk_label(ctx, "License: MIT (see bundled LICENSE file).", NK_TEXT_LEFT);
        nk_label(ctx, "Documentation: README.md and docs/ directory.", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (ui_nav_button_label(internal, ctx, "Close"))
        {
            internal->show_about_window = false;
        }
        if (ui_nav_button_label(internal, ctx, "Project Page"))
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
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_help_window(UIInternal *internal, const UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    bool active =
        internal->show_help_window || ui_animated_panel_visible(&internal->help_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->help_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width           = fminf(420.0f, (float)ui->width * 0.75f);
    float height          = fminf(320.0f, (float)ui->height * 0.75f);
    float x               = ((float)ui->width - width) * 0.5f;
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
        nk_label(ctx,
                 "If overlays disappear, ensure View > Show holographic overlay remains enabled.",
                 NK_TEXT_LEFT);
        nk_label(ctx,
                 "Use Focus Roots after loading large trees to re-center the camera instantly.",
                 NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 30.0f, 1);
        if (ui_nav_button_label(internal, ctx, "Close"))
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
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_search_panel(UIInternal *internal, UIContext *ui, const FamilyTree *tree)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_search_panel;
    bool active  = showing || ui_animated_panel_visible(&internal->search_panel_anim);
    if (!active)
    {
        return;
    }

    if (showing && internal->search_last_tree != tree)
    {
        internal->search_last_tree      = tree;
        internal->search_dirty          = true;
        internal->search_selected_index = -1;
        ui_search_clear_results(internal);
    }

    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->search_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width = fminf(460.0f, (float)ui->width - 40.0f);
    if (width < 320.0f)
    {
        width = 320.0f;
    }
    float height = fminf(540.0f, (float)ui->height - 60.0f);
    if (height < 360.0f)
    {
        height = 360.0f;
    }
    float x = (float)ui->width - width - 20.0f;
    if (x < 10.0f)
    {
        x = 10.0f;
    }
    struct nk_rect bounds = nk_rect(x, 70.0f, width, height);

    if (nk_begin(ctx, "Search##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                     NK_WINDOW_CLOSABLE))
    {
        bool request_refresh = false;
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "Locate persons in the holographic tree", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Name contains", NK_TEXT_LEFT);
        char previous_query[sizeof(internal->search_query)];
        memcpy(previous_query, internal->search_query, sizeof(previous_query));
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, internal->search_query,
                                 (int)sizeof(internal->search_query), nk_filter_default);
        if (strncmp(previous_query, internal->search_query, sizeof(previous_query)) != 0)
        {
            request_refresh = true;
        }

        nk_layout_row_dynamic(ctx, 24.0f, 2);
        nk_bool include_alive    = internal->search_include_alive ? nk_true : nk_false;
        nk_bool include_deceased = internal->search_include_deceased ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "Include alive", &include_alive);
        (void)ui_nav_checkbox_label(internal, ctx, "Include deceased", &include_deceased);
        bool new_include_alive    = (include_alive == nk_true);
        bool new_include_deceased = (include_deceased == nk_true);
        if (!new_include_alive && !new_include_deceased)
        {
            new_include_alive    = true;
            new_include_deceased = true;
        }
        if (new_include_alive != internal->search_include_alive ||
            new_include_deceased != internal->search_include_deceased)
        {
            internal->search_include_alive    = new_include_alive;
            internal->search_include_deceased = new_include_deceased;
            request_refresh                   = true;
        }

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_bool range_enabled = internal->search_use_birth_year_range ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "Filter by birth year", &range_enabled);
        bool new_range = (range_enabled == nk_true);
        if (new_range != internal->search_use_birth_year_range)
        {
            internal->search_use_birth_year_range = new_range;
            request_refresh                       = true;
        }

        if (internal->search_use_birth_year_range)
        {
            int min_year = internal->search_birth_year_min;
            int max_year = internal->search_birth_year_max;
            nk_layout_row_dynamic(ctx, 24.0f, 2);
            ui_nav_property_int(internal, ctx, "From", 1200, &min_year, 2500, 1, 5.0f,
                                "Birth year filter adjusted.");
            ui_nav_property_int(internal, ctx, "To", 1200, &max_year, 2500, 1, 5.0f,
                                "Birth year filter adjusted.");
            if (min_year > max_year)
            {
                int swap = min_year;
                min_year = max_year;
                max_year = swap;
            }
            if (min_year != internal->search_birth_year_min ||
                max_year != internal->search_birth_year_max)
            {
                internal->search_birth_year_min = min_year;
                internal->search_birth_year_max = max_year;
                request_refresh                 = true;
            }
        }

        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (ui_nav_button_label(internal, ctx, "Search"))
        {
            request_refresh = true;
        }
        if (ui_nav_button_label(internal, ctx, "Clear Filters"))
        {
            internal->search_query[0]             = '\0';
            internal->search_include_alive        = true;
            internal->search_include_deceased     = true;
            internal->search_use_birth_year_range = false;
            internal->search_birth_year_min       = 1900;
            internal->search_birth_year_max       = 2100;
            internal->search_selected_index       = -1;
            ui_search_clear_results(internal);
            request_refresh = true;
        }

        if (request_refresh)
        {
            internal->search_dirty = true;
        }
        if (internal->search_dirty)
        {
            ui_search_refresh(internal, tree);
        }

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Results: %u", (unsigned int)internal->search_result_count);

        float results_height = height - 230.0f;
        if (results_height < 160.0f)
        {
            results_height = 160.0f;
        }
        nk_layout_row_dynamic(ctx, results_height, 1);
        if (nk_group_begin(ctx, "SearchResultsGroup", NK_WINDOW_BORDER))
        {
            for (size_t index = 0U; index < internal->search_result_count; ++index)
            {
                uint32_t person_id = internal->search_results[index];
                Person *person     = tree ? family_tree_find_person(tree, person_id) : NULL;
                char name_buffer[128];
                if (!person ||
                    !person_format_display_name(person, name_buffer, sizeof(name_buffer)))
                {
                    (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person_id);
                }

                nk_layout_row_dynamic(ctx, 26.0f, 2);
                nk_bool selected =
                    (internal->search_selected_index == (int)index) ? nk_true : nk_false;
                nk_selectable_label(ctx, name_buffer, NK_TEXT_LEFT, &selected);
                int new_selection = (selected == nk_true) ? (int)index : -1;
                if (new_selection != internal->search_selected_index)
                {
                    internal->search_selected_index = new_selection;
                }

                if (ui_nav_button_label(internal, ctx, "Focus"))
                {
                    if (ui_event_enqueue_with_u32(ui, UI_EVENT_FOCUS_PERSON, person_id))
                    {
                        internal->search_selected_index = (int)index;
                        char status[160];
                        (void)snprintf(status, sizeof(status), "Focus request queued for %s.",
                                       name_buffer);
                        ui_internal_set_status(internal, status);
                    }
                    else
                    {
                        ui_internal_set_status(internal,
                                               "Event queue full; focus request aborted.");
                    }
                }
            }
            nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 28.0f, 1);
        if (ui_nav_button_label(internal, ctx, "Focus Selected"))
        {
            if (internal->search_selected_index >= 0 &&
                (size_t)internal->search_selected_index < internal->search_result_count)
            {
                uint32_t person_id = internal->search_results[internal->search_selected_index];
                Person *person     = tree ? family_tree_find_person(tree, person_id) : NULL;
                char name_buffer[128];
                if (!person ||
                    !person_format_display_name(person, name_buffer, sizeof(name_buffer)))
                {
                    (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person_id);
                }
                if (ui_event_enqueue_with_u32(ui, UI_EVENT_FOCUS_PERSON, person_id))
                {
                    char status[160];
                    (void)snprintf(status, sizeof(status), "Focus request queued for %s.",
                                   name_buffer);
                    ui_internal_set_status(internal, status);
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; focus request aborted.");
                }
            }
            else
            {
                ui_internal_set_status(internal, "Select a person before focusing.");
            }
        }

        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Tip: Use Clear Filters to reset and search again.", NK_TEXT_LEFT);
    }
    else
    {
        internal->show_search_panel = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Search##AncestryTree"))
    {
        internal->show_search_panel = false;
    }
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_analytics_panel(UIInternal *internal, UIContext *ui, const FamilyTree *tree)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_analytics_panel;
    bool active  = showing || ui_animated_panel_visible(&internal->analytics_panel_anim);
    if (!active)
    {
        return;
    }

    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->analytics_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }

    TreeStatistics *stats = &internal->analytics_stats;
    bool stats_error      = false;
    if (showing)
    {
        if (!tree || tree->person_count == 0U)
        {
            tree_statistics_reset(stats);
        }
        else if (!tree_statistics_calculate(stats, tree, 5U))
        {
            stats_error = true;
            tree_statistics_reset(stats);
        }
    }

    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width = fminf(380.0f, (float)ui->width - 40.0f);
    if (width < 300.0f)
    {
        width = 300.0f;
    }
    float height = fminf(440.0f, (float)ui->height - 120.0f);
    if (height < 280.0f)
    {
        height = 280.0f;
    }
    struct nk_rect bounds = nk_rect(36.0f, 260.0f, width, height);

    /* Manual QA: Populate the sample tree, open View -> Tree Analytics, verify generation counts,
     * average lifespan, and location tallies update as persons are edited or toggled
     * alive/deceased.
     */
    if (nk_begin(ctx, "Tree Analytics##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                     NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        nk_label(ctx, "Insights derived from the current holographic family tree.", NK_TEXT_LEFT);

        if (!tree || tree->person_count == 0U)
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            nk_label(ctx, "Load or create a tree to view analytics.", NK_TEXT_LEFT);
        }
        else if (stats_error)
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            nk_label(ctx, "Analytics unavailable: insufficient memory for computation.",
                     NK_TEXT_LEFT);
        }
        else
        {
            char line[160];

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            (void)snprintf(line, sizeof(line), "Generations detected: %zu",
                           stats->generation_count);
            nk_label(ctx, line, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            (void)snprintf(line, sizeof(line), "Living persons: %zu", stats->living_count);
            nk_label(ctx, line, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            (void)snprintf(line, sizeof(line), "Deceased persons: %zu", stats->deceased_count);
            nk_label(ctx, line, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            if (stats->lifespan_sample_count == 0U)
            {
                nk_label(ctx, "Average lifespan: N/A (missing birth or death dates)", NK_TEXT_LEFT);
            }
            else
            {
                (void)snprintf(line, sizeof(line),
                               "Average lifespan: %.1f years across %zu samples",
                               stats->average_lifespan_years, stats->lifespan_sample_count);
                nk_label(ctx, line, NK_TEXT_LEFT);
            }

            nk_layout_row_dynamic(ctx, 6.0f, 1);
            nk_spacer(ctx);

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, "Top birth locations", NK_TEXT_LEFT);
            if (stats->birth_location_count == 0U)
            {
                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "No recorded birth locations.", NK_TEXT_LEFT);
            }
            else
            {
                for (size_t index = 0U; index < stats->birth_location_count; ++index)
                {
                    const TreeLocationCount *entry = &stats->birth_locations[index];
                    nk_layout_row_dynamic(ctx, 18.0f, 1);
                    (void)snprintf(line, sizeof(line), "%zu. %s (%zu)", index + 1U,
                                   entry->location ? entry->location : "(unknown)", entry->count);
                    nk_label(ctx, line, NK_TEXT_LEFT);
                }
            }

            nk_layout_row_dynamic(ctx, 6.0f, 1);
            nk_spacer(ctx);

            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_label(ctx, "Top death locations", NK_TEXT_LEFT);
            if (stats->death_location_count == 0U)
            {
                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "No recorded death locations.", NK_TEXT_LEFT);
            }
            else
            {
                for (size_t index = 0U; index < stats->death_location_count; ++index)
                {
                    const TreeLocationCount *entry = &stats->death_locations[index];
                    nk_layout_row_dynamic(ctx, 18.0f, 1);
                    (void)snprintf(line, sizeof(line), "%zu. %s (%zu)", index + 1U,
                                   entry->location ? entry->location : "(unknown)", entry->count);
                    nk_label(ctx, line, NK_TEXT_LEFT);
                }
            }
        }
    }
    else if (showing)
    {
        internal->show_analytics_panel = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Tree Analytics##AncestryTree"))
    {
        internal->show_analytics_panel = false;
    }
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_settings_window(UIInternal *internal, UIContext *ui, Settings *settings,
                                    bool settings_dirty)
{
    if (!internal || !ui || !settings)
    {
        return;
    }
    bool showing = internal->show_settings_window;
    bool active  = showing || ui_animated_panel_visible(&internal->settings_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->settings_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width = fminf(420.0f, (float)ui->width - 40.0f);
    if (width < 280.0f)
    {
        width = 280.0f;
    }
    struct nk_rect bounds = nk_rect((float)ui->width - width - 20.0f, 60.0f, width, 460.0f);
    if (nk_begin(ctx, "Settings##AncestryTree", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                     NK_WINDOW_CLOSABLE))
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
        if (ui_nav_combo_begin_label(internal, ctx,
                                     ui_settings_graphics_quality_label(settings->graphics_quality),
                                     nk_vec2(220.0f, 80.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (ui_nav_combo_item_label(internal, ctx, "Quality", NK_TEXT_LEFT) &&
                settings->graphics_quality != SETTINGS_GRAPHICS_QUALITY_QUALITY)
            {
                settings->graphics_quality = SETTINGS_GRAPHICS_QUALITY_QUALITY;
                settings_mark_dirty(settings);
            }
            if (ui_nav_combo_item_label(internal, ctx, "Performance", NK_TEXT_LEFT) &&
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
        if (ui_nav_combo_begin_label(internal, ctx,
                                     ui_settings_color_scheme_label(settings->color_scheme),
                                     nk_vec2(220.0f, 80.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (ui_nav_combo_item_label(internal, ctx, "Cyan Graph", NK_TEXT_LEFT) &&
                settings->color_scheme != SETTINGS_COLOR_SCHEME_CYAN_GRAPH)
            {
                settings->color_scheme = SETTINGS_COLOR_SCHEME_CYAN_GRAPH;
                settings_mark_dirty(settings);
            }
            if (ui_nav_combo_item_label(internal, ctx, "Solar Orchid", NK_TEXT_LEFT) &&
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
        nk_label(ctx, "Accessibility", NK_TEXT_LEFT);

        nk_bool high_contrast = settings->high_contrast_mode ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "High contrast UI", &high_contrast);
        bool new_high_contrast = (high_contrast == nk_true);
        if (new_high_contrast != settings->high_contrast_mode)
        {
            settings->high_contrast_mode = new_high_contrast;
            settings_mark_dirty(settings);
        }

        float font_scale = settings->ui_font_scale;
        ui_nav_property_float(internal, ctx, "Font scale", 0.6f, &font_scale, 1.8f, 0.05f, 0.01f,
                              0.05f, NULL);
        if (fabsf(font_scale - settings->ui_font_scale) > 0.0001f)
        {
            settings->ui_font_scale = font_scale;
            settings_mark_dirty(settings);
        }

        nk_bool screen_reader = settings->screen_reader_enabled ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "Enable screen reader narration",
                                    &screen_reader);
        bool new_screen_reader = (screen_reader == nk_true);
        if (new_screen_reader != settings->screen_reader_enabled)
        {
            settings->screen_reader_enabled = new_screen_reader;
            settings_mark_dirty(settings);
            ui_internal_set_status(internal, new_screen_reader
                                                 ? "Screen reader narration enabled."
                                                 : "Screen reader narration disabled.");
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Camera Sensitivity", NK_TEXT_LEFT);

        float rotation = settings->camera_rotation_sensitivity;
        ui_nav_property_float(internal, ctx, "Orbit", 0.05f, &rotation, 5.0f, 0.05f, 0.01f, 0.05f,
                              NULL);
        if (fabsf(rotation - settings->camera_rotation_sensitivity) > 0.0001f)
        {
            settings->camera_rotation_sensitivity = rotation;
            settings_mark_dirty(settings);
        }

        float pan_mouse = settings->camera_pan_sensitivity;
        ui_nav_property_float(internal, ctx, "Pan (mouse)", 0.05f, &pan_mouse, 10.0f, 0.05f, 0.01f,
                              0.05f, NULL);
        if (fabsf(pan_mouse - settings->camera_pan_sensitivity) > 0.0001f)
        {
            settings->camera_pan_sensitivity = pan_mouse;
            settings_mark_dirty(settings);
        }

        float pan_keyboard = settings->camera_keyboard_pan_sensitivity;
        ui_nav_property_float(internal, ctx, "Pan (keys)", 0.05f, &pan_keyboard, 10.0f, 0.05f,
                              0.01f, 0.05f, NULL);
        if (fabsf(pan_keyboard - settings->camera_keyboard_pan_sensitivity) > 0.0001f)
        {
            settings->camera_keyboard_pan_sensitivity = pan_keyboard;
            settings_mark_dirty(settings);
        }

        float zoom = settings->camera_zoom_sensitivity;
        ui_nav_property_float(internal, ctx, "Zoom", 0.05f, &zoom, 5.0f, 0.05f, 0.01f, 0.05f, NULL);
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
        if (ui_nav_combo_begin_label(internal, ctx,
                                     ui_settings_layout_label(settings->default_layout_algorithm),
                                     nk_vec2(220.0f, 70.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (ui_nav_combo_item_label(internal, ctx, "Hierarchical", NK_TEXT_LEFT) &&
                settings->default_layout_algorithm != SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL)
            {
                settings->default_layout_algorithm = SETTINGS_LAYOUT_ALGORITHM_HIERARCHICAL;
                settings_mark_dirty(settings);
            }
            if (ui_nav_combo_item_label(internal, ctx, "Force-directed (prototype)",
                                        NK_TEXT_LEFT) &&
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
        if (ui_nav_combo_begin_label(internal, ctx, ui_settings_language_label(settings->language),
                                     nk_vec2(220.0f, 70.0f)))
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            if (ui_nav_combo_item_label(internal, ctx, "English", NK_TEXT_LEFT) &&
                settings->language != SETTINGS_LANGUAGE_ENGLISH)
            {
                settings->language = SETTINGS_LANGUAGE_ENGLISH;
                settings_mark_dirty(settings);
            }
            if (ui_nav_combo_item_label(internal, ctx, "Future Tongue", NK_TEXT_LEFT) &&
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
        (void)ui_nav_checkbox_label(internal, ctx, "Enable auto-save", &auto_save);
        bool new_auto_save = (auto_save == nk_true);
        if (new_auto_save != settings->auto_save_enabled)
        {
            settings->auto_save_enabled = new_auto_save;
            settings_mark_dirty(settings);
        }

        int interval = (int)settings->auto_save_interval_seconds;
        ui_nav_property_int(internal, ctx, "Interval (sec)", 15, &interval, 3600, 15, 15.0f,
                            "Auto-save interval updated.");
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
        if (ui_nav_button_label(internal, ctx, "Save to Disk"))
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
        if (ui_nav_button_label(internal, ctx, "Reload from Disk"))
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
         * 1. Adjust orbit/pan/zoom sliders and verify camera responsiveness changes after closing
         * the window.
         * 2. Toggle graphics quality and confirm connection smoothing/portraits update per
         * selection.
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
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_exit_prompt(UIInternal *internal, UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_exit_prompt;
    bool active  = showing || ui_animated_panel_visible(&internal->exit_prompt_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->exit_prompt_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width           = 320.0f;
    float height          = 150.0f;
    float x               = ((float)ui->width - width) * 0.5f;
    float y               = ((float)ui->height - height) * 0.5f;
    struct nk_rect bounds = nk_rect(x, y, width, height);
    if (nk_begin(ctx, "Exit##Prompt", bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, "Are you sure you want to exit?", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 28.0f, 2);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_CANCEL, "Cancel"))
        {
            internal->show_exit_prompt = false;
        }
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_EXIT, "Exit"))
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
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_error_dialog(UIInternal *internal, UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_error_dialog;
    bool active  = showing || ui_animated_panel_visible(&internal->error_dialog_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    const char *title =
        internal->error_dialog_title[0] != '\0' ? internal->error_dialog_title : "Error";
    char window_title[96];
    (void)snprintf(window_title, sizeof(window_title), "%s##ErrorDialog", title);
    float width           = fminf(480.0f, (float)ui->width * 0.7f);
    float height          = fminf(220.0f, (float)ui->height * 0.45f);
    float x               = ((float)ui->width - width) * 0.5f;
    float y               = ((float)ui->height - height) * 0.5f;
    struct nk_rect bounds = nk_rect(x, y, width, height);
    float alpha           = ui_animated_panel_alpha(&internal->error_dialog_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    if (nk_begin(ctx, window_title, bounds,
                 NK_WINDOW_TITLE | NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE))
    {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_label(ctx, title, NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label_wrap(ctx, internal->error_dialog_message);
        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacer(ctx);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label_wrap(ctx, "The incident has been recorded in the application log.");
        nk_layout_row_dynamic(ctx, 32.0f, 1);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_CANCEL, "Dismiss"))
        {
            internal->show_error_dialog = false;
        }
    }
    else
    {
        internal->show_error_dialog = false;
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, window_title))
    {
        internal->show_error_dialog = false;
    }
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_menu_bar(UIInternal *internal, UIContext *ui, const FamilyTree *tree,
                             const LayoutResult *layout, CameraController *camera,
                             RenderConfig *render_config, bool settings_dirty)
{
    if (!internal || !ui)
    {
        return;
    }
    struct nk_context *ctx  = &internal->ctx;
    struct nk_rect bar_rect = nk_rect(0.0f, 0.0f, (float)ui->width, 30.0f);
    if (nk_begin(ctx, "MenuBar", bar_rect,
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND))
    {
        nk_menubar_begin(ctx);
        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 11);

        nk_layout_row_push(ctx, 60.0f);
        if (nk_menu_begin_label(ctx, "File", NK_TEXT_LEFT, nk_vec2(200.0f, 220.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_menu_item_label(internal, ctx, "New Tree", NK_TEXT_LEFT))
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
            if (ui_nav_menu_item_label(internal, ctx, "Open...", NK_TEXT_LEFT))
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
            if (ui_nav_menu_item_label(internal, ctx, "Save", NK_TEXT_LEFT))
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
            if (ui_nav_menu_item_label(internal, ctx, "Save As...", NK_TEXT_LEFT))
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
            if (ui_nav_menu_item_label(internal, ctx, "Exit", NK_TEXT_LEFT))
            {
                internal->show_exit_prompt = true;
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 60.0f);
        if (nk_menu_begin_label(ctx, "Edit", NK_TEXT_LEFT, nk_vec2(160.0f, 180.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_menu_item_label(internal, ctx, "Undo", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_UNDO))
                {
                    ui_internal_set_status(internal, "Undo request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; undo aborted.");
                }
            }
            if (ui_nav_menu_item_label(internal, ctx, "Redo", NK_TEXT_LEFT))
            {
                if (ui_event_enqueue(ui, UI_EVENT_REDO))
                {
                    ui_internal_set_status(internal, "Redo request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; redo aborted.");
                }
            }
            nk_layout_row_dynamic(ctx, 6.0f, 1);
            nk_spacing(ctx, 1);
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_menu_item_label(internal, ctx, "Add Person...", NK_TEXT_LEFT))
            {
                internal->show_add_person_panel = true;
                ui_add_person_form_reset(internal);
                ui_add_person_set_error(internal, NULL);
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 70.0f);
        if (nk_menu_begin_label(ctx, "View", NK_TEXT_LEFT, nk_vec2(220.0f, 200.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            const char *settings_label = settings_dirty ? "Settings... *" : "Settings...";
            if (ui_nav_menu_item_label(internal, ctx, settings_label, NK_TEXT_LEFT))
            {
                internal->show_settings_window = true;
            }
            const char *search_label =
                internal->show_search_panel ? "Search / Filter... *" : "Search / Filter...";
            if (ui_nav_menu_item_label(internal, ctx, search_label, NK_TEXT_LEFT))
            {
                internal->show_search_panel = true;
                internal->search_dirty      = true;
            }
            const char *analytics_label =
                internal->show_analytics_panel ? "Tree Analytics... *" : "Tree Analytics...";
            if (ui_nav_menu_item_label(internal, ctx, analytics_label, NK_TEXT_LEFT))
            {
                internal->show_analytics_panel = true;
            }
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_menu_item_label(internal, ctx, "Reset Camera", NK_TEXT_LEFT))
            {
                camera_controller_reset(camera);
                ui_internal_set_status(internal, "Camera reset to default orbit.");
            }
            if (ui_nav_menu_item_label(internal, ctx, "Focus Roots", NK_TEXT_LEFT))
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
                (void)ui_nav_checkbox_label(internal, ctx, "Show grid", &show_grid);
                bool new_grid = (show_grid == nk_true);
                if (new_grid != render_config->show_grid)
                {
                    render_config->show_grid = new_grid;
                    ui_internal_set_status(internal,
                                           new_grid ? "Grid lines enabled." : "Grid lines hidden.");
                }

                nk_bool show_connections = render_config->show_connections ? nk_true : nk_false;
                (void)ui_nav_checkbox_label(internal, ctx, "Show connections", &show_connections);
                bool new_connections = (show_connections == nk_true);
                if (new_connections != render_config->show_connections)
                {
                    render_config->show_connections = new_connections;
                    ui_internal_set_status(internal, new_connections
                                                         ? "Relationship lines enabled."
                                                         : "Relationship lines hidden.");
                }

                nk_bool show_overlay = render_config->show_overlay ? nk_true : nk_false;
                (void)ui_nav_checkbox_label(internal, ctx, "Show holographic overlay",
                                            &show_overlay);
                bool new_overlay = (show_overlay == nk_true);
                if (new_overlay != render_config->show_overlay)
                {
                    render_config->show_overlay = new_overlay;
                    ui_internal_set_status(internal,
                                           new_overlay ? "Overlay enabled." : "Overlay hidden.");
                }

                nk_bool show_labels = render_config->show_name_panels ? nk_true : nk_false;
                (void)ui_nav_checkbox_label(internal, ctx, "Show name panels", &show_labels);
                bool new_labels = (show_labels == nk_true);
                if (new_labels != render_config->show_name_panels)
                {
                    render_config->show_name_panels = new_labels;
                    ui_internal_set_status(internal, new_labels
                                                         ? "Holographic name panels enabled."
                                                         : "Holographic name panels hidden.");
                }

                nk_bool show_profiles = render_config->show_profile_images ? nk_true : nk_false;
                (void)ui_nav_checkbox_label(internal, ctx, "Show profile images", &show_profiles);
                bool new_profiles = (show_profiles == nk_true);
                if (new_profiles != render_config->show_profile_images)
                {
                    render_config->show_profile_images = new_profiles;
                    ui_internal_set_status(internal, new_profiles
                                                         ? "Portraits enabled in name panels."
                                                         : "Portraits hidden from name panels.");
                }

                /* Manual validation checklist:
                 * 1. Toggle "Smooth connection lines" and confirm relationship links switch between
                 * crisp and antialiased rendering without artifacts.
                 * 2. Switch parent/spouse connection styles and ensure the renderer updates
                 * immediately (straight vs. bezier curves) while hover/selection highlights remain
                 * correct.
                 */
                nk_bool smooth_connections =
                    render_config->connection_antialiasing ? nk_true : nk_false;
                (void)ui_nav_checkbox_label(internal, ctx, "Smooth connection lines",
                                            &smooth_connections);
                bool new_smoothing = (smooth_connections == nk_true);
                if (new_smoothing != render_config->connection_antialiasing)
                {
                    render_config->connection_antialiasing = new_smoothing;
                    ui_internal_set_status(internal, new_smoothing
                                                         ? "Smooth line rendering enabled."
                                                         : "Smooth line rendering disabled.");
                }

                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "Parent connection style", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_begin_label(
                        internal, ctx,
                        ui_connection_style_label(render_config->connection_style_parent_child),
                        nk_vec2(180.0f, 70.0f)))
                {
                    nk_layout_row_dynamic(ctx, 20.0f, 1);
                    if (ui_nav_combo_item_label(internal, ctx, "Straight", NK_TEXT_LEFT) &&
                        render_config->connection_style_parent_child !=
                            RENDER_CONNECTION_STYLE_STRAIGHT)
                    {
                        render_config->connection_style_parent_child =
                            RENDER_CONNECTION_STYLE_STRAIGHT;
                        ui_internal_set_status(internal,
                                               "Parent connections set to straight segments.");
                    }
                    if (ui_nav_combo_item_label(internal, ctx, "Bezier", NK_TEXT_LEFT) &&
                        render_config->connection_style_parent_child !=
                            RENDER_CONNECTION_STYLE_BEZIER)
                    {
                        render_config->connection_style_parent_child =
                            RENDER_CONNECTION_STYLE_BEZIER;
                        ui_internal_set_status(internal,
                                               "Parent connections set to bezier curves.");
                    }
                    nk_combo_end(ctx);
                }

                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label(ctx, "Spouse connection style", NK_TEXT_LEFT);
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_begin_label(
                        internal, ctx,
                        ui_connection_style_label(render_config->connection_style_spouse),
                        nk_vec2(180.0f, 70.0f)))
                {
                    nk_layout_row_dynamic(ctx, 20.0f, 1);
                    if (ui_nav_combo_item_label(internal, ctx, "Straight", NK_TEXT_LEFT) &&
                        render_config->connection_style_spouse != RENDER_CONNECTION_STYLE_STRAIGHT)
                    {
                        render_config->connection_style_spouse = RENDER_CONNECTION_STYLE_STRAIGHT;
                        ui_internal_set_status(internal,
                                               "Spouse connections set to straight segments.");
                    }
                    if (ui_nav_combo_item_label(internal, ctx, "Bezier", NK_TEXT_LEFT) &&
                        render_config->connection_style_spouse != RENDER_CONNECTION_STYLE_BEZIER)
                    {
                        render_config->connection_style_spouse = RENDER_CONNECTION_STYLE_BEZIER;
                        ui_internal_set_status(internal,
                                               "Spouse connections set to bezier curves.");
                    }
                    nk_combo_end(ctx);
                }

                float font_control = render_config->name_panel_font_size;
                ui_nav_property_float(internal, ctx, "Name panel font", 16.0f, &font_control, 64.0f,
                                      1.0f, 0.2f, 1.0f, NULL);
                if (fabsf(font_control - render_config->name_panel_font_size) > 0.05f)
                {
                    render_config->name_panel_font_size = font_control;
                    ui_internal_set_status(internal, "Name panel font size updated.");
                }
            }
            if (ui_nav_menu_item_label(internal, ctx,
                                       internal->auto_orbit ? "Disable Auto Orbit"
                                                            : "Enable Auto Orbit",
                                       NK_TEXT_LEFT))
            {
                internal->auto_orbit = !internal->auto_orbit;
                ui_internal_set_status(internal, internal->auto_orbit ? "Auto orbit enabled."
                                                                      : "Auto orbit disabled.");
            }
            if (ui_nav_menu_item_label(internal, ctx, "Layout Options", NK_TEXT_LEFT))
            {
                ui_internal_set_status(internal, "Layout options UI under development.");
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 70.0f);
        if (nk_menu_begin_label(ctx, "Help", NK_TEXT_LEFT, nk_vec2(180.0f, 160.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_menu_item_label(internal, ctx, "About", NK_TEXT_LEFT))
            {
                internal->show_about_window = true;
            }
            if (ui_nav_menu_item_label(internal, ctx, "Quick Help", NK_TEXT_LEFT))
            {
                internal->show_help_window = true;
            }
            if (ui_nav_menu_item_label(internal, ctx, "Documentation", NK_TEXT_LEFT))
            {
                ui_internal_set_status(internal,
                                       "Documentation menu loads roadmap in future milestone.");
            }
            nk_menu_end(ctx);
        }

        nk_layout_row_push(ctx, 10.0f);
        nk_spacing(ctx, 1);

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_SEARCH, "Search / Filter",
                               internal->show_search_panel))
        {
            internal->show_search_panel = true;
            internal->search_dirty      = true;
        }

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_ADD, "Add Person",
                               internal->show_add_person_panel))
        {
            internal->show_add_person_panel = true;
            ui_add_person_form_reset(internal);
            ui_add_person_set_error(internal, NULL);
        }

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_SETTINGS, "Settings",
                               internal->show_settings_window))
        {
            internal->show_settings_window = true;
        }

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_HELP, "Quick Help",
                               internal->show_help_window))
        {
            internal->show_help_window = true;
        }

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_INFO, "About", internal->show_about_window))
        {
            internal->show_about_window = true;
        }

        nk_layout_row_push(ctx, 36.0f);
        if (ui_nav_icon_button(internal, ctx, UI_ICON_EXIT, "Exit", internal->show_exit_prompt))
        {
            internal->show_exit_prompt = true;
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

static void ui_process_input(UIInternal *internal, UIContext *ui, float delta_seconds,
                             float wheel_delta)
{
    if (!internal)
    {
        return;
    }
    ui_internal_tick(internal, delta_seconds);
    struct nk_context *ctx = &internal->ctx;
    bool shift_down        = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool tab_event         = IsKeyPressed(KEY_TAB) || IsKeyPressedRepeat(KEY_TAB);

    bool enter_event = IsKeyPressed(KEY_ENTER) || IsKeyPressedRepeat(KEY_ENTER);
    bool space_event = IsKeyPressed(KEY_SPACE) || IsKeyPressedRepeat(KEY_SPACE);
    bool left_event  = IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT);
    bool right_event = IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT);
    bool up_event    = IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP);
    bool down_event  = IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN);

    internal->nav_input.tab_pressed         = tab_event;
    internal->nav_input.tab_with_shift      = tab_event && shift_down;
    internal->nav_input.activate_pressed    = enter_event || space_event;
    internal->nav_input.arrow_left_pressed  = left_event;
    internal->nav_input.arrow_right_pressed = right_event;
    internal->nav_input.arrow_up_pressed    = up_event;
    internal->nav_input.arrow_down_pressed  = down_event;
    nk_input_begin(ctx);
    (void)ui;
    Vector2 mouse = GetMousePosition();
    nk_input_motion(ctx, (int)mouse.x, (int)mouse.y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)mouse.x, (int)mouse.y,
                    IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)mouse.x, (int)mouse.y,
                    IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)mouse.x, (int)mouse.y,
                    IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    nk_input_scroll(ctx, nk_vec2(0.0f, wheel_delta));

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
    nk_input_key(ctx, NK_KEY_SHIFT, shift_down);
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
    ui->impl      = NULL;
    ui->width     = width;
    ui->height    = height;

#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = (UIInternal *)AT_CALLOC(1U, sizeof(UIInternal));
    if (!internal)
    {
        return false;
    }
    internal->font_owned                  = false;
    internal->accessibility_high_contrast = false;
    internal->accessibility_font_scale    = 1.0f;

    float computed_font_size = ui_scaling_default_font_size(height);
    if (!(computed_font_size > 0.0f))
    {
        computed_font_size = 20.0f;
    }

    ui_internal_acquire_font(internal, computed_font_size);

    internal->auto_orbit                  = false;
    internal->show_error_dialog           = false;
    internal->error_dialog_title[0]       = '\0';
    internal->error_dialog_message[0]     = '\0';
    internal->show_search_panel           = false;
    internal->show_analytics_panel        = false;
    internal->search_query[0]             = '\0';
    internal->search_include_alive        = true;
    internal->search_include_deceased     = true;
    internal->search_use_birth_year_range = false;
    internal->search_birth_year_min       = 1900;
    internal->search_birth_year_max       = 2100;
    internal->search_result_count         = 0U;
    internal->search_selected_index       = -1;
    internal->search_dirty                = true;
    internal->search_last_tree            = NULL;
    internal->show_add_person_panel       = false;
    internal->add_person_request_pending  = false;
    memset(&internal->add_person_request, 0, sizeof(UIAddPersonRequest));
    ui_add_person_form_reset(internal);
    internal->show_edit_person_panel      = false;
    internal->edit_person_request_pending = false;
    memset(&internal->edit_person_request, 0, sizeof(UIEditPersonRequest));
    memset(&internal->edit_person_form, 0, sizeof(internal->edit_person_form));
    ui_edit_person_form_reset(internal);
    tree_statistics_init(&internal->analytics_stats);

    if (!nk_init_default(&internal->ctx, &internal->font))
    {
        AT_FREE(internal);
        return false;
    }
    ui_internal_bind_font(internal, true);
    ui_internal_apply_theme(internal);
    internal->icons_ready    = ui_icons_init(&internal->icons);
    internal->ctx.clip.copy  = ui_clipboard_copy;
    internal->ctx.clip.paste = ui_clipboard_paste;
    onboarding_init(&internal->onboarding, false);
    progress_tracker_reset(&internal->progress);
    internal->progress_target_value  = 0.0f;
    internal->progress_target_valid  = false;
    internal->progress_spinner_phase = 0.0f;
    internal->frame_delta            = 0.0f;
    ui_accessibility_reset(&internal->screen_reader);

    ui->impl      = internal;
    ui->available = true;
    /* Manual QA: Run the prototype at 720p, 1080p, and 4K to verify font legibility and confirm
     * warning logs fire when custom font assets are absent. */
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
    ui->width  = width;
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
        screen_reader_shutdown(&internal->screen_reader.channel);
        tree_statistics_reset(&internal->analytics_stats);
        ui_icons_shutdown(&internal->icons);
        nk_free(&internal->ctx);
        if (internal->font_owned)
        {
            UnloadFont(internal->rl_font);
        }
        AT_FREE(internal);
    }
#endif
    ui->impl      = NULL;
    ui->available = false;
}

void ui_shutdown(UIContext *ui) { ui_cleanup(ui); }

bool ui_begin_frame(UIContext *ui, float delta_seconds, float wheel_delta)
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
    float clamped_delta = delta_seconds;
    if (!(clamped_delta > 0.0f))
    {
        clamped_delta = 0.0f;
    }
    if (clamped_delta > 0.25f)
    {
        clamped_delta = 0.25f;
    }
    internal->frame_delta = clamped_delta;
    onboarding_update(&internal->onboarding, clamped_delta);
    float progress_value =
        internal->progress_target_valid ? internal->progress_target_value : -1.0f;
    progress_tracker_update(&internal->progress, clamped_delta, progress_value);
    internal->progress_target_valid = false;
    float spinner_speed             = 6.0f;
    internal->progress_spinner_phase += clamped_delta * spinner_speed;
    if (internal->progress_spinner_phase > (float)(2.0 * NK_PI))
    {
        internal->progress_spinner_phase =
            fmodf(internal->progress_spinner_phase, (float)(2.0 * NK_PI));
    }
    ui_process_input(internal, ui, clamped_delta, wheel_delta);
    return true;
#else
    (void)delta_seconds;
    (void)wheel_delta;
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

    size_t written       = 0U;
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
    Vector2 mouse          = GetMousePosition();
    float width            = 260.0f;
    float height           = 120.0f;
    float x                = mouse.x + 18.0f;
    float y                = mouse.y + 18.0f;
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
        const char *birth_location =
            hovered->dates.birth_location ? hovered->dates.birth_location : "Unknown";
        nk_labelf(ctx, NK_TEXT_LEFT, "Born: %s", birth_date);
        nk_labelf(ctx, NK_TEXT_LEFT, "Birthplace: %s", birth_location);
        if (hovered->is_alive)
        {
            nk_label(ctx, "Status: Alive", NK_TEXT_LEFT);
        }
        else
        {
            const char *death_date =
                hovered->dates.death_date ? hovered->dates.death_date : "Unknown";
            const char *death_location =
                hovered->dates.death_location ? hovered->dates.death_location : "Unknown";
            nk_labelf(ctx, NK_TEXT_LEFT, "Died: %s", death_date);
            nk_labelf(ctx, NK_TEXT_LEFT, "Place of death: %s", death_location);
        }
    }
    nk_end(ctx);
}

static void ui_draw_tree_panel(UIInternal *internal, const FamilyTree *tree,
                               const LayoutResult *layout, CameraController *camera, float fps,
                               const Person *selected_person, const Person *hovered_person)
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

        if (selected_person)
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_button_label(internal, ctx, "Edit Selected..."))
            {
                ui_edit_person_populate(internal, selected_person);
                ui_internal_set_status(internal, "Edit panel opened for selected hologram.");
            }
        }

        if (internal->status_timer > 0.0f && internal->status_message[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 22.0f, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "Status: %s", internal->status_message);
        }

        nk_layout_row_dynamic(ctx, 24.0f, 2);
        if (ui_nav_button_label(internal, ctx, "Reset Camera"))
        {
            camera_controller_reset(camera);
            ui_internal_set_status(internal, "Camera reset to default orbit.");
        }
        if (ui_nav_button_label(internal, ctx, "Focus Roots"))
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
        (void)ui_nav_checkbox_label(internal, ctx, "Auto orbit when idle", &orbit);
        bool new_auto_orbit = (orbit == nk_true);
        if (internal->auto_orbit != new_auto_orbit)
        {
            internal->auto_orbit = new_auto_orbit;
            ui_internal_set_status(internal,
                                   new_auto_orbit ? "Auto orbit enabled." : "Auto orbit disabled.");
        }
    }
    nk_end(ctx);
}

static void ui_draw_onboarding_overlay(UIInternal *internal, UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    if (!onboarding_is_active(&internal->onboarding))
    {
        return;
    }
    const char *title = onboarding_current_title(&internal->onboarding);
    const char *body  = onboarding_current_body(&internal->onboarding);
    if ((!title || title[0] == '\0') && (!body || body[0] == '\0'))
    {
        return;
    }

    struct nk_context *ctx = &internal->ctx;
    float width            = 340.0f;
    float height           = 220.0f;
    float margin           = 24.0f;
    if (ui->width <= 0)
    {
        return;
    }
    float screen_width = (float)ui->width;
    if (margin > screen_width * 0.5f)
    {
        margin = screen_width * 0.5f;
    }
    if (screen_width < width + margin)
    {
        width = screen_width - margin;
        if (width <= 0.0f)
        {
            width  = screen_width;
            margin = 0.0f;
        }
    }
    float top = 80.0f;
    if (ui->height > 0)
    {
        float max_top = (float)ui->height - height - margin;
        if (max_top < 12.0f)
        {
            max_top = 12.0f;
        }
        if (top > max_top)
        {
            top = max_top;
        }
    }
    struct nk_rect bounds = nk_rect(screen_width - width - margin, top, width, height);
    nk_flags flags        = NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND;
    if (nk_begin(ctx, "OnboardingOverlay", bounds, flags))
    {
        size_t step_index =
            (size_t)((internal->onboarding.current_step >= 0) ? internal->onboarding.current_step
                                                              : 0);
        size_t step_total = onboarding_step_count();
        if (step_total == 0U)
        {
            step_total = 1U;
        }
        if (step_index >= step_total)
        {
            step_index = step_total - 1U;
        }
        char progress_label[48];
        (void)snprintf(progress_label, sizeof(progress_label), "Step %u of %u",
                       (unsigned int)(step_index + 1U), (unsigned int)step_total);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label_colored(ctx, progress_label, NK_TEXT_RIGHT, internal->theme.accent_hover);

        if (title && title[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            nk_label_colored(ctx, title, NK_TEXT_LEFT, internal->theme.accent_color);
        }

        if (body && body[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 18.0f, 1);
            nk_label_wrap(ctx, body);
        }

        size_t hint_total     = onboarding_step_hint_total(&internal->onboarding);
        size_t hint_remaining = onboarding_remaining_hints(&internal->onboarding);
        size_t hint_completed = (hint_total >= hint_remaining) ? (hint_total - hint_remaining) : 0U;
        float completion      = onboarding_step_completion(&internal->onboarding);

        if (hint_total > 0U)
        {
            nk_layout_row_dynamic(ctx, 16.0f, 1);
            char hint_status[64];
            (void)snprintf(hint_status, sizeof(hint_status), "%u of %u guided actions completed",
                           (unsigned int)hint_completed, (unsigned int)hint_total);
            nk_label(ctx, hint_status, NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 18.0f, 1);
            nk_size progress_value = (nk_size)(completion * 100.0f + 0.5f);
            nk_progress(ctx, &progress_value, 100, nk_false);

            nk_layout_row_dynamic(ctx, 16.0f, 1);
            if (hint_remaining == 0U)
            {
                nk_label_colored(ctx, "Great! Press Next to continue.", NK_TEXT_LEFT,
                                 internal->theme.accent_active);
            }
            else
            {
                char reminder[64];
                (void)snprintf(reminder, sizeof(reminder), "%u interaction%s remaining",
                               (unsigned int)hint_remaining, (hint_remaining == 1U) ? "" : "s");
                nk_label(ctx, reminder, NK_TEXT_LEFT);
            }
        }
        else
        {
            nk_layout_row_dynamic(ctx, 16.0f, 1);
            nk_label(ctx, "Review this holographic guide, then continue when ready.", NK_TEXT_LEFT);
        }

        bool final_step  = (step_index + 1U) >= step_total;
        bool can_advance = (hint_total == 0U) || (hint_remaining == 0U);

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 28.0f, 2);
        struct nk_style_button default_button  = ctx->style.button;
        struct nk_style_button disabled_button = default_button;
        struct nk_color disabled_colour        = nk_rgba(80, 80, 80, 180);
        disabled_button.normal                 = nk_style_item_color(disabled_colour);
        disabled_button.hover                  = nk_style_item_color(disabled_colour);
        disabled_button.active                 = nk_style_item_color(disabled_colour);
        disabled_button.text_normal            = internal->theme.window_border;
        disabled_button.text_hover             = internal->theme.window_border;
        disabled_button.text_active            = internal->theme.window_border;

        bool next_pressed = false;
        if (can_advance)
        {
            next_pressed = nk_button_label(ctx, final_step ? "Finish" : "Next");
        }
        else
        {
            ctx->style.button = disabled_button;
            (void)nk_button_label(ctx, "Next");
            ctx->style.button = default_button;
        }

        bool skip_pressed = nk_button_label(ctx, "Skip for now");

        ctx->style.button = default_button;

        if (next_pressed && can_advance)
        {
            if (final_step)
            {
                ui_onboarding_mark_completed(ui);
            }
            else
            {
                onboarding_advance(&internal->onboarding);
            }
        }

        if (skip_pressed)
        {
            onboarding_skip(&internal->onboarding);
            onboarding_enable_tooltips(&internal->onboarding, false);
            ui_internal_set_status(internal,
                                   "Onboarding paused. Re-enable it from Settings when ready.");
        }
    }
    nk_end(ctx);
}

static void ui_draw_progress_overlay(UIInternal *internal, UIContext *ui)
{
    if (!internal || !ui)
    {
        return;
    }
    if (!progress_tracker_is_active(&internal->progress))
    {
        return;
    }

    struct nk_context *ctx = &internal->ctx;
    float width            = 320.0f;
    float height           = 120.0f;
    float margin           = 24.0f;
    if (ui->width <= 0)
    {
        return;
    }
    float screen_width = (float)ui->width;
    if (margin > screen_width * 0.5f)
    {
        margin = screen_width * 0.5f;
    }
    if (screen_width < width + margin)
    {
        width = screen_width - margin;
        if (width <= 0.0f)
        {
            width  = screen_width;
            margin = 0.0f;
        }
    }
    float screen_height = (ui->height > 0) ? (float)ui->height : (height + margin + 12.0f);
    float bottom        = screen_height - height - margin;
    if (bottom < 12.0f)
    {
        bottom = 12.0f;
    }
    struct nk_rect bounds = nk_rect(screen_width - width - margin, bottom, width, height);
    nk_flags flags =
        NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT;
    if (nk_begin(ctx, "ProgressOverlay", bounds, flags))
    {
        const char *label = progress_tracker_label(&internal->progress);
        if (!label || label[0] == '\0')
        {
            label = "Processing...";
        }
        ProgressPhase phase = progress_tracker_phase(&internal->progress);
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Operation Status", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label_wrap(ctx, label);

        if (phase == PROGRESS_PHASE_SUCCESS)
        {
            const char *completion = progress_tracker_completion_label(&internal->progress);
            if (completion && completion[0] != '\0')
            {
                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label_colored(ctx, completion, NK_TEXT_LEFT, internal->theme.accent_active);
            }
        }
        else if (phase == PROGRESS_PHASE_FAILURE)
        {
            const char *failure = progress_tracker_completion_label(&internal->progress);
            if (failure && failure[0] != '\0')
            {
                nk_layout_row_dynamic(ctx, 18.0f, 1);
                nk_label_colored(ctx, failure, NK_TEXT_LEFT, internal->theme.warning_color);
            }
        }

        if (progress_tracker_has_value(&internal->progress))
        {
            nk_layout_row_dynamic(ctx, 16.0f, 1);
            nk_spacing(ctx, 1);
            struct nk_rect bar_bounds        = nk_widget_bounds(ctx);
            struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
            if (canvas)
            {
                struct nk_color background = nk_rgba(40, 50, 70, 220);
                struct nk_color foreground = nk_rgba(80, 190, 255, 255);
                nk_fill_rect(canvas, bar_bounds, 3.0f, background);
                struct nk_rect fill = bar_bounds;
                fill.w *= progress_tracker_display_value(&internal->progress);
                nk_fill_rect(canvas, fill, 3.0f, foreground);
            }
        }
        else
        {
            nk_layout_row_dynamic(ctx, 34.0f, 1);
            nk_label(ctx, " ", NK_TEXT_LEFT);
            struct nk_rect spinner_bounds = nk_widget_bounds(ctx);
            spinner_bounds.x += 4.0f;
            spinner_bounds.y += 4.0f;
            spinner_bounds.w = 26.0f;
            spinner_bounds.h = 26.0f;
            ui_draw_progress_spinner(internal, ctx, spinner_bounds);
        }
    }
    nk_end(ctx);
}

static void ui_draw_add_person_panel(UIInternal *internal, UIContext *ui, const FamilyTree *tree)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_add_person_panel;
    bool active  = showing || ui_animated_panel_visible(&internal->add_person_panel_anim);
    if (!active)
    {
        return;
    }
    struct nk_context *ctx = &internal->ctx;
    float alpha            = ui_animated_panel_alpha(&internal->add_person_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width  = 420.0f;
    float height = (float)ui->height - 80.0f;
    if (height < 360.0f)
    {
        height = 360.0f;
    }
    float x = (float)ui->width - width - 24.0f;
    if (x < 18.0f)
    {
        x = 18.0f;
    }
    struct nk_rect bounds = nk_rect(x, 48.0f, width, height);
    if (nk_begin(ctx, "Add Person##Panel", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE |
                     NK_WINDOW_SCROLL_AUTO_HIDE))
    {
        UIAddPersonRequest *draft = &internal->add_person_form.draft;
        nk_layout_row_dynamic(ctx, 26.0f, 1);
        nk_label(ctx, "Create Holographic Profile", NK_TEXT_LEFT);

        if (internal->add_person_form.error_message[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            nk_label_colored(ctx, internal->add_person_form.error_message, NK_TEXT_LEFT,
                             internal->theme.warning_color);
        }

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "First", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->first,
                                 (int)sizeof(draft->first), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Middle", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->middle,
                                 (int)sizeof(draft->middle), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Last", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->last,
                                 (int)sizeof(draft->last), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Birth Date", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->birth_date,
                                 (int)sizeof(draft->birth_date), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Birthplace", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->birth_location,
                                 (int)sizeof(draft->birth_location), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_bool alive = draft->is_alive ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "Is currently alive", &alive);
        draft->is_alive = (alive == nk_true);

        if (!draft->is_alive)
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Death Date", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->death_date,
                                     (int)sizeof(draft->death_date), nk_filter_default);
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Death Place", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->death_location,
                                     (int)sizeof(draft->death_location), nk_filter_default);
            nk_layout_row_end(ctx);
        }
        else
        {
            draft->death_date[0]     = '\0';
            draft->death_location[0] = '\0';
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Relationships", NK_TEXT_LEFT);

        char label_buffer[160];
        char name_buffer[128];
        const char *father_label = "None";
        if (draft->father_id != 0U && tree)
        {
            Person *father = family_tree_find_person(tree, draft->father_id);
            if (father)
            {
                ui_compose_person_name(father, name_buffer, sizeof(name_buffer));
                snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer, father->id);
                father_label = label_buffer;
            }
            else
            {
                snprintf(label_buffer, sizeof(label_buffer), "Missing (#%u)", draft->father_id);
                father_label = label_buffer;
            }
        }
        if (nk_group_begin(ctx, "FatherGroup", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Father", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, father_label, nk_vec2(260.0f, 260.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->father_id = 0U;
                }
                if (tree)
                {
                    for (size_t index = 0U; index < tree->person_count; ++index)
                    {
                        Person *candidate = tree->persons[index];
                        if (!candidate)
                        {
                            continue;
                        }
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                 candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->father_id = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_group_end(ctx);
        }

        const char *mother_label = "None";
        if (draft->mother_id != 0U && tree)
        {
            Person *mother = family_tree_find_person(tree, draft->mother_id);
            if (mother)
            {
                ui_compose_person_name(mother, name_buffer, sizeof(name_buffer));
                snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer, mother->id);
                mother_label = label_buffer;
            }
            else
            {
                snprintf(label_buffer, sizeof(label_buffer), "Missing (#%u)", draft->mother_id);
                mother_label = label_buffer;
            }
        }
        if (nk_group_begin(ctx, "MotherGroup", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Mother", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, mother_label, nk_vec2(260.0f, 260.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->mother_id = 0U;
                }
                if (tree)
                {
                    for (size_t index = 0U; index < tree->person_count; ++index)
                    {
                        Person *candidate = tree->persons[index];
                        if (!candidate)
                        {
                            continue;
                        }
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                 candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->mother_id = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_group_end(ctx);
        }

        const char *spouse_label = "None";
        if (draft->spouse_id != 0U && tree)
        {
            Person *spouse = family_tree_find_person(tree, draft->spouse_id);
            if (spouse)
            {
                ui_compose_person_name(spouse, name_buffer, sizeof(name_buffer));
                snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer, spouse->id);
                spouse_label = label_buffer;
            }
            else
            {
                snprintf(label_buffer, sizeof(label_buffer), "Missing (#%u)", draft->spouse_id);
                spouse_label = label_buffer;
            }
        }
        if (nk_group_begin(ctx, "SpouseGroup", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Spouse", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, spouse_label, nk_vec2(260.0f, 260.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->spouse_id = 0U;
                }
                if (tree)
                {
                    for (size_t index = 0U; index < tree->person_count; ++index)
                    {
                        Person *candidate = tree->persons[index];
                        if (!candidate)
                        {
                            continue;
                        }
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                 candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->spouse_id = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Timeline Entries", NK_TEXT_LEFT);

        for (size_t index = 0U; index < draft->timeline_count; ++index)
        {
            UIAddPersonTimelineItem *item = &draft->timeline_entries[index];
            nk_layout_row_dynamic(ctx, 20.0f, 1);
            nk_labelf(ctx, NK_TEXT_LEFT, "%s — %s", ui_timeline_type_label(item->type),
                      item->description);
            nk_layout_row_begin(ctx, NK_STATIC, 20.0f, 3);
            nk_layout_row_push(ctx, width - 140.0f);
            nk_label(ctx, item->date[0] != '\0' ? item->date : "(no date)", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 140.0f);
            nk_label(ctx, item->location[0] != '\0' ? item->location : "", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 60.0f);
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_DELETE, "Remove"))
            {
                ui_add_person_remove_timeline_entry(draft, index);
                --index;
            }
            nk_layout_row_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 4.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "New Timeline Entry", NK_TEXT_LEFT);

        const char *entry_type_label =
            ui_timeline_type_label(internal->add_person_form.new_timeline_type);
        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Type", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        if (ui_nav_combo_begin_label(internal, ctx, entry_type_label, nk_vec2(220.0f, 160.0f)))
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_combo_item_label(internal, ctx, "Birth", NK_TEXT_LEFT))
            {
                internal->add_person_form.new_timeline_type = TIMELINE_EVENT_BIRTH;
            }
            if (ui_nav_combo_item_label(internal, ctx, "Marriage", NK_TEXT_LEFT))
            {
                internal->add_person_form.new_timeline_type = TIMELINE_EVENT_MARRIAGE;
            }
            if (ui_nav_combo_item_label(internal, ctx, "Death", NK_TEXT_LEFT))
            {
                internal->add_person_form.new_timeline_type = TIMELINE_EVENT_DEATH;
            }
            if (ui_nav_combo_item_label(internal, ctx, "Custom", NK_TEXT_LEFT))
            {
                internal->add_person_form.new_timeline_type = TIMELINE_EVENT_CUSTOM;
            }
            nk_combo_end(ctx);
        }
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Date", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(
            internal, ctx, NK_EDIT_FIELD, internal->add_person_form.new_timeline_date,
            (int)sizeof(internal->add_person_form.new_timeline_date), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 60.0f, 1);
        (void)ui_nav_edit_string(
            internal, ctx, NK_EDIT_BOX, internal->add_person_form.new_timeline_description,
            (int)sizeof(internal->add_person_form.new_timeline_description), nk_filter_default);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Location", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(
            internal, ctx, NK_EDIT_FIELD, internal->add_person_form.new_timeline_location,
            (int)sizeof(internal->add_person_form.new_timeline_location), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_ADD, "Add Entry"))
        {
            if (draft->timeline_count >= UI_ADD_PERSON_MAX_TIMELINE_ENTRIES)
            {
                ui_add_person_set_error(internal, "Maximum timeline entries reached.");
            }
            else if (internal->add_person_form.new_timeline_description[0] == '\0')
            {
                ui_add_person_set_error(internal, "Provide a description for the timeline entry.");
            }
            else if (internal->add_person_form.new_timeline_date[0] != '\0' &&
                     !at_date_is_valid_iso8601(internal->add_person_form.new_timeline_date))
            {
                ui_add_person_set_error(internal, "Timeline entry date must be YYYY-MM-DD format.");
            }
            else
            {
                size_t slot                   = draft->timeline_count;
                UIAddPersonTimelineItem *item = &draft->timeline_entries[slot];
                item->type                    = internal->add_person_form.new_timeline_type;
                (void)snprintf(item->date, sizeof(item->date), "%s",
                               internal->add_person_form.new_timeline_date);
                (void)snprintf(item->description, sizeof(item->description), "%s",
                               internal->add_person_form.new_timeline_description);
                (void)snprintf(item->location, sizeof(item->location), "%s",
                               internal->add_person_form.new_timeline_location);
                draft->timeline_count += 1U;
                internal->add_person_form.new_timeline_date[0]        = '\0';
                internal->add_person_form.new_timeline_description[0] = '\0';
                internal->add_person_form.new_timeline_location[0]    = '\0';
                ui_add_person_set_error(internal, NULL);
            }
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Certificates", NK_TEXT_LEFT);
        for (size_t index = 0U; index < draft->certificate_count; ++index)
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, width - 120.0f);
            nk_label(ctx, draft->certificate_paths[index], NK_TEXT_LEFT);
            nk_layout_row_push(ctx, 60.0f);
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_DELETE, "Remove"))
            {
                ui_add_person_remove_certificate(draft, index);
                --index;
            }
            nk_layout_row_end(ctx);
        }

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(
            internal, ctx, NK_EDIT_FIELD, internal->add_person_form.new_certificate_path,
            (int)sizeof(internal->add_person_form.new_certificate_path), nk_filter_default);
        nk_layout_row_push(ctx, 60.0f);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_ADD, "Add"))
        {
            if (internal->add_person_form.new_certificate_path[0] == '\0')
            {
                ui_add_person_set_error(internal, "Certificate path cannot be empty.");
            }
            else if (draft->certificate_count >= UI_ADD_PERSON_MAX_CERTIFICATES)
            {
                ui_add_person_set_error(internal, "Certificate limit reached.");
            }
            else
            {
                size_t slot = draft->certificate_count;
                (void)snprintf(draft->certificate_paths[slot],
                               sizeof(draft->certificate_paths[slot]), "%s",
                               internal->add_person_form.new_certificate_path);
                draft->certificate_count += 1U;
                internal->add_person_form.new_certificate_path[0] = '\0';
                ui_add_person_set_error(internal, NULL);
            }
        }
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Profile Image", NK_TEXT_LEFT);
        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 1);
        nk_layout_row_push(ctx, width - 40.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->profile_image_path,
                                 (int)sizeof(draft->profile_image_path), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 30.0f, 2);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_SAVE, "Save"))
        {
            char message[192];
            message[0] = '\0';
            if (ui_add_person_validate(internal, tree, message, sizeof(message)))
            {
                internal->add_person_request         = *draft;
                internal->add_person_request_pending = true;
                internal->show_add_person_panel      = false;
                ui_add_person_set_error(internal, NULL);
                ui_internal_set_status(internal, "Add-person request queued.");
                ui_add_person_form_reset(internal);
            }
            else
            {
                ui_add_person_set_error(internal, message);
            }
        }
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_CANCEL, "Cancel"))
        {
            internal->show_add_person_panel = false;
            ui_add_person_form_reset(internal);
        }
    }
    nk_end(ctx);
    ui_theme_pop_panel_alpha(ctx, &token);
}

static void ui_draw_edit_person_panel(UIInternal *internal, UIContext *ui, const FamilyTree *tree)
{
    if (!internal || !ui)
    {
        return;
    }
    bool showing = internal->show_edit_person_panel;
    bool active  = showing || ui_animated_panel_visible(&internal->edit_person_panel_anim);
    if (!active)
    {
        return;
    }
    UIEditPersonRequest *draft = &internal->edit_person_form.draft;
    struct nk_context *ctx     = &internal->ctx;
    float alpha                = ui_animated_panel_alpha(&internal->edit_person_panel_anim);
    if (!(alpha > 0.0f))
    {
        return;
    }
    UIThemePanelStyleToken token;
    ui_theme_push_panel_alpha(ctx, &internal->theme, alpha, &token);
    float width  = 420.0f;
    float height = (float)ui->height - 80.0f;
    if (height < 360.0f)
    {
        height = 360.0f;
    }
    float x = (float)ui->width - width - 24.0f;
    if (x < 18.0f)
    {
        x = 18.0f;
    }
    ui_theme_pop_panel_alpha(ctx, &token);
    struct nk_rect bounds = nk_rect(x, 48.0f, width, height);
    if (nk_begin(ctx, "Edit Person##Panel", bounds,
                 NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_TITLE |
                     NK_WINDOW_SCROLL_AUTO_HIDE))
    {
        nk_layout_row_dynamic(ctx, 26.0f, 1);
        nk_label(ctx, "Edit Holographic Profile", NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_labelf(ctx, NK_TEXT_LEFT, "Person ID: %u", draft->person_id);

        if (internal->edit_person_form.error_message[0] != '\0')
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            nk_label_colored(ctx, internal->edit_person_form.error_message, NK_TEXT_LEFT,
                             internal->theme.warning_color);
        }

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "First", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->first,
                                 (int)sizeof(draft->first), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Middle", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->middle,
                                 (int)sizeof(draft->middle), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Last", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->last,
                                 (int)sizeof(draft->last), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Birth Date", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->birth_date,
                                 (int)sizeof(draft->birth_date), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
        nk_layout_row_push(ctx, 100.0f);
        nk_label(ctx, "Birthplace", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, width - 120.0f);
        (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->birth_location,
                                 (int)sizeof(draft->birth_location), nk_filter_default);
        nk_layout_row_end(ctx);

        nk_layout_row_dynamic(ctx, 24.0f, 1);
        nk_bool deceased = draft->has_death ? nk_true : nk_false;
        (void)ui_nav_checkbox_label(internal, ctx, "Person is deceased", &deceased);
        draft->has_death = (deceased == nk_true);

        if (draft->has_death)
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Death Date", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->death_date,
                                     (int)sizeof(draft->death_date), nk_filter_default);
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Death Place", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            (void)ui_nav_edit_string(internal, ctx, NK_EDIT_FIELD, draft->death_location,
                                     (int)sizeof(draft->death_location), nk_filter_default);
            nk_layout_row_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Relationships", NK_TEXT_LEFT);

        char label_buffer[160];
        const char *father_label = ui_edit_person_relationship_label(
            tree, draft->father_id, label_buffer, sizeof(label_buffer));
        if (nk_group_begin(ctx, "EditFatherGroup", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Father", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, father_label, nk_vec2(260.0f, 260.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->father_id = 0U;
                }
                if (tree)
                {
                    for (size_t index = 0U; index < tree->person_count; ++index)
                    {
                        Person *candidate = tree->persons[index];
                        if (!candidate || candidate->id == draft->person_id)
                        {
                            continue;
                        }
                        char name_buffer[128];
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        (void)snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                       candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->father_id = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_group_end(ctx);
        }

        const char *mother_label = ui_edit_person_relationship_label(
            tree, draft->mother_id, label_buffer, sizeof(label_buffer));
        if (nk_group_begin(ctx, "EditMotherGroup", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, 100.0f);
            nk_label(ctx, "Mother", NK_TEXT_LEFT);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, mother_label, nk_vec2(260.0f, 260.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->mother_id = 0U;
                }
                if (tree)
                {
                    for (size_t index = 0U; index < tree->person_count; ++index)
                    {
                        Person *candidate = tree->persons[index];
                        if (!candidate || candidate->id == draft->person_id)
                        {
                            continue;
                        }
                        char name_buffer[128];
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        (void)snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                       candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->mother_id = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_end(ctx);
            nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 20.0f, 1);
        nk_label(ctx, "Spouses", NK_TEXT_LEFT);
        for (size_t index = 0U; index < draft->spouse_count; ++index)
        {
            uint32_t spouse_id       = draft->spouse_ids[index];
            const char *spouse_label = ui_edit_person_relationship_label(
                tree, spouse_id, label_buffer, sizeof(label_buffer));
            nk_layout_row_begin(ctx, NK_STATIC, 24.0f, 2);
            nk_layout_row_push(ctx, width - 120.0f);
            if (ui_nav_combo_begin_label(internal, ctx, spouse_label, nk_vec2(260.0f, 300.0f)))
            {
                nk_layout_row_dynamic(ctx, 24.0f, 1);
                if (ui_nav_combo_item_label(internal, ctx, "None", NK_TEXT_LEFT))
                {
                    draft->spouse_ids[index] = 0U;
                }
                if (tree)
                {
                    for (size_t candidate_index = 0U; candidate_index < tree->person_count;
                         ++candidate_index)
                    {
                        Person *candidate = tree->persons[candidate_index];
                        if (!candidate || candidate->id == draft->person_id)
                        {
                            continue;
                        }
                        char name_buffer[128];
                        ui_compose_person_name(candidate, name_buffer, sizeof(name_buffer));
                        (void)snprintf(label_buffer, sizeof(label_buffer), "%s (#%u)", name_buffer,
                                       candidate->id);
                        if (ui_nav_combo_item_label(internal, ctx, label_buffer, NK_TEXT_LEFT))
                        {
                            draft->spouse_ids[index] = candidate->id;
                        }
                    }
                }
                nk_combo_end(ctx);
            }
            nk_layout_row_push(ctx, 60.0f);
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_DELETE, "Remove"))
            {
                ui_edit_person_remove_spouse(draft, index);
                --index;
            }
            nk_layout_row_end(ctx);
        }

        if (draft->spouse_count < UI_EDIT_PERSON_MAX_SPOUSES)
        {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_ADD, "Add Spouse Slot"))
            {
                draft->spouse_ids[draft->spouse_count] = 0U;
                draft->spouse_count += 1U;
            }
        }

        nk_layout_row_dynamic(ctx, 6.0f, 1);
        nk_spacing(ctx, 1);

        nk_layout_row_dynamic(ctx, 30.0f, 2);
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_SAVE, "Save Changes"))
        {
            char message[192];
            message[0] = '\0';
            if (ui_edit_person_validate(internal, tree, message, sizeof(message)))
            {
                internal->edit_person_request             = *draft;
                internal->edit_person_request_pending     = true;
                internal->show_edit_person_panel          = false;
                internal->edit_person_form.confirm_delete = false;
                ui_edit_person_set_error(internal, NULL);
                ui_edit_person_form_reset(internal);
                ui_internal_set_status(internal, "Edit-person request queued.");
            }
            else
            {
                ui_edit_person_set_error(internal, message);
            }
        }
        if (ui_nav_button_icon_label(internal, ctx, UI_ICON_CANCEL, "Cancel"))
        {
            internal->show_edit_person_panel          = false;
            internal->edit_person_form.confirm_delete = false;
            ui_edit_person_set_error(internal, NULL);
            ui_edit_person_form_reset(internal);
            ui_internal_set_status(internal, "Edit panel closed.");
        }

        nk_layout_row_dynamic(ctx, internal->edit_person_form.confirm_delete ? 26.0f : 24.0f,
                              internal->edit_person_form.confirm_delete ? 2 : 1);
        if (!internal->edit_person_form.confirm_delete)
        {
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_DELETE, "Delete Person..."))
            {
                internal->edit_person_form.confirm_delete = true;
                ui_internal_set_status(internal, "Confirm deletion to remove profile.");
            }
        }
        else
        {
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_DELETE, "Confirm Delete"))
            {
                if (draft->person_id != 0U &&
                    ui_event_enqueue_with_u32(ui, UI_EVENT_DELETE_PERSON, draft->person_id))
                {
                    internal->show_edit_person_panel = false;
                    ui_edit_person_set_error(internal, NULL);
                    ui_edit_person_form_reset(internal);
                    internal->edit_person_form.confirm_delete = false;
                    ui_internal_set_status(internal, "Delete request queued.");
                }
                else
                {
                    ui_internal_set_status(internal, "Event queue full; delete aborted.");
                }
            }
            if (ui_nav_button_icon_label(internal, ctx, UI_ICON_CANCEL, "Keep Person"))
            {
                internal->edit_person_form.confirm_delete = false;
                ui_internal_set_status(internal, "Deletion cancelled.");
            }
        }
    }
    nk_end(ctx);
    if (nk_window_is_closed(ctx, "Edit Person##Panel"))
    {
        internal->show_edit_person_panel          = false;
        internal->edit_person_form.confirm_delete = false;
        ui_edit_person_set_error(internal, NULL);
        ui_edit_person_form_reset(internal);
    }
}

#endif /* ANCESTRYTREE_HAVE_RAYLIB && ANCESTRYTREE_HAVE_NUKLEAR */

void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout,
                     CameraController *camera, float fps, const Person *selected_person,
                     const Person *hovered_person, RenderConfig *render_config, Settings *settings,
                     bool settings_dirty)
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
    ui_navigation_begin_frame(&internal->nav_state, &internal->nav_input);
    ui_internal_sync_accessibility(internal, ui, settings);
    if (internal->search_last_tree != tree)
    {
        internal->search_last_tree = tree;
        internal->search_dirty     = true;
        ui_search_clear_results(internal);
        internal->search_selected_index = -1;
    }
    ui_draw_menu_bar(internal, ui, tree, layout, camera, render_config, settings_dirty);
    ui_draw_tree_panel(internal, tree, layout, camera, fps, selected_person, hovered_person);
    ui_draw_about_window(internal, ui);
    ui_draw_help_window(internal, ui);
    ui_draw_search_panel(internal, ui, tree);
    ui_draw_analytics_panel(internal, ui, tree);
    ui_draw_add_person_panel(internal, ui, tree);
    ui_draw_edit_person_panel(internal, ui, tree);
    ui_draw_settings_window(internal, ui, settings, settings_dirty);
    ui_draw_exit_prompt(internal, ui);
    ui_draw_error_dialog(internal, ui);
    if (hovered_person)
    {
        ui_draw_hover_tooltip(internal, ui, hovered_person);
    }
    ui_draw_onboarding_overlay(internal, ui);
    ui_draw_progress_overlay(internal, ui);
    ui_accessibility_report_selection(internal, selected_person);
    ui_navigation_end_frame(&internal->nav_state);
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

bool ui_is_available(const UIContext *ui) { return ui ? ui->available : false; }

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
    UIEvent event;
    event.type      = type;
    event.param_u32 = 0U;
    return ui_event_queue_push(&ui->event_queue, &event);
}

bool ui_event_enqueue_with_u32(UIContext *ui, UIEventType type, uint32_t value)
{
    if (!ui)
    {
        return false;
    }
    UIEvent event;
    event.type      = type;
    event.param_u32 = value;
    return ui_event_queue_push(&ui->event_queue, &event);
}

size_t ui_poll_events(UIContext *ui, UIEvent *events, size_t capacity)
{
    if (!ui || !events || capacity == 0U)
    {
        return 0U;
    }
    UIEventQueue *queue = &ui->event_queue;
    size_t count        = queue->count;
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
        events[index]      = queue->events[event_index];
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

bool ui_show_error_dialog(UIContext *ui, const char *title, const char *message)
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
    ui_internal_show_error(internal, title, message);
    return true;
#else
    (void)title;
    (void)message;
    return false;
#endif
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
    if (internal->show_error_dialog)
    {
        internal->show_error_dialog = false;
        dismissed                   = true;
    }
    if (internal->show_exit_prompt)
    {
        internal->show_exit_prompt = false;
        dismissed                  = true;
    }
    if (internal->show_about_window)
    {
        internal->show_about_window = false;
        dismissed                   = true;
    }
    if (internal->show_help_window)
    {
        internal->show_help_window = false;
        dismissed                  = true;
    }
    if (internal->show_search_panel)
    {
        internal->show_search_panel = false;
        dismissed                   = true;
    }
    if (internal->show_add_person_panel)
    {
        internal->show_add_person_panel = false;
        ui_add_person_form_reset(internal);
        dismissed = true;
    }
    if (internal->show_edit_person_panel)
    {
        internal->show_edit_person_panel          = false;
        internal->edit_person_form.confirm_delete = false;
        ui_edit_person_set_error(internal, NULL);
        ui_edit_person_form_reset(internal);
        dismissed = true;
    }
    if (internal->show_settings_window)
    {
        internal->show_settings_window = false;
        dismissed                      = true;
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

bool ui_consume_add_person_request(UIContext *ui, UIAddPersonRequest *out_request)
{
    if (!ui || !out_request)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal || !internal->add_person_request_pending)
    {
        return false;
    }
    *out_request = internal->add_person_request;
    memset(&internal->add_person_request, 0, sizeof(UIAddPersonRequest));
    internal->add_person_request_pending = false;
    return true;
#else
    (void)ui;
    (void)out_request;
    return false;
#endif
}

bool ui_open_edit_person_panel(UIContext *ui, const Person *person)
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
    ui_edit_person_populate(internal, person);
    if (person)
    {
        ui_internal_set_status(internal, "Edit panel opened for selected hologram.");
    }
    return person != NULL;
#else
    (void)person;
    return false;
#endif
}

bool ui_consume_edit_person_request(UIContext *ui, UIEditPersonRequest *out_request)
{
    if (!ui || !out_request)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal || !internal->edit_person_request_pending)
    {
        return false;
    }
    *out_request = internal->edit_person_request;
    memset(&internal->edit_person_request, 0, sizeof(UIEditPersonRequest));
    internal->edit_person_request_pending = false;
    return true;
#else
    (void)ui;
    (void)out_request;
    return false;
#endif
}

bool ui_pointer_over_ui(const UIContext *ui)
{
    if (!ui || !ui->available)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    const UIInternal *internal = ui_internal_const_cast(ui);
    if (!internal)
    {
        return false;
    }
    const struct nk_context *ctx = &internal->ctx;
    if (nk_item_is_any_active(ctx))
    {
        return true;
    }
    if (nk_window_is_any_hovered(ctx))
    {
        return true;
    }
    const struct nk_mouse *mouse = &ctx->input.mouse;
    if ((mouse->grab != 0) || (mouse->grabbed != 0))
    {
        return true;
    }
    return false;
#else
    (void)ui;
    return false;
#endif
}

void ui_onboarding_configure(UIContext *ui, bool active, bool enable_tooltips)
{
    if (!ui)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return;
    }
    if (active && onboarding_is_completed(&internal->onboarding))
    {
        onboarding_reset(&internal->onboarding);
    }
    onboarding_set_active(&internal->onboarding, active);
    onboarding_enable_tooltips(&internal->onboarding, enable_tooltips);
    if (active)
    {
        ui_internal_set_status(internal, "Onboarding started.");
    }
#else
    (void)active;
    (void)enable_tooltips;
#endif
}

void ui_onboarding_restart(UIContext *ui, bool enable_tooltips)
{
    if (!ui)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return;
    }
    onboarding_reset(&internal->onboarding);
    onboarding_enable_tooltips(&internal->onboarding, enable_tooltips);
    onboarding_set_active(&internal->onboarding, true);
    ui_internal_set_status(internal, "Onboarding restarted.");
#else
    (void)enable_tooltips;
#endif
}

bool ui_onboarding_active(const UIContext *ui)
{
    if (!ui || !ui->available)
    {
        return false;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    const UIInternal *internal = ui_internal_const_cast(ui);
    if (!internal)
    {
        return false;
    }
    return onboarding_is_active(&internal->onboarding);
#else
    return false;
#endif
}

void ui_onboarding_mark_completed(UIContext *ui)
{
    if (!ui)
    {
        return;
    }
#if defined(ANCESTRYTREE_HAVE_RAYLIB) && defined(ANCESTRYTREE_HAVE_NUKLEAR)
    UIInternal *internal = ui_internal_cast(ui);
    if (!internal)
    {
        return;
    }
    bool already_completed = onboarding_is_completed(&internal->onboarding);
    onboarding_mark_completed(&internal->onboarding);
    onboarding_set_active(&internal->onboarding, false);
    onboarding_enable_tooltips(&internal->onboarding, false);
    ui_internal_set_status(internal, "Onboarding complete.");
    if (!already_completed)
    {
        UIEvent event;
        event.type      = UI_EVENT_COMPLETE_ONBOARDING;
        event.param_u32 = 0U;
        (void)ui_event_queue_push(&ui->event_queue, &event);
    }
#endif
}

void ui_progress_begin(UIContext *ui, const char *label)
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
    progress_tracker_begin(&internal->progress, label);
    internal->progress_target_value = 0.0f;
    internal->progress_target_valid = false;
    ui_accessibility_progress_begin(internal, label);
    ui_internal_set_status(internal, label ? label : "Operation in progress...");
#else
    (void)label;
#endif
}

void ui_progress_update(UIContext *ui, float value)
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
    if (!progress_tracker_is_active(&internal->progress))
    {
        return;
    }
    float clamped = value;
    if (!(clamped >= 0.0f))
    {
        clamped = 0.0f;
    }
    if (clamped > 1.0f)
    {
        clamped = 1.0f;
    }
    internal->progress_target_value = clamped;
    internal->progress_target_valid = true;
    ui_accessibility_progress_update(internal, clamped);
#else
    (void)value;
#endif
}

void ui_progress_complete(UIContext *ui, bool success, const char *label)
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
    progress_tracker_complete(&internal->progress, success, label);
    internal->progress_target_valid = false;
    ui_accessibility_progress_complete(internal, success, label);
    if (label && label[0] != '\0')
    {
        ui_internal_set_status(internal, label);
    }
    else
    {
        ui_internal_set_status(internal,
                               success ? "Operation completed successfully." : "Operation failed.");
    }
#else
    (void)success;
    (void)label;
#endif
}

/* Manual validation checklist (requires raylib + Nuklear):
 * 1. Launch the application and confirm the menu bar renders with File/View/Help entries.
 * 2. Trigger "Reset Camera" and "Focus Roots" from both the menu and HUD buttons; verify camera
 * motion.
 * 3. Toggle auto orbit via the checkbox or menu and observe status banner updates.
 * 4. Open the About dialog from Help and close it via the button and window close control.
 * 5. Launch the Quick Help overlay, review shortcut descriptions, and close it with both the Close
 * button and the Escape key.
 * 6. Select File->Exit and confirm the confirmation dialog responds and closes the application when
 * accepted.
 * 7. Open View->Settings, tweak sensitivities and graphics quality, then save and reload to confirm
 * persistence and runtime application.
 * 8. Open View->Search / Filter, exercise the name and birth-year filters, and focus a result to
 * verify the camera recenters and the selection highlight updates.
 */
