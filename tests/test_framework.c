#include "test_framework.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct TestRunnerState
{
    TestResult result;
} TestRunnerState;

static TestRunnerState g_runner_state;

void test_registry_init(TestRegistry *registry, TestCase *storage, int capacity)
{
    if (!registry)
    {
        return;
    }
    registry->cases = storage;
    registry->capacity = capacity;
    registry->count = 0;
}

bool test_registry_add(TestRegistry *registry, const char *name, TestFunction function)
{
    if (!registry || !registry->cases || registry->count >= registry->capacity)
    {
        return false;
    }
    registry->cases[registry->count].name = name;
    registry->cases[registry->count].function = function;
    registry->count++;
    return true;
}

static void test_runner_reset_state(void)
{
    g_runner_state.result.executed = 0;
    g_runner_state.result.failures = 0;
}

void test_framework_record_failure(const char *expression, const char *file, int line, const char *message)
{
    g_runner_state.result.failures++;
    const char *expr = expression ? expression : "(no expression)";
    const char *msg = message ? message : "";
    fprintf(stderr, "[FAIL] %s:%d :: %s %s\n", file, line, expr, msg);
}

static void test_runner_print_summary(const TestResult *result)
{
    fprintf(stdout, "\n=== Test Summary ===\n");
    fprintf(stdout, "Executed: %d\n", result->executed);
    fprintf(stdout, "Failures: %d\n", result->failures);
    fprintf(stdout, "Status: %s\n", result->failures == 0 ? "PASS" : "FAIL");
}

TestResult test_registry_run(const TestRegistry *registry)
{
    test_runner_reset_state();
    if (!registry || registry->count == 0)
    {
        fprintf(stderr, "No tests registered.\n");
        return g_runner_state.result;
    }

    for (int index = 0; index < registry->count; ++index)
    {
        const TestCase *test_case = &registry->cases[index];
        fprintf(stdout, "[TEST] %s...\n", test_case->name);
        clock_t start = clock();
        int failures_before = g_runner_state.result.failures;
        test_case->function();
        clock_t end = clock();
        double duration_ms = 0.0;
        if (end > start)
        {
            duration_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        }
        if (g_runner_state.result.failures == failures_before)
        {
            fprintf(stdout, "[ OK ] %s (%.2f ms)\n", test_case->name, duration_ms);
        }
        else
        {
            fprintf(stdout, "[FAIL] %s (%.2f ms)\n", test_case->name, duration_ms);
        }
        g_runner_state.result.executed++;
    }

    test_runner_print_summary(&g_runner_state.result);
    return g_runner_state.result;
}
