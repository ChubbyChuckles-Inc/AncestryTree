#include "test_framework.h"

#include "app.h"
#include "camera_controller.h"
#include "expansion.h"
#include "settings.h"

static bool setup_basic_app_state(AppState *app_state, FamilyTree **out_tree, LayoutResult *out_layout,
                                  CameraController *out_camera, InteractionState *out_interaction,
                                  Settings *out_settings, Settings *out_persisted)
{
    if (!app_state || !out_tree || !out_layout || !out_camera || !out_interaction || !out_settings ||
        !out_persisted)
    {
        return false;
    }

    settings_init_defaults(out_settings);
    *out_persisted = *out_settings;

    FamilyTree *tree = family_tree_create("Detail View Test");
    if (!tree)
    {
        return false;
    }

    Person *person = person_create(1);
    if (!person)
    {
        family_tree_destroy(tree);
        return false;
    }
    if (!family_tree_add_person(tree, person))
    {
        person_destroy(person);
        family_tree_destroy(tree);
        return false;
    }

    LayoutResult layout = layout_calculate_with_algorithm(tree, LAYOUT_ALGORITHM_HIERARCHICAL);
    if (layout.count == 0U)
    {
        layout_result_destroy(&layout);
        family_tree_destroy(tree);
        return false;
    }

    CameraControllerConfig camera_config;
    camera_controller_config_default(&camera_config);
    if (!camera_controller_init(out_camera, &camera_config))
    {
        layout_result_destroy(&layout);
        family_tree_destroy(tree);
        return false;
    }

    interaction_state_init(out_interaction);

    app_state_init(app_state);
    *out_tree = tree;
    *out_layout = layout;
    if (!app_state_configure(app_state, out_tree, out_layout, out_interaction, out_camera, out_settings, out_persisted))
    {
        app_state_shutdown(app_state);
        layout_result_destroy(out_layout);
        family_tree_destroy(tree);
        return false;
    }

    return true;
}

static void teardown_basic_app_state(AppState *app_state, FamilyTree *tree, LayoutResult *layout)
{
    if (!app_state || !tree || !layout)
    {
        return;
    }
    app_state_shutdown(app_state);
    layout_result_destroy(layout);
    family_tree_destroy(tree);
}

TEST(test_detail_view_begin_marks_expansion_active)
{
    AppState app_state;
    FamilyTree *tree = NULL;
    LayoutResult layout = {0};
    CameraController camera;
    InteractionState interaction;
    Settings settings;
    Settings persisted;

    ASSERT_TRUE(setup_basic_app_state(&app_state, &tree, &layout, &camera, &interaction, &settings, &persisted));

    Person *person = family_tree_find_person(tree, 1);
    ASSERT_NOT_NULL(person);

    ASSERT_TRUE(app_state_begin_detail_view(&app_state, person));

    const ExpansionState *expansion = app_state_get_expansion(&app_state);
    ASSERT_NOT_NULL(expansion);
    ASSERT_TRUE(expansion_is_active(expansion));
    ASSERT_FALSE(expansion_is_reversing(expansion));
    ASSERT_FALSE(expansion_is_in_detail_mode(expansion));

    app_state_tick(&app_state, 1.0f);

    expansion = app_state_get_expansion(&app_state);
    ASSERT_TRUE(expansion_is_active(expansion));
    ASSERT_TRUE(expansion_is_in_detail_mode(expansion));
    ASSERT_FALSE(expansion_is_reversing(expansion));

    teardown_basic_app_state(&app_state, tree, &layout);
}

TEST(test_detail_view_exit_triggers_reversing)
{
    AppState app_state;
    FamilyTree *tree = NULL;
    LayoutResult layout = {0};
    CameraController camera;
    InteractionState interaction;
    Settings settings;
    Settings persisted;

    ASSERT_TRUE(setup_basic_app_state(&app_state, &tree, &layout, &camera, &interaction, &settings, &persisted));

    Person *person = family_tree_find_person(tree, 1);
    ASSERT_NOT_NULL(person);

    ASSERT_TRUE(app_state_begin_detail_view(&app_state, person));
    app_state_tick(&app_state, 1.0f);

    app_state_request_detail_exit(&app_state);
    const ExpansionState *expansion = app_state_get_expansion(&app_state);
    ASSERT_NOT_NULL(expansion);
    ASSERT_TRUE(expansion_is_active(expansion));
    ASSERT_TRUE(expansion_is_reversing(expansion));
    ASSERT_FALSE(expansion_is_in_detail_mode(expansion));

    app_state_tick(&app_state, 1.0f);
    expansion = app_state_get_expansion(&app_state);
    ASSERT_FALSE(expansion_is_active(expansion));

    teardown_basic_app_state(&app_state, tree, &layout);
}

void register_detail_view_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_detail_view_begin_marks_expansion_active);
    REGISTER_TEST(registry, test_detail_view_exit_triggers_reversing);
}
