#include "test_framework.h"

#include "assets.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void register_assets_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_asset_copy_creates_destination);
    REGISTER_TEST(registry, test_asset_copy_generates_unique_names);
    REGISTER_TEST(registry, test_asset_copy_missing_source_reports_error);
}
