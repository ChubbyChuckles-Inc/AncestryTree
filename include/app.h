#ifndef APP_H
#define APP_H

#include "camera_controller.h"
#include "interaction.h"
#include "layout.h"
#include "person.h"
#include "settings.h"
#include "tree.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct AppUIState
    {
        bool show_add_person_panel;
        bool show_edit_person_panel;
        bool show_settings_window;
        bool show_search_panel;
        bool show_about_window;
        bool show_help_window;
        bool show_exit_prompt;
    } AppUIState;

    typedef enum AppInteractionMode
    {
        APP_INTERACTION_MODE_TREE_VIEW = 0,
        APP_INTERACTION_MODE_DETAIL_VIEW,
        APP_INTERACTION_MODE_EDIT_PERSON
    } AppInteractionMode;

    typedef struct AppFileState
    {
        char current_path[512];
    } AppFileState;

    struct AppState;

    typedef struct AppCommand AppCommand;

    typedef struct AppCommandVTable
    {
        bool (*execute)(AppCommand *command, struct AppState *state);
        bool (*undo)(AppCommand *command, struct AppState *state);
        void (*destroy)(AppCommand *command);
    } AppCommandVTable;

    struct AppCommand
    {
        const AppCommandVTable *vtable;
    };

    typedef struct AppCommandStack
    {
        AppCommand **items;
        size_t count;
        size_t capacity;
    } AppCommandStack;

    typedef struct AppState
    {
        FamilyTree **tree;
        LayoutResult *layout;
        InteractionState *interaction;
        CameraController *camera;
        Settings *settings;
        Settings *persisted_settings;
        Person *selected_person;
        AppUIState ui_state;
        AppInteractionMode interaction_mode;
        AppCommandStack undo_stack;
        AppCommandStack redo_stack;
        bool tree_dirty;
    } AppState;

    typedef struct AppPersonEditData
    {
        const char *first;
        const char *middle;
        const char *last;
        const char *birth_date;
        const char *birth_location;
        const char *death_date;
        const char *death_location;
        bool clear_death;
    } AppPersonEditData;

    void app_state_init(AppState *state);
    bool app_state_configure(AppState *state, FamilyTree **tree, LayoutResult *layout, InteractionState *interaction,
                             CameraController *camera, Settings *settings, Settings *persisted_settings);
    void app_state_shutdown(AppState *state);
    void app_state_reset_history(AppState *state);

    bool app_state_push_command(AppState *state, AppCommand *command, char *error_buffer, size_t error_buffer_size);
    bool app_state_undo(AppState *state, char *error_buffer, size_t error_buffer_size);
    bool app_state_redo(AppState *state, char *error_buffer, size_t error_buffer_size);

    bool app_state_add_person(AppState *state, Person *person, char *error_buffer, size_t error_buffer_size);
    bool app_state_delete_person(AppState *state, uint32_t person_id, char *error_buffer, size_t error_buffer_size);
    bool app_state_edit_person(AppState *state, uint32_t person_id, const AppPersonEditData *edit_data,
                               char *error_buffer, size_t error_buffer_size);

    bool app_state_is_tree_dirty(const AppState *state);
    bool app_state_is_history_empty(const AppState *state);
    void app_state_mark_tree_dirty(AppState *state);
    void app_state_clear_tree_dirty(AppState *state);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
