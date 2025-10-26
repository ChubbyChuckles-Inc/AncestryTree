#include "progress_tracker.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define PROGRESS_COMPLETION_HOLD 2.0f
#define PROGRESS_SMOOTHING_RATE 6.0f

static void progress_tracker_copy_label(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0U)
    {
        return;
    }
    if (!source)
    {
        destination[0] = '\0';
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(destination, capacity, source, _TRUNCATE);
#else
    (void)snprintf(destination, capacity, "%s", source);
#endif
}

void progress_tracker_reset(ProgressTracker *tracker)
{
    if (!tracker)
    {
        return;
    }
    memset(tracker, 0, sizeof(*tracker));
    tracker->phase           = PROGRESS_PHASE_IDLE;
    tracker->value           = 0.0f;
    tracker->displayed_value = 0.0f;
    tracker->smoothing_rate  = PROGRESS_SMOOTHING_RATE;
}

void progress_tracker_begin(ProgressTracker *tracker, const char *label)
{
    if (!tracker)
    {
        return;
    }
    tracker->phase              = PROGRESS_PHASE_ACTIVE;
    tracker->value              = 0.0f;
    tracker->displayed_value    = 0.0f;
    tracker->value_valid        = false;
    tracker->elapsed            = 0.0f;
    tracker->completion_elapsed = 0.0f;
    progress_tracker_copy_label(tracker->label, sizeof(tracker->label),
                                label ? label : "Processing...");
    tracker->completion_label[0] = '\0';
}

void progress_tracker_update(ProgressTracker *tracker, float delta_seconds, float value)
{
    if (!tracker)
    {
        return;
    }
    if (!(delta_seconds > 0.0f))
    {
        delta_seconds = 0.0f;
    }

    tracker->elapsed += delta_seconds;

    if (tracker->phase == PROGRESS_PHASE_ACTIVE)
    {
        if (value >= 0.0f && value <= 1.0f)
        {
            tracker->value       = value;
            tracker->value_valid = true;
        }
        if (tracker->value_valid)
        {
            float target = tracker->value;
            float rate   = tracker->smoothing_rate;
            float factor = 1.0f - expf(-rate * delta_seconds);
            tracker->displayed_value += (target - tracker->displayed_value) * factor;
            if (tracker->displayed_value > 1.0f)
            {
                tracker->displayed_value = 1.0f;
            }
            if (tracker->displayed_value < 0.0f)
            {
                tracker->displayed_value = 0.0f;
            }
        }
        else if (delta_seconds > 0.0f)
        {
            tracker->displayed_value += delta_seconds * 0.12f;
            if (tracker->displayed_value > 1.0f)
            {
                tracker->displayed_value = fmodf(tracker->displayed_value, 1.0f);
            }
        }
    }
    else if (tracker->phase == PROGRESS_PHASE_SUCCESS || tracker->phase == PROGRESS_PHASE_FAILURE)
    {
        tracker->completion_elapsed += delta_seconds;
        if (tracker->completion_elapsed >= PROGRESS_COMPLETION_HOLD)
        {
            progress_tracker_reset(tracker);
        }
    }
}

void progress_tracker_complete(ProgressTracker *tracker, bool success, const char *label)
{
    if (!tracker)
    {
        return;
    }
    tracker->phase              = success ? PROGRESS_PHASE_SUCCESS : PROGRESS_PHASE_FAILURE;
    tracker->completion_elapsed = 0.0f;
    if (tracker->value_valid)
    {
        tracker->displayed_value = 1.0f;
    }
    progress_tracker_copy_label(tracker->completion_label, sizeof(tracker->completion_label),
                                label ? label
                                      : (success ? "Operation completed." : "Operation failed."));
}

bool progress_tracker_is_active(const ProgressTracker *tracker)
{
    if (!tracker)
    {
        return false;
    }
    return tracker->phase != PROGRESS_PHASE_IDLE;
}

bool progress_tracker_has_value(const ProgressTracker *tracker)
{
    return tracker ? tracker->value_valid : false;
}

float progress_tracker_display_value(const ProgressTracker *tracker)
{
    if (!tracker)
    {
        return 0.0f;
    }
    if (!tracker->value_valid)
    {
        return tracker->displayed_value;
    }
    if (tracker->displayed_value < 0.0f)
    {
        return 0.0f;
    }
    if (tracker->displayed_value > 1.0f)
    {
        return 1.0f;
    }
    return tracker->displayed_value;
}

const char *progress_tracker_label(const ProgressTracker *tracker)
{
    if (!tracker)
    {
        return "";
    }
    if (tracker->phase == PROGRESS_PHASE_SUCCESS || tracker->phase == PROGRESS_PHASE_FAILURE)
    {
        if (tracker->completion_label[0] != '\0')
        {
            return tracker->completion_label;
        }
    }
    return tracker->label;
}

const char *progress_tracker_completion_label(const ProgressTracker *tracker)
{
    if (!tracker)
    {
        return "";
    }
    return tracker->completion_label;
}

ProgressPhase progress_tracker_phase(const ProgressTracker *tracker)
{
    return tracker ? tracker->phase : PROGRESS_PHASE_IDLE;
}
