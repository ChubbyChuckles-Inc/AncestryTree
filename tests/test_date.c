#include "at_date.h"
#include "test_framework.h"

TEST(test_date_valid_iso8601)
{
    ASSERT_TRUE(at_date_is_valid_iso8601("2025-10-15"));
    ASSERT_TRUE(at_date_is_valid_iso8601("2000-02-29"));
}

TEST(test_date_invalid_iso8601)
{
    ASSERT_FALSE(at_date_is_valid_iso8601("2025-13-01"));
    ASSERT_FALSE(at_date_is_valid_iso8601("2025-02-30"));
    ASSERT_FALSE(at_date_is_valid_iso8601("20-02-20"));
}

TEST(test_date_year_or_date)
{
    ASSERT_TRUE(at_date_is_valid_year_or_date("1999"));
    ASSERT_FALSE(at_date_is_valid_year_or_date("99"));
    ASSERT_TRUE(at_date_is_valid_year_or_date("1999-12-31"));
}

void register_date_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_date_valid_iso8601);
    REGISTER_TEST(registry, test_date_invalid_iso8601);
    REGISTER_TEST(registry, test_date_year_or_date);
}
