#include "app.h"

#include "at_memory.h"
#include "layout.h"
#include "person.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void app_command_stack_clear(AppCommandStack *stack)
{
    if (!stack)
    {
        return;
    }
    while (stack->count > 0U)
    {
        AppCommand *command = stack->items[stack->count - 1U];
        if (command && command->vtable && command->vtable->destroy)
        {
            command->vtable->destroy(command);
        }
        stack->count -= 1U;
    }
}

static void app_command_stack_reset(AppCommandStack *stack)
{
    if (!stack)
    {
        return;
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0U;
    stack->capacity = 0U;
}

static bool app_command_stack_reserve(AppCommandStack *stack, size_t capacity)
{
    if (!stack)
    {
        return false;
    }
    if (stack->capacity >= capacity)
    {
        return true;
    }
    AppCommand **items = at_secure_realloc(stack->items, capacity, sizeof(AppCommand *));
    if (!items)
    {
        return false;
    }
    stack->items = items;
    stack->capacity = capacity;
    return true;
}

static bool app_command_stack_push(AppCommandStack *stack, AppCommand *command)
{
    if (!stack || !command)
    {
        return false;
    }
    if (!app_command_stack_reserve(stack, stack->count + 1U))
    {
        return false;
    }
    stack->items[stack->count++] = command;
    return true;
}

static AppCommand *app_command_stack_pop(AppCommandStack *stack)
{
    if (!stack || stack->count == 0U)
    {
        return NULL;
    }
    stack->count -= 1U;
    return stack->items[stack->count];
}

void app_state_init(AppState *state)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(AppState));
    state->interaction_mode = APP_INTERACTION_MODE_TREE_VIEW;
}

bool app_state_configure(AppState *state, FamilyTree **tree, LayoutResult *layout, InteractionState *interaction,
                         CameraController *camera, Settings *settings, Settings *persisted_settings)
{
    if (!state || !tree || !layout || !interaction || !camera || !settings || !persisted_settings)
    {
        return false;
    }
    state->tree = tree;
    state->layout = layout;
    state->interaction = interaction;
    state->camera = camera;
    state->settings = settings;
    state->persisted_settings = persisted_settings;
    state->selected_person = NULL;
    state->tree_dirty = false;
    return true;
}

void app_state_shutdown(AppState *state)
{
    if (!state)
    {
        return;
    }
    app_state_reset_history(state);
    state->tree = NULL;
    state->layout = NULL;
    state->interaction = NULL;
    state->camera = NULL;
    state->settings = NULL;
    state->persisted_settings = NULL;
    state->selected_person = NULL;
}

void app_state_reset_history(AppState *state)
{
    if (!state)
    {
        return;
    }
    app_command_stack_clear(&state->undo_stack);
    app_command_stack_clear(&state->redo_stack);
    app_command_stack_reset(&state->undo_stack);
    app_command_stack_reset(&state->redo_stack);
    state->tree_dirty = false;
}

bool app_state_push_command(AppState *state, AppCommand *command, char *error_buffer, size_t error_buffer_size)
{
    if (!state || !command)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid command push request");
        }
        return false;
    }
    if (!command->vtable || !command->vtable->execute)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Command lacks execute handler");
        }
        if (command->vtable && command->vtable->destroy)
        {
            command->vtable->destroy(command);
        }
        return false;
    }
    if (!command->vtable->execute(command, state))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Command execution failed");
        }
        if (command->vtable->destroy)
        {
            command->vtable->destroy(command);
        }
        return false;
    }
    if (!app_command_stack_push(&state->undo_stack, command))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Undo stack overflow");
        }
        if (command->vtable->undo)
        {
            (void)command->vtable->undo(command, state);
        }
        if (command->vtable->destroy)
        {
            command->vtable->destroy(command);
        }
        return false;
    }
    app_command_stack_clear(&state->redo_stack);
    state->tree_dirty = true;
    return true;
}

static bool app_state_transfer_command(AppState *state, AppCommandStack *from_stack, AppCommandStack *to_stack,
                                       bool use_execute, char *error_buffer, size_t error_buffer_size)
{
    if (!state || !from_stack || !to_stack)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid stack operation");
        }
        return false;
    }
    AppCommand *command = app_command_stack_pop(from_stack);
    if (!command)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "No command to apply");
        }
        return false;
    }
    if (!command->vtable)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Command lacks vtable definition");
        }
        (void)app_command_stack_push(from_stack, command);
        return false;
    }
    bool (*handler)(AppCommand *, AppState *) = use_execute ? command->vtable->execute : command->vtable->undo;
    bool (*compensate)(AppCommand *, AppState *) = use_execute ? command->vtable->undo : command->vtable->execute;
    if (!handler)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, use_execute ? "Command lacks execute handler" : "Command lacks undo handler");
        }
        (void)app_command_stack_push(from_stack, command);
        return false;
    }
    if (!handler(command, state))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size,
                           use_execute ? "Redo operation failed" : "Undo operation failed");
        }
        (void)app_command_stack_push(from_stack, command);
        return false;
    }
    if (!app_command_stack_push(to_stack, command))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "History stack overflow");
        }
        if (compensate)
        {
            (void)compensate(command, state);
        }
        (void)app_command_stack_push(from_stack, command);
        return false;
    }
    return true;
}

bool app_state_undo(AppState *state, char *error_buffer, size_t error_buffer_size)
{
    if (!app_state_transfer_command(state, &state->undo_stack, &state->redo_stack, false, error_buffer,
                                    error_buffer_size))
    {
        return false;
    }
    state->tree_dirty = state->undo_stack.count > 0U;
    return true;
}

bool app_state_redo(AppState *state, char *error_buffer, size_t error_buffer_size)
{
    if (!app_state_transfer_command(state, &state->redo_stack, &state->undo_stack, true, error_buffer,
                                    error_buffer_size))
    {
        return false;
    }
    state->tree_dirty = true;
    return true;
}

static void app_state_refresh_layout(AppState *state)
{
    if (!state || !state->tree || !state->layout)
    {
        return;
    }
    layout_result_destroy(state->layout);
    *state->layout = layout_calculate(*state->tree);
}

bool app_state_add_person(AppState *state, Person *person, char *error_buffer, size_t error_buffer_size)
{
    if (!state || !state->tree || !person)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid add person request");
        }
        return false;
    }
    if (!family_tree_add_person(*state->tree, person))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Failed to append person to tree");
        }
        return false;
    }
    app_state_refresh_layout(state);
    state->tree_dirty = true;
    return true;
}

static void app_remove_person_relationship_links(FamilyTree *tree, Person *person)
{
    if (!tree || !person)
    {
        return;
    }
    for (size_t index = 0; index < person->children_count; ++index)
    {
        Person *child = person->children[index];
        if (!child)
        {
            continue;
        }
        for (size_t slot = 0; slot < 2U; ++slot)
        {
            if (child->parents[slot] == person)
            {
                child->parents[slot] = NULL;
            }
        }
    }
    for (size_t slot = 0; slot < 2U; ++slot)
    {
        Person *parent = person->parents[slot];
        if (!parent)
        {
            continue;
        }
        for (size_t child_index = 0; child_index < parent->children_count; ++child_index)
        {
            if (parent->children[child_index] == person)
            {
                for (size_t shift = child_index + 1U; shift < parent->children_count; ++shift)
                {
                    parent->children[shift - 1U] = parent->children[shift];
                }
                parent->children_count -= 1U;
                break;
            }
        }
    }
    for (size_t index = 0; index < person->spouses_count; ++index)
    {
        Person *spouse = person->spouses[index].partner;
        if (!spouse)
        {
            continue;
        }
        for (size_t spouse_index = 0; spouse_index < spouse->spouses_count; ++spouse_index)
        {
            if (spouse->spouses[spouse_index].partner == person)
            {
                for (size_t shift = spouse_index + 1U; shift < spouse->spouses_count; ++shift)
                {
                    spouse->spouses[shift - 1U] = spouse->spouses[shift];
                }
                spouse->spouses_count -= 1U;
                break;
            }
        }
    }
}

bool app_state_delete_person(AppState *state, uint32_t person_id, char *error_buffer, size_t error_buffer_size)
{
    if (!state || !state->tree || !*state->tree || person_id == 0U)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid delete request");
        }
        return false;
    }
    Person *person = family_tree_find_person(*state->tree, person_id);
    if (!person)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u not found", person_id);
        }
        return false;
    }
    app_remove_person_relationship_links(*state->tree, person);
    if (!family_tree_remove_person(*state->tree, person_id))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Failed to remove person %u", person_id);
        }
        return false;
    }
    app_state_refresh_layout(state);
    state->tree_dirty = true;
    return true;
}

bool app_state_edit_person(AppState *state, uint32_t person_id, const AppPersonEditData *edit_data,
                           char *error_buffer, size_t error_buffer_size)
{
    if (!state || !state->tree || !*state->tree || person_id == 0U)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid edit request");
        }
        return false;
    }
    Person *person = family_tree_find_person(*state->tree, person_id);
    if (!person)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Person %u not found", person_id);
        }
        return false;
    }
    if (!edit_data)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Missing edit data");
        }
        return false;
    }
    if (!person_set_name(person, edit_data->first, edit_data->middle, edit_data->last))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Unable to update name for %u", person_id);
        }
        return false;
    }
    if (!person_set_birth(person, edit_data->birth_date, edit_data->birth_location))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid birth data for %u", person_id);
        }
        return false;
    }
    if (edit_data->clear_death)
    {
        if (!person_set_death(person, NULL, NULL))
        {
            if (error_buffer && error_buffer_size > 0U)
            {
                (void)snprintf(error_buffer, error_buffer_size, "Unable to clear death info for %u", person_id);
            }
            return false;
        }
    }
    else if (!person_set_death(person, edit_data->death_date, edit_data->death_location))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid death data for %u", person_id);
        }
        return false;
    }
    state->tree_dirty = true;
    return true;
}

bool app_state_is_tree_dirty(const AppState *state)
{
    if (!state)
    {
        return false;
    }
    return state->tree_dirty;
}

bool app_state_is_history_empty(const AppState *state)
{
    if (!state)
    {
        return true;
    }
    return state->undo_stack.count == 0U && state->redo_stack.count == 0U;
}

void app_state_mark_tree_dirty(AppState *state)
{
    if (!state)
    {
        return;
    }
    state->tree_dirty = true;
}

void app_state_clear_tree_dirty(AppState *state)
{
    if (!state)
    {
        return;
    }
    state->tree_dirty = false;
}
