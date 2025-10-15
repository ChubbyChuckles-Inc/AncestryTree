#include "at_string.h"
#include "test_framework.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

DECLARE_TEST(test_at_string_dup_null_returns_null);
DECLARE_TEST(test_at_string_dup_copies_content);
DECLARE_TEST(test_at_string_dup_range_copies_subset);
DECLARE_TEST(test_at_string_copy_handles_null_source);
DECLARE_TEST(test_at_string_copy_rejects_overflow);
DECLARE_TEST(test_at_string_equals_behaviour);
DECLARE_TEST(test_at_string_length_safe);

DECLARE_TEST(test_at_string_dup_null_returns_null)
{
    ASSERT_NULL(at_string_dup(NULL));
}

DECLARE_TEST(test_at_string_dup_copies_content)
{
    const char *message = "hello";
    char *duplicate = at_string_dup(message);
    ASSERT_NOT_NULL(duplicate);
    ASSERT_STREQ(duplicate, message);
    ASSERT_EQ(strlen(duplicate), strlen(message));
    free(duplicate);
}

DECLARE_TEST(test_at_string_dup_range_copies_subset)
{
    const char *message = "holographic";
    char *subset = at_string_dup_range(message, 5U);
    ASSERT_NOT_NULL(subset);
    ASSERT_STREQ(subset, "holog");
    free(subset);
}

DECLARE_TEST(test_at_string_copy_handles_null_source)
{
    char buffer[8];
    ASSERT_TRUE(at_string_copy(buffer, sizeof(buffer), NULL));
    ASSERT_STREQ(buffer, "");
}

DECLARE_TEST(test_at_string_copy_rejects_overflow)
{
    char buffer[4];
    ASSERT_FALSE(at_string_copy(buffer, sizeof(buffer), "long"));
}

DECLARE_TEST(test_at_string_equals_behaviour)
{
    ASSERT_TRUE(at_string_equals("alpha", "alpha"));
    ASSERT_FALSE(at_string_equals("alpha", "beta"));
    ASSERT_FALSE(at_string_equals(NULL, "beta"));
    ASSERT_TRUE(at_string_equals(NULL, NULL));
}

DECLARE_TEST(test_at_string_length_safe)
{
    ASSERT_EQ(at_string_length_safe("abc"), 3U);
    ASSERT_EQ(at_string_length_safe(NULL), 0U);
}

void register_string_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_at_string_dup_null_returns_null);
    REGISTER_TEST(registry, test_at_string_dup_copies_content);
    REGISTER_TEST(registry, test_at_string_dup_range_copies_subset);
    REGISTER_TEST(registry, test_at_string_copy_handles_null_source);
    REGISTER_TEST(registry, test_at_string_copy_rejects_overflow);
    REGISTER_TEST(registry, test_at_string_equals_behaviour);
    REGISTER_TEST(registry, test_at_string_length_safe);
}
