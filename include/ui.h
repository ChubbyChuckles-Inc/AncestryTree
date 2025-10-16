#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stddef.h>

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
    UI_EVENT_UNDO,
    UI_EVENT_REDO,
    UI_EVENT_RESET_CAMERA,
    UI_EVENT_ESCAPE,
    UI_EVENT_REQUEST_EXIT,
    UI_EVENT_SAVE_SETTINGS,
    UI_EVENT_RELOAD_SETTINGS
} UIEventType;

typedef struct UIEvent
{
    UIEventType type;
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

bool ui_init(UIContext *ui, int width, int height);
void ui_resize(UIContext *ui, int width, int height);
void ui_cleanup(UIContext *ui);
void ui_shutdown(UIContext *ui);

bool ui_begin_frame(UIContext *ui, float delta_seconds);
void ui_draw_overlay(UIContext *ui, const FamilyTree *tree, const LayoutResult *layout, CameraController *camera,
                     float fps, const struct Person *selected_person, const struct Person *hovered_person,
                     struct RenderConfig *render_config, Settings *settings, bool settings_dirty);
void ui_end_frame(UIContext *ui);
bool ui_is_available(const UIContext *ui);
bool ui_auto_orbit_enabled(const UIContext *ui);
bool ui_event_enqueue(UIContext *ui, UIEventType type);
size_t ui_poll_events(UIContext *ui, UIEvent *events, size_t capacity);
bool ui_notify_status(UIContext *ui, const char *message);
bool ui_handle_escape(UIContext *ui);

#endif /* UI_H */
