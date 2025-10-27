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
    Person *avery   = make_person(1U, "Avery", "1988-02-14", true);
    Person *brenda  = make_person(2U, "Brenda", "1960-06-01", false);
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
    filter.name_substring       = "AVE";
    filter.include_alive        = true;
    filter.include_deceased     = true;
    filter.use_birth_year_range = false;
    filter.birth_year_min       = 0;
    filter.birth_year_max       = 0;
    filter.query_mode           = SEARCH_QUERY_MODE_SUBSTRING;
    filter.query_expression     = filter.name_substring;

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
    filter.name_substring       = NULL;
    filter.include_alive        = true;
    filter.include_deceased     = false;
    filter.use_birth_year_range = false;
    filter.birth_year_min       = 0;
    filter.birth_year_max       = 0;
    filter.query_mode           = SEARCH_QUERY_MODE_SUBSTRING;
    filter.query_expression     = NULL;

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 2U);
    ASSERT_NOT_NULL(results[0]);
    ASSERT_TRUE(results[0]->is_alive);
    ASSERT_NOT_NULL(results[1]);
    ASSERT_TRUE(results[1]->is_alive);

    filter.include_alive    = false;
    filter.include_deceased = true;
    filter.query_expression = NULL;
    count                   = search_execute(tree, &filter, results, 8U);
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
    filter.name_substring       = NULL;
    filter.include_alive        = true;
    filter.include_deceased     = true;
    filter.use_birth_year_range = true;
    filter.birth_year_min       = 1980;
    filter.birth_year_max       = 1990;
    filter.query_mode           = SEARCH_QUERY_MODE_SUBSTRING;
    filter.query_expression     = NULL;

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    filter.birth_year_min = 1950;
    filter.birth_year_max = 1970;
    count                 = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 2U);

    family_tree_destroy(tree);
}

DECLARE_TEST(test_search_boolean_combines_terms)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring       = NULL;
    filter.include_alive        = true;
    filter.include_deceased     = true;
    filter.use_birth_year_range = false;
    filter.birth_year_min       = 0;
    filter.birth_year_max       = 0;
    filter.query_mode           = SEARCH_QUERY_MODE_BOOLEAN;
    filter.query_expression     = "name:avery AND NOT deceased";

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    filter.query_expression = "alive AND birth:2005";
    count                   = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 3U);

    family_tree_destroy(tree);
}

DECLARE_TEST(test_search_boolean_metadata_clause)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    Person *avery = family_tree_find_person(tree, 1U);
    ASSERT_NOT_NULL(avery);
    ASSERT_TRUE(person_metadata_set(avery, "Hobby", "Stargazing"));

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring       = NULL;
    filter.include_alive        = true;
    filter.include_deceased     = true;
    filter.use_birth_year_range = false;
    filter.birth_year_min       = 0;
    filter.birth_year_max       = 0;
    filter.query_mode           = SEARCH_QUERY_MODE_BOOLEAN;
    filter.query_expression     = "metadata:stargazing";

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    family_tree_destroy(tree);
}

DECLARE_TEST(test_search_regex_matches_name_prefix)
{
    FamilyTree *tree = make_sample_tree();
    ASSERT_NOT_NULL(tree);

    const Person *results[8];
    SearchFilter filter;
    filter.name_substring       = NULL;
    filter.include_alive        = true;
    filter.include_deceased     = true;
    filter.use_birth_year_range = false;
    filter.birth_year_min       = 0;
    filter.birth_year_max       = 0;
    filter.query_mode           = SEARCH_QUERY_MODE_REGEX;
    filter.query_expression     = "^avery";

    size_t count = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    filter.query_expression = "^avery.*1988";
    count                   = search_execute(tree, &filter, results, 8U);
    ASSERT_EQ(count, 1U);
    ASSERT_EQ(results[0]->id, 1U);

    family_tree_destroy(tree);
}

void register_search_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_search_name_substring_matches_case_insensitive);
    REGISTER_TEST(registry, test_search_filters_alive_status);
    REGISTER_TEST(registry, test_search_birth_year_range_limits_results);
    REGISTER_TEST(registry, test_search_boolean_combines_terms);
    REGISTER_TEST(registry, test_search_boolean_metadata_clause);
    REGISTER_TEST(registry, test_search_regex_matches_name_prefix);
}
