#include "at_log.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

void at_logger_init(AtLogger *logger)
{
    if (!logger)
    {
        return;
    }
    logger->minimum_level = AT_LOG_INFO;
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

    fprintf(stderr, "%s [%s] (%s:%d) ", timestamp, at_log_level_to_string(level), file, line);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);

    if (level == AT_LOG_FATAL)
    {
        fflush(stderr);
    }
}

void at_logger_message(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    at_logger_message_v(logger, level, file, line, format, args);
    va_end(args);
}
