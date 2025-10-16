#include "at_log.h"
#include "test_framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *test_log_path(void)
{
    return "Testing/Temporary/logger_test_output.log";
}

static void test_log_cleanup(void)
{
    (void)remove(test_log_path());
}

DECLARE_TEST(test_logger_writes_to_file)
{
    test_log_cleanup();
    AtLogger logger;
    at_logger_init(&logger);
    at_logger_enable_console(&logger, false);

    char error_buffer[128];
    ASSERT_TRUE(at_logger_open_file(&logger, test_log_path(), error_buffer, sizeof(error_buffer)));

    AT_LOG(&logger, AT_LOG_INFO, "Logging to file with value %d", 42);

    at_logger_close_file(&logger);

    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, test_log_path(), "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(test_log_path(), "rb");
#endif
    ASSERT_NOT_NULL(file);

    char buffer[512];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, file);
    buffer[read] = '\0';
    fclose(file);

    ASSERT_NE(strstr(buffer, "Logging to file with value 42"), NULL);
    test_log_cleanup();
}

DECLARE_TEST(test_logger_open_failure_sets_error)
{
    AtLogger logger;
    at_logger_init(&logger);

    char error_buffer[128];
    ASSERT_FALSE(at_logger_open_file(&logger, "Testing", error_buffer, sizeof(error_buffer)));
    ASSERT_NE(strstr(error_buffer, "Unable to open log file"), NULL);
}

void register_log_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_logger_writes_to_file);
    REGISTER_TEST(registry, test_logger_open_failure_sets_error);
}
