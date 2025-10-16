#include "persistence.h"
#include "test_framework.h"
#include "test_persistence_helpers.h"

#include <string.h>

typedef struct AutoSaveFixture
{
    FamilyTree *tree;
} AutoSaveFixture;

static FamilyTree *fixture_tree_supplier(void *context)
{
    AutoSaveFixture *fixture = (AutoSaveFixture *)context;
    return fixture ? fixture->tree : NULL;
}

static void fixture_init(AutoSaveFixture *fixture)
{
    fixture->tree = test_build_sample_tree();
    ASSERT_NOT_NULL(fixture->tree);
}

static void fixture_cleanup(AutoSaveFixture *fixture)
{
    if (fixture->tree)
    {
        family_tree_destroy(fixture->tree);
        fixture->tree = NULL;
    }
}

TEST(test_persistence_auto_save_triggers_after_interval)
{
    AutoSaveFixture fixture;
    fixture_init(&fixture);

    char auto_save_path[128];
    test_temp_file_path(auto_save_path, sizeof(auto_save_path), "autosave.json");
    test_delete_file(auto_save_path);

    PersistenceAutoSave state;
    memset(&state, 0, sizeof(state));
    PersistenceAutoSaveConfig config;
    config.tree_supplier = fixture_tree_supplier;
    config.user_data = &fixture;
    config.path = auto_save_path;
    config.interval_seconds = 1U;

    char error[256];
    ASSERT_TRUE(persistence_auto_save_init(&state, &config, error, sizeof(error)));
    ASSERT_FALSE(test_file_exists(auto_save_path));

    ASSERT_TRUE(persistence_auto_save_tick(&state, 0.4, error, sizeof(error)));
    ASSERT_FALSE(test_file_exists(auto_save_path));

    persistence_auto_save_mark_dirty(&state);
    ASSERT_TRUE(persistence_auto_save_tick(&state, 0.4, error, sizeof(error)));
    ASSERT_FALSE(test_file_exists(auto_save_path));

    ASSERT_TRUE(persistence_auto_save_tick(&state, 0.6, error, sizeof(error)));
    ASSERT_TRUE(test_file_exists(auto_save_path));

    persistence_auto_save_shutdown(&state);
    test_delete_file(auto_save_path);
    fixture_cleanup(&fixture);
}

TEST(test_persistence_auto_save_flush_saves_immediately)
{
    AutoSaveFixture fixture;
    fixture_init(&fixture);

    char auto_save_path[128];
    test_temp_file_path(auto_save_path, sizeof(auto_save_path), "flush.json");
    test_delete_file(auto_save_path);

    PersistenceAutoSave state;
    memset(&state, 0, sizeof(state));
    PersistenceAutoSaveConfig config;
    config.tree_supplier = fixture_tree_supplier;
    config.user_data = &fixture;
    config.path = auto_save_path;
    config.interval_seconds = 30U;

    char error[256];
    ASSERT_TRUE(persistence_auto_save_init(&state, &config, error, sizeof(error)));

    persistence_auto_save_mark_dirty(&state);
    ASSERT_TRUE(persistence_auto_save_flush(&state, error, sizeof(error)));
    ASSERT_TRUE(test_file_exists(auto_save_path));

    persistence_auto_save_shutdown(&state);
    test_delete_file(auto_save_path);
    fixture_cleanup(&fixture);
}

TEST(test_persistence_auto_save_handles_path_updates)
{
    AutoSaveFixture fixture;
    fixture_init(&fixture);

    char first_path[128];
    char second_path[128];
    test_temp_file_path(first_path, sizeof(first_path), "first.json");
    test_temp_file_path(second_path, sizeof(second_path), "second.json");
    test_delete_file(first_path);
    test_delete_file(second_path);

    PersistenceAutoSave state;
    memset(&state, 0, sizeof(state));
    PersistenceAutoSaveConfig config;
    config.tree_supplier = fixture_tree_supplier;
    config.user_data = &fixture;
    config.path = first_path;
    config.interval_seconds = 10U;

    char error[256];
    ASSERT_TRUE(persistence_auto_save_init(&state, &config, error, sizeof(error)));

    persistence_auto_save_mark_dirty(&state);
    ASSERT_TRUE(persistence_auto_save_update_path(&state, second_path, error, sizeof(error)));
    ASSERT_TRUE(persistence_auto_save_flush(&state, error, sizeof(error)));

    ASSERT_FALSE(test_file_exists(first_path));
    ASSERT_TRUE(test_file_exists(second_path));

    persistence_auto_save_shutdown(&state);
    test_delete_file(first_path);
    test_delete_file(second_path);
    fixture_cleanup(&fixture);
}

TEST(test_persistence_auto_save_respects_disable)
{
    AutoSaveFixture fixture;
    fixture_init(&fixture);

    char auto_save_path[128];
    test_temp_file_path(auto_save_path, sizeof(auto_save_path), "disabled.json");
    test_delete_file(auto_save_path);

    PersistenceAutoSave state;
    memset(&state, 0, sizeof(state));
    PersistenceAutoSaveConfig config;
    config.tree_supplier = fixture_tree_supplier;
    config.user_data = &fixture;
    config.path = auto_save_path;
    config.interval_seconds = 1U;

    char error[256];
    ASSERT_TRUE(persistence_auto_save_init(&state, &config, error, sizeof(error)));

    persistence_auto_save_mark_dirty(&state);
    persistence_auto_save_set_enabled(&state, false);
    ASSERT_TRUE(persistence_auto_save_tick(&state, 5.0, error, sizeof(error)));
    ASSERT_FALSE(test_file_exists(auto_save_path));

    persistence_auto_save_set_enabled(&state, true);
    ASSERT_TRUE(persistence_auto_save_tick(&state, 5.0, error, sizeof(error)));
    ASSERT_TRUE(test_file_exists(auto_save_path));

    persistence_auto_save_shutdown(&state);
    test_delete_file(auto_save_path);
    fixture_cleanup(&fixture);
}

TEST(test_persistence_auto_save_interval_updates_reset_timer)
{
    AutoSaveFixture fixture;
    fixture_init(&fixture);

    char auto_save_path[128];
    test_temp_file_path(auto_save_path, sizeof(auto_save_path), "interval.json");
    test_delete_file(auto_save_path);

    PersistenceAutoSave state;
    memset(&state, 0, sizeof(state));
    PersistenceAutoSaveConfig config;
    config.tree_supplier = fixture_tree_supplier;
    config.user_data = &fixture;
    config.path = auto_save_path;
    config.interval_seconds = 15U;

    char error[256];
    ASSERT_TRUE(persistence_auto_save_init(&state, &config, error, sizeof(error)));

    state.elapsed_seconds = 9.5;
    persistence_auto_save_set_interval(&state, 42U);
    ASSERT_EQ(state.interval_seconds, 42U);
    ASSERT_FLOAT_NEAR(state.elapsed_seconds, 0.0f, 0.0001f);

    persistence_auto_save_shutdown(&state);
    test_delete_file(auto_save_path);
    fixture_cleanup(&fixture);
}

void register_persistence_auto_save_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_persistence_auto_save_triggers_after_interval);
    REGISTER_TEST(registry, test_persistence_auto_save_flush_saves_immediately);
    REGISTER_TEST(registry, test_persistence_auto_save_handles_path_updates);
    REGISTER_TEST(registry, test_persistence_auto_save_respects_disable);
    REGISTER_TEST(registry, test_persistence_auto_save_interval_updates_reset_timer);
}
