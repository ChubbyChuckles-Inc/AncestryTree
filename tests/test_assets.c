#include "test_framework.h"

#include "assets.h"
#include "person.h"
#include "timeline.h"
#include "tree.h"
#include "at_string.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static bool testfs_create_directory(const char *path)
{
    if (!path)
    {
        return false;
    }
#if defined(_WIN32)
    if (_mkdir(path) == 0)
    {
        return true;
    }
    if (errno == EEXIST)
    {
        return true;
    }
#else
    if (mkdir(path, 0775) == 0)
    {
        return true;
    }
    if (errno == EEXIST)
    {
        return true;
    }
#endif
    return false;
}

static bool testfs_remove_file(const char *path)
{
    if (!path)
    {
        return false;
    }
    return remove(path) == 0 || errno == ENOENT;
}

static bool testfs_write_sample(const char *path, const unsigned char *data, size_t size)
{
    if (!path || !data || size == 0U)
    {
        return false;
    }
    FILE *file = NULL;
#if defined(_WIN32)
    if (fopen_s(&file, path, "wb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (!file)
    {
        return false;
    }
    size_t written = fwrite(data, 1U, size, file);
    fclose(file);
    return written == size;
}

static bool testfs_file_exists(const char *path)
{
    if (!path)
    {
        return false;
    }
    FILE *file = NULL;
#if defined(_WIN32)
    if (fopen_s(&file, path, "rb") != 0)
    {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (!file)
    {
        return false;
    }
    fclose(file);
    return true;
}

static FamilyTree *test_create_tree_with_person(uint32_t id)
{
    FamilyTree *tree = family_tree_create("AssetCleanupTest");
    if (!tree)
    {
        return NULL;
    }
    Person *person = person_create(id);
    if (!person)
    {
        family_tree_destroy(tree);
        return NULL;
    }
    if (!person_set_name(person, "Test", NULL, "Person") ||
        !person_set_birth(person, "1990-01-01", "Testville") || !family_tree_add_person(tree, person))
    {
        person_destroy(person);
        family_tree_destroy(tree);
        return NULL;
    }
    return tree;
}

static bool test_assign_profile(Person *person, const char *relative_path)
{
    if (!person)
    {
        return false;
    }
    char *copy = at_string_dup(relative_path);
    if (!copy)
    {
        return false;
    }
    person->profile_image_path = copy;
    return true;
}

static void test_asset_copy_creates_destination(void)
{
    const char *root_dir = "Testing/Temporary/asset_copy_root";
    const char *imports_dir = "Testing/Temporary/asset_copy_root/imports";
    const char *source_path = "Testing/Temporary/asset_copy_source.bin";
    unsigned char payload[] = {0x42, 0x19, 0x7E, 0xAA, 0x00, 0xFF};

    ASSERT_TRUE(testfs_create_directory("Testing"));
    ASSERT_TRUE(testfs_create_directory("Testing/Temporary"));
    ASSERT_TRUE(testfs_create_directory(root_dir));
    ASSERT_TRUE(testfs_create_directory(imports_dir));
    ASSERT_TRUE(testfs_remove_file(source_path));

    ASSERT_TRUE(testfs_write_sample(source_path, payload, sizeof(payload)));

    AssetCopyRequest request;
    request.source_path = source_path;
    request.asset_root = root_dir;
    request.subdirectory = "imports";
    request.name_prefix = "profile";

    char relative[256];
    char error[128];
    ASSERT_TRUE(asset_copy(&request, relative, sizeof(relative), error, sizeof(error)));
    ASSERT_STREQ(error, "");
    ASSERT_TRUE(strlen(relative) > 0U);
    ASSERT_TRUE(strchr(relative, '\\') == NULL);

    char destination[512];
    int written = snprintf(destination, sizeof(destination), "%s/%s", root_dir, relative);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(destination));

    FILE *copied = NULL;
#if defined(_WIN32)
    if (fopen_s(&copied, destination, "rb") != 0)
    {
        copied = NULL;
    }
#else
    copied = fopen(destination, "rb");
#endif
    ASSERT_NOT_NULL(copied);
    unsigned char buffer[16];
    size_t total = fread(buffer, 1U, sizeof(buffer), copied);
    fclose(copied);
    ASSERT_EQ(total, sizeof(payload));
    ASSERT_TRUE(memcmp(buffer, payload, sizeof(payload)) == 0);
}

static void test_asset_copy_generates_unique_names(void)
{
    const char *root_dir = "Testing/Temporary/asset_copy_unique";
    const char *source_path = "Testing/Temporary/asset_copy_unique.bin";
    unsigned char payload[] = {0x11, 0x22, 0x33, 0x44};

    ASSERT_TRUE(testfs_create_directory("Testing"));
    ASSERT_TRUE(testfs_create_directory("Testing/Temporary"));
    ASSERT_TRUE(testfs_create_directory(root_dir));

    ASSERT_TRUE(testfs_write_sample(source_path, payload, sizeof(payload)));

    AssetCopyRequest request;
    request.source_path = source_path;
    request.asset_root = root_dir;
    request.subdirectory = NULL;
    request.name_prefix = "asset";

    char relative_a[128];
    char relative_b[128];
    char error[64];

    ASSERT_TRUE(asset_copy(&request, relative_a, sizeof(relative_a), error, sizeof(error)));
    ASSERT_TRUE(asset_copy(&request, relative_b, sizeof(relative_b), error, sizeof(error)));
    ASSERT_TRUE(strcmp(relative_a, relative_b) != 0);
}

static void test_asset_copy_missing_source_reports_error(void)
{
    AssetCopyRequest request;
    request.source_path = "Testing/Temporary/does_not_exist.bin";
    request.asset_root = "Testing/Temporary/asset_error";
    request.subdirectory = "imports";
    request.name_prefix = NULL;

    char error[128];
    char relative[32];
    bool result = asset_copy(&request, relative, sizeof(relative), error, sizeof(error));
    ASSERT_FALSE(result);
    ASSERT_TRUE(strcmp(error, "") != 0);
}

static void test_asset_cleanup_removes_unreferenced_files(void)
{
    const char *root_dir = "Testing/Temporary/asset_cleanup_case1";
    const char *imports_dir = "Testing/Temporary/asset_cleanup_case1/imports";
    const char *profile_rel = "imports/profile.png";
    const char *certificate_rel = "imports/certificate.pdf";
    const char *media_rel = "imports/event.png";
    const char *orphan_rel = "imports/orphan.bin";

    ASSERT_TRUE(testfs_create_directory("Testing"));
    ASSERT_TRUE(testfs_create_directory("Testing/Temporary"));
    ASSERT_TRUE(testfs_create_directory(root_dir));
    ASSERT_TRUE(testfs_create_directory(imports_dir));

    char profile_abs[256];
    char certificate_abs[256];
    char media_abs[256];
    char orphan_abs[256];

    int written = snprintf(profile_abs, sizeof(profile_abs), "%s/%s", root_dir, profile_rel);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(profile_abs));
    written = snprintf(certificate_abs, sizeof(certificate_abs), "%s/%s", root_dir, certificate_rel);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(certificate_abs));
    written = snprintf(media_abs, sizeof(media_abs), "%s/%s", root_dir, media_rel);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(media_abs));
    written = snprintf(orphan_abs, sizeof(orphan_abs), "%s/%s", root_dir, orphan_rel);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(orphan_abs));

    (void)testfs_remove_file(profile_abs);
    (void)testfs_remove_file(certificate_abs);
    (void)testfs_remove_file(media_abs);
    (void)testfs_remove_file(orphan_abs);

    unsigned char payload[] = {0x01, 0x23, 0x45, 0x67};
    ASSERT_TRUE(testfs_write_sample(profile_abs, payload, sizeof(payload)));
    ASSERT_TRUE(testfs_write_sample(certificate_abs, payload, sizeof(payload)));
    ASSERT_TRUE(testfs_write_sample(media_abs, payload, sizeof(payload)));
    ASSERT_TRUE(testfs_write_sample(orphan_abs, payload, sizeof(payload)));

    FamilyTree *tree = test_create_tree_with_person(101U);
    ASSERT_NOT_NULL(tree);
    ASSERT_EQ(tree->person_count, 1U);
    Person *person = tree->persons[0];
    ASSERT_TRUE(test_assign_profile(person, profile_rel));
    ASSERT_TRUE(person_add_certificate(person, certificate_rel));

    TimelineEntry entry;
    timeline_entry_init(&entry, TIMELINE_EVENT_CUSTOM);
    ASSERT_TRUE(timeline_entry_add_media(&entry, media_rel));
    ASSERT_TRUE(person_add_timeline_entry(person, &entry));
    timeline_entry_reset(&entry);

    AssetCleanupStats stats;
    char error[128];
    ASSERT_TRUE(asset_cleanup(tree, root_dir, "imports", &stats, error, sizeof(error)));
    ASSERT_STREQ(error, "");
    ASSERT_EQ(stats.referenced_files, 3U);
    ASSERT_EQ(stats.removed_files, 1U);
    ASSERT_EQ(stats.missing_files, 0U);
    ASSERT_EQ(stats.integrity_failures, 0U);

    ASSERT_TRUE(testfs_file_exists(profile_abs));
    ASSERT_TRUE(testfs_file_exists(certificate_abs));
    ASSERT_TRUE(testfs_file_exists(media_abs));
    ASSERT_FALSE(testfs_file_exists(orphan_abs));

    family_tree_destroy(tree);
    (void)testfs_remove_file(profile_abs);
    (void)testfs_remove_file(certificate_abs);
    (void)testfs_remove_file(media_abs);
}

static void test_asset_cleanup_detects_missing_files(void)
{
    const char *root_dir = "Testing/Temporary/asset_cleanup_case2";
    const char *imports_dir = "Testing/Temporary/asset_cleanup_case2/imports";
    const char *profile_rel = "imports/missing.png";
    const char *orphan_rel = "imports/remains.bin";

    ASSERT_TRUE(testfs_create_directory("Testing"));
    ASSERT_TRUE(testfs_create_directory("Testing/Temporary"));
    ASSERT_TRUE(testfs_create_directory(root_dir));
    ASSERT_TRUE(testfs_create_directory(imports_dir));

    char orphan_abs[256];
    int written = snprintf(orphan_abs, sizeof(orphan_abs), "%s/%s", root_dir, orphan_rel);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(orphan_abs));
    (void)testfs_remove_file(orphan_abs);
    unsigned char payload[] = {0xAA, 0xBB, 0xCC};
    ASSERT_TRUE(testfs_write_sample(orphan_abs, payload, sizeof(payload)));

    FamilyTree *tree = test_create_tree_with_person(202U);
    ASSERT_NOT_NULL(tree);
    ASSERT_EQ(tree->person_count, 1U);
    Person *person = tree->persons[0];
    ASSERT_TRUE(test_assign_profile(person, profile_rel));

    AssetCleanupStats stats;
    char error[128];
    bool result = asset_cleanup(tree, root_dir, "imports", &stats, error, sizeof(error));
    ASSERT_FALSE(result);
    ASSERT_TRUE(strlen(error) > 0U);
    ASSERT_EQ(stats.referenced_files, 1U);
    ASSERT_EQ(stats.removed_files, 0U);
    ASSERT_EQ(stats.missing_files, 1U);

    ASSERT_TRUE(testfs_file_exists(orphan_abs));

    family_tree_destroy(tree);
    (void)testfs_remove_file(orphan_abs);
}

void register_assets_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_asset_copy_creates_destination);
    REGISTER_TEST(registry, test_asset_copy_generates_unique_names);
    REGISTER_TEST(registry, test_asset_copy_missing_source_reports_error);
    REGISTER_TEST(registry, test_asset_cleanup_removes_unreferenced_files);
    REGISTER_TEST(registry, test_asset_cleanup_detects_missing_files);
}
