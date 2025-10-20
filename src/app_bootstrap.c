#include "app_bootstrap.h"

#include <stdio.h>
#include <string.h>

static void app_bootstrap_copy_path(char (*destination)[512], const char *source)
{
    if (!destination)
    {
        return;
    }
    if (!source || source[0] == '\0')
    {
        (*destination)[0] = '\0';
        return;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(*destination, sizeof(*destination), source, _TRUNCATE);
#else
    (void)snprintf(*destination, sizeof(*destination), "%s", source);
#endif
}

bool app_bootstrap_decide_tree_source(const AppLaunchOptions *options, const char *sample_tree_path,
                                      AppStartupDecision *decision, char *message_buffer, size_t message_capacity)
{
    if (message_buffer && message_capacity > 0U)
    {
        message_buffer[0] = '\0';
    }
    if (!options || !decision)
    {
        if (message_buffer && message_capacity > 0U)
        {
            (void)snprintf(message_buffer, message_capacity,
                           "Invalid startup parameters: options and decision must be non-null.");
        }
        return false;
    }

    decision->source = APP_STARTUP_SOURCE_NONE;
    decision->resolved_path[0] = '\0';

    if (options->tree_path[0] != '\0')
    {
        decision->source = APP_STARTUP_SOURCE_CLI_PATH;
        app_bootstrap_copy_path(&decision->resolved_path, options->tree_path);
        if (message_buffer && message_capacity > 0U)
        {
            (void)snprintf(message_buffer, message_capacity, "Loading family tree from '%s'.", options->tree_path);
        }
        return true;
    }

    bool sample_available = (sample_tree_path && sample_tree_path[0] != '\0');
    if (!options->disable_sample_tree && sample_available)
    {
        decision->source = APP_STARTUP_SOURCE_SAMPLE_ASSET;
        app_bootstrap_copy_path(&decision->resolved_path, sample_tree_path);
        if (message_buffer && message_capacity > 0U)
        {
            (void)snprintf(message_buffer, message_capacity, "Sample family tree detected at '%s'.", sample_tree_path);
        }
        return true;
    }

    decision->source = APP_STARTUP_SOURCE_PLACEHOLDER;
    if (!sample_available && message_buffer && message_capacity > 0U)
    {
        (void)snprintf(message_buffer, message_capacity,
                       "No sample tree located; the session will start with a placeholder hologram.");
    }
    else if (options->disable_sample_tree && message_buffer && message_capacity > 0U)
    {
        (void)snprintf(message_buffer, message_capacity,
                       "Sample tree disabled by command-line switch; starting with a placeholder hologram.");
    }
    return true;
}
