#include "at_error.h"
#include "test_framework.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *error_log_path(void)
{
    return "Testing/Temporary/error_macro_output.log";
}

static void error_log_cleanup(void)
{
    (void)remove(error_log_path());
}

static bool helper_check_null_macro(bool *flag)
{
    AtLogger logger;
    at_logger_init(&logger);
    at_logger_enable_console(&logger, false);
    char error_buffer[128];
    if (!at_logger_open_file(&logger, error_log_path(), error_buffer, sizeof(error_buffer)))
    {
        return false;
    }

    int *value = NULL;
    AT_CHECK_NULL(&logger, value, {
        *flag = true;
        at_logger_close_file(&logger);
        return false;
    });

    at_logger_close_file(&logger);
    return true;
}

DECLARE_TEST(test_check_null_macro_executes_action)
{
    error_log_cleanup();
    bool flag = false;
    ASSERT_FALSE(helper_check_null_macro(&flag));
    ASSERT_TRUE(flag);

    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, error_log_path(), "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(error_log_path(), "rb");
#endif
    ASSERT_NOT_NULL(file);
    char buffer[256];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, file);
    buffer[read] = '\0';
    fclose(file);
    ASSERT_NE(strstr(buffer, "Null pointer detected"), NULL);
    error_log_cleanup();
}

static bool helper_check_alloc_macro(bool *flag)
{
    AtLogger logger;
    at_logger_init(&logger);
    at_logger_enable_console(&logger, false);
    char error_buffer[128];
    if (!at_logger_open_file(&logger, error_log_path(), error_buffer, sizeof(error_buffer)))
    {
        return false;
    }

    void *allocation = NULL;
    AT_CHECK_ALLOC(&logger, allocation, {
        *flag = true;
        at_logger_close_file(&logger);
        return false;
    });

    at_logger_close_file(&logger);
    return true;
}

DECLARE_TEST(test_check_alloc_macro_executes_action)
{
    error_log_cleanup();
    bool flag = false;
    ASSERT_FALSE(helper_check_alloc_macro(&flag));
    ASSERT_TRUE(flag);

    FILE *file = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&file, error_log_path(), "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(error_log_path(), "rb");
#endif
    ASSERT_NOT_NULL(file);
    char buffer[256];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1U, file);
    buffer[read] = '\0';
    fclose(file);
    ASSERT_NE(strstr(buffer, "Allocation failed"), NULL);
    error_log_cleanup();
}

DECLARE_TEST(test_try_catch_macros_capture_message)
{
    AtTryContext context;
    at_try_context_init(&context);

    bool caught = false;
    AT_TRY(&context)
    {
        AT_THROW(&context, 7, "quantum flux");
    }
    AT_CATCH(&context)
    {
        caught = true;
        ASSERT_EQ(context.code, 7);
        ASSERT_STREQ(context.message, "quantum flux");
    }
    ASSERT_TRUE(caught);
}

DECLARE_TEST(test_try_block_completes_without_throw)
{
    AtTryContext context;
    at_try_context_init(&context);

    bool reached_catch = false;
    AT_TRY(&context)
    {
        /* Manual validation: integrate with long-running IO paths when available. */
    }
    AT_CATCH(&context)
    {
        reached_catch = true;
    }
    ASSERT_FALSE(reached_catch);
}

void register_error_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_check_null_macro_executes_action);
    REGISTER_TEST(registry, test_check_alloc_macro_executes_action);
    REGISTER_TEST(registry, test_try_catch_macros_capture_message);
    REGISTER_TEST(registry, test_try_block_completes_without_throw);
}
