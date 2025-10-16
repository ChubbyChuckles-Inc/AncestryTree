#include "at_memory.h"
#include "test_framework.h"

#include <stdint.h>
#include <stdlib.h>

DECLARE_TEST(test_check_mul_overflow_detects_overflow);
DECLARE_TEST(test_check_mul_overflow_handles_zero);
DECLARE_TEST(test_secure_realloc_prevents_overflow);
DECLARE_TEST(test_secure_realloc_handles_zero_count);
DECLARE_TEST(test_tracking_reports_outstanding_allocation);
DECLARE_TEST(test_tracking_updates_stats_on_alloc_free);
DECLARE_TEST(test_tracking_reports_outstanding_allocation);
DECLARE_TEST(test_tracking_updates_stats_on_alloc_free);

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
    void *ptr = AT_MALLOC(8U);
    ASSERT_NOT_NULL(ptr);
    ptr = at_secure_realloc(ptr, 0U, sizeof(int));
    ASSERT_NULL(ptr);
}

DECLARE_TEST(test_tracking_reports_outstanding_allocation)
{
#if AT_MEMORY_ENABLE_TRACKING
    at_memory_reset_tracking();
    void *ptr = AT_MALLOC(32U);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(at_memory_outstanding_allocations(), 1U);
    ASSERT_EQ(at_memory_outstanding_bytes(), 32U);
    AT_FREE(ptr);
    ASSERT_EQ(at_memory_outstanding_allocations(), 0U);
    ASSERT_EQ(at_memory_outstanding_bytes(), 0U);
#else
    at_memory_reset_tracking();
    ASSERT_EQ(at_memory_outstanding_allocations(), 0U);
#endif
}

DECLARE_TEST(test_tracking_updates_stats_on_alloc_free)
{
#if AT_MEMORY_ENABLE_TRACKING
    at_memory_reset_tracking();
    AtMemoryStats stats;
    at_memory_get_stats(&stats);
    ASSERT_EQ(stats.total_allocations, 0U);
    void *first = AT_MALLOC(16U);
    ASSERT_NOT_NULL(first);
    void *second = AT_CALLOC(2U, 24U);
    ASSERT_NOT_NULL(second);
    stats.total_allocations = 0U;
    at_memory_get_stats(&stats);
    ASSERT_EQ(stats.total_allocations, 2U);
    ASSERT_EQ(stats.outstanding_allocations, 2U);
    ASSERT_EQ(stats.outstanding_bytes, 16U + 48U);
    AT_FREE(first);
    AT_FREE(second);
    at_memory_get_stats(&stats);
    ASSERT_EQ(stats.total_allocations, 2U);
    ASSERT_EQ(stats.total_frees, 2U);
    ASSERT_EQ(stats.outstanding_allocations, 0U);
    ASSERT_EQ(stats.outstanding_bytes, 0U);
#else
    at_memory_reset_tracking();
    ASSERT_EQ(at_memory_outstanding_allocations(), 0U);
#endif
}

void register_memory_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_check_mul_overflow_detects_overflow);
    REGISTER_TEST(registry, test_check_mul_overflow_handles_zero);
    REGISTER_TEST(registry, test_secure_realloc_prevents_overflow);
    REGISTER_TEST(registry, test_secure_realloc_handles_zero_count);
    REGISTER_TEST(registry, test_tracking_reports_outstanding_allocation);
    REGISTER_TEST(registry, test_tracking_updates_stats_on_alloc_free);
}
