#include "app_bootstrap.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void app_bootstrap_write_message(char *buffer, size_t capacity, const char *format, ...)
{
    if (!buffer || capacity == 0U || !format)
    {
        return;
    }
    va_list args;
    va_start(args, format);
    int written = vsnprintf(buffer, capacity, format, args);
    va_end(args);
    if (written < 0)
    {
        buffer[0] = '\0';
        return;
    }
    if ((size_t)written >= capacity)
    {
        buffer[capacity - 1U] = '\0';
    }
}

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
                                      bool sample_allowed, AppStartupDecision *decision,
                                      char *message_buffer, size_t message_capacity)
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

    decision->source           = APP_STARTUP_SOURCE_NONE;
    decision->resolved_path[0] = '\0';

    if (options->tree_path[0] != '\0')
    {
        decision->source = APP_STARTUP_SOURCE_CLI_PATH;
        app_bootstrap_copy_path(&decision->resolved_path, options->tree_path);
        if (message_buffer && message_capacity > 0U)
        {
            app_bootstrap_write_message(
                message_buffer, message_capacity,
                "Loading family tree from '%s'. Press Ctrl+O later to switch files if needed.",
                options->tree_path);
        }
        return true;
    }

    bool sample_available = (sample_tree_path && sample_tree_path[0] != '\0');
    bool use_sample       = sample_allowed && !options->disable_sample_tree && sample_available;
    if (use_sample)
    {
        decision->source = APP_STARTUP_SOURCE_SAMPLE_ASSET;
        app_bootstrap_copy_path(&decision->resolved_path, sample_tree_path);
        if (message_buffer && message_capacity > 0U)
        {
            app_bootstrap_write_message(message_buffer, message_capacity,
                                        "Sample family tree detected at '%s'. The guided hologram "
                                        "tour will launch automatically.",
                                        sample_tree_path);
        }
        return true;
    }

    decision->source = APP_STARTUP_SOURCE_PLACEHOLDER;
    /* Manual QA: rename or remove assets/example_tree.json and launch to verify the
     * guidance recommends rerunning the setup script and still falls back safely. */
    if (!sample_available && message_buffer && message_capacity > 0U)
    {
        const char *expected_path =
            sample_tree_path ? sample_tree_path : "assets/example_tree.json";
        app_bootstrap_write_message(
            message_buffer, message_capacity,
            "Sample family tree not found at '%s'. Re-run scripts/setup_dependencies to restore "
            "bundled assets or copy the file manually before relaunching. Launching with a "
            "placeholder hologram.",
            expected_path);
    }
    else if (options->disable_sample_tree && message_buffer && message_capacity > 0U)
    {
        app_bootstrap_write_message(
            message_buffer, message_capacity,
            "Sample tree disabled by the --no-sample switch. Remove the flag from your launch "
            "options or update your configuration to re-enable the holographic tour. Starting with "
            "a placeholder hologram.");
    }
    else if (!sample_allowed && message_buffer && message_capacity > 0U)
    {
        app_bootstrap_write_message(message_buffer, message_capacity,
                                    "Sample tree already showcased for this profile. Reset "
                                    "'has_loaded_sample_tree' in assets/settings.cfg to review the "
                                    "guided tour again. Launching placeholder hologram instead.");
    }
    return true;
}
