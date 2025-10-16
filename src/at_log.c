#include "at_log.h"
#include "at_memory.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void at_logger_init(AtLogger *logger)
{
    if (!logger)
    {
        return;
    }
    logger->minimum_level = AT_LOG_INFO;
    logger->console_enabled = true;
    logger->file = NULL;
}

void at_logger_set_level(AtLogger *logger, AtLogLevel level)
{
    if (!logger)
    {
        return;
    }
    logger->minimum_level = level;
}

static const char *at_log_level_to_string(AtLogLevel level)
{
    switch (level)
    {
    case AT_LOG_DEBUG:
        return "DEBUG";
    case AT_LOG_INFO:
        return "INFO";
    case AT_LOG_WARN:
        return "WARN";
    case AT_LOG_ERROR:
        return "ERROR";
    case AT_LOG_FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

static bool at_logger_can_log(const AtLogger *logger, AtLogLevel level)
{
    if (!logger)
    {
        return false;
    }
    return level >= logger->minimum_level;
}

void at_logger_message_v(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, va_list args)
{
    if (!at_logger_can_log(logger, level))
    {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &now);
#else
    localtime_r(&now, &tm_now);
#endif

    char timestamp[32];
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now) == 0U)
    {
        timestamp[0] = '\0';
    }

    char stack_buffer[256];
    char *message = stack_buffer;
    va_list args_copy;
    va_copy(args_copy, args);
    int required = vsnprintf(stack_buffer, sizeof(stack_buffer), format, args_copy);
    va_end(args_copy);

    if (required < 0)
    {
        (void)snprintf(stack_buffer, sizeof(stack_buffer), "<logging error>");
        required = (int)strlen(stack_buffer);
    }
    else if ((size_t)required >= sizeof(stack_buffer))
    {
        size_t length = (size_t)required + 1U;
        message = (char *)AT_MALLOC(length);
        if (message)
        {
            va_list args_retry;
            va_copy(args_retry, args);
            int written = vsnprintf(message, length, format, args_retry);
            va_end(args_retry);
            if (written < 0)
            {
                AT_FREE(message);
                message = stack_buffer;
                (void)snprintf(stack_buffer, sizeof(stack_buffer), "<logging error>");
            }
        }
        else
        {
            message = stack_buffer;
            (void)snprintf(stack_buffer, sizeof(stack_buffer), "<logging allocation failure>");
        }
    }

    if (logger->console_enabled)
    {
        fprintf(stderr, "%s [%s] (%s:%d) %s\n", timestamp, at_log_level_to_string(level), file, line, message);
        if (level == AT_LOG_FATAL)
        {
            fflush(stderr);
        }
    }

    if (logger->file)
    {
        fprintf(logger->file, "%s [%s] (%s:%d) %s\n", timestamp, at_log_level_to_string(level), file, line, message);
        fflush(logger->file);
    }

    if (message != stack_buffer)
    {
        AT_FREE(message);
    }
}

void at_logger_message(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    at_logger_message_v(logger, level, file, line, format, args);
    va_end(args);
}

void at_logger_enable_console(AtLogger *logger, bool enabled)
{
    if (!logger)
    {
        return;
    }
    logger->console_enabled = enabled;
}

bool at_logger_open_file(AtLogger *logger, const char *path, char *error_buffer, size_t error_buffer_size)
{
    if (!logger || !path || path[0] == '\0')
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Invalid logger or path");
        }
        return false;
    }

    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (!file)
    {
        if (error_buffer && error_buffer_size > 0U)
        {
            (void)snprintf(error_buffer, error_buffer_size, "Unable to open log file '%s' (errno=%d)", path, errno);
        }
        return false;
    }

    at_logger_close_file(logger);
    logger->file = file;
    if (error_buffer && error_buffer_size > 0U)
    {
        error_buffer[0] = '\0';
    }
    return true;
}

void at_logger_close_file(AtLogger *logger)
{
    if (!logger || !logger->file)
    {
        return;
    }
    fclose(logger->file);
    logger->file = NULL;
}
