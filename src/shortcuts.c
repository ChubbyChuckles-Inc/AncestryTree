#include "shortcuts.h"

#include <stddef.h>

void shortcuts_evaluate(const ShortcutState *state, ShortcutResult *result)
{
    if (!result)
    {
        return;
    }
    result->event = UI_EVENT_NONE;
    if (!state)
    {
        return;
    }

    if (state->ctrl_down)
    {
        if (state->shift_down && state->key_save_pressed)
        {
            result->event = UI_EVENT_SAVE_TREE_AS;
            return;
        }
        if (state->key_new_pressed)
        {
            result->event = UI_EVENT_NEW_TREE;
            return;
        }
        if (state->key_open_pressed)
        {
            result->event = UI_EVENT_OPEN_TREE;
            return;
        }
        if (state->key_save_pressed)
        {
            result->event = UI_EVENT_SAVE_TREE;
            return;
        }
        if (state->key_undo_pressed)
        {
            result->event = UI_EVENT_UNDO;
            return;
        }
        if (state->key_redo_pressed)
        {
            result->event = UI_EVENT_REDO;
            return;
        }
    }

    if (state->key_space_pressed)
    {
        result->event = UI_EVENT_RESET_CAMERA;
        return;
    }

    if (state->key_escape_pressed)
    {
        result->event = UI_EVENT_ESCAPE;
    }
}
