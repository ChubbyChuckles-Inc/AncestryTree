#include "screen_reader.h"

#include <stdio.h>
#include <string.h>

#if defined(_MSC_VER)
#include <errno.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

static void screen_reader_reset_state(ScreenReaderChannel *channel)
{
    if (!channel)
    {
        return;
    }
    channel->initialised     = false;
    channel->enabled         = false;
    channel->path[0]         = '\0';
    channel->last_message[0] = '\0';
    channel->sink            = NULL;
    channel->sink_user_data  = NULL;
}

void screen_reader_init(ScreenReaderChannel *channel)
{
    if (!channel)
    {
        return;
    }
    screen_reader_reset_state(channel);
    channel->initialised = true;
#if defined(_MSC_VER)
    (void)strncpy_s(channel->path, sizeof(channel->path), SCREEN_READER_DEFAULT_PATH, _TRUNCATE);
#else
    (void)snprintf(channel->path, sizeof(channel->path), "%s", SCREEN_READER_DEFAULT_PATH);
#endif
}

void screen_reader_shutdown(ScreenReaderChannel *channel)
{
    if (!channel)
    {
        return;
    }
    screen_reader_reset_state(channel);
}

void screen_reader_set_log_path(ScreenReaderChannel *channel, const char *path)
{
    if (!channel || !path)
    {
        return;
    }
    if (!channel->initialised)
    {
        screen_reader_init(channel);
    }
#if defined(_MSC_VER)
    (void)strncpy_s(channel->path, sizeof(channel->path), path, _TRUNCATE);
#else
    (void)snprintf(channel->path, sizeof(channel->path), "%s", path);
#endif
}

void screen_reader_set_sink(ScreenReaderChannel *channel, ScreenReaderSink sink, void *user_data)
{
    if (!channel)
    {
        return;
    }
    if (!channel->initialised)
    {
        screen_reader_init(channel);
    }
    channel->sink           = sink;
    channel->sink_user_data = user_data;
}

void screen_reader_set_enabled(ScreenReaderChannel *channel, bool enabled)
{
    if (!channel)
    {
        return;
    }
    if (!channel->initialised)
    {
        screen_reader_init(channel);
    }
    channel->enabled = enabled;
    if (!enabled)
    {
        channel->last_message[0] = '\0';
    }
}

bool screen_reader_enabled(const ScreenReaderChannel *channel)
{
    return channel && channel->initialised && channel->enabled;
}

static void screen_reader_write_default(const ScreenReaderChannel *channel, const char *message)
{
    if (!channel || !message || message[0] == '\0')
    {
        return;
    }
    if (channel->path[0] == '\0')
    {
        return;
    }
#if defined(_MSC_VER)
    FILE *stream = NULL;
    if (fopen_s(&stream, channel->path, "ab") != 0)
    {
        return;
    }
#else
    FILE *stream = fopen(channel->path, "ab");
    if (!stream)
    {
        return;
    }
#endif
    size_t length = strlen(message);
    if (length > 0U)
    {
        (void)fwrite(message, 1U, length, stream);
    }
    (void)fwrite("\n", 1U, 1U, stream);
    (void)fflush(stream);
    fclose(stream);
#if defined(_WIN32)
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
#endif
}

void screen_reader_announce(ScreenReaderChannel *channel, const char *message)
{
    if (!channel || !message || message[0] == '\0')
    {
        return;
    }
    if (!channel->initialised)
    {
        screen_reader_init(channel);
    }
    if (!channel->enabled)
    {
        return;
    }
    if (strncmp(channel->last_message, message, sizeof(channel->last_message)) == 0)
    {
        return;
    }

    if (channel->sink)
    {
        channel->sink(channel->sink_user_data, message);
    }
    else
    {
        screen_reader_write_default(channel, message);
    }

#if defined(_MSC_VER)
    (void)strncpy_s(channel->last_message, sizeof(channel->last_message), message, _TRUNCATE);
#else
    (void)snprintf(channel->last_message, sizeof(channel->last_message), "%s", message);
#endif
}
