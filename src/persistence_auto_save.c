#include "persistence.h"

#include "at_string.h"
#include "persistence_internal.h"

#include <assert.h>
#include <stdlib.h>

static bool persistence_auto_save_perform_save(PersistenceAutoSave *state, char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state is NULL");
    }
    if (!state->tree_supplier)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "tree supplier is NULL");
    }
    if (!state->path || state->path[0] == '\0')
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save path is not set");
    }
    FamilyTree *tree = state->tree_supplier(state->user_data);
    if (!tree)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "tree supplier returned NULL");
    }
    if (!persistence_tree_save(tree, state->path, error_buffer, error_buffer_size))
    {
        return false;
    }
    state->dirty = false;
    state->elapsed_seconds = 0.0;
    return true;
}

bool persistence_auto_save_init(PersistenceAutoSave *state, const PersistenceAutoSaveConfig *config,
                                char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state pointer is NULL");
    }
    if (!config)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save config pointer is NULL");
    }
    if (!config->tree_supplier)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "tree supplier callback is required");
    }
    if (!config->path || config->path[0] == '\0')
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save path must be provided");
    }

    char *path_copy = at_string_dup(config->path);
    if (!path_copy)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "failed to duplicate auto-save path");
    }

    state->tree_supplier = config->tree_supplier;
    state->user_data = config->user_data;
    state->path = path_copy;
    state->interval_seconds = config->interval_seconds;
    state->elapsed_seconds = 0.0;
    state->enabled = true;
    state->dirty = false;
    return true;
}

void persistence_auto_save_shutdown(PersistenceAutoSave *state)
{
    if (!state)
    {
        return;
    }
    free(state->path);
    state->path = NULL;
    state->tree_supplier = NULL;
    state->user_data = NULL;
    state->interval_seconds = 0U;
    state->elapsed_seconds = 0.0;
    state->enabled = false;
    state->dirty = false;
}

void persistence_auto_save_mark_dirty(PersistenceAutoSave *state)
{
    if (!state)
    {
        return;
    }
    state->dirty = true;
    state->elapsed_seconds = 0.0;
}

bool persistence_auto_save_tick(PersistenceAutoSave *state, double delta_seconds, char *error_buffer,
                                size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state pointer is NULL");
    }
    if (delta_seconds < 0.0)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "delta time cannot be negative");
    }
    if (!state->enabled)
    {
        return true;
    }
    if (!state->dirty)
    {
        return true;
    }
    if (state->interval_seconds == 0U)
    {
        return true;
    }

    state->elapsed_seconds += delta_seconds;
    if (state->elapsed_seconds + 1e-9 < (double)state->interval_seconds)
    {
        return true;
    }
    return persistence_auto_save_perform_save(state, error_buffer, error_buffer_size);
}

bool persistence_auto_save_flush(PersistenceAutoSave *state, char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state pointer is NULL");
    }
    if (!state->dirty)
    {
        return true;
    }
    return persistence_auto_save_perform_save(state, error_buffer, error_buffer_size);
}

bool persistence_auto_save_update_path(PersistenceAutoSave *state, const char *path, char *error_buffer,
                                       size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state pointer is NULL");
    }
    if (!path || path[0] == '\0')
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save path must be provided");
    }
    char *copy = at_string_dup(path);
    if (!copy)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "failed to duplicate auto-save path");
    }
    free(state->path);
    state->path = copy;
    state->elapsed_seconds = 0.0;
    return true;
}

void persistence_auto_save_set_enabled(PersistenceAutoSave *state, bool enabled)
{
    if (!state)
    {
        return;
    }
    state->enabled = enabled;
    if (!enabled)
    {
        state->elapsed_seconds = 0.0;
    }
}

void persistence_auto_save_set_interval(PersistenceAutoSave *state, unsigned int interval_seconds)
{
    if (!state)
    {
        return;
    }
    state->interval_seconds = interval_seconds;
    state->elapsed_seconds = 0.0;
}

bool persistence_auto_save_set_tree_supplier(PersistenceAutoSave *state, FamilyTree *(*tree_supplier)(void *user_data),
                                             void *user_data, char *error_buffer, size_t error_buffer_size)
{
    if (!state)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "auto-save state pointer is NULL");
    }
    if (!tree_supplier)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "tree supplier callback is required");
    }
    state->tree_supplier = tree_supplier;
    state->user_data = user_data;
    return true;
}
