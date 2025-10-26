#ifndef STATUS_MESSAGES_H
#define STATUS_MESSAGES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool status_message_format_save_error(const char *path, const char *detail, char *buffer,
                                          size_t capacity);
    bool status_message_format_load_error(const char *path, const char *detail, char *buffer,
                                          size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* STATUS_MESSAGES_H */
