#include "persistence_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <io.h>
#endif

bool persistence_set_error_message(char *buffer, size_t buffer_size, const char *message)
{
    if (!buffer || buffer_size == 0U)
    {
        return false;
    }
    const char *text = message ? message : "unknown error";
    (void)snprintf(buffer, buffer_size, "%s", text);
    return false;
}

bool persistence_utf8_validate(const char *value)
{
    if (!value)
    {
        return true;
    }
    const unsigned char *cursor = (const unsigned char *)value;
    while (*cursor != '\0')
    {
        unsigned char byte = *cursor;
        if (byte < 0x80U)
        {
            cursor++;
            continue;
        }
        if ((byte & 0xE0U) == 0xC0U)
        {
            unsigned char b1 = cursor[1];
            if (b1 == '\0' || byte < 0xC2U || (b1 & 0xC0U) != 0x80U)
            {
                return false;
            }
            cursor += 2U;
            continue;
        }
        if ((byte & 0xF0U) == 0xE0U)
        {
            unsigned char b1 = cursor[1];
            unsigned char b2 = cursor[2];
            if (b1 == '\0' || b2 == '\0' || (b1 & 0xC0U) != 0x80U || (b2 & 0xC0U) != 0x80U)
            {
                return false;
            }
            unsigned int codepoint = ((byte & 0x0FU) << 12) | ((b1 & 0x3FU) << 6) | (b2 & 0x3FU);
            if (codepoint < 0x800U || (codepoint >= 0xD800U && codepoint <= 0xDFFFU))
            {
                return false;
            }
            cursor += 3U;
            continue;
        }
        if ((byte & 0xF8U) == 0xF0U)
        {
            unsigned char b1 = cursor[1];
            unsigned char b2 = cursor[2];
            unsigned char b3 = cursor[3];
            if (b1 == '\0' || b2 == '\0' || b3 == '\0' || (b1 & 0xC0U) != 0x80U ||
                (b2 & 0xC0U) != 0x80U || (b3 & 0xC0U) != 0x80U)
            {
                return false;
            }
            unsigned int codepoint = ((byte & 0x07U) << 18) | ((b1 & 0x3FU) << 12) | ((b2 & 0x3FU) << 6) |
                                     (b3 & 0x3FU);
            if (codepoint < 0x10000U || codepoint > 0x10FFFFU)
            {
                return false;
            }
            cursor += 4U;
            continue;
        }
        return false;
    }
    return true;
}

void persistence_format_errno(char *buffer, size_t buffer_size, const char *prefix, const char *path)
{
    if (!buffer || buffer_size == 0U)
    {
        return;
    }
#if defined(_MSC_VER)
    char error_text[256];
    if (strerror_s(error_text, sizeof(error_text), errno) != 0)
    {
        error_text[0] = '\0';
    }
    (void)snprintf(buffer, buffer_size, "%s %s: %s", prefix ? prefix : "", path ? path : "(null)", error_text);
#else
    (void)snprintf(buffer, buffer_size, "%s %s: %s", prefix ? prefix : "", path ? path : "(null)", strerror(errno));
#endif
}

int persistence_portable_fopen(FILE **stream, const char *path, const char *mode)
{
    if (!stream)
    {
        return EINVAL;
    }
#if defined(_MSC_VER)
    errno_t result = fopen_s(stream, path, mode);
    if (result != 0)
    {
        *stream = NULL;
        errno = result;
    }
    return (int)result;
#else
    *stream = fopen(path, mode);
    if (!*stream)
    {
        return errno;
    }
    return 0;
#endif
}

bool persistence_create_backup_if_needed(const char *path, char *error_buffer, size_t error_buffer_size)
{
    if (!path)
    {
        return persistence_set_error_message(error_buffer, error_buffer_size, "path pointer is NULL");
    }

    FILE *source = NULL;
    int open_result = persistence_portable_fopen(&source, path, "rb");
    if (open_result != 0)
    {
        if (open_result == ENOENT)
        {
            return true;
        }
        persistence_format_errno(error_buffer, error_buffer_size, "failed to open for backup", path);
        return false;
    }

    size_t path_length = strlen(path);
    const char *suffix = ".bak";
    size_t suffix_length = strlen(suffix);
    char *backup_path = malloc(path_length + suffix_length + 1U);
    if (!backup_path)
    {
        fclose(source);
        return persistence_set_error_message(error_buffer, error_buffer_size, "failed to allocate backup path");
    }
    memcpy(backup_path, path, path_length);
    memcpy(backup_path + path_length, suffix, suffix_length + 1U);

    FILE *destination = NULL;
    int dest_result = persistence_portable_fopen(&destination, backup_path, "wb");
    if (dest_result != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to create backup", backup_path);
        free(backup_path);
        fclose(source);
        return false;
    }

    unsigned char buffer[4096];
    bool success = true;
    while (success)
    {
        size_t read_bytes = fread(buffer, 1U, sizeof(buffer), source);
        if (read_bytes > 0U)
        {
            if (fwrite(buffer, 1U, read_bytes, destination) != read_bytes)
            {
                persistence_format_errno(error_buffer, error_buffer_size, "failed to write backup", backup_path);
                success = false;
                break;
            }
        }
        if (read_bytes < sizeof(buffer))
        {
            if (ferror(source))
            {
                persistence_format_errno(error_buffer, error_buffer_size, "failed to read source for backup", path);
                success = false;
            }
            break;
        }
    }

    if (fclose(destination) != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to close backup", backup_path);
        success = false;
    }
    if (fclose(source) != 0)
    {
        persistence_format_errno(error_buffer, error_buffer_size, "failed to close source", path);
        success = false;
    }
    free(backup_path);
    return success;
}
