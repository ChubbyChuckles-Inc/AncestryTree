#ifndef ONBOARDING_H
#define ONBOARDING_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum OnboardingHint
    {
        ONBOARDING_HINT_NONE = 0,
        ONBOARDING_HINT_MENU_NEW_TREE,
        ONBOARDING_HINT_MENU_OPEN_TREE,
        ONBOARDING_HINT_MENU_SAVE,
        ONBOARDING_HINT_MENU_SAVE_AS,
        ONBOARDING_HINT_RESET_CAMERA,
        ONBOARDING_HINT_FOCUS_ROOTS,
        ONBOARDING_HINT_ICON_SEARCH,
        ONBOARDING_HINT_ICON_ADD_PERSON,
        ONBOARDING_HINT_ICON_SETTINGS,
        ONBOARDING_HINT_ICON_HELP,
        ONBOARDING_HINT_COUNT
    } OnboardingHint;

    typedef struct OnboardingState
    {
        bool active;
        bool completed;
        bool tooltips_enabled;
        int current_step;
        float elapsed_step;
        float completion_timer;
        unsigned int hint_progress_mask;
    } OnboardingState;

    void onboarding_init(OnboardingState *state, bool completed);
    void onboarding_set_active(OnboardingState *state, bool active);
    void onboarding_mark_completed(OnboardingState *state);
    void onboarding_reset(OnboardingState *state);
    void onboarding_skip(OnboardingState *state);
    void onboarding_update(OnboardingState *state, float delta_seconds);
    void onboarding_advance(OnboardingState *state);
    void onboarding_enable_tooltips(OnboardingState *state, bool enabled);
    bool onboarding_is_active(const OnboardingState *state);
    bool onboarding_is_completed(const OnboardingState *state);
    bool onboarding_tooltips_enabled(const OnboardingState *state);
    const char *onboarding_current_title(const OnboardingState *state);
    const char *onboarding_current_body(const OnboardingState *state);
    size_t onboarding_step_count(void);
    const char *onboarding_hint_text(OnboardingHint hint);
    bool onboarding_hint_active(const OnboardingState *state, OnboardingHint hint);
    void onboarding_track_hint(OnboardingState *state, OnboardingHint hint);
    size_t onboarding_remaining_hints(const OnboardingState *state);
    size_t onboarding_step_hint_total(const OnboardingState *state);
    float onboarding_step_completion(const OnboardingState *state);

#ifdef __cplusplus
}
#endif

#endif /* ONBOARDING_H */
