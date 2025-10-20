#ifndef APP_BOOTSTRAP_H
#define APP_BOOTSTRAP_H

#include "app_cli.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum AppStartupSource
    {
        APP_STARTUP_SOURCE_NONE = 0,
        APP_STARTUP_SOURCE_CLI_PATH,
        APP_STARTUP_SOURCE_SAMPLE_ASSET,
        APP_STARTUP_SOURCE_PLACEHOLDER
    } AppStartupSource;

    typedef struct AppStartupDecision
    {
        AppStartupSource source;
        char resolved_path[512];
    } AppStartupDecision;

    bool app_bootstrap_decide_tree_source(const AppLaunchOptions *options, const char *sample_tree_path,
                                          AppStartupDecision *decision, char *message_buffer, size_t message_capacity);

#ifdef __cplusplus
}
#endif

#endif /* APP_BOOTSTRAP_H */
