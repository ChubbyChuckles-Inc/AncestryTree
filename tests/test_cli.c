#include "test_framework.h"

#include "app_cli.h"

#include <string.h>

DECLARE_TEST(test_cli_defaults_when_no_arguments)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree"};
    ASSERT_TRUE(app_cli_parse(1, argv, &options, error, sizeof(error)));
    ASSERT_STREQ(error, "");
    ASSERT_FALSE(options.show_help);
    ASSERT_FALSE(options.disable_sample_tree);
    ASSERT_EQ(options.log_level, AT_LOG_INFO);
    ASSERT_STREQ(options.tree_path, "");
}

DECLARE_TEST(test_cli_parses_help_switch)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree", (char *)"--help"};
    ASSERT_TRUE(app_cli_parse(2, argv, &options, error, sizeof(error)));
    ASSERT_TRUE(options.show_help);
}

DECLARE_TEST(test_cli_parses_load_option)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree", (char *)"--load", (char *)"custom_tree.json"};
    ASSERT_TRUE(app_cli_parse(3, argv, &options, error, sizeof(error)));
    ASSERT_STREQ(options.tree_path, "custom_tree.json");
}

DECLARE_TEST(test_cli_rejects_duplicate_paths)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree", (char *)"--load", (char *)"first.json", (char *)"second.json"};
    ASSERT_FALSE(app_cli_parse(4, argv, &options, error, sizeof(error)));
    ASSERT_TRUE(strcmp(error, "") != 0);
}

DECLARE_TEST(test_cli_parses_log_level)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree", (char *)"--log-level", (char *)"debug"};
    ASSERT_TRUE(app_cli_parse(3, argv, &options, error, sizeof(error)));
    ASSERT_EQ(options.log_level, AT_LOG_DEBUG);
}

DECLARE_TEST(test_cli_invalid_log_level_reports_error)
{
    AppLaunchOptions options;
    char error[128];
    char *argv[] = {(char *)"ancestrytree", (char *)"--log-level", (char *)"verbose"};
    ASSERT_FALSE(app_cli_parse(3, argv, &options, error, sizeof(error)));
    ASSERT_TRUE(strcmp(error, "") != 0);
}

DECLARE_TEST(test_cli_parses_no_sample_flag)
{
    AppLaunchOptions options;
    char error[64];
    char *argv[] = {(char *)"ancestrytree", (char *)"--no-sample"};
    ASSERT_TRUE(app_cli_parse(2, argv, &options, error, sizeof(error)));
    ASSERT_TRUE(options.disable_sample_tree);
}

void register_cli_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_cli_defaults_when_no_arguments);
    REGISTER_TEST(registry, test_cli_parses_help_switch);
    REGISTER_TEST(registry, test_cli_parses_load_option);
    REGISTER_TEST(registry, test_cli_rejects_duplicate_paths);
    REGISTER_TEST(registry, test_cli_parses_log_level);
    REGISTER_TEST(registry, test_cli_invalid_log_level_reports_error);
    REGISTER_TEST(registry, test_cli_parses_no_sample_flag);
}
