#include "test_framework.h"

#include "app_bootstrap.h"

#include <stdio.h>
#include <string.h>

DECLARE_TEST(test_bootstrap_prefers_cli_path)
{
    AppLaunchOptions options;
    memset(&options, 0, sizeof(options));
    (void)snprintf(options.tree_path, sizeof(options.tree_path), "%s", "custom.json");

    AppStartupDecision decision;
    char message[256];
    ASSERT_TRUE(app_bootstrap_decide_tree_source(&options, "assets/example_tree.json", true, NULL,
                                                 &decision, message, sizeof(message)));
    ASSERT_EQ(decision.source, APP_STARTUP_SOURCE_CLI_PATH);
    ASSERT_STREQ(decision.resolved_path, "custom.json");
    ASSERT_TRUE(strstr(message, "custom.json") != NULL);
    ASSERT_TRUE(strstr(message, "Ctrl+O") != NULL);
}

DECLARE_TEST(test_bootstrap_uses_sample_when_available)
{
    AppLaunchOptions options;
    memset(&options, 0, sizeof(options));

    AppStartupDecision decision;
    char message[256];
    ASSERT_TRUE(app_bootstrap_decide_tree_source(&options, "assets/example_tree.json", true, NULL,
                                                 &decision, message, sizeof(message)));
    ASSERT_EQ(decision.source, APP_STARTUP_SOURCE_SAMPLE_ASSET);
    ASSERT_STREQ(decision.resolved_path, "assets/example_tree.json");
}

DECLARE_TEST(test_bootstrap_respects_disable_flag)
{
    AppLaunchOptions options;
    memset(&options, 0, sizeof(options));
    options.disable_sample_tree = true;

    AppStartupDecision decision;
    char message[256];
    ASSERT_TRUE(app_bootstrap_decide_tree_source(&options, "assets/example_tree.json", true, NULL,
                                                 &decision, message, sizeof(message)));
    ASSERT_EQ(decision.source, APP_STARTUP_SOURCE_PLACEHOLDER);
    ASSERT_STREQ(decision.resolved_path, "");
    ASSERT_TRUE(strstr(message, "--no-sample") != NULL);
    ASSERT_TRUE(strstr(message, "placeholder") != NULL);
}

DECLARE_TEST(test_bootstrap_handles_missing_sample)
{
    AppLaunchOptions options;
    memset(&options, 0, sizeof(options));

    AppStartupDecision decision;
    char message[256];
    ASSERT_TRUE(app_bootstrap_decide_tree_source(&options, NULL, true, NULL, &decision, message,
                                                 sizeof(message)));
    ASSERT_EQ(decision.source, APP_STARTUP_SOURCE_PLACEHOLDER);
    ASSERT_TRUE(strstr(message, "scripts/setup_dependencies") != NULL);
    ASSERT_TRUE(strstr(message, "placeholder") != NULL);
}

DECLARE_TEST(test_bootstrap_rejects_null_inputs)
{
    AppStartupDecision decision;
    char message[256];
    ASSERT_FALSE(app_bootstrap_decide_tree_source(NULL, NULL, true, NULL, &decision, message,
                                                  sizeof(message)));
    ASSERT_TRUE(strstr(message, "Invalid") != NULL);
}

DECLARE_TEST(test_bootstrap_respects_sample_history_flag)
{
    AppLaunchOptions options;
    memset(&options, 0, sizeof(options));

    AppStartupDecision decision;
    char message[256];
    ASSERT_TRUE(app_bootstrap_decide_tree_source(&options, "assets/example_tree.json", false, NULL,
                                                 &decision, message, sizeof(message)));
    ASSERT_EQ(decision.source, APP_STARTUP_SOURCE_PLACEHOLDER);
    ASSERT_TRUE(strstr(message, "already showcased") != NULL);
    ASSERT_TRUE(strstr(message, "has_loaded_sample_tree") != NULL);
}

void register_bootstrap_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_bootstrap_prefers_cli_path);
    REGISTER_TEST(registry, test_bootstrap_uses_sample_when_available);
    REGISTER_TEST(registry, test_bootstrap_respects_disable_flag);
    REGISTER_TEST(registry, test_bootstrap_handles_missing_sample);
    REGISTER_TEST(registry, test_bootstrap_rejects_null_inputs);
    REGISTER_TEST(registry, test_bootstrap_respects_sample_history_flag);
}
