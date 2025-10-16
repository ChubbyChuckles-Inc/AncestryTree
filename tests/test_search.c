#include "search.h"

#include "person.h"
#include "test_framework.h"

#include <stdlib.h>
#include <string.h>

static Person *make_person(uint32_t id, const char *first_name, const char *birth_date, bool alive)
{
    Person *person = person_create(id);
    if (!person)
    {
        return NULL;
    }
    if (!person_set_name(person, first_name, NULL, "Tester"))
    {
        person_destroy(person);
        return NULL;
    }
    if (birth_date && !person_set_birth(person, birth_date, "Somewhere"))
    {
        person_destroy(person);
        return NULL;
    }
    person->is_alive = alive;
    return person;
}

static FamilyTree *make_sample_tree(void)
{
    FamilyTree *tree = family_tree_create("Search Test");
    if (!tree)
    {
        return NULL;
    }
    Person *avery = make_person(1U, "Avery", "1988-02-14", true);
    Person *brenda = make_person(2U, "Brenda", "1960-06-01", false);
    Person *charles = make_person(3U, "Charles", "2005-09-30", true);
    if (!avery || !brenda || !charles)
    {
        person_destroy(avery);
        person_destroy(brenda);
        person_destroy(charles);
        family_tree_destroy(tree);
        return NULL;
    }
    if (!family_tree_add_person(tree, avery) || !family_tree_add_person(tree, brenda) ||
        !family_tree_add_person(tree, charles))
    {
        family_tree_destroy(tree);
        return NULL;
    }
    return tree;
}

DECLARE_TEST(test_search_name_substring_matches_case_insensitive)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring = "AVE";
    filter.include_alive = true;
    filter.include_deceased = true;
    filter.use_birth_year_range = false;
    filter.birth_year_min = 0;
    filter.birth_year_max = 0;

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_NOT_NULL(results[0]);
    ASSERT_EQ(results[0]->id, 1U);

    family_tree_destroy(tree);
}

DECLARE_TEST(test_search_filters_alive_status)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring = NULL;
    filter.include_alive = true;
    filter.include_deceased = false;
    filter.use_birth_year_range = false;
    filter.birth_year_min = 0;
    filter.birth_year_max = 0;

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 2U);
    ASSERT_NOT_NULL(results[0]);
    ASSERT_TRUE(results[0]->is_alive);
    ASSERT_NOT_NULL(results[1]);
    ASSERT_TRUE(results[1]->is_alive);

    filter.include_alive = false;
    filter.include_deceased = true;
    count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_NOT_NULL(results[0]);
    ASSERT_FALSE(results[0]->is_alive);

    family_tree_destroy(tree);
}

DECLARE_TEST(test_search_birth_year_range_limits_results)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring = NULL;
    filter.include_alive = true;
    filter.include_deceased = true;
    filter.use_birth_year_range = true;
    filter.birth_year_min = 1980;
    filter.birth_year_max = 1990;

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    filter.birth_year_min = 1950;
    filter.birth_year_max = 1970;
    count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 2U);

    family_tree_destroy(tree);
}

void register_search_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_search_name_substring_matches_case_insensitive);
    REGISTER_TEST(registry, test_search_filters_alive_status);
    REGISTER_TEST(registry, test_search_birth_year_range_limits_results);
}
