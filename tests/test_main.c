#include "test_framework.h"

#include <stdio.h>

void register_string_tests(TestRegistry *registry);
void register_memory_tests(TestRegistry *registry);
void register_person_tests(TestRegistry *registry);
void register_tree_tests(TestRegistry *registry);
void register_timeline_tests(TestRegistry *registry);
void register_date_tests(TestRegistry *registry);
void register_persistence_tests(TestRegistry *registry);
void register_person_tests(TestRegistry *registry);
void register_tree_tests(TestRegistry *registry);

int main(void)
{
    TestCase cases[64];
    TestRegistry registry;
    test_registry_init(&registry, cases, 64);

    register_string_tests(&registry);
    register_memory_tests(&registry);
    register_person_tests(&registry);
    register_tree_tests(&registry);
    register_timeline_tests(&registry);
    register_date_tests(&registry);
    register_persistence_tests(&registry);
    register_person_tests(&registry);
    register_tree_tests(&registry);

    TestResult result = test_registry_run(&registry);
    if (result.failures != 0)
    {
        fprintf(stderr, "%d test(s) failed.\n", result.failures);
        return 1;
    }
    return 0;
}
