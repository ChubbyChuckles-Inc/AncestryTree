#ifndef APP_H
#define APP_H

#include "camera_controller.h"
#include "interaction.h"
#include "layout.h"
#include "person.h"
#include "settings.h"
#include "tree.h"
#include "timeline.h"

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
        LayoutResult layout_transition_start;
        LayoutResult layout_transition_target;
        InteractionState *interaction;
        CameraController *camera;
        Settings *settings;
        Settings *persisted_settings;
        LayoutAlgorithm active_layout_algorithm;
        float layout_transition_elapsed;
        float layout_transition_duration;
        Person *selected_person;
        AppUIState ui_state;
        AppInteractionMode interaction_mode;
        AppCommandStack undo_stack;
        AppCommandStack redo_stack;
        bool layout_transition_active;
        bool tree_dirty;
    } AppState;

#define APP_PERSON_EDIT_MAX_SPOUSES 4U

    typedef struct AppPersonEditRelationships
    {
        bool apply_father;
        uint32_t father_id;
        bool apply_mother;
        uint32_t mother_id;
        bool apply_spouses;
        uint32_t spouse_ids[APP_PERSON_EDIT_MAX_SPOUSES];
        size_t spouse_count;
    } AppPersonEditRelationships;

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
        AppPersonEditRelationships relationships;
    } AppPersonEditData;

#define APP_PERSON_CREATE_MAX_CERTIFICATES 8U
#define APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES 8U

    typedef struct AppPersonCreateTimelineEntry
    {
        TimelineEventType type;
        const char *date;
        const char *description;
        const char *location;
    } AppPersonCreateTimelineEntry;

    typedef struct AppPersonCreateData
    {
        const char *first;
        const char *middle;
        const char *last;
        const char *birth_date;
        const char *birth_location;
        bool is_alive;
        const char *death_date;
        const char *death_location;
        const char *profile_image_path;
        const char *certificate_paths[APP_PERSON_CREATE_MAX_CERTIFICATES];
        size_t certificate_count;
        AppPersonCreateTimelineEntry timeline_entries[APP_PERSON_CREATE_MAX_TIMELINE_ENTRIES];
        size_t timeline_count;
        uint32_t father_id;
        uint32_t mother_id;
        uint32_t spouse_id;
    } AppPersonCreateData;

    void app_state_init(AppState *state);
    bool app_state_configure(AppState *state, FamilyTree **tree, LayoutResult *layout, InteractionState *interaction,
                             CameraController *camera, Settings *settings, Settings *persisted_settings);
    void app_state_shutdown(AppState *state);
    void app_state_reset_history(AppState *state);
    void app_state_tick(AppState *state, float delta_seconds);

    bool app_state_push_command(AppState *state, AppCommand *command, char *error_buffer, size_t error_buffer_size);
    bool app_state_undo(AppState *state, char *error_buffer, size_t error_buffer_size);
    bool app_state_redo(AppState *state, char *error_buffer, size_t error_buffer_size);

    bool app_state_add_person(AppState *state, Person *person, char *error_buffer, size_t error_buffer_size);
    bool app_state_delete_person(AppState *state, uint32_t person_id, char *error_buffer, size_t error_buffer_size);
    bool app_state_edit_person(AppState *state, uint32_t person_id, const AppPersonEditData *edit_data,
                               char *error_buffer, size_t error_buffer_size);
    bool app_state_create_person(AppState *state, const AppPersonCreateData *data, uint32_t *out_person_id,
                                 char *error_buffer, size_t error_buffer_size);

    bool app_state_is_tree_dirty(const AppState *state);
    bool app_state_is_history_empty(const AppState *state);
    void app_state_mark_tree_dirty(AppState *state);
    void app_state_clear_tree_dirty(AppState *state);

    AppCommand *app_command_create_add_person(Person *person);
    AppCommand *app_command_create_delete_person(uint32_t person_id);
    AppCommand *app_command_create_edit_person(uint32_t person_id, const AppPersonEditData *edit_data);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
