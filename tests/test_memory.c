#include "at_memory.h"
#include "test_framework.h"

#include <stdint.h>
#include <stdlib.h>

DECLARE_TEST(test_check_mul_overflow_detects_overflow);
DECLARE_TEST(test_check_mul_overflow_handles_zero);
DECLARE_TEST(test_secure_realloc_prevents_overflow);
DECLARE_TEST(test_secure_realloc_handles_zero_count);

DECLARE_TEST(test_check_mul_overflow_detects_overflow)
{
    size_t result = 0U;
    bool overflow = at_check_mul_overflow_size(SIZE_MAX, 2U, &result);
    ASSERT_TRUE(overflow);
    ASSERT_EQ(result, 0U);
}

DECLARE_TEST(test_check_mul_overflow_handles_zero)
{
    size_t result = 1234U;
    bool overflow = at_check_mul_overflow_size(0U, 42U, &result);
    ASSERT_FALSE(overflow);
    ASSERT_EQ(result, 0U);
}

DECLARE_TEST(test_secure_realloc_prevents_overflow)
{
    void *ptr = at_secure_realloc(NULL, SIZE_MAX, SIZE_MAX);
    ASSERT_NULL(ptr);
}

DECLARE_TEST(test_secure_realloc_handles_zero_count)
{
    void *ptr = malloc(8U);
    ASSERT_NOT_NULL(ptr);
    ptr = at_secure_realloc(ptr, 0U, sizeof(int));
    ASSERT_NULL(ptr);
}

void register_memory_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_check_mul_overflow_detects_overflow);
    REGISTER_TEST(registry, test_check_mul_overflow_handles_zero);
    REGISTER_TEST(registry, test_secure_realloc_prevents_overflow);
    REGISTER_TEST(registry, test_secure_realloc_handles_zero_count);
}
