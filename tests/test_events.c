#include "test_framework.h"

#include "event.h"

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
