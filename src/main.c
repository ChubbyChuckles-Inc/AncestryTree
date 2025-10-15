#include "at_log.h"

#include <stdio.h>

int main(void)
{
    AtLogger logger;
    at_logger_init(&logger);
    at_logger_set_level(&logger, AT_LOG_INFO);
    AT_LOG(&logger, AT_LOG_INFO, "AncestryTree prototype starting.");
    AT_LOG(&logger, AT_LOG_INFO, "Rendering systems are not yet initialized.");

    printf("AncestryTree stub runtime. Replace with engine initialization.\n");
    return 0;
}
