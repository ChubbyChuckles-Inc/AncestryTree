#include "ui_navigation.h"

#include <stddef.h>
#include <stdint.h>

void ui_navigation_init(UINavigationState *state)
{
    if (!state)
    {
        return;
    }
    state->active              = false;
    state->current             = -1;
    state->count               = 0U;
    state->count_last_frame    = 0U;
    state->tab_direction       = 0;
    state->activation_pending  = false;
    state->activation_consumed = false;
}

void ui_navigation_begin_frame(UINavigationState *state, const UINavigationInput *input)
{
    if (!state)
    {
        return;
    }

    state->count               = 0U;
    state->tab_direction       = 0;
    state->activation_consumed = false;
    state->activation_pending  = (input && input->activate_pressed);

    size_t available = state->count_last_frame;
    if (input && input->tab_pressed)
    {
        state->tab_direction = (input->tab_with_shift ? -1 : 1);
        if (available > 0U)
        {
            if (!state->active)
            {
                state->active  = true;
                state->current = (state->tab_direction < 0) ? (int)(available - 1U) : 0;
            }
            else
            {
                state->current += state->tab_direction;
                if (state->current < 0)
                {
                    state->current = (int)(available - 1U);
                }
                if ((size_t)state->current >= available)
                {
                    state->current = 0;
                }
            }
        }
    }
    else if (available == 0U)
    {
        state->active  = false;
        state->current = -1;
    }
}

size_t ui_navigation_register_item(UINavigationState *state)
{
    if (!state)
    {
        return SIZE_MAX;
    }
    if (state->count >= UI_NAVIGATION_MAX_ITEMS)
    {
        return SIZE_MAX;
    }
    size_t index = state->count;
    state->count += 1U;
    return index;
}

bool ui_navigation_is_focused(const UINavigationState *state, size_t index)
{
    if (!state)
    {
        return false;
    }
    if (!state->active)
    {
        return false;
    }
    if (state->count == 0U)
    {
        return false;
    }
    return state->current == (int)index;
}

bool ui_navigation_consume_activation(UINavigationState *state)
{
    if (!state)
    {
        return false;
    }
    if (!state->activation_pending || state->activation_consumed)
    {
        return false;
    }
    state->activation_consumed = true;
    return true;
}

void ui_navigation_end_frame(UINavigationState *state)
{
    if (!state)
    {
        return;
    }
    if (state->count == 0U)
    {
        state->active  = false;
        state->current = -1;
    }
    else
    {
        if (state->current < 0)
        {
            state->current = 0;
        }
        if ((size_t)state->current >= state->count)
        {
            state->current = (int)(state->count - 1U);
        }
    }
    state->count_last_frame = state->count;
}
