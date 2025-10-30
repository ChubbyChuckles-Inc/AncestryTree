#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "timeline.h"

typedef struct FamilyTree FamilyTree;
typedef struct LayoutResult LayoutResult;
typedef struct CameraController CameraController;
struct RenderConfig;
struct Person;
typedef struct Settings Settings;

typedef enum UIEventType
{
    UI_EVENT_NONE = 0,
    UI_EVENT_NEW_TREE,
    UI_EVENT_OPEN_TREE,
    UI_EVENT_SAVE_TREE,
    UI_EVENT_SAVE_TREE_AS,
    UI_EVENT_IMPORT_PACKAGE,
    UI_EVENT_EXPORT_PACKAGE,
    UI_EVENT_DELETE_PERSON,
    UI_EVENT_UNDO,
    UI_EVENT_REDO,
    UI_EVENT_RESET_CAMERA,
    UI_EVENT_ESCAPE,
    UI_EVENT_REQUEST_EXIT,
    UI_EVENT_SAVE_SETTINGS,
    UI_EVENT_RELOAD_SETTINGS,
    UI_EVENT_FOCUS_PERSON,
    UI_EVENT_COMPLETE_ONBOARDING
} UIEventType;

typedef struct UIEvent
{
    UIEventType type;
    uint32_t param_u32;
} UIEvent;

#define UI_EVENT_QUEUE_CAPACITY 16U

typedef struct UIEventQueue
{
    UIEvent events[UI_EVENT_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} UIEventQueue;

typedef struct UIContext
{
    bool available;
    void *impl;
    int width;
    int height;
    UIEventQueue event_queue;
} UIContext;

#define UI_ADD_PERSON_MAX_CERTIFICATES 8U
#define UI_ADD_PERSON_MAX_TIMELINE_ENTRIES 8U

typedef struct UIAddPersonTimelineItem
{
    TimelineEventType type;
    char date[32];
    char description[128];
    char location[96];
} UIAddPersonTimelineItem;

typedef struct UIAddPersonRequest
{
    char first[64];
    char middle[64];
    char last[64];
    char maiden[64];
    char blood_type[16];
    bool is_adopted;
    char birth_date[32];
    char birth_location[96];
    bool is_alive;
    char death_date[32];
    char death_location[96];
    char profile_image_path[128];
    char certificate_paths[UI_ADD_PERSON_MAX_CERTIFICATES][128];
    size_t certificate_count;
    UIAddPersonTimelineItem timeline_entries[UI_ADD_PERSON_MAX_TIMELINE_ENTRIES];
    size_t timeline_count;
    uint32_t father_id;
    uint32_t mother_id;
    uint32_t spouse_id;
} UIAddPersonRequest;

#define UI_EDIT_PERSON_MAX_SPOUSES 4U

typedef struct UIEditPersonRequest
{
    uint32_t person_id;
    char first[64];
    char middle[64];
    char last[64];
    char maiden[64];
    char blood_type[16];
    bool is_adopted;
    char birth_date[32];
    char birth_location[96];
    bool has_death;
    char death_date[32];
    char death_location[96];
    bool profile_image_defined;
    char profile_image_path[128];
    bool clear_profile_image;
    char certificate_paths[UI_ADD_PERSON_MAX_CERTIFICATES][128];
    size_t certificate_count;
    UIAddPersonTimelineItem timeline_entries[UI_ADD_PERSON_MAX_TIMELINE_ENTRIES];
    size_t timeline_count;
    bool update_father;
    uint32_t father_id;
    bool update_mother;
    uint32_t mother_id;
    bool update_spouses;
    uint32_t spouse_ids[UI_EDIT_PERSON_MAX_SPOUSES];
    size_t spouse_count;
} UIEditPersonRequest;

bool ui_init(UIContext *ui, int width, int height);
void ui_resize(UIContext *ui, int width, int height);
bool ui_configure_search_storage(UIContext *ui, const char *path);
void ui_cleanup(UIContext *ui);
void ui_shutdown(UIContext *ui);

bool ui_begin_frame(UIContext *ui, float delta_seconds, float wheel_delta);
void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout,
                     CameraController *camera, float fps, const struct Person *selected_person,
                     const struct Person *hovered_person, struct RenderConfig *render_config,
                     Settings *settings, bool settings_dirty, float top_margin);
void ui_end_frame(UIContext *ui);
bool ui_is_available(const UIContext *ui);
bool ui_auto_orbit_enabled(const UIContext *ui);
bool ui_event_enqueue(UIContext *ui, UIEventType type);
bool ui_event_enqueue_with_u32(UIContext *ui, UIEventType type, uint32_t value);
size_t ui_poll_events(UIContext *ui, UIEvent *events, size_t capacity);
bool ui_notify_status(UIContext *ui, const char *message);
bool ui_handle_escape(UIContext *ui);
bool ui_show_error_dialog(UIContext *ui, const char *title, const char *message);
bool ui_consume_add_person_request(UIContext *ui, UIAddPersonRequest *out_request);
bool ui_open_edit_person_panel(UIContext *ui, const struct Person *person);
bool ui_consume_edit_person_request(UIContext *ui, UIEditPersonRequest *out_request);
bool ui_pointer_over_ui(const UIContext *ui);
void ui_onboarding_configure(UIContext *ui, bool active, bool enable_tooltips);
void ui_onboarding_restart(UIContext *ui, bool enable_tooltips);
bool ui_onboarding_active(const UIContext *ui);
void ui_onboarding_mark_completed(UIContext *ui);
void ui_progress_begin(UIContext *ui, const char *label);
void ui_progress_update(UIContext *ui, float value);
void ui_progress_complete(UIContext *ui, bool success, const char *label);

/* Manual validation checklist (requires raylib + Nuklear):
 * 1. Open the "Add Person" panel from the Edit menu and populate name, birth, parents, spouse, and
 * timeline.
 * 2. Verify Save queues a request and the detail view reflects the new holographic node.
 */

#endif /* UI_H */
