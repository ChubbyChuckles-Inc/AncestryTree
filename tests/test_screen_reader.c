#include "screen_reader.h"
#include "test_framework.h"

#include <string.h>

typedef struct ScreenReaderCapture
{
    char messages[4][SCREEN_READER_MAX_MESSAGE_LENGTH];
    size_t count;
} ScreenReaderCapture;

static void screen_reader_capture_sink(void *user_data, const char *message)
{
    ScreenReaderCapture *capture = (ScreenReaderCapture *)user_data;
    if (!capture || !message || message[0] == '\0')
    {
        return;
    }
    if (capture->count >= (sizeof(capture->messages) / sizeof(capture->messages[0])))
    {
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(capture->messages[capture->count], SCREEN_READER_MAX_MESSAGE_LENGTH, message,
                    _TRUNCATE);
#else
    (void)snprintf(capture->messages[capture->count], SCREEN_READER_MAX_MESSAGE_LENGTH, "%s",
                   message);
#endif
    capture->count += 1U;
}

TEST(test_screen_reader_defaults)
{
    ScreenReaderChannel channel;
    screen_reader_init(&channel);
    ASSERT_TRUE(channel.initialised);
    ASSERT_FALSE(screen_reader_enabled(&channel));
    ASSERT_STREQ(channel.path, SCREEN_READER_DEFAULT_PATH);
    ASSERT_STREQ(channel.last_message, "");
    screen_reader_shutdown(&channel);
    ASSERT_FALSE(channel.initialised);
}

TEST(test_screen_reader_duplicate_suppression)
{
    ScreenReaderChannel channel;
    screen_reader_init(&channel);
    ScreenReaderCapture capture;
    memset(&capture, 0, sizeof(capture));
    screen_reader_set_sink(&channel, screen_reader_capture_sink, &capture);
    screen_reader_set_enabled(&channel, true);

    screen_reader_announce(&channel, "Hello hologram");
    screen_reader_announce(&channel, "Hello hologram");
    screen_reader_announce(&channel, "New focus acquired");

    ASSERT_EQ(capture.count, 2U);
    ASSERT_STREQ(capture.messages[0], "Hello hologram");
    ASSERT_STREQ(capture.messages[1], "New focus acquired");
}

TEST(test_screen_reader_resets_history_when_disabled)
{
    ScreenReaderChannel channel;
    screen_reader_init(&channel);
    ScreenReaderCapture capture;
    memset(&capture, 0, sizeof(capture));
    screen_reader_set_sink(&channel, screen_reader_capture_sink, &capture);

    screen_reader_set_enabled(&channel, true);
    screen_reader_announce(&channel, "Initial message");
    ASSERT_EQ(capture.count, 1U);

    screen_reader_set_enabled(&channel, false);
    screen_reader_set_enabled(&channel, true);
    screen_reader_announce(&channel, "Initial message");

    ASSERT_EQ(capture.count, 2U);
    ASSERT_STREQ(capture.messages[1], "Initial message");
}

void register_screen_reader_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_screen_reader_defaults);
    REGISTER_TEST(registry, test_screen_reader_duplicate_suppression);
    REGISTER_TEST(registry, test_screen_reader_resets_history_when_disabled);
}
