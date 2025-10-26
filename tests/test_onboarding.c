#include "onboarding.h"
#include "test_framework.h"

TEST(onboarding_init_defaults)
{
    OnboardingState state;
    onboarding_init(&state, false);

    ASSERT_TRUE(onboarding_is_active(&state));
    ASSERT_FALSE(onboarding_is_completed(&state));
    ASSERT_TRUE(onboarding_tooltips_enabled(&state));
    ASSERT_EQ(state.current_step, 0);
    ASSERT_TRUE(onboarding_hint_active(&state, ONBOARDING_HINT_RESET_CAMERA));
}

TEST(onboarding_progress_advances_after_hints)
{
    OnboardingState state;
    onboarding_init(&state, false);

    onboarding_track_hint(&state, ONBOARDING_HINT_RESET_CAMERA);
    ASSERT_EQ(state.current_step, 0);
    ASSERT_EQ(onboarding_remaining_hints(&state), 1U);

    onboarding_track_hint(&state, ONBOARDING_HINT_FOCUS_ROOTS);
    ASSERT_EQ(state.current_step, 1);
    ASSERT_TRUE(onboarding_is_active(&state));
}

TEST(onboarding_completion_ratio_updates)
{
    OnboardingState state;
    onboarding_init(&state, false);

    ASSERT_FLOAT_NEAR(onboarding_step_completion(&state), 0.0f, 0.01f);

    onboarding_track_hint(&state, ONBOARDING_HINT_RESET_CAMERA);
    ASSERT_FLOAT_NEAR(onboarding_step_completion(&state), 0.5f, 0.05f);

    onboarding_track_hint(&state, ONBOARDING_HINT_FOCUS_ROOTS);
    ASSERT_EQ(state.current_step, 1);
    ASSERT_FLOAT_NEAR(onboarding_step_completion(&state), 0.0f, 0.05f);
}

TEST(onboarding_skip_disables_overlay)
{
    OnboardingState state;
    onboarding_init(&state, false);

    onboarding_skip(&state);

    ASSERT_FALSE(onboarding_is_active(&state));
    ASSERT_FALSE(onboarding_tooltips_enabled(&state));
}

void register_onboarding_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, onboarding_init_defaults);
    REGISTER_TEST(registry, onboarding_progress_advances_after_hints);
    REGISTER_TEST(registry, onboarding_completion_ratio_updates);
    REGISTER_TEST(registry, onboarding_skip_disables_overlay);
}
