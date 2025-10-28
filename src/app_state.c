#include "app.h"

#include "at_memory.h"
#include "at_string.h"
#include "layout.h"
#include "person.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    AT_FREE(stack->items);
    stack->items    = NULL;
    stack->count    = 0U;
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
    stack->items    = items;
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
    state->interaction_mode        = APP_INTERACTION_MODE_TREE_VIEW;
    state->active_layout_algorithm = LAYOUT_ALGORITHM_HIERARCHICAL;
    layout_cache_init(&state->layout_cache);
}

bool app_state_configure(AppState *state, FamilyTree **tree, LayoutResult *layout,
                         InteractionState *interaction, CameraController *camera,
                         Settings *settings, Settings *persisted_settings)
{
    if (!state || !tree || !layout || !interaction || !camera || !settings || !persisted_settings)
    {
        return false;
    }
    state->tree               = tree;
    state->layout             = layout;
    state->interaction        = interaction;
    state->camera             = camera;
    state->settings           = settings;
    state->persisted_settings = persisted_settings;
    state->selected_person    = NULL;
    state->tree_dirty         = false;
    layout_result_destroy(&state->layout_transition_start);
    layout_result_destroy(&state->layout_transition_target);
    state->layout_transition_active   = false;
    state->layout_transition_elapsed  = 0.0f;
    state->layout_transition_duration = 0.0f;
    state->active_layout_algorithm    = app_state_resolve_algorithm_from_settings(settings);
    layout_cache_invalidate(&state->layout_cache);
    return true;
}

static void app_state_write_error(char *error_buffer, size_t error_buffer_size, const char *message)
{
    if (!message)
    {
        return;
    }
    if (error_buffer && error_buffer_size > 0U)
    {
        (void)snprintf(error_buffer, error_buffer_size, "%s", message);
    }
}

void app_state_shutdown(AppState *state)
{
    if (!state)
    {
        return;
    }
    app_state_reset_history(state);
    state->tree               = NULL;
    state->layout             = NULL;
    state->interaction        = NULL;
    state->camera             = NULL;
    state->settings           = NULL;
    state->persisted_settings = NULL;
    state->selected_person    = NULL;
    layout_result_destroy(&state->layout_transition_start);
    layout_result_destroy(&state->layout_transition_target);
    state->layout_transition_active = false;
    layout_cache_reset(&state->layout_cache);
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

bool app_state_push_command(AppState *state, AppCommand *command, char *error_buffer,
                            size_t error_buffer_size)
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
    app_state_mark_tree_dirty(state);
    return true;
}

static bool app_state_transfer_command(AppState *state, AppCommandStack *from_stack,
                                       AppCommandStack *to_stack, bool use_execute,
                                       char *error_buffer, size_t error_buffer_size)
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
    bool (*handler)(AppCommand *, AppState *) =
        use_execute ? command->vtable->execute : command->vtable->undo;
    bool (*compensate)(AppCommand *, AppState *) =
        use_execute ? command->vtable->undo : command->vtable->execute;
    if (!handler)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size,
                           use_execute ? "Command lacks execute handler"
                                       : "Command lacks undo handler");
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
    if (!app_state_transfer_command(state, &state->undo_stack, &state->redo_stack, false,
                                    error_buffer, error_buffer_size))
    {
        return false;
    }
    state->tree_dirty = state->undo_stack.count > 0U;
    layout_cache_invalidate(&state->layout_cache);
    return true;
}

bool app_state_redo(AppState *state, char *error_buffer, size_t error_buffer_size)
{
    if (!app_state_transfer_command(state, &state->redo_stack, &state->undo_stack, true,
                                    error_buffer, error_buffer_size))
    {
        return false;
    }
    app_state_mark_tree_dirty(state);
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

static bool app_state_add_unique_person(Person **buffer, size_t *count, size_t capacity,
                                        Person *person)
{
    if (!buffer || !count || capacity == 0U || !person)
    {
        return true;
    }
    for (size_t index = 0U; index < *count; ++index)
    {
        if (buffer[index] == person)
        {
            return true;
        }
    }
    if (*count >= capacity)
    {
        return false;
    }
    buffer[*count] = person;
    *count += 1U;
    return true;
}

static size_t app_state_collect_layout_neighbourhood(Person *person, Person **buffer,
                                                     size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return 0U;
    }
    size_t count    = 0U;
    bool overflowed = false;

    if (!app_state_add_unique_person(buffer, &count, capacity, person))
    {
        overflowed = true;
    }

    if (!person)
    {
        return overflowed ? 0U : count;
    }

    for (size_t slot = 0U; slot < 2U; ++slot)
    {
        Person *parent = person->parents[slot];
        if (!app_state_add_unique_person(buffer, &count, capacity, parent))
        {
            overflowed = true;
        }
        if (parent)
        {
            for (size_t child_index = 0U; child_index < parent->children_count; ++child_index)
            {
                Person *sibling = parent->children[child_index];
                if (!app_state_add_unique_person(buffer, &count, capacity, sibling))
                {
                    overflowed = true;
                }
            }
        }
    }

    for (size_t child_index = 0U; child_index < person->children_count; ++child_index)
    {
        Person *child = person->children[child_index];
        if (!app_state_add_unique_person(buffer, &count, capacity, child))
        {
            overflowed = true;
        }
    }

    for (size_t spouse_index = 0U; spouse_index < person->spouses_count; ++spouse_index)
    {
        Person *partner = person->spouses[spouse_index].partner;
        if (!app_state_add_unique_person(buffer, &count, capacity, partner))
        {
            overflowed = true;
        }
        if (partner)
        {
            for (size_t child_index = 0U; child_index < partner->children_count; ++child_index)
            {
                Person *shared_child = partner->children[child_index];
                if (!app_state_add_unique_person(buffer, &count, capacity, shared_child))
                {
                    overflowed = true;
                }
            }
        }
    }

    return overflowed ? 0U : count;
}

static void app_state_refresh_layout(AppState *state, LayoutAlgorithm algorithm,
                                     bool allow_animation, Person *const *changed,
                                     size_t changed_count)
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
        state->active_layout_algorithm  = algorithm;
        return;
    }

    LayoutResult target;
    bool have_target = false;
    if (changed && changed_count > 0U)
    {
        have_target =
            layout_cache_incremental(&state->layout_cache, tree, algorithm,
                                     (const Person *const *)changed, changed_count, &target);
    }
    if (!have_target)
    {
        if (!layout_cache_calculate(&state->layout_cache, tree, algorithm, &target))
        {
            return;
        }
    }
    if (tree->person_count > 0U && (!target.nodes || target.count == 0U))
    {
        layout_result_destroy(&target);
        return;
    }

    size_t current_count = (state->layout && state->layout->nodes) ? state->layout->count : 0U;
    bool counts_match    = (current_count == target.count);
    bool can_animate     = allow_animation && counts_match && current_count > 0U;

    if (!can_animate)
    {
        layout_result_destroy(state->layout);
        layout_result_move(state->layout, &target);
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active   = false;
        state->layout_transition_elapsed  = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm    = algorithm;
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
        state->layout_transition_active   = false;
        state->layout_transition_elapsed  = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm    = algorithm;
        return;
    }

    if (!layout_result_copy(state->layout, &state->layout_transition_start))
    {
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        layout_result_destroy(state->layout);
        layout_result_move(state->layout, &target);
        layout_result_destroy(&target);
        state->layout_transition_active   = false;
        state->layout_transition_elapsed  = 0.0f;
        state->layout_transition_duration = 0.0f;
        state->active_layout_algorithm    = algorithm;
        return;
    }

    layout_result_destroy(&target);
    state->layout_transition_active   = true;
    state->layout_transition_elapsed  = 0.0f;
    state->layout_transition_duration = 0.9f;
    state->active_layout_algorithm    = algorithm;
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
            app_state_refresh_layout(state, desired, true, NULL, 0U);
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
        state->layout_transition_active   = false;
        state->layout_transition_elapsed  = 0.0f;
        state->layout_transition_duration = 0.0f;
        return;
    }

    if (!layout_animate(&state->layout_transition_start, &state->layout_transition_target, alpha,
                        state->layout))
    {
        layout_result_destroy(state->layout);
        if (!layout_result_copy(state->layout, &state->layout_transition_target))
        {
            layout_result_move(state->layout, &state->layout_transition_target);
        }
        layout_result_destroy(&state->layout_transition_start);
        layout_result_destroy(&state->layout_transition_target);
        state->layout_transition_active   = false;
        state->layout_transition_elapsed  = 0.0f;
        state->layout_transition_duration = 0.0f;
    }
}

bool app_state_add_person(AppState *state, Person *person, char *error_buffer,
                          size_t error_buffer_size)
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
    Person *neighbors[32];
    size_t neighbor_count = app_state_collect_layout_neighbourhood(
        person, neighbors, sizeof(neighbors) / sizeof(neighbors[0]));
    app_state_refresh_layout(state, state->active_layout_algorithm, true,
                             (neighbor_count > 0U) ? neighbors : NULL, neighbor_count);
    app_state_mark_tree_dirty(state);
    return true;
}

static uint32_t app_state_next_person_id(const AppState *state)
{
    if (!state || !state->tree || !*state->tree)
    {
        return 1U;
    }
    const FamilyTree *tree = *state->tree;
    uint32_t max_id        = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!person)
        {
            continue;
        }
        if (person->id > max_id)
        {
            max_id = person->id;
        }
    }
    if (max_id >= UINT32_MAX)
    {
        return 0U;
    }
    return max_id + 1U;
}

bool app_state_create_person(AppState *state, const AppPersonCreateData *data,
                             uint32_t *out_person_id, char *error_buffer, size_t error_buffer_size)
{
    if (!state || !state->tree || !state->layout || !data)
    {
        app_state_write_error(error_buffer, error_buffer_size,
                              "Invalid arguments for person creation");
        return false;
    }

    if (!data->first || data->first[0] == '\0' || !data->last || data->last[0] == '\0')
    {
        app_state_write_error(error_buffer, error_buffer_size, "First and last name are required");
        return false;
    }
    if (!data->birth_date || data->birth_date[0] == '\0')
    {
        app_state_write_error(error_buffer, error_buffer_size, "Birth date is required");
        return false;
    }
    if (data->is_alive && data->death_date && data->death_date[0] != '\0')
    {
        app_state_write_error(error_buffer, error_buffer_size,
                              "Alive persons cannot have a death date");
        return false;
    }
    if (!data->is_alive && (!data->death_date || data->death_date[0] == '\0'))
    {
        app_state_write_error(error_buffer, error_buffer_size,
                              "Deceased persons require a death date");
        return false;
    }
    if (data->certificate_count > APP_PERSON_CREATE_MAX_CERTIFICATES ||
        data->timeline_count > APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES)
    {
        app_state_write_error(error_buffer, error_buffer_size, "Person creation limits exceeded");
        return false;
    }

    uint32_t new_id = app_state_next_person_id(state);
    if (new_id == 0U)
    {
        app_state_write_error(error_buffer, error_buffer_size,
                              "No identifiers available for new person");
        return false;
    }

    Person *person = person_create(new_id);
    if (!person)
    {
        app_state_write_error(error_buffer, error_buffer_size, "Failed to allocate person");
        return false;
    }

    const char *middle_name = (data->middle && data->middle[0] != '\0') ? data->middle : NULL;
    if (!person_set_name(person, data->first, middle_name, data->last))
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size, "Unable to assign person name");
        return false;
    }
    if (!person_set_birth(person, data->birth_date, data->birth_location))
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size, "Invalid birth information");
        return false;
    }
    if (!data->is_alive)
    {
        if (!person_set_death(person, data->death_date, data->death_location))
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size, "Invalid death information");
            return false;
        }
    }
    else
    {
        (void)person_set_death(person, NULL, NULL);
        person->is_alive = true;
    }

    if (!person_set_maiden_name(person, data->maiden_name))
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size, "Failed to assign maiden name");
        return false;
    }
    if (!person_set_blood_type(person, data->blood_type))
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size, "Failed to assign blood type");
        return false;
    }
    person_set_adopted(person, data->is_adopted);

    if (!person_set_profile_image(person, data->profile_image_path))
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size,
                              "Failed to assign profile image path");
        return false;
    }

    for (size_t index = 0U; index < data->certificate_count; ++index)
    {
        const char *certificate = data->certificate_paths[index];
        if (!certificate || certificate[0] == '\0' || !person_add_certificate(person, certificate))
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size, "Invalid certificate path");
            return false;
        }
    }

    for (size_t index = 0U; index < data->timeline_count; ++index)
    {
        const AppPersonCreateTimelineEntry *entry_data = &data->timeline_entries[index];
        if (!entry_data->description || entry_data->description[0] == '\0')
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size,
                                  "Timeline entries require a description");
            return false;
        }
        TimelineEntry entry;
        timeline_entry_init(&entry, entry_data->type);
        bool entry_ok = true;
        if (entry_data->date && entry_data->date[0] != '\0')
        {
            entry_ok = timeline_entry_set_date(&entry, entry_data->date);
        }
        if (entry_ok && entry_data->description && entry_data->description[0] != '\0')
        {
            entry_ok = timeline_entry_set_description(&entry, entry_data->description);
        }
        if (entry_ok && entry_data->location && entry_data->location[0] != '\0')
        {
            entry_ok = timeline_entry_set_location(&entry, entry_data->location);
        }
        if (entry_ok)
        {
            entry_ok = person_add_timeline_entry(person, &entry);
        }
        timeline_entry_reset(&entry);
        if (!entry_ok)
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size,
                                  "Failed to record timeline entry");
            return false;
        }
    }

    FamilyTree *tree = *state->tree;
    if (!tree)
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size,
                              "Tree unavailable for person creation");
        return false;
    }

    if (data->father_id != 0U)
    {
        Person *father = family_tree_find_person(tree, data->father_id);
        if (!father || !person_set_parent(person, father, PERSON_PARENT_FATHER) ||
            !person_add_child(father, person))
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size, "Invalid father selection");
            return false;
        }
    }
    if (data->mother_id != 0U)
    {
        Person *mother = family_tree_find_person(tree, data->mother_id);
        if (!mother || !person_set_parent(person, mother, PERSON_PARENT_MOTHER) ||
            !person_add_child(mother, person))
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size, "Invalid mother selection");
            return false;
        }
    }
    if (data->spouse_id != 0U)
    {
        Person *spouse = family_tree_find_person(tree, data->spouse_id);
        if (!spouse || !person_add_spouse(person, spouse))
        {
            person_destroy(person);
            app_state_write_error(error_buffer, error_buffer_size, "Invalid spouse selection");
            return false;
        }
    }

    AppCommand *command = app_command_create_add_person(person);
    if (!command)
    {
        person_destroy(person);
        app_state_write_error(error_buffer, error_buffer_size,
                              "Failed to create add-person command");
        return false;
    }
    if (!app_state_push_command(state, command, error_buffer, error_buffer_size))
    {
        return false;
    }

    if (out_person_id)
    {
        *out_person_id = new_id;
    }
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

bool app_state_delete_person(AppState *state, uint32_t person_id, char *error_buffer,
                             size_t error_buffer_size)
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
    Person *neighbors[32];
    size_t neighbor_count = app_state_collect_layout_neighbourhood(
        person, neighbors, sizeof(neighbors) / sizeof(neighbors[0]));

    Person *filtered[32];
    size_t filtered_count = 0U;
    for (size_t index = 0U; index < neighbor_count; ++index)
    {
        if (neighbors[index] && neighbors[index] != person)
        {
            filtered[filtered_count++] = neighbors[index];
        }
    }

    app_remove_person_relationship_links(*state->tree, person);
    if (!family_tree_remove_person(*state->tree, person_id))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Failed to remove person %u",
                           person_id);
        }
        return false;
    }
    app_state_refresh_layout(state, state->active_layout_algorithm, false,
                             (filtered_count > 0U) ? filtered : NULL, filtered_count);
    app_state_mark_tree_dirty(state);
    return true;
}

static bool app_command_person_apply_parent(FamilyTree *tree, Person *person, PersonParentSlot slot,
                                            bool present, uint32_t parent_id);
static bool app_command_person_apply_spouses(FamilyTree *tree, Person *person, bool present,
                                             const uint32_t *spouse_ids, size_t spouse_count);

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
            (void)snprintf(error_buffer, error_buffer_size, "Unable to update name for %u",
                           person_id);
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
                (void)snprintf(error_buffer, error_buffer_size, "Unable to clear death info for %u",
                               person_id);
                if (!person_set_maiden_name(person, edit_data->maiden_name))
                {
                    if (error_buffer && error_buffer_size > 0U)
                    {
                        (void)snprintf(error_buffer, error_buffer_size,
                                       "Unable to update maiden name for %u", person_id);
                    }
                    return false;
                }
                if (!person_set_blood_type(person, edit_data->blood_type))
                {
                    if (error_buffer && error_buffer_size > 0U)
                    {
                        (void)snprintf(error_buffer, error_buffer_size,
                                       "Unable to update blood type for %u", person_id);
                    }
                    return false;
                }
                person_set_adopted(person, edit_data->is_adopted);

                if (edit_data->update_profile_image)
                {
                    if (!person_set_profile_image(person, edit_data->profile_image_path))
                    {
                        if (error_buffer && error_buffer_size > 0U)
                        {
                            (void)snprintf(error_buffer, error_buffer_size,
                                           "Unable to update profile image for %u", person_id);
                        }
                        return false;
                    }
                }

                if (edit_data->update_certificates)
                {
                    const char *const *paths = NULL;
                    if (edit_data->certificate_count > 0U)
                    {
                        paths = edit_data->certificate_paths;
                    }
                    if (!person_assign_certificates(person, paths, edit_data->certificate_count))
                    {
                        if (error_buffer && error_buffer_size > 0U)
                        {
                            (void)snprintf(error_buffer, error_buffer_size,
                                           "Unable to update certificates for %u", person_id);
                        }
                        return false;
                    }
                }

                if (edit_data->update_timeline)
                {
                    size_t timeline_count = edit_data->timeline_count;
                    if (timeline_count > APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES)
                    {
                        timeline_count = APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES;
                    }
                    TimelineEntry temp_entries[APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES];
                    bool timeline_ok = true;
                    for (size_t index = 0U; index < timeline_count; ++index)
                    {
                        const AppPersonCreateTimelineEntry *entry_data =
                            &edit_data->timeline_entries[index];
                        timeline_entry_init(&temp_entries[index], entry_data->type);
                        if (entry_data->date && entry_data->date[0] != '\0')
                        {
                            timeline_ok =
                                timeline_entry_set_date(&temp_entries[index], entry_data->date);
                        }
                        if (timeline_ok && entry_data->description &&
                            entry_data->description[0] != '\0')
                        {
                            timeline_ok = timeline_entry_set_description(&temp_entries[index],
                                                                         entry_data->description);
                        }
                        if (timeline_ok && entry_data->location && entry_data->location[0] != '\0')
                        {
                            timeline_ok = timeline_entry_set_location(&temp_entries[index],
                                                                      entry_data->location);
                        }
                        if (!timeline_ok)
                        {
                            break;
                        }
                    }

                    if (timeline_ok)
                    {
                        timeline_ok = person_assign_timeline(person, temp_entries, timeline_count);
                    }

                    for (size_t index = 0U; index < timeline_count; ++index)
                    {
                        timeline_entry_reset(&temp_entries[index]);
                    }

                    if (!timeline_ok)
                    {
                        if (error_buffer && error_buffer_size > 0U)
                        {
                            (void)snprintf(error_buffer, error_buffer_size,
                                           "Unable to update timeline entries for %u", person_id);
                        }
                        return false;
                    }
                }
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
    const AppPersonEditRelationships *relationships = &edit_data->relationships;
    FamilyTree *tree                                = *state->tree;
    if (!app_command_person_apply_parent(tree, person, PERSON_PARENT_FATHER,
                                         relationships->apply_father, relationships->father_id))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size,
                           "Unable to update father selection for %u", person_id);
        }
        return false;
    }
    if (!app_command_person_apply_parent(tree, person, PERSON_PARENT_MOTHER,
                                         relationships->apply_mother, relationships->mother_id))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size,
                           "Unable to update mother selection for %u", person_id);
        }
        return false;
    }
    if (!app_command_person_apply_spouses(tree, person, relationships->apply_spouses,
                                          relationships->spouse_ids, relationships->spouse_count))
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size,
                           "Unable to update spouse relationships for %u", person_id);
        }
        return false;
    }
    app_state_mark_tree_dirty(state);
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
    layout_cache_invalidate(&state->layout_cache);
}

void app_state_clear_tree_dirty(AppState *state)
{
    if (!state)
    {
        return;
    }
    state->tree_dirty = false;
}

void app_state_on_tree_replaced(AppState *state)
{
    if (!state)
    {
        return;
    }
    layout_cache_invalidate(&state->layout_cache);
    layout_result_destroy(&state->layout_transition_start);
    layout_result_destroy(&state->layout_transition_target);
    state->layout_transition_active   = false;
    state->layout_transition_elapsed  = 0.0f;
    state->layout_transition_duration = 0.0f;
    state->selected_person            = NULL;
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
                slot_found           = true;
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
        Person *partner            = record->partner;
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
    app_state_mark_tree_dirty(state);
    state->selected_person = self->person;
    self->inserted         = true;
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
    app_state_mark_tree_dirty(state);
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
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
    app_state_mark_tree_dirty(state);
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
    char *maiden_name;
    char *blood_type;
    char *birth_date;
    char *birth_location;
    char *death_date;
    char *death_location;
    bool is_adopted;
    bool profile_image_present;
    char *profile_image_path;
    bool certificates_present;
    char **certificate_paths;
    size_t certificate_count;
    bool timeline_present;
    TimelineEntry *timeline_entries;
    size_t timeline_count;
    bool father_present;
    uint32_t father_id;
    bool mother_present;
    uint32_t mother_id;
    bool spouses_present;
    uint32_t *spouse_ids;
    size_t spouse_count;
} AppCommandPersonFields;

static void app_command_person_fields_reset(AppCommandPersonFields *fields)
{
    if (!fields)
    {
        return;
    }
    AT_FREE(fields->first);
    AT_FREE(fields->middle);
    AT_FREE(fields->last);
    AT_FREE(fields->maiden_name);
    AT_FREE(fields->blood_type);
    AT_FREE(fields->birth_date);
    AT_FREE(fields->birth_location);
    AT_FREE(fields->death_date);
    AT_FREE(fields->death_location);
    if (fields->certificate_paths)
    {
        for (size_t index = 0U; index < fields->certificate_count; ++index)
        {
            AT_FREE(fields->certificate_paths[index]);
        }
        AT_FREE(fields->certificate_paths);
    }
    if (fields->timeline_entries)
    {
        for (size_t index = 0U; index < fields->timeline_count; ++index)
        {
            timeline_entry_reset(&fields->timeline_entries[index]);
        }
        AT_FREE(fields->timeline_entries);
    }
    AT_FREE(fields->profile_image_path);
    AT_FREE(fields->spouse_ids);
    memset(fields, 0, sizeof(AppCommandPersonFields));
}

static bool app_command_person_fields_capture(AppCommandPersonFields *fields, const Person *person)
{
    if (!fields || !person)
    {
        return false;
    }
    app_command_person_fields_reset(fields);
    fields->first       = at_string_dup(person->name.first);
    fields->middle      = person->name.middle ? at_string_dup(person->name.middle) : NULL;
    fields->last        = at_string_dup(person->name.last);
    fields->maiden_name = person->maiden_name ? at_string_dup(person->maiden_name) : NULL;
    fields->blood_type  = person->blood_type ? at_string_dup(person->blood_type) : NULL;
    fields->birth_date  = at_string_dup(person->dates.birth_date);
    fields->birth_location =
        person->dates.birth_location ? at_string_dup(person->dates.birth_location) : NULL;
    fields->death_date = person->dates.death_date ? at_string_dup(person->dates.death_date) : NULL;
    fields->death_location =
        person->dates.death_location ? at_string_dup(person->dates.death_location) : NULL;
    fields->is_adopted            = person->is_adopted;
    fields->profile_image_present = true;
    fields->profile_image_path =
        person->profile_image_path ? at_string_dup(person->profile_image_path) : NULL;
    fields->certificates_present = true;
    fields->certificate_count    = person->certificate_count;
    if (fields->certificate_count > 0U)
    {
        fields->certificate_paths = (char **)AT_MALLOC(sizeof(char *) * fields->certificate_count);
        if (!fields->certificate_paths)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
        for (size_t index = 0U; index < fields->certificate_count; ++index)
        {
            fields->certificate_paths[index] = person->certificate_paths[index]
                                                   ? at_string_dup(person->certificate_paths[index])
                                                   : NULL;
            if (person->certificate_paths[index] && !fields->certificate_paths[index])
            {
                app_command_person_fields_reset(fields);
                return false;
            }
        }
    }
    fields->timeline_present = true;
    fields->timeline_count   = person->timeline_count;
    if (fields->timeline_count > 0U)
    {
        fields->timeline_entries =
            (TimelineEntry *)AT_MALLOC(sizeof(TimelineEntry) * fields->timeline_count);
        if (!fields->timeline_entries)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
        for (size_t index = 0U; index < fields->timeline_count; ++index)
        {
            timeline_entry_init(&fields->timeline_entries[index],
                                person->timeline_entries[index].type);
            if (!timeline_entry_clone(&person->timeline_entries[index],
                                      &fields->timeline_entries[index]))
            {
                app_command_person_fields_reset(fields);
                return false;
            }
        }
    }
    fields->father_present = true;
    fields->father_id =
        person->parents[PERSON_PARENT_FATHER] ? person->parents[PERSON_PARENT_FATHER]->id : 0U;
    fields->mother_present = true;
    fields->mother_id =
        person->parents[PERSON_PARENT_MOTHER] ? person->parents[PERSON_PARENT_MOTHER]->id : 0U;
    fields->spouses_present = true;
    fields->spouse_count    = person->spouses_count;
    if (fields->spouse_count > 0U)
    {
        fields->spouse_ids = (uint32_t *)AT_MALLOC(sizeof(uint32_t) * fields->spouse_count);
        if (!fields->spouse_ids)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
        for (size_t index = 0U; index < fields->spouse_count; ++index)
        {
            const PersonSpouseRecord *record = &person->spouses[index];
            fields->spouse_ids[index]        = record->partner ? record->partner->id : 0U;
        }
    }
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
    fields->middle =
        edit_data->middle && edit_data->middle[0] != '\0' ? at_string_dup(edit_data->middle) : NULL;
    fields->last           = edit_data->last ? at_string_dup(edit_data->last) : NULL;
    fields->maiden_name    = edit_data->maiden_name && edit_data->maiden_name[0] != '\0'
                                 ? at_string_dup(edit_data->maiden_name)
                                 : NULL;
    fields->blood_type     = edit_data->blood_type && edit_data->blood_type[0] != '\0'
                                 ? at_string_dup(edit_data->blood_type)
                                 : NULL;
    fields->birth_date     = edit_data->birth_date ? at_string_dup(edit_data->birth_date) : NULL;
    fields->birth_location = edit_data->birth_location && edit_data->birth_location[0] != '\0'
                                 ? at_string_dup(edit_data->birth_location)
                                 : NULL;
    if (edit_data->clear_death)
    {
        fields->death_date     = NULL;
        fields->death_location = NULL;
    }
    else
    {
        fields->death_date     = edit_data->death_date && edit_data->death_date[0] != '\0'
                                     ? at_string_dup(edit_data->death_date)
                                     : NULL;
        fields->death_location = edit_data->death_location && edit_data->death_location[0] != '\0'
                                     ? at_string_dup(edit_data->death_location)
                                     : NULL;
    }
    fields->is_adopted            = edit_data->is_adopted;
    fields->profile_image_present = edit_data->update_profile_image;
    if (fields->profile_image_present)
    {
        fields->profile_image_path =
            edit_data->profile_image_path && edit_data->profile_image_path[0] != '\0'
                ? at_string_dup(edit_data->profile_image_path)
                : NULL;
        if (edit_data->profile_image_path && edit_data->profile_image_path[0] != '\0' &&
            !fields->profile_image_path)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
    }

    fields->certificates_present = edit_data->update_certificates;
    fields->certificate_count    = fields->certificates_present ? edit_data->certificate_count : 0U;
    if (fields->certificates_present && fields->certificate_count > 0U)
    {
        fields->certificate_paths = (char **)AT_MALLOC(sizeof(char *) * fields->certificate_count);
        if (!fields->certificate_paths)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
        for (size_t index = 0U; index < fields->certificate_count; ++index)
        {
            const char *source               = edit_data->certificate_paths[index];
            fields->certificate_paths[index] = source ? at_string_dup(source) : NULL;
            if (source && !fields->certificate_paths[index])
            {
                app_command_person_fields_reset(fields);
                return false;
            }
        }
    }

    fields->timeline_present = edit_data->update_timeline;
    size_t timeline_count    = edit_data->timeline_count;
    if (timeline_count > APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES)
    {
        timeline_count = APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES;
    }
    fields->timeline_count = fields->timeline_present ? timeline_count : 0U;
    if (fields->timeline_present && fields->timeline_count > 0U)
    {
        fields->timeline_entries =
            (TimelineEntry *)AT_MALLOC(sizeof(TimelineEntry) * fields->timeline_count);
        if (!fields->timeline_entries)
        {
            app_command_person_fields_reset(fields);
            return false;
        }
        for (size_t index = 0U; index < fields->timeline_count; ++index)
        {
            const AppPersonCreateTimelineEntry *entry_data = &edit_data->timeline_entries[index];
            timeline_entry_init(&fields->timeline_entries[index], entry_data->type);
            bool ok = true;
            if (entry_data->date && entry_data->date[0] != '\0')
            {
                ok = timeline_entry_set_date(&fields->timeline_entries[index], entry_data->date);
            }
            if (ok && entry_data->description && entry_data->description[0] != '\0')
            {
                ok = timeline_entry_set_description(&fields->timeline_entries[index],
                                                    entry_data->description);
            }
            if (ok && entry_data->location && entry_data->location[0] != '\0')
            {
                ok = timeline_entry_set_location(&fields->timeline_entries[index],
                                                 entry_data->location);
            }
            if (!ok)
            {
                app_command_person_fields_reset(fields);
                return false;
            }
        }
    }

    const AppPersonEditRelationships *relationships = &edit_data->relationships;
    if (relationships->apply_father)
    {
        fields->father_present = true;
        fields->father_id      = relationships->father_id;
    }
    if (relationships->apply_mother)
    {
        fields->mother_present = true;
        fields->mother_id      = relationships->mother_id;
    }
    if (relationships->apply_spouses)
    {
        fields->spouses_present = true;
        size_t count            = relationships->spouse_count;
        if (count > APP_PERSON_EDIT_MAX_SPOUSES)
        {
            count = APP_PERSON_EDIT_MAX_SPOUSES;
        }
        fields->spouse_count = count;
        if (count > 0U)
        {
            fields->spouse_ids = (uint32_t *)AT_MALLOC(sizeof(uint32_t) * count);
            if (!fields->spouse_ids)
            {
                app_command_person_fields_reset(fields);
                return false;
            }
            for (size_t index = 0U; index < count; ++index)
            {
                fields->spouse_ids[index] = relationships->spouse_ids[index];
            }
        }
    }
    if (!fields->first || !fields->last || !fields->birth_date)
    {
        app_command_person_fields_reset(fields);
        return false;
    }
    return true;
}

static bool app_command_person_apply_parent(FamilyTree *tree, Person *person, PersonParentSlot slot,
                                            bool present, uint32_t parent_id)
{
    if (!tree || !person)
    {
        return false;
    }
    if (!present)
    {
        return true;
    }
    Person *current_parent = person->parents[slot];
    if (parent_id == 0U)
    {
        if (current_parent && !person_remove_child(current_parent, person))
        {
            return false;
        }
        if (!person_clear_parent(person, slot))
        {
            return false;
        }
        return true;
    }
    Person *parent = family_tree_find_person(tree, parent_id);
    if (!parent || parent == person)
    {
        return false;
    }
    if (current_parent && current_parent != parent)
    {
        if (!person_remove_child(current_parent, person))
        {
            return false;
        }
    }
    if (!person_add_child(parent, person))
    {
        return false;
    }
    if (!person_set_parent(person, parent, slot))
    {
        return false;
    }
    for (size_t other_slot = 0U; other_slot < 2U; ++other_slot)
    {
        if (other_slot == (size_t)slot)
        {
            continue;
        }
        if (person->parents[other_slot] == parent)
        {
            if (!person_clear_parent(person, (PersonParentSlot)other_slot))
            {
                return false;
            }
        }
    }
    return true;
}

static bool app_command_person_apply_spouses(FamilyTree *tree, Person *person, bool present,
                                             const uint32_t *spouse_ids, size_t spouse_count)
{
    if (!tree || !person)
    {
        return false;
    }
    if (!present)
    {
        return true;
    }
    Person *desired_spouses[APP_PERSON_EDIT_MAX_SPOUSES];
    size_t desired_count = 0U;
    if (spouse_count > APP_PERSON_EDIT_MAX_SPOUSES)
    {
        spouse_count = APP_PERSON_EDIT_MAX_SPOUSES;
    }
    for (size_t index = 0U; index < spouse_count; ++index)
    {
        uint32_t spouse_id = spouse_ids ? spouse_ids[index] : 0U;
        if (spouse_id == 0U)
        {
            continue;
        }
        Person *spouse = family_tree_find_person(tree, spouse_id);
        if (!spouse || spouse == person)
        {
            return false;
        }
        bool duplicate = false;
        for (size_t check = 0U; check < desired_count; ++check)
        {
            if (desired_spouses[check] == spouse)
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
        {
            desired_spouses[desired_count++] = spouse;
        }
    }
    size_t existing_index = 0U;
    while (existing_index < person->spouses_count)
    {
        PersonSpouseRecord *record = &person->spouses[existing_index];
        Person *partner            = record->partner;
        bool keep                  = false;
        for (size_t check = 0U; check < desired_count; ++check)
        {
            if (desired_spouses[check] == partner)
            {
                keep = true;
                break;
            }
        }
        if (!keep)
        {
            if (partner && !person_remove_spouse(person, partner))
            {
                return false;
            }
            else if (!partner)
            {
                for (size_t shift = existing_index + 1U; shift < person->spouses_count; ++shift)
                {
                    person->spouses[shift - 1U] = person->spouses[shift];
                }
                person->spouses_count -= 1U;
            }
            continue;
        }
        existing_index += 1U;
    }
    for (size_t index = 0U; index < desired_count; ++index)
    {
        if (!person_add_spouse(person, desired_spouses[index]))
        {
            return false;
        }
    }
    return true;
}

static bool app_command_person_fields_apply(FamilyTree *tree, Person *person,
                                            const AppCommandPersonFields *fields)
{
    if (!tree || !person || !fields)
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
    if (!person_set_maiden_name(person, fields->maiden_name))
    {
        return false;
    }
    if (!person_set_blood_type(person, fields->blood_type))
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
    person_set_adopted(person, fields->is_adopted);
    if (fields->profile_image_present)
    {
        if (!person_set_profile_image(person, fields->profile_image_path))
        {
            return false;
        }
    }
    if (fields->certificates_present)
    {
        const char *const *paths = (fields->certificate_count > 0U)
                                       ? (const char *const *)fields->certificate_paths
                                       : NULL;
        if (!person_assign_certificates(person, paths, fields->certificate_count))
        {
            return false;
        }
    }
    if (fields->timeline_present)
    {
        const TimelineEntry *entries =
            (fields->timeline_count > 0U) ? fields->timeline_entries : NULL;
        if (!person_assign_timeline(person, entries, fields->timeline_count))
        {
            return false;
        }
    }
    if (!app_command_person_apply_parent(tree, person, PERSON_PARENT_FATHER, fields->father_present,
                                         fields->father_id))
    {
        return false;
    }
    if (!app_command_person_apply_parent(tree, person, PERSON_PARENT_MOTHER, fields->mother_present,
                                         fields->mother_id))
    {
        return false;
    }
    if (!app_command_person_apply_spouses(tree, person, fields->spouses_present, fields->spouse_ids,
                                          fields->spouse_count))
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
    if (!app_command_person_fields_apply(*state->tree, person, &self->new_fields))
    {
        return false;
    }
    self->applied_new_state = true;
    state->selected_person  = person;
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
    app_state_mark_tree_dirty(state);
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
    if (!app_command_person_fields_apply(*state->tree, person, &self->original_fields))
    {
        return false;
    }
    self->applied_new_state = false;
    state->selected_person  = person;
    app_state_refresh_layout(state, state->active_layout_algorithm, false, NULL, 0U);
    app_state_mark_tree_dirty(state);
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
    command->person      = person;
    command->inserted    = false;
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
    command->person_id   = person_id;
    command->inserted    = true;
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
    command->person_id   = person_id;
    if (!app_command_person_fields_from_edit_data(&command->new_fields, edit_data))
    {
        app_command_edit_person_destroy((AppCommand *)command);
        return NULL;
    }
    command->has_snapshot      = false;
    command->applied_new_state = false;
    /* Manual QA: cross-check edit form behaviour in holographic panel workflows. */
    return (AppCommand *)command;
}
