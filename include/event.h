#ifndef EVENT_H
#define EVENT_H

#include "at_log.h"
#include "camera_controller.h"
#include "graphics.h"
#include "interaction.h"
#include "layout.h"
#include "render.h"
#include "ui.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum EventProcessPhase
    {
        EVENT_PROCESS_PHASE_PRE_FRAME  = 0,
        EVENT_PROCESS_PHASE_POST_FRAME = 1
    } EventProcessPhase;

    typedef void (*EventShortcutHandler)(void *user_data, float delta_seconds);
    typedef void (*EventQueueHandler)(void *user_data, float delta_seconds);

    typedef struct EventProcessContext
    {
        GraphicsState *graphics_state;
        UIContext *ui;
        InteractionState *interaction_state;
        LayoutResult *layout;
        CameraController *camera;
        RenderState *render_state;
        bool render_ready;
        char *render_error_buffer;
        size_t render_error_capacity;
        bool *render_target_warned;
        AtLogger *logger;
        EventShortcutHandler shortcut_handler;
        void *shortcut_user_data;
        EventQueueHandler queue_handler;
        void *queue_user_data;
        bool pointer_over_chrome;
    } EventProcessContext;

    void event_process(EventProcessContext *context, EventProcessPhase phase, float delta_seconds);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_H */
