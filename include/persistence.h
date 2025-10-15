#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "tree.h"
#include "persistence_schema.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct PersistenceAutoSaveConfig PersistenceAutoSaveConfig;
typedef struct PersistenceAutoSave PersistenceAutoSave;

struct PersistenceAutoSaveConfig
{
    FamilyTree *(*tree_supplier)(void *user_data);
    void *user_data;
    const char *path;
    unsigned int interval_seconds;
};

struct PersistenceAutoSave
{
    FamilyTree *(*tree_supplier)(void *user_data);
    void *user_data;
    char *path;
    unsigned int interval_seconds;
    double elapsed_seconds;
    bool enabled;
    bool dirty;
};

#ifdef __cplusplus
extern "C"
{
#endif

    bool persistence_tree_save(const FamilyTree *tree, const char *path, char *error_buffer, size_t error_buffer_size);
    FamilyTree *persistence_tree_load(const char *path, char *error_buffer, size_t error_buffer_size);

    bool persistence_auto_save_init(PersistenceAutoSave *state, const PersistenceAutoSaveConfig *config,
                                    char *error_buffer, size_t error_buffer_size);
    void persistence_auto_save_shutdown(PersistenceAutoSave *state);
    void persistence_auto_save_mark_dirty(PersistenceAutoSave *state);
    bool persistence_auto_save_tick(PersistenceAutoSave *state, double delta_seconds, char *error_buffer,
                                    size_t error_buffer_size);
    bool persistence_auto_save_flush(PersistenceAutoSave *state, char *error_buffer, size_t error_buffer_size);
    bool persistence_auto_save_update_path(PersistenceAutoSave *state, const char *path, char *error_buffer,
                                           size_t error_buffer_size);
    void persistence_auto_save_set_enabled(PersistenceAutoSave *state, bool enabled);
    bool persistence_auto_save_set_tree_supplier(PersistenceAutoSave *state,
                                                 FamilyTree *(*tree_supplier)(void *user_data), void *user_data,
                                                 char *error_buffer, size_t error_buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* PERSISTENCE_H */
