#include "test_framework.h"

#include <stdio.h>

void register_string_tests(TestRegistry *registry);
void register_memory_tests(TestRegistry *registry);
void register_log_tests(TestRegistry *registry);
void register_person_tests(TestRegistry *registry);
void register_tree_tests(TestRegistry *registry);
void register_timeline_tests(TestRegistry *registry);
void register_date_tests(TestRegistry *registry);
void register_persistence_tests(TestRegistry *registry);
void register_persistence_auto_save_tests(TestRegistry *registry);
void register_json_parser_tests(TestRegistry *registry);
void register_layout_tests(TestRegistry *registry);
void register_graphics_tests(TestRegistry *registry);
void register_camera_controller_tests(TestRegistry *registry);
void register_path_utils_tests(TestRegistry *registry);
void register_render_tests(TestRegistry *registry);
void register_interaction_tests(TestRegistry *registry);
void register_render_labels_tests(TestRegistry *registry);
void register_shortcuts_tests(TestRegistry *registry);
void register_settings_tests(TestRegistry *registry);
void register_settings_runtime_tests(TestRegistry *registry);
void register_search_tests(TestRegistry *registry);
void register_integration_tests(TestRegistry *registry);
void register_app_state_tests(TestRegistry *registry);
void register_error_tests(TestRegistry *registry);
void register_assets_tests(TestRegistry *registry);

int main(void)
{
    TestRegistry registry;
    TestCase cases[128];
    test_registry_init(&registry, cases, 128);

    register_string_tests(&registry);
    register_memory_tests(&registry);
    register_log_tests(&registry);
    register_person_tests(&registry);
    register_tree_tests(&registry);
    register_timeline_tests(&registry);
    register_date_tests(&registry);
    register_persistence_tests(&registry);
    register_persistence_auto_save_tests(&registry);
    register_json_parser_tests(&registry);
    register_layout_tests(&registry);
    register_graphics_tests(&registry);
    register_camera_controller_tests(&registry);
    register_path_utils_tests(&registry);
    register_render_tests(&registry);
    register_render_labels_tests(&registry);
    register_interaction_tests(&registry);
    register_app_state_tests(&registry);
    register_shortcuts_tests(&registry);
    register_settings_tests(&registry);
    register_settings_runtime_tests(&registry);
    register_search_tests(&registry);
    register_integration_tests(&registry);
    register_error_tests(&registry);
    register_assets_tests(&registry);

    TestResult result = test_registry_run(&registry);
    if (result.failures != 0)
    {
        fprintf(stderr, "%d test(s) failed.\n", result.failures);
        return 1;
    }
    return 0;
}
