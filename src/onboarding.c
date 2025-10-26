#include "onboarding.h"

#include <stddef.h>
#include <string.h>

#define ONBOARDING_COMPLETION_HOLD_SECONDS 2.0f

typedef struct OnboardingStep
{
    const char *title;
    const char *body;
    const OnboardingHint *hints;
    size_t hint_count;
} OnboardingStep;

static const OnboardingHint STEP_VIEW_HINTS[] = {ONBOARDING_HINT_RESET_CAMERA,
                                                 ONBOARDING_HINT_FOCUS_ROOTS};

static const OnboardingHint STEP_FILE_HINTS[] = {
    ONBOARDING_HINT_MENU_NEW_TREE, ONBOARDING_HINT_MENU_OPEN_TREE, ONBOARDING_HINT_MENU_SAVE,
    ONBOARDING_HINT_MENU_SAVE_AS};

static const OnboardingHint STEP_TOOLBAR_HINTS[] = {
    ONBOARDING_HINT_ICON_SEARCH, ONBOARDING_HINT_ICON_ADD_PERSON, ONBOARDING_HINT_ICON_SETTINGS,
    ONBOARDING_HINT_ICON_HELP};

static const OnboardingStep ONBOARDING_STEPS[] = {
    {"Calibrate Your View",
     "Use Reset Camera to center the holographic tree, then trigger Focus Roots to anchor the "
     "oldest generation before you explore further.",
     STEP_VIEW_HINTS, sizeof(STEP_VIEW_HINTS) / sizeof(STEP_VIEW_HINTS[0])},
    {"Manage Your Archive",
     "Open the File menu to experiment with New, Open, Save, and Save As. Each action keeps the "
     "progress overlay updated so you always know when the holographic archive is written.",
     STEP_FILE_HINTS, sizeof(STEP_FILE_HINTS) / sizeof(STEP_FILE_HINTS[0])},
    {"Command Center Icons",
     "Hover the search, add person, settings, and quick help icons in the top bar. Interactive "
     "tooltips reveal how each panel accelerates holographic curation.",
     STEP_TOOLBAR_HINTS, sizeof(STEP_TOOLBAR_HINTS) / sizeof(STEP_TOOLBAR_HINTS[0])}};

static size_t onboarding_step_index(const OnboardingState *state);

static unsigned int onboarding_hint_bit(OnboardingHint hint)
{
    if (hint <= ONBOARDING_HINT_NONE || hint >= ONBOARDING_HINT_COUNT)
    {
        return 0U;
    }
    return 1U << (unsigned int)hint;
}

static unsigned int onboarding_step_mask(const OnboardingStep *step)
{
    if (!step || !step->hints || step->hint_count == 0U)
    {
        return 0U;
    }
    unsigned int mask = 0U;
    for (size_t index = 0U; index < step->hint_count; ++index)
    {
        mask |= onboarding_hint_bit(step->hints[index]);
    }
    return mask;
}

static size_t onboarding_popcount(unsigned int value)
{
    size_t count = 0U;
    while (value != 0U)
    {
        value &= (value - 1U);
        ++count;
    }
    return count;
}

static const OnboardingStep *onboarding_current_step(const OnboardingState *state)
{
    if (!state)
    {
        return NULL;
    }
    size_t count = onboarding_step_count();
    if (count == 0U)
    {
        return NULL;
    }
    size_t index = onboarding_step_index(state);
    if (index >= count)
    {
        index = count - 1U;
    }
    return &ONBOARDING_STEPS[index];
}

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
    state->completed          = completed;
    state->active             = !completed;
    state->tooltips_enabled   = !completed;
    state->current_step       = 0;
    state->hint_progress_mask = 0U;
}

void onboarding_set_active(OnboardingState *state, bool active)
{
    if (!state)
    {
        return;
    }
    if (active)
    {
        state->active             = true;
        state->current_step       = 0;
        state->elapsed_step       = 0.0f;
        state->hint_progress_mask = 0U;
    }
    else
    {
        state->active             = false;
        state->completion_timer   = 0.0f;
        state->elapsed_step       = 0.0f;
        state->current_step       = 0;
        state->hint_progress_mask = 0U;
    }
}

void onboarding_mark_completed(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    state->active             = false;
    state->completed          = true;
    state->tooltips_enabled   = false;
    state->current_step       = 0;
    state->elapsed_step       = 0.0f;
    state->completion_timer   = 0.0f;
    state->hint_progress_mask = 0U;
}

void onboarding_reset(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    state->completed          = false;
    state->active             = true;
    state->tooltips_enabled   = true;
    state->current_step       = 0;
    state->elapsed_step       = 0.0f;
    state->completion_timer   = 0.0f;
    state->hint_progress_mask = 0U;
}

void onboarding_skip(OnboardingState *state)
{
    if (!state)
    {
        return;
    }
    state->active             = false;
    state->tooltips_enabled   = false;
    state->elapsed_step       = 0.0f;
    state->completion_timer   = 0.0f;
    state->current_step       = 0;
    state->hint_progress_mask = 0U;
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
    state->elapsed_step       = 0.0f;
    state->hint_progress_mask = 0U;
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

bool onboarding_hint_active(const OnboardingState *state, OnboardingHint hint)
{
    if (!state || !state->tooltips_enabled)
    {
        return false;
    }
    if (!state->active)
    {
        return false;
    }
    if (hint <= ONBOARDING_HINT_NONE || hint >= ONBOARDING_HINT_COUNT)
    {
        return false;
    }
    const OnboardingStep *step = onboarding_current_step(state);
    if (!step || !step->hints || step->hint_count == 0U)
    {
        return false;
    }
    for (size_t index = 0U; index < step->hint_count; ++index)
    {
        if (step->hints[index] == hint)
        {
            return true;
        }
    }
    return false;
}

void onboarding_track_hint(OnboardingState *state, OnboardingHint hint)
{
    if (!state || !state->active)
    {
        return;
    }
    unsigned int bit = onboarding_hint_bit(hint);
    if (bit == 0U)
    {
        return;
    }
    const OnboardingStep *step = onboarding_current_step(state);
    if (!step || !step->hints || step->hint_count == 0U)
    {
        return;
    }
    bool matches = false;
    for (size_t index = 0U; index < step->hint_count; ++index)
    {
        if (step->hints[index] == hint)
        {
            matches = true;
            break;
        }
    }
    if (!matches)
    {
        return;
    }
    if ((state->hint_progress_mask & bit) != 0U)
    {
        return;
    }
    state->hint_progress_mask |= bit;
    unsigned int required = onboarding_step_mask(step);
    if (required != 0U && (state->hint_progress_mask & required) == required)
    {
        onboarding_advance(state);
    }
}

size_t onboarding_remaining_hints(const OnboardingState *state)
{
    if (!state)
    {
        return 0U;
    }
    const OnboardingStep *step = onboarding_current_step(state);
    if (!step)
    {
        return 0U;
    }
    unsigned int required = onboarding_step_mask(step);
    if (required == 0U)
    {
        return 0U;
    }
    unsigned int completed = state->hint_progress_mask & required;
    unsigned int pending   = required & ~completed;
    return onboarding_popcount(pending);
}

size_t onboarding_step_hint_total(const OnboardingState *state)
{
    if (!state)
    {
        return 0U;
    }
    const OnboardingStep *step = onboarding_current_step(state);
    if (!step)
    {
        return 0U;
    }
    return onboarding_popcount(onboarding_step_mask(step));
}

float onboarding_step_completion(const OnboardingState *state)
{
    if (!state)
    {
        return 0.0f;
    }
    size_t total = onboarding_step_hint_total(state);
    if (total == 0U)
    {
        return state->active ? 0.0f : 1.0f;
    }
    size_t remaining = onboarding_remaining_hints(state);
    if (remaining >= total)
    {
        return 0.0f;
    }
    float completed = (float)(total - remaining);
    float denom     = (float)total;
    if (!(denom > 0.0f))
    {
        return 0.0f;
    }
    float value = completed / denom;
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value > 1.0f)
    {
        value = 1.0f;
    }
    return value;
}
