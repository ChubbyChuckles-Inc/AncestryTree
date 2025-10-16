#include "test_framework.h"

#include "event.h"
#include "person.h"

#include <string.h>

typedef struct EventTestCounters
{
    int shortcut_calls;
    int queue_calls;
} EventTestCounters;

static void test_shortcut_handler(void *user_data, float delta_seconds)
{
    (void)delta_seconds;
    EventTestCounters *counters = (EventTestCounters *)user_data;
    if (!counters)
    {
        return;
    }
    counters->shortcut_calls += 1;
}

static void test_queue_handler(void *user_data, float delta_seconds)
{
    (void)delta_seconds;
    EventTestCounters *counters = (EventTestCounters *)user_data;
    if (!counters)
    {
        return;
    }
    counters->queue_calls += 1;
}

TEST(test_events_null_context_safe)
{
    event_process(NULL, EVENT_PROCESS_PHASE_PRE_FRAME, 0.016f);
    event_process(NULL, EVENT_PROCESS_PHASE_POST_FRAME, 0.016f);
    ASSERT_TRUE(true);
}

TEST(test_events_shortcut_handler_invoked)
{
    EventTestCounters counters;
    memset(&counters, 0, sizeof(counters));

    EventProcessContext context;
    memset(&context, 0, sizeof(context));
    context.shortcut_handler = test_shortcut_handler;
    context.shortcut_user_data = &counters;

    event_process(&context, EVENT_PROCESS_PHASE_PRE_FRAME, 0.033f);

    ASSERT_EQ(1, counters.shortcut_calls);
    ASSERT_EQ(0, counters.queue_calls);
}

TEST(test_events_queue_handler_invoked)
{
    EventTestCounters counters;
    memset(&counters, 0, sizeof(counters));

    EventProcessContext context;
    memset(&context, 0, sizeof(context));
    context.queue_handler = test_queue_handler;
    context.queue_user_data = &counters;

    event_process(&context, EVENT_PROCESS_PHASE_POST_FRAME, 0.033f);

    ASSERT_EQ(0, counters.shortcut_calls);
    ASSERT_EQ(1, counters.queue_calls);
}

TEST(test_events_pointer_block_prevents_selection_clear)
{
    CameraControllerConfig camera_config;
    camera_controller_config_default(&camera_config);
    CameraController camera;
    ASSERT_TRUE(camera_controller_init(&camera, &camera_config));

    LayoutResult layout;
    memset(&layout, 0, sizeof(layout));

    InteractionState interaction;
    interaction_state_init(&interaction);

    Person person;
    memset(&person, 0, sizeof(person));
    person.id = 42U;
    ASSERT_TRUE(interaction_select_person(&interaction, &person));

    EventProcessContext context;
    memset(&context, 0, sizeof(context));
    context.camera = &camera;
    context.layout = &layout;
    context.interaction_state = &interaction;
    context.mouse_x = 0.0f;
    context.mouse_y = 0.0f;
    context.shift_down = false;

    context.pointer_blocked = false;
    context.mouse_left_pressed = true;
    event_process(&context, EVENT_PROCESS_PHASE_PRE_FRAME, 0.016f);
    ASSERT_NULL(interaction_get_selected(&interaction));

    ASSERT_TRUE(interaction_select_person(&interaction, &person));
    context.pointer_blocked = true;
    context.mouse_left_pressed = true;
    event_process(&context, EVENT_PROCESS_PHASE_PRE_FRAME, 0.016f);
    ASSERT_EQ(&person, interaction_get_selected(&interaction));
}
