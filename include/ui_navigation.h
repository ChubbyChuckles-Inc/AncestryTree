#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include <stdbool.h>
#include <stddef.h>

#define UI_NAVIGATION_MAX_ITEMS 256U

typedef struct UINavigationInput
{
    bool tab_pressed;
    bool tab_with_shift;
    bool activate_pressed;
    bool arrow_left_pressed;
    bool arrow_right_pressed;
    bool arrow_up_pressed;
    bool arrow_down_pressed;
} UINavigationInput;

typedef struct UINavigationState
{
    bool active;
    int current;
    size_t count;
    size_t count_last_frame;
    int tab_direction;
    bool activation_pending;
    bool activation_consumed;
} UINavigationState;

void ui_navigation_init(UINavigationState *state);
void ui_navigation_begin_frame(UINavigationState *state, const UINavigationInput *input);
size_t ui_navigation_register_item(UINavigationState *state);
bool ui_navigation_is_focused(const UINavigationState *state, size_t index);
bool ui_navigation_consume_activation(UINavigationState *state);
void ui_navigation_end_frame(UINavigationState *state);

#endif /* UI_NAVIGATION_H */
