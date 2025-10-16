#ifndef APP_CLI_H
#define APP_CLI_H

#include "at_log.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct AppLaunchOptions
{
    bool show_help;
    bool disable_sample_tree;
    AtLogLevel log_level;
    char tree_path[512];
} AppLaunchOptions;

bool app_cli_parse(int argc, char **argv, AppLaunchOptions *options, char *error_buffer, size_t error_capacity);
void app_cli_print_usage(const char *program_name);

#endif /* APP_CLI_H */
