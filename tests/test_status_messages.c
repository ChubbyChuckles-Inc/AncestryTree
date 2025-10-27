#include "status_messages.h"
#include "test_framework.h"

#include <string.h>

TEST(test_status_messages_save_error_contains_advice)
{
    char buffer[256];
    ASSERT_TRUE(status_message_format_save_error("assets/manual_save.json", "Access denied", buffer,
                                                 sizeof(buffer)));
    ASSERT_TRUE(strstr(buffer, "write access") != NULL);
    ASSERT_TRUE(strstr(buffer, "free disk space") != NULL);
}

TEST(test_status_messages_load_error_contains_advice)
{
    char buffer[256];
    ASSERT_TRUE(status_message_format_load_error("assets/example_tree.json", "Checksum mismatch",
                                                 buffer, sizeof(buffer)));
    ASSERT_TRUE(strstr(buffer, "Check that the file exists") != NULL);
    ASSERT_TRUE(strstr(buffer, "valid AncestryTree JSON data") != NULL);
}

TEST(test_status_messages_handles_missing_detail)
{
    char buffer[128];
    ASSERT_TRUE(
        status_message_format_save_error("assets/manual_save.json", NULL, buffer, sizeof(buffer)));
    ASSERT_TRUE(strstr(buffer, "Try choosing a different folder") != NULL);
}

void register_status_message_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_status_messages_save_error_contains_advice);
    REGISTER_TEST(registry, test_status_messages_load_error_contains_advice);
    REGISTER_TEST(registry, test_status_messages_handles_missing_detail);
}
