#include "json_parser.h"
#include "test_framework.h"

#include <stdlib.h>

TEST(test_json_parser_simple_object)
{
    char error[128];
    int line        = 0;
    int column      = 0;
    JsonValue *root = json_parse("{\"key\": \"value\"}", error, sizeof(error), &line, &column);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(json_value_type(root), JSON_VALUE_OBJECT);
    const JsonValue *string_value = json_value_object_get(root, "key");
    ASSERT_EQ(json_value_type(string_value), JSON_VALUE_STRING);
    ASSERT_STREQ(json_value_get_string(string_value), "value");
    json_value_destroy(root);
}

TEST(test_json_parser_handles_unicode)
{
    char error[128];
    int line        = 0;
    int column      = 0;
    JsonValue *root = json_parse("{\"key\": \"A\xC3\x9F\xE6\x9D\xB1\xF0\x9D\x84\x9E\"}", error,
                                 sizeof(error), &line, &column);
    ASSERT_NOT_NULL(root);
    ASSERT_EQ(json_value_type(root), JSON_VALUE_OBJECT);
    const JsonValue *string_value = json_value_object_get(root, "key");
    ASSERT_STREQ(json_value_get_string(string_value), "A\xC3\x9F\xE6\x9D\xB1\xF0\x9D\x84\x9E");
    json_value_destroy(root);
}

TEST(test_json_parser_invalid_unicode_reports_error)
{
    char error[128];
    int line        = 0;
    int column      = 0;
    JsonValue *root = json_parse("{\"key\": \"\\uD800\"}", error, sizeof(error), &line, &column);
    ASSERT_NULL(root);
    ASSERT_TRUE(line > 0);
}

TEST(test_json_parser_allocator_reuses_blocks)
{
    json_parser_allocator_reset();

    const char *payload = "{\"values\": [1, 2, 3, 4]}";
    for (int iteration = 0; iteration < 6; ++iteration)
    {
        char error[128];
        int line        = 0;
        int column      = 0;
        JsonValue *root = json_parse(payload, error, sizeof(error), &line, &column);
        ASSERT_NOT_NULL(root);
        json_value_destroy(root);
    }

    JsonAllocatorStats stats = json_parser_allocator_stats();
    ASSERT_TRUE(stats.total_blocks <= 2U);
    ASSERT_TRUE(stats.block_capacity >= 1U);

    json_parser_allocator_reset();
}

void register_json_parser_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_json_parser_simple_object);
    REGISTER_TEST(registry, test_json_parser_handles_unicode);
    REGISTER_TEST(registry, test_json_parser_invalid_unicode_reports_error);
    REGISTER_TEST(registry, test_json_parser_allocator_reuses_blocks);
}
