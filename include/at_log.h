#ifndef AT_LOG_H
#define AT_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum AtLogLevel
{
    AT_LOG_DEBUG = 0,
    AT_LOG_INFO,
    AT_LOG_WARN,
    AT_LOG_ERROR,
    AT_LOG_FATAL
} AtLogLevel;

typedef struct AtLogger
{
    AtLogLevel minimum_level;
    bool console_enabled;
    FILE *file;
} AtLogger;

void at_logger_init(AtLogger *logger);
void at_logger_set_level(AtLogger *logger, AtLogLevel level);
void at_logger_message(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, ...);
void at_logger_message_v(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, va_list args);
void at_logger_enable_console(AtLogger *logger, bool enabled);
bool at_logger_open_file(AtLogger *logger, const char *path, char *error_buffer, size_t error_buffer_size);
void at_logger_close_file(AtLogger *logger);

#define AT_LOG(logger_ptr, level, ...) at_logger_message((logger_ptr), (level), __FILE__, __LINE__, __VA_ARGS__)
#define AT_LOG_WARN_IF(logger_ptr, condition, ...)                                         \
    do                                                                                     \
    {                                                                                      \
        if (condition)                                                                     \
        {                                                                                  \
            at_logger_message((logger_ptr), AT_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__); \
        }                                                                                  \
    } while (0)

#endif /* AT_LOG_H */
