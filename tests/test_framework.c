#include "test_framework.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#endif

typedef struct TestRunnerState
{
    TestResult result;
} TestRunnerState;

static TestRunnerState g_runner_state;

typedef struct TestWatchdog
{
    unsigned int timeout_ms;
    volatile int running;
    volatile int armed;
    unsigned long long start_ms;
    const char *volatile test_name;
#if defined(_WIN32)
    HANDLE thread;
#endif
} TestWatchdog;

static TestWatchdog g_watchdog = {0U,
                                  0,
                                  0,
                                  0ULL,
                                  NULL
#if defined(_WIN32)
                                  ,
                                  NULL
#endif
};

static unsigned int test_watchdog_resolve_timeout(void)
{
    static unsigned int cached = UINT_MAX;
    if (cached != UINT_MAX)
    {
        return cached;
    }
    cached          = 60000U;
    const char *env = NULL;
#if defined(_WIN32)
    char *env_copy    = NULL;
    size_t env_length = 0U;
    if (_dupenv_s(&env_copy, &env_length, "ANCESTRYTREE_TEST_TIMEOUT_MS") == 0 && env_copy)
    {
        env = env_copy;
    }
#else
    env = getenv("ANCESTRYTREE_TEST_TIMEOUT_MS");
#endif
    if (env && env[0] != '\0')
    {
        char *end            = NULL;
        unsigned long parsed = strtoul(env, &end, 10);
        if (end != env)
        {
            if (parsed == 0UL)
            {
                cached = 0U;
            }
            else if (parsed <= 3600000UL)
            {
                cached = (unsigned int)parsed;
            }
        }
    }
#if defined(_WIN32)
    if (env_copy)
    {
        free(env_copy);
    }
#endif
    return cached;
}

static unsigned long long test_watchdog_now_ms(void)
{
#if defined(_WIN32)
    return (unsigned long long)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000ULL + (unsigned long long)(tv.tv_usec / 1000);
#endif
}

#if defined(_WIN32)
static DWORD WINAPI test_watchdog_thread(LPVOID param)
{
    (void)param;
    while (g_watchdog.running)
    {
        if (g_watchdog.armed && g_watchdog.timeout_ms > 0U)
        {
            unsigned long long now   = test_watchdog_now_ms();
            unsigned long long start = g_watchdog.start_ms;
            if (now >= start && (now - start) > g_watchdog.timeout_ms)
            {
                const char *name = g_watchdog.test_name ? g_watchdog.test_name : "<unknown>";
                fprintf(stderr, "\n[TIMEOUT] Test '%s' exceeded %u ms. Terminating test run.\n",
                        name, g_watchdog.timeout_ms);
                fflush(stderr);
                g_watchdog.armed = 0;
                TerminateProcess(GetCurrentProcess(), 0xDEAD0101U);
                break;
            }
        }
        Sleep(10U);
    }
    return 0U;
}

static void test_watchdog_start(void)
{
    if (g_watchdog.running)
    {
        return;
    }
    g_watchdog.timeout_ms = test_watchdog_resolve_timeout();
    if (g_watchdog.timeout_ms == 0U)
    {
        return;
    }
    g_watchdog.running = 1;
    g_watchdog.thread  = CreateThread(NULL, 0U, test_watchdog_thread, NULL, 0U, NULL);
    if (!g_watchdog.thread)
    {
        g_watchdog.running = 0;
        fprintf(stderr, "[WARN] Failed to create test watchdog thread. Timeouts disabled.\n");
    }
}

static void test_watchdog_stop(void)
{
    if (!g_watchdog.running)
    {
        return;
    }
    g_watchdog.running = 0;
    WaitForSingleObject(g_watchdog.thread, INFINITE);
    CloseHandle(g_watchdog.thread);
    g_watchdog.thread = NULL;
}

static void test_watchdog_arm(const char *name)
{
    if (!g_watchdog.running || g_watchdog.timeout_ms == 0U)
    {
        return;
    }
    g_watchdog.test_name = name;
    g_watchdog.start_ms  = test_watchdog_now_ms();
    g_watchdog.armed     = 1;
}

static void test_watchdog_disarm(void)
{
    if (!g_watchdog.running)
    {
        return;
    }
    g_watchdog.armed = 0;
}
#else
static void test_watchdog_start(void)
{
    unsigned int timeout = test_watchdog_resolve_timeout();
    if (timeout > 0U)
    {
        fprintf(stderr,
                "[WARN] Test watchdog is not implemented on this platform."
                " Timeout request (%u ms) ignored.\n",
                timeout);
    }
}

static void test_watchdog_stop(void) {}

static void test_watchdog_arm(const char *name) { (void)name; }

static void test_watchdog_disarm(void) {}
#endif

void test_registry_init(TestRegistry *registry, TestCase *storage, int capacity)
{
    if (!registry)
    {
        return;
    }
    registry->cases    = storage;
    registry->capacity = capacity;
    registry->count    = 0;
}

bool test_registry_add(TestRegistry *registry, const char *name, TestFunction function)
{
    if (!registry || !registry->cases || registry->count >= registry->capacity)
    {
        return false;
    }
    registry->cases[registry->count].name     = name;
    registry->cases[registry->count].function = function;
    registry->count++;
    return true;
}

static void test_runner_reset_state(void)
{
    g_runner_state.result.executed = 0;
    g_runner_state.result.failures = 0;
}

void test_framework_record_failure(const char *expression, const char *file, int line,
                                   const char *message)
{
    g_runner_state.result.failures++;
    const char *expr = expression ? expression : "(no expression)";
    const char *msg  = message ? message : "";
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

    test_watchdog_start();

    for (int index = 0; index < registry->count; ++index)
    {
        const TestCase *test_case = &registry->cases[index];
        fprintf(stdout, "[TEST] %s...\n", test_case->name);
        clock_t start       = clock();
        int failures_before = g_runner_state.result.failures;
        test_watchdog_arm(test_case->name);
        test_case->function();
        test_watchdog_disarm();
        clock_t end        = clock();
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
    test_watchdog_stop();
    return g_runner_state.result;
}
