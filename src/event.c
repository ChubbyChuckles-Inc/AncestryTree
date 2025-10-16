#include "event.h"
#include "app.h"

#include <stddef.h>

void event_process(EventProcessContext *context, EventProcessPhase phase, float delta_seconds)
{
    (void)delta_seconds;
    if (!context)
    {
        return;
    }

    switch (phase)
    {
    case EVENT_PROCESS_PHASE_PRE_FRAME:
    {
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        bool resized = false;
#endif
        if (context->graphics_state)
        {
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
            resized = graphics_window_handle_resize(context->graphics_state);
#else
            (void)graphics_window_handle_resize;
#endif
        }
#if defined(ANCESTRYTREE_HAVE_RAYLIB)
        if (resized)
        {
            if (context->ui)
            {
                ui_resize(context->ui, context->graphics_state->width, context->graphics_state->height);
            }
            if (context->render_ready && context->render_state)
            {
                bool resize_ok = render_resize(context->render_state, context->graphics_state->width,
                                               context->graphics_state->height, context->render_error_buffer,
                                               context->render_error_capacity);
                if (!resize_ok)
                {
                    bool already_warned = context->render_target_warned && *context->render_target_warned;
                    if (!already_warned && context->logger)
                    {
                        const char *message = (context->render_error_buffer && context->render_error_buffer[0] != '\0')
                                                  ? context->render_error_buffer
                                                  : "render target resize failed";
                        AT_LOG(context->logger, AT_LOG_WARN, "%s", message);
                    }
                    if (context->render_target_warned)
                    {
                        *context->render_target_warned = true;
                    }
                }
                else if (context->render_target_warned && *context->render_target_warned)
                {
                    if (context->logger)
                    {
                        AT_LOG(context->logger, AT_LOG_INFO, "Render target restored after resize.");
                    }
                    *context->render_target_warned = false;
                }
            }
            if (context->render_error_buffer && context->render_error_capacity > 0U)
            {
                context->render_error_buffer[0] = '\0';
            }
        }

        if (context->interaction_state && context->layout && context->camera)
        {
            float mouse_x = context->mouse_x;
            float mouse_y = context->mouse_y;
            InteractionState *interaction = context->interaction_state;
            interaction_update_hover(interaction, context->layout, context->camera, mouse_x, mouse_y);
            if (context->mouse_left_pressed)
            {
                bool keep_selection = context->shift_down;
                if (!context->pointer_blocked)
                {
                    interaction_select_at_cursor(interaction, context->layout, context->camera, mouse_x, mouse_y,
                                                 !keep_selection);
                }
            }
        }
#endif
        if (context->shortcut_handler)
        {
            context->shortcut_handler(context->shortcut_user_data, delta_seconds);
        }
    }
    break;
    case EVENT_PROCESS_PHASE_POST_FRAME:
        if (context->queue_handler)
        {
            context->queue_handler(context->queue_user_data, delta_seconds);
        }
        break;
    default:
        break;
    }
}
