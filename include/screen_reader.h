#ifndef SCREEN_READER_H
#define SCREEN_READER_H

#include <stdbool.h>
#include <stddef.h>

#define SCREEN_READER_DEFAULT_PATH "ancestrytree_screen_reader.txt"
#define SCREEN_READER_MAX_MESSAGE_LENGTH 256

typedef void (*ScreenReaderSink)(void *user_data, const char *message);

typedef struct ScreenReaderChannel
{
    bool initialised;
    bool enabled;
    char path[260];
    char last_message[SCREEN_READER_MAX_MESSAGE_LENGTH];
    ScreenReaderSink sink;
    void *sink_user_data;
} ScreenReaderChannel;

void screen_reader_init(ScreenReaderChannel *channel);
void screen_reader_shutdown(ScreenReaderChannel *channel);
void screen_reader_set_log_path(ScreenReaderChannel *channel, const char *path);
void screen_reader_set_sink(ScreenReaderChannel *channel, ScreenReaderSink sink, void *user_data);
void screen_reader_set_enabled(ScreenReaderChannel *channel, bool enabled);
bool screen_reader_enabled(const ScreenReaderChannel *channel);
void screen_reader_announce(ScreenReaderChannel *channel, const char *message);

#endif /* SCREEN_READER_H */
