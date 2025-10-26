#ifndef PROGRESS_TRACKER_H
#define PROGRESS_TRACKER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ProgressPhase
    {
        PROGRESS_PHASE_IDLE = 0,
        PROGRESS_PHASE_ACTIVE,
        PROGRESS_PHASE_SUCCESS,
        PROGRESS_PHASE_FAILURE
    } ProgressPhase;

    typedef struct ProgressTracker
    {
        ProgressPhase phase;
        float value;
        float displayed_value;
        bool value_valid;
        char label[96];
        char completion_label[96];
        float elapsed;
        float completion_elapsed;
        float smoothing_rate;
    } ProgressTracker;

    void progress_tracker_reset(ProgressTracker *tracker);
    void progress_tracker_begin(ProgressTracker *tracker, const char *label);
    void progress_tracker_update(ProgressTracker *tracker, float delta_seconds, float value);
    void progress_tracker_complete(ProgressTracker *tracker, bool success, const char *label);
    bool progress_tracker_is_active(const ProgressTracker *tracker);
    bool progress_tracker_has_value(const ProgressTracker *tracker);
    float progress_tracker_display_value(const ProgressTracker *tracker);
    const char *progress_tracker_label(const ProgressTracker *tracker);
    const char *progress_tracker_completion_label(const ProgressTracker *tracker);
    ProgressPhase progress_tracker_phase(const ProgressTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif /* PROGRESS_TRACKER_H */
