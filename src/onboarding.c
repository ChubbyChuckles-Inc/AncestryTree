#include "onboarding.h"

#include <stddef.h>
#include <string.h>

#define ONBOARDING_COMPLETION_HOLD_SECONDS 2.0f

typedef struct OnboardingStep
{
    const char *title;
    const char *body;
} OnboardingStep;

static const OnboardingStep ONBOARDING_STEPS[] = {
    {"Welcome to AncestryTree",
     "Explore immersive holograms of your family history. Use the mouse or touchpad to orbit the "
     "tree, zoom with the scroll wheel, and click a sphere to inspect that person."},
    {"Guided Controls",
     "The menu bar offers New/Open/Save actions, while the left HUD lists camera shortcuts. Use "
     "Reset Camera to recenter, Focus Roots to view the oldest generation, and the search icon to "
     "filter by name."},
    {"Discover Detail View",
     "Enter detail mode by pressing Enter or clicking a person. You can now click another node "
     "while inside the sphere to instantly switch holographic rooms, review documents, and scroll "
     "the timeline."}};

static size_t onboarding_step_index(const OnboardingState *state)
{
    if (!state)
    {
        return 0U;
    }
    size_t count = onboarding_step_count();
    if (state->current_step < 0)
    {
        return 0U;
    }
    size_t step = (size_t)state->current_step;
    if (step >= count)
    {
        return count > 0U ? (count - 1U) : 0U;
    }
    return step;
}

void onboarding_init(OnboardingState *state, bool completed)
{
    if (!state)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->completed        = completed;
    state->active           = !completed;
    state->tooltips_enabled = !completed;
    state->current_step     = 0;
}

void onboarding_set_active(OnboardingState *state, bool active)
{
    if (!state)
    {
        return;
    }
    if (active)
    {
        state->active       = true;
        state->current_step = 0;
        state->elapsed_step = 0.0f;
    }
    else
    {
        state->active           = false;
        state->completion_timer = 0.0f;
        state->elapsed_step     = 0.0f;
        state->current_step     = 0;
    }
}

void onboarding_mark_completed(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    state->active           = false;
    state->completed        = true;
    state->tooltips_enabled = false;
    state->current_step     = 0;
    state->elapsed_step     = 0.0f;
    state->completion_timer = 0.0f;
}

void onboarding_reset(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    state->completed        = false;
    state->active           = true;
    state->tooltips_enabled = true;
    state->current_step     = 0;
    state->elapsed_step     = 0.0f;
    state->completion_timer = 0.0f;
}

void onboarding_update(OnboardingState *state, float delta_seconds)
{
    if (!state)
    {
        return;
    }
    if (!(delta_seconds > 0.0f))
    {
        return;
    }
    if (state->active)
    {
        state->elapsed_step += delta_seconds;
    }
    else if (state->completed)
    {
        state->completion_timer += delta_seconds;
        if (state->completion_timer > ONBOARDING_COMPLETION_HOLD_SECONDS)
        {
            state->completion_timer = ONBOARDING_COMPLETION_HOLD_SECONDS;
        }
    }
}

void onboarding_advance(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    if (!state->active)
    {
        return;
    }
    size_t count = onboarding_step_count();
    if (count == 0U)
    {
        onboarding_mark_completed(state);
        return;
    }
    state->current_step += 1;
    state->elapsed_step = 0.0f;
    if ((size_t)state->current_step >= count)
    {
        onboarding_mark_completed(state);
    }
}

void onboarding_enable_tooltips(OnboardingState *state, bool enabled)
{
    if (!state)
    {
        return;
    }
    state->tooltips_enabled = enabled;
}

bool onboarding_is_active(const OnboardingState *state) { return state ? state->active : false; }

bool onboarding_is_completed(const OnboardingState *state)
{
    return state ? state->completed : false;
}

bool onboarding_tooltips_enabled(const OnboardingState *state)
{
    return state ? state->tooltips_enabled : false;
}

size_t onboarding_step_count(void)
{
    return sizeof(ONBOARDING_STEPS) / sizeof(ONBOARDING_STEPS[0]);
}

const char *onboarding_current_title(const OnboardingState *state)
{
    size_t index = onboarding_step_index(state);
    if (onboarding_step_count() == 0U)
    {
        return "";
    }
    return ONBOARDING_STEPS[index].title;
}

const char *onboarding_current_body(const OnboardingState *state)
{
    size_t index = onboarding_step_index(state);
    if (onboarding_step_count() == 0U)
    {
        return "";
    }
    return ONBOARDING_STEPS[index].body;
}

const char *onboarding_hint_text(OnboardingHint hint)
{
    switch (hint)
    {
    case ONBOARDING_HINT_MENU_NEW_TREE:
        return "Create a fresh holographic tree. Existing data remains cached until you save.";
    case ONBOARDING_HINT_MENU_OPEN_TREE:
        return "Load an existing JSON tree file. We recommend keeping backups before editing.";
    case ONBOARDING_HINT_MENU_SAVE:
        return "Write changes to the current file. Use Save As to branch into a new snapshot.";
    case ONBOARDING_HINT_MENU_SAVE_AS:
        return "Export the tree to a new JSON file. Great for versioned archives or sharing.";
    case ONBOARDING_HINT_RESET_CAMERA:
        return "Snap the orbit camera back to its default distance and orientation.";
    case ONBOARDING_HINT_FOCUS_ROOTS:
        return "Frame the earliest generation so you can branch outward from the family origins.";
    case ONBOARDING_HINT_ICON_SEARCH:
        return "Search by name, alive status, or birth year range. Select a result to focus it.";
    case ONBOARDING_HINT_ICON_ADD_PERSON:
        return "Add a new holographic profile with relationships, documents, and timeline events.";
    case ONBOARDING_HINT_ICON_SETTINGS:
        return "Tune graphics quality, camera sensitivity, color schemes, and auto-save cadence.";
    case ONBOARDING_HINT_ICON_HELP:
        return "Open quick help with keyboard shortcuts, troubleshooting tips, and support links.";
    case ONBOARDING_HINT_NONE:
    default:
        return NULL;
    }
}
