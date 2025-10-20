#include "test_framework.h"

#include "file_dialog.h"

#include <stdio.h>
#include <string.h>

TEST(test_file_dialog_extension_appends_when_missing)
{
    char buffer[64];
    (void)snprintf(buffer, sizeof(buffer), "output/tree");
    ASSERT_TRUE(file_dialog_ensure_extension(buffer, sizeof(buffer), ".json"));
    ASSERT_STREQ(buffer, "output/tree.json");
}

TEST(test_file_dialog_extension_skips_when_present)
{
    char buffer[64];
    (void)snprintf(buffer, sizeof(buffer), "output/tree.json");
    ASSERT_TRUE(file_dialog_ensure_extension(buffer, sizeof(buffer), ".json"));
    ASSERT_STREQ(buffer, "output/tree.json");
}

TEST(test_file_dialog_extension_detects_case_insensitive_match)
{
    char buffer[64];
    (void)snprintf(buffer, sizeof(buffer), "output/tree.JSON");
    ASSERT_TRUE(file_dialog_ensure_extension(buffer, sizeof(buffer), ".json"));
    ASSERT_STREQ(buffer, "output/tree.JSON");
}

TEST(test_file_dialog_extension_fails_when_capacity_insufficient)
{
    char buffer[8];
    (void)snprintf(buffer, sizeof(buffer), "tree");
    ASSERT_FALSE(file_dialog_ensure_extension(buffer, sizeof(buffer), ".longjson"));
}

void register_file_dialog_tests(TestRegistry *registry)
{
    RUN_TEST(registry, test_file_dialog_extension_appends_when_missing);
    RUN_TEST(registry, test_file_dialog_extension_skips_when_present);
    RUN_TEST(registry, test_file_dialog_extension_detects_case_insensitive_match);
    RUN_TEST(registry, test_file_dialog_extension_fails_when_capacity_insufficient);
}
