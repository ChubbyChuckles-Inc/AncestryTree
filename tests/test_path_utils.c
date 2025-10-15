#include "path_utils.h"
#include "test_framework.h"

#include <stdio.h>
#include <string.h>

static void normalise_expected(char *buffer)
{
#if defined(_WIN32)
    for (size_t index = 0U; buffer[index] != '\0'; ++index)
    {
        if (buffer[index] == '/')
        {
            buffer[index] = '\\';
        }
    }
#else
    for (size_t index = 0U; buffer[index] != '\0'; ++index)
    {
        if (buffer[index] == '\\')
        {
            buffer[index] = '/';
        }
    }
#endif
}

TEST(test_path_join_relative_with_two_levels_up)
{
#if defined(_WIN32)
    const char *base = "C:\\Projects\\AncestryTree\\build_windows\\bin\\";
#else
    const char *base = "/home/user/AncestryTree/build/linux/bin/";
#endif
    char buffer[256];
    ASSERT_TRUE(path_join_relative(base, 2U, "assets/example_tree.json", buffer, sizeof(buffer)));

#if defined(_WIN32)
    ASSERT_STREQ(buffer, "C:\\Projects\\AncestryTree\\assets\\example_tree.json");
#else
    ASSERT_STREQ(buffer, "/home/user/AncestryTree/assets/example_tree.json");
#endif
}

TEST(test_path_join_relative_handles_missing_trailing_slash)
{
#if defined(_WIN32)
    const char *base = "D:\\Data\\build";
    const char *expected = "D:\\Data\\assets\\file.txt";
#else
    const char *base = "/data/build";
    const char *expected = "/data/assets/file.txt";
#endif
    char expected_buffer[128];
    snprintf(expected_buffer, sizeof(expected_buffer), "%s", expected);
    normalise_expected(expected_buffer);

    char buffer[256];
    ASSERT_TRUE(path_join_relative(base, 1U, "assets/file.txt", buffer, sizeof(buffer)));
    ASSERT_STREQ(buffer, expected_buffer);
}

TEST(test_path_join_relative_fails_with_small_buffer)
{
    char buffer[8];
#if defined(_WIN32)
    ASSERT_FALSE(path_join_relative("C:\\root", 0U, "very_long_path_component", buffer, sizeof(buffer)));
#else
    ASSERT_FALSE(path_join_relative("/root", 0U, "very_long_path_component", buffer, sizeof(buffer)));
#endif
}

void register_path_utils_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_path_join_relative_with_two_levels_up);
    REGISTER_TEST(registry, test_path_join_relative_handles_missing_trailing_slash);
    REGISTER_TEST(registry, test_path_join_relative_fails_with_small_buffer);
}
