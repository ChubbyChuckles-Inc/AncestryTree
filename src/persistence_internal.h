#ifndef PERSISTENCE_INTERNAL_H
#define PERSISTENCE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool persistence_set_error_message(char *buffer, size_t buffer_size, const char *message);
    bool persistence_utf8_validate(const char *value);
    void persistence_format_errno(char *buffer, size_t buffer_size, const char *prefix, const char *path);
    int persistence_portable_fopen(FILE **stream, const char *path, const char *mode);
    bool persistence_create_backup_if_needed(const char *path, char *error_buffer, size_t error_buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* PERSISTENCE_INTERNAL_H */
