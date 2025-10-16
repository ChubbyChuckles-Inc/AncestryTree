#ifndef SHORTCUTS_H
#define SHORTCUTS_H

#include "ui.h"

#include <stdbool.h>

typedef struct ShortcutState
{
    bool ctrl_down;
    bool shift_down;
    bool key_new_pressed;
    bool key_open_pressed;
    bool key_save_pressed;
    bool key_undo_pressed;
    bool key_redo_pressed;
    bool key_space_pressed;
    bool key_escape_pressed;
} ShortcutState;

typedef struct ShortcutResult
{
    UIEventType event;
} ShortcutResult;

void shortcuts_evaluate(const ShortcutState *state, ShortcutResult *result);

#endif /* SHORTCUTS_H */
