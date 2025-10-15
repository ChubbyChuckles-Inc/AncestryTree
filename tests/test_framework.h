#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <math.h>
#include <stdbool.h>
#include <string.h>

typedef void (*TestFunction)(void);

typedef struct TestCase
{
    const char *name;
    TestFunction function;
} TestCase;

typedef struct TestRegistry
{
    TestCase *cases;
    int capacity;
    int count;
} TestRegistry;

typedef struct TestResult
{
    int executed;
    int failures;
} TestResult;

void test_registry_init(TestRegistry *registry, TestCase *storage, int capacity);
bool test_registry_add(TestRegistry *registry, const char *name, TestFunction function);
TestResult test_registry_run(const TestRegistry *registry);

void test_framework_record_failure(const char *expression, const char *file, int line, const char *message);

#define DECLARE_TEST(name) static void name(void)

#define TEST(name) DECLARE_TEST(name)

#define REGISTER_TEST(registry_ptr, test_name)                                     \
    do                                                                             \
    {                                                                              \
        if (!test_registry_add((registry_ptr), #test_name, (test_name)))           \
        {                                                                          \
            test_framework_record_failure("test_registry_add", __FILE__, __LINE__, \
                                          "Failed to register test");              \
        }                                                                          \
    } while (0)

#define RUN_TEST(registry_ptr, test_name) REGISTER_TEST(registry_ptr, test_name)

#define TEST_FAIL_WITH_MESSAGE(message)                                     \
    do                                                                      \
    {                                                                       \
        test_framework_record_failure(NULL, __FILE__, __LINE__, (message)); \
        return;                                                             \
    } while (0)

#define ASSERT_TRUE(condition)                                                   \
    do                                                                           \
    {                                                                            \
        if (!(condition))                                                        \
        {                                                                        \
            test_framework_record_failure(#condition, __FILE__, __LINE__, NULL); \
            return;                                                              \
        }                                                                        \
    } while (0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_NOT_NULL(pointer)                                                              \
    do                                                                                        \
    {                                                                                         \
        if ((pointer) == NULL)                                                                \
        {                                                                                     \
            test_framework_record_failure(#pointer, __FILE__, __LINE__, "Expected non-NULL"); \
            return;                                                                           \
        }                                                                                     \
    } while (0)

#define ASSERT_NULL(pointer)                                                              \
    do                                                                                    \
    {                                                                                     \
        if ((pointer) != NULL)                                                            \
        {                                                                                 \
            test_framework_record_failure(#pointer, __FILE__, __LINE__, "Expected NULL"); \
            return;                                                                       \
        }                                                                                 \
    } while (0)

#define ASSERT_EQ(actual, expected)                                                            \
    do                                                                                         \
    {                                                                                          \
        long long _actual_value = (long long)(actual);                                         \
        long long _expected_value = (long long)(expected);                                     \
        if (_actual_value != _expected_value)                                                  \
        {                                                                                      \
            test_framework_record_failure(#actual " == " #expected, __FILE__, __LINE__, NULL); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define ASSERT_NE(actual, expected)                                                            \
    do                                                                                         \
    {                                                                                          \
        long long _actual_value = (long long)(actual);                                         \
        long long _expected_value = (long long)(expected);                                     \
        if (_actual_value == _expected_value)                                                  \
        {                                                                                      \
            test_framework_record_failure(#actual " != " #expected, __FILE__, __LINE__, NULL); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define ASSERT_STREQ(actual, expected)                                                         \
    do                                                                                         \
    {                                                                                          \
        const char *_actual_str = (actual);                                                    \
        const char *_expected_str = (expected);                                                \
        bool _equal = false;                                                                   \
        if (_actual_str == _expected_str)                                                      \
        {                                                                                      \
            _equal = true;                                                                     \
        }                                                                                      \
        else if (_actual_str && _expected_str && strcmp(_actual_str, _expected_str) == 0)      \
        {                                                                                      \
            _equal = true;                                                                     \
        }                                                                                      \
        if (!_equal)                                                                           \
        {                                                                                      \
            test_framework_record_failure(#actual " == " #expected, __FILE__, __LINE__, NULL); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#define ASSERT_FLOAT_NEAR(actual, expected, tolerance)                                         \
    do                                                                                         \
    {                                                                                          \
        float _actual_value = (float)(actual);                                                 \
        float _expected_value = (float)(expected);                                             \
        float _difference = fabsf(_actual_value - _expected_value);                            \
        if (_difference > (float)(tolerance))                                                  \
        {                                                                                      \
            test_framework_record_failure(#actual " ~= " #expected, __FILE__, __LINE__, NULL); \
            return;                                                                            \
        }                                                                                      \
    } while (0)

#endif /* TEST_FRAMEWORK_H */
