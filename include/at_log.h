#ifndef AT_LOG_H
#define AT_LOG_H

#include <stdarg.h>

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
} AtLogger;

void at_logger_init(AtLogger *logger);
void at_logger_set_level(AtLogger *logger, AtLogLevel level);
void at_logger_message(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, ...);
void at_logger_message_v(AtLogger *logger, AtLogLevel level, const char *file, int line, const char *format, va_list args);

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
