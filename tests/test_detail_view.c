#include "detail_view.h"
#include "expansion.h"
#include "test_framework.h"

#include <stdio.h>
#include <string.h>

TEST(test_detail_view_init_configures_slots)
{
    DetailViewSystem *system = detail_view_create();
    ASSERT_NOT_NULL(system);
    detail_view_cleanup(system);
    ASSERT_TRUE(detail_view_init(system));

    DetailViewTimelineInfo timeline_info;
    ASSERT_TRUE(detail_view_get_timeline_info(system, 0U, &timeline_info));
    ASSERT_FALSE(detail_view_get_timeline_info(system, DETAIL_VIEW_MAX_TIMELINE_SLOTS, &timeline_info));

    DetailViewPanelInfo panel_info;
    ASSERT_TRUE(detail_view_get_panel_info(system, 0U, &panel_info));
    ASSERT_FALSE(detail_view_get_panel_info(system, DETAIL_VIEW_MAX_PANEL_SLOTS, &panel_info));

    detail_view_destroy(system);
}

TEST(test_detail_view_set_content_clamps_fact_count)
{
    DetailViewSystem *system = detail_view_create();
    ASSERT_NOT_NULL(system);

    DetailViewContent content;
    memset(&content, 0, sizeof(content));
    snprintf(content.name, sizeof(content.name), "Test Subject");
    snprintf(content.lifespan, sizeof(content.lifespan), "1990-2020");
    content.fact_count = DETAIL_VIEW_MAX_FACTS + 3U;
    for (size_t index = 0; index < DETAIL_VIEW_MAX_FACTS; ++index)
    {
        snprintf(content.facts[index], sizeof(content.facts[index]), "Fact %zu", index);
    }

    ASSERT_TRUE(detail_view_set_content(system, &content));
    ASSERT_TRUE(detail_view_content_ready(system));

    const DetailViewContent *stored = detail_view_get_content(system);
    ASSERT_NOT_NULL(stored);
    ASSERT_EQ(stored->fact_count, DETAIL_VIEW_MAX_FACTS);
    ASSERT_STREQ(stored->facts[DETAIL_VIEW_MAX_FACTS - 1U], "Fact 4");

    detail_view_destroy(system);
}

TEST(test_detail_view_update_tracks_phases)
{
    DetailViewSystem *system = detail_view_create();
    ASSERT_NOT_NULL(system);

    for (int step = 0; step < 12; ++step)
    {
        detail_view_update(system, 0.1f, NULL, 1.0f, 0.5f, 0.0f);
    }
    ASSERT_FLOAT_NEAR(detail_view_get_timeline_phase(system), 1.0f, 0.15f);
    ASSERT_FLOAT_NEAR(detail_view_get_panel_phase(system), 0.5f, 0.15f);

    for (int step = 0; step < 18; ++step)
    {
        detail_view_update(system, 0.1f, NULL, 0.0f, 0.0f, 0.0f);
    }
    ASSERT_FLOAT_NEAR(detail_view_get_timeline_phase(system), 0.0f, 0.15f);
    ASSERT_FLOAT_NEAR(detail_view_get_panel_phase(system), 0.0f, 0.15f);

    detail_view_destroy(system);
}

TEST(test_detail_view_update_responds_to_expansion_activation)
{
    DetailViewSystem *system = detail_view_create();
    ASSERT_NOT_NULL(system);

    ExpansionState expansion;
    expansion_state_reset(&expansion);
    expansion.active = true;
    expansion.progress_current = 0.6f;
    expansion.room_light_current = 0.7f;
    expansion.fade_current = 0.3f;
    expansion.inactive_current = 0.6f;
    expansion.tree_light_current = 0.5f;
    expansion.baseline_radius = 1.0f;
    expansion.detail_radius = 3.0f;
    expansion.radius_current = 2.2f;

    detail_view_update(system, 0.16f, &expansion, 0.0f, 0.0f, 0.0f);

    ASSERT_TRUE(detail_view_get_detail_phase(system) > 0.0f);
    ASSERT_TRUE(detail_view_get_timeline_phase(system) > 0.0f);
    ASSERT_TRUE(detail_view_get_panel_phase(system) > 0.0f);

    detail_view_destroy(system);
}

void register_detail_view_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_detail_view_init_configures_slots);
    REGISTER_TEST(registry, test_detail_view_set_content_clamps_fact_count);
    REGISTER_TEST(registry, test_detail_view_update_tracks_phases);
    REGISTER_TEST(registry, test_detail_view_update_responds_to_expansion_activation);
}
