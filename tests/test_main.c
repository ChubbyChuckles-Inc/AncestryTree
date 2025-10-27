#include "at_memory.h"
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
void register_detail_view_tests(TestRegistry *registry);
void register_detail_content_tests(TestRegistry *registry);
void register_image_panel_tests(TestRegistry *registry);
void register_expansion_tests(TestRegistry *registry);
void register_render_labels_tests(TestRegistry *registry);
void register_shortcuts_tests(TestRegistry *registry);
void register_settings_tests(TestRegistry *registry);
void register_settings_runtime_tests(TestRegistry *registry);
void register_search_tests(TestRegistry *registry);
void register_integration_tests(TestRegistry *registry);
void register_app_state_tests(TestRegistry *registry);
void register_error_tests(TestRegistry *registry);
void register_assets_tests(TestRegistry *registry);
void register_cli_tests(TestRegistry *registry);
void register_file_dialog_tests(TestRegistry *registry);
void register_bootstrap_tests(TestRegistry *registry);
void register_ui_scaling_tests(TestRegistry *registry);
void register_ui_theme_tests(TestRegistry *registry);
void register_memory_suite_tests(TestRegistry *registry);
void register_selection_particles_tests(TestRegistry *registry);
void register_life_particles_tests(TestRegistry *registry);
void register_ui_navigation_tests(TestRegistry *registry);
void register_onboarding_tests(TestRegistry *registry);
void register_screen_reader_tests(TestRegistry *registry);
void register_status_message_tests(TestRegistry *registry);

int main(void)
{
    TestRegistry registry;
    TestCase cases[192];
    test_registry_init(&registry, cases, 192);

#if AT_MEMORY_ENABLE_TRACKING
    at_memory_reset_tracking();
#endif

    register_string_tests(&registry);
    register_memory_tests(&registry);
    register_memory_suite_tests(&registry);
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
    register_selection_particles_tests(&registry);
    register_life_particles_tests(&registry);
    register_interaction_tests(&registry);
    register_detail_content_tests(&registry);
    register_detail_view_tests(&registry);
    register_image_panel_tests(&registry);
    register_expansion_tests(&registry);
    register_app_state_tests(&registry);
    register_shortcuts_tests(&registry);
    register_settings_tests(&registry);
    register_settings_runtime_tests(&registry);
    register_search_tests(&registry);
    register_integration_tests(&registry);
    register_error_tests(&registry);
    register_status_message_tests(&registry);
    register_assets_tests(&registry);
    register_cli_tests(&registry);
    register_file_dialog_tests(&registry);
    register_bootstrap_tests(&registry);
    register_ui_scaling_tests(&registry);
    register_ui_theme_tests(&registry);
    register_onboarding_tests(&registry);
    register_ui_navigation_tests(&registry);
    register_screen_reader_tests(&registry);

    TestResult result = test_registry_run(&registry);
    if (result.failures != 0)
    {
        fprintf(stderr, "%d test(s) failed.\n", result.failures);
#if AT_MEMORY_ENABLE_TRACKING
        AtMemoryStats failure_stats;
        at_memory_get_stats(&failure_stats);
        if (failure_stats.outstanding_allocations != 0U)
        {
            fprintf(
                stderr,
                "Detected %zu outstanding allocation(s) totalling %zu bytes while tests failed.\n",
                failure_stats.outstanding_allocations, failure_stats.outstanding_bytes);
            at_memory_report_leaks();
        }
#endif
        return 1;
    }

#if AT_MEMORY_ENABLE_TRACKING
    AtMemoryStats stats;
    at_memory_get_stats(&stats);
    if (stats.outstanding_allocations != 0U || stats.outstanding_bytes != 0U)
    {
        fprintf(
            stderr,
            "Detected %zu outstanding allocation(s) totalling %zu bytes after tests completed.\n",
            stats.outstanding_allocations, stats.outstanding_bytes);
        at_memory_report_leaks();
        return 1;
    }
#endif

    return 0;
}
