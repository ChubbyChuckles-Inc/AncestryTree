#include "app.h"

#include "at_memory.h"
#include "at_string.h"
#include "layout.h"
#include "person.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static LayoutAlgorithm app_state_resolve_algorithm_from_settings(const Settings *settings);

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
    state->active_layout_algorithm = LAYOUT_ALGORITHM_HIERARCHICAL;
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
    layout_result_destroy(&state->layout_transition_start);
    layout_result_destroy(&state->layout_transition_target);
    state->layout_transition_active = false;
    state->layout_transition_elapsed = 0.0f;
    state->layout_transition_duration = 0.0f;
    state->active_layout_algorithm = app_state_resolve_algorithm_from_settings(settings);
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
    layout_result_destroy(&state->layout_transition_start);
    layout_result_destroy(&state->layout_transition_target);
    state->layout_transition_active = false;
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

static LayoutAlgorithm app_state_resolve_algorithm_from_settings(const Settings *settings)
{
    if (!settings)
    {
        return LAYOUT_ALGORITHM_HIERARCHICAL;
    }
    return (settings->default_layout_algorithm == SETTINGS_LAYOUT_ALGORITHM_FORCE_DIRECTED)
               ? LAYOUT_ALGORITHM_FORCE_DIRECTED
               : LAYOUT_ALGORITHM_HIERARCHICAL;
}

static void app_state_refresh_layout(AppState *state, LayoutAlgorithm algorithm, bool allow_animation)
{
    if (!state || !state->tree || !state->layout)
    {
        return;
    }

    FamilyTree *tree = state->tree ? *state->tree : NULL;
    if (!tree)
    {
        layout_result_destroy(state->layout);
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active = false;
        state->active_layout_algorithm = algorithm;
        return;
    }

    LayoutResult target = layout_calculate_with_algorithm(tree, algorithm);
    if (tree->person_count > 0U && (!target.nodes || target.count == 0U))
    {
        layout_result_destroy(&target);
        return;
    }

    size_t current_count = (state->layout && state->layout->nodes) ? state->layout->count : 0U;
    bool counts_match = (current_count == target.count);
    bool can_animate = allow_animation && counts_match && current_count > 0U;

    if (!can_animate)
    {
        layout_result_destroy(state->layout);
        layout_result_move(state->layout, &target);
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active = false;
        state->layout_transition_elapsed = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm = algorithm;
        return;
    }

    if (!layout_result_copy(&state->layout_transition_start, state->layout) ||
        !layout_result_copy(&state->layout_transition_target, &target))
    {
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        layout_result_destroy(state->layout);
        layout_result_move(state->layout, &target);
        layout_result_destroy(&target);
        state->layout_transition_active = false;
        state->layout_transition_elapsed = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm = algorithm;
        return;
    }

    if (!layout_result_copy(state->layout, &state->layout_transition_start))
    {
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        layout_result_destroy(state->layout);
        layout_result_move(state->layout, &target);
        layout_result_destroy(&target);
        state->layout_transition_active = false;
        state->layout_transition_elapsed = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm = algorithm;
        return;
    }

    layout_result_destroy(&target);
    state->layout_transition_active = true;
    state->layout_transition_elapsed = 0.0f;
    state->layout_transition_duration = 0.9f;
    state->active_layout_algorithm = algorithm;
}

void app_state_tick(AppState *state, float delta_seconds)
{
    if (!state)
    {
        return;
    }

    if (state->settings)
    {
        LayoutAlgorithm desired = app_state_resolve_algorithm_from_settings(state->settings);
        if (desired != state->active_layout_algorithm)
        {
            app_state_refresh_layout(state, desired, true);
        }
    }

    if (!state->layout_transition_active)
    {
        return;
    }
    if (!state->layout)
    {
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active = false;
        return;
    }

    if (!(delta_seconds > 0.0f))
    {
        delta_seconds = 0.0f;
    }

    state->layout_transition_elapsed += delta_seconds;
    float duration = state->layout_transition_duration;
    if (!(duration > 0.0f))
    {
        duration = 0.0f;
    }
    float alpha = (duration > 0.0f) ? (state->layout_transition_elapsed / duration) : 1.0f;
    if (alpha >= 1.0f)
    {
        layout_result_destroy(state->layout);
        if (!layout_result_copy(state->layout, &state->layout_transition_target))
        {
            layout_result_move(state->layout, &state->layout_transition_target);
        }
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active = false;
        state->layout_transition_elapsed = 0.0f;
        state->layout_transition_duration = 0.0f;
        return;
    }

    if (!layout_animate(&state->layout_transition_start, &state->layout_transition_target, alpha, state->layout))
    {
        layout_result_destroy(state->layout);
        if (!layout_result_copy(state->layout, &state->layout_transition_target))
        {
            layout_result_move(state->layout, &state->layout_transition_target);
        }
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active = false;
        state->layout_transition_elapsed = 0.0f;
        state->layout_transition_duration = 0.0f;
    }
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
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

static void app_state_clear_selection_if_matches(AppState *state, const Person *person)
{
    if (!state)
    {
        return;
    }
    if (state->selected_person == person)
    {
        state->selected_person = NULL;
    }
}

static bool app_restore_person_relationship_links(AppState *state, Person *person)
{
    if (!state || !state->tree || !*state->tree || !person)
    {
        return false;
    }
    FamilyTree *tree = *state->tree;
    for (size_t index = 0U; index < 2U; ++index)
    {
        Person *parent = person->parents[index];
        if (!parent)
        {
            continue;
        }
        if (!family_tree_find_person(tree, parent->id))
        {
            continue;
        }
        if (!person_add_child(parent, person))
        {
            return false;
        }
    }
    for (size_t index = 0U; index < person->children_count; ++index)
    {
        Person *child = person->children[index];
        if (!child)
        {
            continue;
        }
        if (!family_tree_find_person(tree, child->id))
        {
            continue;
        }
        bool slot_found = false;
        for (size_t slot = 0U; slot < 2U; ++slot)
        {
            if (child->parents[slot] == person)
            {
                slot_found = true;
                break;
            }
            if (!child->parents[slot])
            {
                child->parents[slot] = person;
                slot_found = true;
                break;
            }
        }
        if (!slot_found)
        {
            return false;
        }
    }
    for (size_t index = 0U; index < person->spouses_count; ++index)
    {
        PersonSpouseRecord *record = &person->spouses[index];
        Person *partner = record->partner;
        if (!partner)
        {
            continue;
        }
        if (!family_tree_find_person(tree, partner->id))
        {
            continue;
        }
        if (!person_add_spouse(partner, person))
        {
            return false;
        }
        if (!person_set_marriage(partner, person, record->marriage_date, record->marriage_location))
        {
            return false;
        }
    }
    return true;
}

typedef struct AppCommandAddPerson
{
    AppCommand base;
    Person *person;
    bool inserted;
} AppCommandAddPerson;

static bool app_command_add_person_execute(AppCommand *command, AppState *state)
{
    AppCommandAddPerson *self = (AppCommandAddPerson *)command;
    if (!self || !state || !state->tree || !*state->tree || !self->person)
    {
        return false;
    }
    if (self->inserted)
    {
        return true;
    }
    if (!family_tree_add_person(*state->tree, self->person))
    {
        return false;
    }
    if (!app_restore_person_relationship_links(state, self->person))
    {
        (void)family_tree_extract_person(*state->tree, self->person->id);
        return false;
    }
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
    state->tree_dirty = true;
    state->selected_person = self->person;
    self->inserted = true;
    return true;
}

static bool app_command_add_person_undo(AppCommand *command, AppState *state)
{
    AppCommandAddPerson *self = (AppCommandAddPerson *)command;
    if (!self || !state || !state->tree || !*state->tree)
    {
        return false;
    }
    if (!self->inserted)
    {
        return true;
    }
    app_remove_person_relationship_links(*state->tree, self->person);
    if (!family_tree_extract_person(*state->tree, self->person->id))
    {
        return false;
    }
    self->inserted = false;
    app_state_clear_selection_if_matches(state, self->person);
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
    return true;
}

static void app_command_add_person_destroy(AppCommand *command)
{
    AppCommandAddPerson *self = (AppCommandAddPerson *)command;
    if (!self)
    {
        return;
    }
    if (!self->inserted && self->person)
    {
        person_destroy(self->person);
    }
    free(self);
}

static const AppCommandVTable APP_COMMAND_ADD_PERSON_VTABLE = {
    app_command_add_person_execute,
    app_command_add_person_undo,
    app_command_add_person_destroy,
};

typedef struct AppCommandDeletePerson
{
    AppCommand base;
    uint32_t person_id;
    Person *person;
    bool inserted;
    bool selection_was_target;
} AppCommandDeletePerson;

static bool app_command_delete_person_execute(AppCommand *command, AppState *state)
{
    AppCommandDeletePerson *self = (AppCommandDeletePerson *)command;
    if (!self || !state || !state->tree || !*state->tree || self->person_id == 0U)
    {
        return false;
    }
    if (!self->inserted)
    {
        return true;
    }
    Person *person = family_tree_find_person(*state->tree, self->person_id);
    if (!person)
    {
        return false;
    }
    self->selection_was_target = (state->selected_person == person);
    app_remove_person_relationship_links(*state->tree, person);
    self->person = family_tree_extract_person(*state->tree, self->person_id);
    if (!self->person)
    {
        return false;
    }
    self->inserted = false;
    app_state_clear_selection_if_matches(state, person);
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
    state->tree_dirty = true;
    return true;
}

static bool app_command_delete_person_undo(AppCommand *command, AppState *state)
{
    AppCommandDeletePerson *self = (AppCommandDeletePerson *)command;
    if (!self || !state || !state->tree || !*state->tree || !self->person)
    {
        return false;
    }
    if (self->inserted)
    {
        return true;
    }
    if (!family_tree_add_person(*state->tree, self->person))
    {
        return false;
    }
    if (!app_restore_person_relationship_links(state, self->person))
    {
        (void)family_tree_extract_person(*state->tree, self->person->id);
        return false;
    }
    self->inserted = true;
    if (self->selection_was_target)
    {
        state->selected_person = self->person;
    }
    app_state_refresh_layout(state, state->active_layout_algorithm, false);
    state->tree_dirty = true;
    return true;
}

static void app_command_delete_person_destroy(AppCommand *command)
{
    AppCommandDeletePerson *self = (AppCommandDeletePerson *)command;
    if (!self)
    {
        return;
    }
    if (!self->inserted && self->person)
    {
        person_destroy(self->person);
    }
    free(self);
}

static const AppCommandVTable APP_COMMAND_DELETE_PERSON_VTABLE = {
    app_command_delete_person_execute,
    app_command_delete_person_undo,
    app_command_delete_person_destroy,
};

typedef struct AppCommandPersonFields
{
    char *first;
    char *middle;
    char *last;
    char *birth_date;
    char *birth_location;
    char *death_date;
    char *death_location;
} AppCommandPersonFields;

static void app_command_person_fields_reset(AppCommandPersonFields *fields)
{
    if (!fields)
    {
        return;
    }
    free(fields->first);
    free(fields->middle);
    free(fields->last);
    free(fields->birth_date);
    free(fields->birth_location);
    free(fields->death_date);
    free(fields->death_location);
    memset(fields, 0, sizeof(AppCommandPersonFields));
}

static bool app_command_person_fields_capture(AppCommandPersonFields *fields, const Person *person)
{
    if (!fields || !person)
    {
        return false;
    }
    app_command_person_fields_reset(fields);
    fields->first = at_string_dup(person->name.first);
    fields->middle = person->name.middle ? at_string_dup(person->name.middle) : NULL;
    fields->last = at_string_dup(person->name.last);
    fields->birth_date = at_string_dup(person->dates.birth_date);
    fields->birth_location = person->dates.birth_location ? at_string_dup(person->dates.birth_location) : NULL;
    fields->death_date = person->dates.death_date ? at_string_dup(person->dates.death_date) : NULL;
    fields->death_location = person->dates.death_location ? at_string_dup(person->dates.death_location) : NULL;
    if (!fields->first || !fields->last || !fields->birth_date)
    {
        app_command_person_fields_reset(fields);
        return false;
    }
    return true;
}

static bool app_command_person_fields_from_edit_data(AppCommandPersonFields *fields,
                                                     const AppPersonEditData *edit_data)
{
    if (!fields || !edit_data)
    {
        return false;
    }
    app_command_person_fields_reset(fields);
    fields->first = edit_data->first ? at_string_dup(edit_data->first) : NULL;
    fields->middle = edit_data->middle && edit_data->middle[0] != '\0' ? at_string_dup(edit_data->middle) : NULL;
    fields->last = edit_data->last ? at_string_dup(edit_data->last) : NULL;
    fields->birth_date = edit_data->birth_date ? at_string_dup(edit_data->birth_date) : NULL;
    fields->birth_location = edit_data->birth_location && edit_data->birth_location[0] != '\0'
                                 ? at_string_dup(edit_data->birth_location)
                                 : NULL;
    if (edit_data->clear_death)
    {
        fields->death_date = NULL;
        fields->death_location = NULL;
    }
    else
    {
        fields->death_date = edit_data->death_date && edit_data->death_date[0] != '\0'
                                 ? at_string_dup(edit_data->death_date)
                                 : NULL;
        fields->death_location = edit_data->death_location && edit_data->death_location[0] != '\0'
                                     ? at_string_dup(edit_data->death_location)
                                     : NULL;
    }
    if (!fields->first || !fields->last || !fields->birth_date)
    {
        app_command_person_fields_reset(fields);
        return false;
    }
    return true;
}

static bool app_command_person_fields_apply(Person *person, const AppCommandPersonFields *fields)
{
    if (!person || !fields)
    {
        return false;
    }
    if (!person_set_name(person, fields->first, fields->middle, fields->last))
    {
        return false;
    }
    if (!person_set_birth(person, fields->birth_date, fields->birth_location))
    {
        return false;
    }
    if (fields->death_date)
    {
        if (!person_set_death(person, fields->death_date, fields->death_location))
        {
            return false;
        }
    }
    else if (!person_set_death(person, NULL, NULL))
    {
        return false;
    }
    return true;
}

typedef struct AppCommandEditPerson
{
    AppCommand base;
    uint32_t person_id;
    AppCommandPersonFields original_fields;
    AppCommandPersonFields new_fields;
    bool has_snapshot;
    bool applied_new_state;
} AppCommandEditPerson;

static bool app_command_edit_person_execute(AppCommand *command, AppState *state)
{
    AppCommandEditPerson *self = (AppCommandEditPerson *)command;
    if (!self || !state || !state->tree || !*state->tree || self->person_id == 0U)
    {
        return false;
    }
    Person *person = family_tree_find_person(*state->tree, self->person_id);
    if (!person)
    {
        return false;
    }
    if (!self->has_snapshot)
    {
        if (!app_command_person_fields_capture(&self->original_fields, person))
        {
            return false;
        }
        self->has_snapshot = true;
    }
    if (!app_command_person_fields_apply(person, &self->new_fields))
    {
        return false;
    }
    self->applied_new_state = true;
    state->tree_dirty = true;
    return true;
}

static bool app_command_edit_person_undo(AppCommand *command, AppState *state)
{
    AppCommandEditPerson *self = (AppCommandEditPerson *)command;
    if (!self || !state || !state->tree || !*state->tree || !self->has_snapshot)
    {
        return false;
    }
    Person *person = family_tree_find_person(*state->tree, self->person_id);
    if (!person)
    {
        return false;
    }
    if (!app_command_person_fields_apply(person, &self->original_fields))
    {
        return false;
    }
    self->applied_new_state = false;
    state->tree_dirty = true;
    return true;
}

static void app_command_edit_person_destroy(AppCommand *command)
{
    AppCommandEditPerson *self = (AppCommandEditPerson *)command;
    if (!self)
    {
        return;
    }
    app_command_person_fields_reset(&self->original_fields);
    app_command_person_fields_reset(&self->new_fields);
    free(self);
}

static const AppCommandVTable APP_COMMAND_EDIT_PERSON_VTABLE = {
    app_command_edit_person_execute,
    app_command_edit_person_undo,
    app_command_edit_person_destroy,
};

AppCommand *app_command_create_add_person(Person *person)
{
    if (!person)
    {
        return NULL;
    }
    AppCommandAddPerson *command = calloc(1U, sizeof(AppCommandAddPerson));
    if (!command)
    {
        return NULL;
    }
    command->base.vtable = &APP_COMMAND_ADD_PERSON_VTABLE;
    command->person = person;
    command->inserted = false;
    /* Manual QA: verify holographic add/undo cycle via UI panel once available. */
    return (AppCommand *)command;
}

AppCommand *app_command_create_delete_person(uint32_t person_id)
{
    if (person_id == 0U)
    {
        return NULL;
    }
    AppCommandDeletePerson *command = calloc(1U, sizeof(AppCommandDeletePerson));
    if (!command)
    {
        return NULL;
    }
    command->base.vtable = &APP_COMMAND_DELETE_PERSON_VTABLE;
    command->person_id = person_id;
    command->inserted = true;
    /* Manual QA: exercise delete/undo from UI context when panel wiring lands. */
    return (AppCommand *)command;
}

AppCommand *app_command_create_edit_person(uint32_t person_id, const AppPersonEditData *edit_data)
{
    if (person_id == 0U || !edit_data)
    {
        return NULL;
    }
    AppCommandEditPerson *command = calloc(1U, sizeof(AppCommandEditPerson));
    if (!command)
    {
        return NULL;
    }
    command->base.vtable = &APP_COMMAND_EDIT_PERSON_VTABLE;
    command->person_id = person_id;
    if (!app_command_person_fields_from_edit_data(&command->new_fields, edit_data))
    {
        app_command_edit_person_destroy((AppCommand *)command);
        return NULL;
    }
    command->has_snapshot = false;
    command->applied_new_state = false;
    /* Manual QA: cross-check edit form behaviour in holographic panel workflows. */
    return (AppCommand *)command;
}
