#include "app_cli.h"

#include "at_string.h"

#include <stdio.h>
#include <string.h>

static void app_cli_options_reset(AppLaunchOptions *options)
{
    if (!options)
    {
        return;
    }
    options->show_help = false;
    options->disable_sample_tree = false;
    options->log_level = AT_LOG_INFO;
    options->tree_path[0] = '\0';
}

static bool app_cli_set_tree_path(AppLaunchOptions *options, const char *path, char *error_buffer,
                                  size_t error_capacity)
{
    if (!options || !path || path[0] == '\0')
    {
        if (error_buffer && error_capacity > 0U)
        {
            (void)snprintf(error_buffer, error_capacity, "Missing tree path value");
        }
        return false;
    }
    if (options->tree_path[0] != '\0')
    {
        if (error_buffer && error_capacity > 0U)
        {
            (void)snprintf(error_buffer, error_capacity, "Tree path already specified");
        }
        return false;
    }
    if (!at_string_copy(options->tree_path, sizeof(options->tree_path), path))
    {
        if (error_buffer && error_capacity > 0U)
        {
            (void)snprintf(error_buffer, error_capacity, "Tree path too long");
        }
        return false;
    }
    return true;
}

static bool app_cli_parse_log_level(const char *value, AtLogLevel *out_level)
{
    if (!value || !out_level)
    {
        return false;
    }
    if (strcmp(value, "debug") == 0 || strcmp(value, "DEBUG") == 0)
    {
        *out_level = AT_LOG_DEBUG;
        return true;
    }
    if (strcmp(value, "info") == 0 || strcmp(value, "INFO") == 0)
    {
        *out_level = AT_LOG_INFO;
        return true;
    }
    if (strcmp(value, "warn") == 0 || strcmp(value, "WARN") == 0 || strcmp(value, "warning") == 0)
    {
        *out_level = AT_LOG_WARN;
        return true;
    }
    if (strcmp(value, "error") == 0 || strcmp(value, "ERROR") == 0)
    {
        *out_level = AT_LOG_ERROR;
        return true;
    }
    if (strcmp(value, "fatal") == 0 || strcmp(value, "FATAL") == 0)
    {
        *out_level = AT_LOG_FATAL;
        return true;
    }
    return false;
}

bool app_cli_parse(int argc, char **argv, AppLaunchOptions *options, char *error_buffer, size_t error_capacity)
{
    app_cli_options_reset(options);
    if (argc <= 1)
    {
        if (error_buffer && error_capacity > 0U)
        {
            error_buffer[0] = '\0';
        }
        return true;
    }

    for (int index = 1; index < argc; ++index)
    {
        const char *argument = argv[index];
        if (!argument)
        {
            continue;
        }
        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0 || strcmp(argument, "-?") == 0)
        {
            options->show_help = true;
            continue;
        }
        if (strcmp(argument, "--no-sample") == 0)
        {
            options->disable_sample_tree = true;
            continue;
        }
        if (strcmp(argument, "--log-level") == 0 || strcmp(argument, "-l") == 0)
        {
            if ((index + 1) >= argc)
            {
                if (error_buffer && error_capacity > 0U)
                {
                    (void)snprintf(error_buffer, error_capacity, "--log-level requires a value");
                }
                return false;
            }
            const char *value = argv[++index];
            AtLogLevel level;
            if (!app_cli_parse_log_level(value, &level))
            {
                if (error_buffer && error_capacity > 0U)
                {
                    (void)snprintf(error_buffer, error_capacity, "Unknown log level '%s'", value);
                }
                return false;
            }
            options->log_level = level;
            continue;
        }
        if (strcmp(argument, "--load") == 0 || strcmp(argument, "-o") == 0)
        {
            if ((index + 1) >= argc)
            {
                if (error_buffer && error_capacity > 0U)
                {
                    (void)snprintf(error_buffer, error_capacity, "--load requires a path value");
                }
                return false;
            }
            const char *path = argv[++index];
            if (!app_cli_set_tree_path(options, path, error_buffer, error_capacity))
            {
                return false;
            }
            continue;
        }
        if (argument[0] == '-')
        {
            if (error_buffer && error_capacity > 0U)
            {
                (void)snprintf(error_buffer, error_capacity, "Unknown option '%s'", argument);
            }
            return false;
        }
        if (!app_cli_set_tree_path(options, argument, error_buffer, error_capacity))
        {
            return false;
        }
    }

    if (error_buffer && error_capacity > 0U)
    {
        error_buffer[0] = '\0';
    }
    return true;
}

void app_cli_print_usage(const char *program_name)
{
    const char *exe_name = program_name ? program_name : "ancestrytree";
    printf("Usage: %s [options] [tree.json]\n", exe_name);
    printf("Options:\n");
    printf("  --help, -h            Show this help information and exit.\n");
    printf("  --load <path>         Load the specified tree file at startup.\n");
    printf("  --log-level <level>   Set minimum log level (debug, info, warn, error, fatal).\n");
    printf("  --no-sample           Skip loading the bundled sample tree when no file is supplied.\n");
    printf("\nIf no file is provided, the sample tree is loaded when available; otherwise a placeholder\nfamily is generated.\n");
}
