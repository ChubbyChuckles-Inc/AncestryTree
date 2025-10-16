#include "assets.h"
#include "at_memory.h"
#include "at_string.h"
#include "timeline.h"
#include "tree.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define ASSET_PATH_MAX 512
#define ASSET_FILENAME_MAX 128

static void asset_set_error(char *error_buffer, size_t error_capacity, const char *message)
{
    if (error_buffer && error_capacity > 0U)
    {
        if (message && message[0] != '\0')
        {
            (void)snprintf(error_buffer, error_capacity, "%s", message);
        }
        else
        {
            error_buffer[0] = '\0';
        }
    }
}

static bool asset_file_exists(const char *path)
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

static bool asset_create_directory(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return false;
    }
#if defined(_WIN32)
    int result = _mkdir(path);
#else
    int result = mkdir(path, 0775);
#endif
    if (result == 0)
    {
        return true;
    }
    if (errno == EEXIST)
    {
        return true;
    }
    return false;
}

static bool asset_ensure_directories(const char *path)
{
    if (!path || path[0] == '\0')
    {
        return false;
    }
    char buffer[ASSET_PATH_MAX];
    size_t length = strlen(path);
    if (length >= sizeof(buffer))
    {
        return false;
    }
    memcpy(buffer, path, length + 1U);
    for (size_t index = 1U; index < length; ++index)
    {
        char ch = buffer[index];
        if (ch == '/' || ch == '\\')
        {
            buffer[index] = '\0';
            if (!asset_create_directory(buffer))
            {
                if (errno != EEXIST)
                {
                    return false;
                }
            }
            buffer[index] = '/';
        }
    }
    return asset_create_directory(buffer);
}

static void asset_normalise_path(char *path)
{
    if (!path)
    {
        return;
    }
    for (size_t index = 0U; path[index] != '\0'; ++index)
    {
        if (path[index] == '\\')
        {
            path[index] = '/';
        }
    }
}

static void asset_trim_leading_slashes(char *path)
{
    if (!path)
    {
        return;
    }
    size_t read_index = 0U;
    while (path[read_index] == '/' || path[read_index] == '\\')
    {
        ++read_index;
    }
    if (read_index == 0U)
    {
        return;
    }
    size_t write_index = 0U;
    while (path[read_index] != '\0')
    {
        path[write_index++] = path[read_index++];
    }
    path[write_index] = '\0';
}

static void asset_set_error_once(char *error_buffer, size_t error_capacity, const char *message)
{
    if (!error_buffer || error_capacity == 0U)
    {
        return;
    }
    if (error_buffer[0] != '\0')
    {
        return;
    }
    asset_set_error(error_buffer, error_capacity, message);
}

typedef struct AssetPathList
{
    char **items;
    size_t count;
    size_t capacity;
} AssetPathList;

static void asset_path_list_init(AssetPathList *list)
{
    if (!list)
    {
        return;
    }
    list->items = NULL;
    list->count = 0U;
    list->capacity = 0U;
}

static void asset_path_list_dispose(AssetPathList *list)
{
    if (!list)
    {
        return;
    }
    for (size_t index = 0U; index < list->count; ++index)
    {
        AT_FREE(list->items[index]);
    }
    AT_FREE(list->items);
    list->items = NULL;
    list->count = 0U;
    list->capacity = 0U;
}

static bool asset_path_list_contains(const AssetPathList *list, const char *path)
{
    if (!list || !path)
    {
        return false;
    }
    for (size_t index = 0U; index < list->count; ++index)
    {
        if (strcmp(list->items[index], path) == 0)
        {
            return true;
        }
    }
    return false;
}

static bool asset_path_list_add_unique(AssetPathList *list, const char *path)
{
    if (!list || !path)
    {
        return false;
    }
    if (asset_path_list_contains(list, path))
    {
        return true;
    }
    size_t required = list->count + 1U;
    if (required > list->capacity)
    {
        size_t new_capacity = (list->capacity == 0U) ? 16U : list->capacity * 2U;
        while (new_capacity < required)
        {
            new_capacity *= 2U;
        }
        char **items = (char **)at_secure_realloc(list->items, new_capacity, sizeof(char *));
        if (!items)
        {
            return false;
        }
        list->items = items;
        list->capacity = new_capacity;
    }
    char *copy = at_string_dup(path);
    if (!copy)
    {
        return false;
    }
    list->items[list->count++] = copy;
    return true;
}

#if defined(_WIN32)
static void asset_convert_to_windows_separators(char *path)
{
    if (!path)
    {
        return;
    }
    for (size_t index = 0U; path[index] != '\0'; ++index)
    {
        if (path[index] == '/')
        {
            path[index] = '\\';
        }
    }
}

static bool asset_remove_file_native(const char *path)
{
    if (!path)
    {
        return false;
    }
    char native[ASSET_PATH_MAX];
    if (snprintf(native, sizeof(native), "%s", path) < 0)
    {
        return false;
    }
    asset_convert_to_windows_separators(native);
    BOOL result = DeleteFileA(native);
    if (!result)
    {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND;
    }
    return true;
}

static bool asset_remove_directory_native(const char *path)
{
    if (!path)
    {
        return false;
    }
    char native[ASSET_PATH_MAX];
    if (snprintf(native, sizeof(native), "%s", path) < 0)
    {
        return false;
    }
    asset_convert_to_windows_separators(native);
    BOOL result = RemoveDirectoryA(native);
    if (!result)
    {
        DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    return true;
}
#else
static bool asset_remove_file_native(const char *path)
{
    if (!path)
    {
        return false;
    }
    return remove(path) == 0 || errno == ENOENT;
}

static bool asset_remove_directory_native(const char *path)
{
    if (!path)
    {
        return false;
    }
    return rmdir(path) == 0;
}
#endif

static const char *asset_find_extension(const char *path)
{
    if (!path)
    {
        return "";
    }
    const char *last_dot = strrchr(path, '.');
    if (!last_dot || last_dot[1] == '\0')
    {
        return "";
    }
    return last_dot;
}

static bool asset_prepare_relative_path(const char *path, char *normalized, size_t capacity)
{
    if (!path || !normalized || capacity == 0U)
    {
        return false;
    }
    size_t length = strlen(path);
    if (length == 0U || length + 1U > capacity)
    {
        return false;
    }
    memcpy(normalized, path, length + 1U);
    asset_normalise_path(normalized);
    asset_trim_leading_slashes(normalized);
    if (normalized[0] == '\0')
    {
        return false;
    }
    if (strstr(normalized, ".."))
    {
        return false;
    }
    if (strchr(normalized, ':'))
    {
        return false;
    }
    return true;
}

static bool asset_get_file_size(const char *path, size_t *out_size)
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
    if (fseek(file, 0L, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    long end_position = ftell(file);
    if (end_position < 0L)
    {
        fclose(file);
        return false;
    }
    if (out_size)
    {
        *out_size = (size_t)end_position;
    }
    fclose(file);
    return true;
}

static bool asset_join_path(char *buffer, size_t capacity, const char *base, const char *suffix)
{
    if (!buffer || capacity == 0U || !base || !suffix)
    {
        return false;
    }
    size_t base_length = strlen(base);
    bool base_has_separator = false;
    if (base_length > 0U)
    {
        char last = base[base_length - 1U];
        base_has_separator = (last == '/' || last == '\\');
    }
    int written;
    if (base_has_separator)
    {
        written = snprintf(buffer, capacity, "%s%s", base, suffix);
    }
    else
    {
        written = snprintf(buffer, capacity, "%s/%s", base, suffix);
    }
    if (written < 0 || (size_t)written >= capacity)
    {
        return false;
    }
    return true;
}

static bool asset_build_relative_path(char *buffer, size_t capacity, const char *prefix, const char *name)
{
    if (!buffer || capacity == 0U || !name)
    {
        return false;
    }
    int written;
    if (prefix && prefix[0] != '\0')
    {
        written = snprintf(buffer, capacity, "%s/%s", prefix, name);
    }
    else
    {
        written = snprintf(buffer, capacity, "%s", name);
    }
    if (written < 0 || (size_t)written >= capacity)
    {
        return false;
    }
    asset_normalise_path(buffer);
    asset_trim_leading_slashes(buffer);
    if (buffer[0] == '\0')
    {
        return false;
    }
    return true;
}

static bool asset_copy_stream(FILE *source, FILE *destination)
{
    if (!source || !destination)
    {
        return false;
    }
    unsigned char buffer[64U * 1024U];
    size_t read_bytes = fread(buffer, 1U, sizeof(buffer), source);
    while (read_bytes > 0U)
    {
        size_t written = fwrite(buffer, 1U, read_bytes, destination);
        if (written != read_bytes)
        {
            return false;
        }
        read_bytes = fread(buffer, 1U, sizeof(buffer), source);
    }
    return ferror(source) == 0;
}

static bool asset_generate_unique_name(const char *prefix, const char *extension, const char *directory,
                                       char *out_name, size_t name_capacity)
{
    if (!prefix || prefix[0] == '\0')
    {
        prefix = "asset";
    }
    if (!extension)
    {
        extension = "";
    }
    time_t now = time(NULL);
    if (now == (time_t)-1)
    {
        now = 0;
    }
    for (unsigned int attempt = 0U; attempt < 2000U; ++attempt)
    {
        unsigned long long stamp = (unsigned long long)now * 1000ULL + (unsigned long long)(attempt);
        int written;
        if (extension[0] != '\0')
        {
            written = snprintf(out_name, name_capacity, "%s_%llu%s", prefix, stamp, extension);
        }
        else
        {
            written = snprintf(out_name, name_capacity, "%s_%llu", prefix, stamp);
        }
        if (written < 0 || (size_t)written >= name_capacity)
        {
            return false;
        }
        char absolute[ASSET_PATH_MAX];
        if (snprintf(absolute, sizeof(absolute), "%s/%s", directory, out_name) >= (int)sizeof(absolute))
        {
            return false;
        }
        asset_normalise_path(absolute);
        if (!asset_file_exists(absolute))
        {
            return true;
        }
        now += 1;
    }
    return false;
}

bool asset_copy(const AssetCopyRequest *request, char *out_relative_path, size_t relative_capacity,
                char *error_buffer, size_t error_capacity)
{
    if (!request || !request->source_path || request->source_path[0] == '\0')
    {
        asset_set_error(error_buffer, error_capacity, "Invalid asset copy request");
        return false;
    }
    const char *asset_root = (request->asset_root && request->asset_root[0] != '\0') ? request->asset_root : "assets";
    const char *subdirectory = (request->subdirectory && request->subdirectory[0] != '\0') ? request->subdirectory : "imports";

    char root_buffer[ASSET_PATH_MAX];
    if (snprintf(root_buffer, sizeof(root_buffer), "%s", asset_root) >= (int)sizeof(root_buffer))
    {
        asset_set_error(error_buffer, error_capacity, "Asset root path too long");
        return false;
    }
    asset_normalise_path(root_buffer);

    char subdir_buffer[ASSET_PATH_MAX];
    if (snprintf(subdir_buffer, sizeof(subdir_buffer), "%s", subdirectory) >= (int)sizeof(subdir_buffer))
    {
        asset_set_error(error_buffer, error_capacity, "Asset subdirectory too long");
        return false;
    }
    asset_normalise_path(subdir_buffer);
    asset_trim_leading_slashes(subdir_buffer);
    if (strstr(subdir_buffer, ".."))
    {
        asset_set_error(error_buffer, error_capacity, "Relative asset path must not traverse upward");
        return false;
    }

    char destination_directory[ASSET_PATH_MAX];
    if (subdir_buffer[0] != '\0')
    {
        if (snprintf(destination_directory, sizeof(destination_directory), "%s/%s", root_buffer, subdir_buffer) >=
            (int)sizeof(destination_directory))
        {
            asset_set_error(error_buffer, error_capacity, "Destination path too long");
            return false;
        }
    }
    else
    {
        if (snprintf(destination_directory, sizeof(destination_directory), "%s", root_buffer) >=
            (int)sizeof(destination_directory))
        {
            asset_set_error(error_buffer, error_capacity, "Destination path too long");
            return false;
        }
    }
    asset_normalise_path(destination_directory);

    if (!asset_ensure_directories(destination_directory))
    {
        asset_set_error(error_buffer, error_capacity, "Failed to prepare destination directory");
        return false;
    }

    FILE *source = NULL;
#if defined(_WIN32)
    if (fopen_s(&source, request->source_path, "rb") != 0)
    {
        source = NULL;
    }
#else
    source = fopen(request->source_path, "rb");
#endif
    if (!source)
    {
        asset_set_error(error_buffer, error_capacity, "Source asset unavailable");
        return false;
    }

    const char *extension = asset_find_extension(request->source_path);
    char filename[ASSET_FILENAME_MAX];
    if (!asset_generate_unique_name(request->name_prefix ? request->name_prefix : "asset", extension,
                                    destination_directory, filename, sizeof(filename)))
    {
        fclose(source);
        asset_set_error(error_buffer, error_capacity, "Unable to generate unique filename");
        return false;
    }

    char destination_path[ASSET_PATH_MAX];
    if (snprintf(destination_path, sizeof(destination_path), "%s/%s", destination_directory, filename) >=
        (int)sizeof(destination_path))
    {
        fclose(source);
        asset_set_error(error_buffer, error_capacity, "Destination file path too long");
        return false;
    }
    asset_normalise_path(destination_path);

    FILE *destination = NULL;
#if defined(_WIN32)
    if (fopen_s(&destination, destination_path, "wb") != 0)
    {
        destination = NULL;
    }
#else
    destination = fopen(destination_path, "wb");
#endif
    if (!destination)
    {
        fclose(source);
        asset_set_error(error_buffer, error_capacity, "Unable to open destination asset file");
        return false;
    }

    bool copy_result = asset_copy_stream(source, destination);
    fclose(source);
    if (fflush(destination) != 0)
    {
        copy_result = false;
    }
    if (fclose(destination) != 0)
    {
        copy_result = false;
    }
    if (!copy_result)
    {
        (void)remove(destination_path);
        asset_set_error(error_buffer, error_capacity, "Asset copy failed");
        return false;
    }

    if (out_relative_path && relative_capacity > 0U)
    {
        if (subdir_buffer[0] != '\0')
        {
            if (snprintf(out_relative_path, relative_capacity, "%s/%s", subdir_buffer, filename) >=
                (int)relative_capacity)
            {
                asset_set_error(error_buffer, error_capacity, "Relative path buffer too small");
                return false;
            }
        }
        else
        {
            if (snprintf(out_relative_path, relative_capacity, "%s", filename) >= (int)relative_capacity)
            {
                asset_set_error(error_buffer, error_capacity, "Relative path buffer too small");
                return false;
            }
        }
        asset_normalise_path(out_relative_path);
    }

    asset_set_error(error_buffer, error_capacity, "");
    return true;
}

static bool asset_collect_reference(AssetPathList *list, AssetCleanupStats *stats, const char *path,
                                    char *error_buffer, size_t error_capacity)
{
    if (!path || path[0] == '\0')
    {
        return true;
    }
    char normalized[ASSET_PATH_MAX];
    if (!asset_prepare_relative_path(path, normalized, sizeof(normalized)))
    {
        if (stats)
        {
            stats->integrity_failures += 1U;
        }
        char message[ASSET_PATH_MAX + 64U];
        (void)snprintf(message, sizeof(message), "Asset path invalid: %s", path);
        asset_set_error_once(error_buffer, error_capacity, message);
        return true;
    }
    if (!asset_path_list_add_unique(list, normalized))
    {
        asset_set_error(error_buffer, error_capacity, "Out of memory while tracking asset references");
        return false;
    }
    return true;
}

static bool asset_collect_person_assets(const Person *person, AssetPathList *list, AssetCleanupStats *stats,
                                        char *error_buffer, size_t error_capacity)
{
    if (!person)
    {
        return true;
    }
    if (!asset_collect_reference(list, stats, person->profile_image_path, error_buffer, error_capacity))
    {
        return false;
    }
    if (person->certificate_count > 0U && !person->certificate_paths)
    {
        if (stats)
        {
            stats->integrity_failures += 1U;
        }
        asset_set_error_once(error_buffer, error_capacity, "Person certificate list is missing");
    }
    if (person->certificate_paths)
    {
        for (size_t index = 0U; index < person->certificate_count; ++index)
        {
            if (!asset_collect_reference(list, stats, person->certificate_paths[index], error_buffer, error_capacity))
            {
                return false;
            }
        }
    }
    if (person->timeline_count > 0U && !person->timeline_entries)
    {
        if (stats)
        {
            stats->integrity_failures += 1U;
        }
        asset_set_error_once(error_buffer, error_capacity, "Person timeline entries are missing");
    }
    if (person->timeline_entries)
    {
        for (size_t entry_index = 0U; entry_index < person->timeline_count; ++entry_index)
        {
            const TimelineEntry *entry = &person->timeline_entries[entry_index];
            if (!entry)
            {
                continue;
            }
            if (entry->media_count > 0U && !entry->media_paths)
            {
                if (stats)
                {
                    stats->integrity_failures += 1U;
                }
                asset_set_error_once(error_buffer, error_capacity, "Timeline entry media list is missing");
                continue;
            }
            if (!entry->media_paths)
            {
                continue;
            }
            for (size_t media = 0U; media < entry->media_count; ++media)
            {
                if (!asset_collect_reference(list, stats, entry->media_paths[media], error_buffer, error_capacity))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool asset_collect_tree_assets(const FamilyTree *tree, AssetPathList *list, AssetCleanupStats *stats,
                                      char *error_buffer, size_t error_capacity)
{
    if (!tree || !list)
    {
        return true;
    }
    if (!tree->persons && tree->person_count > 0U)
    {
        if (stats)
        {
            stats->integrity_failures += 1U;
        }
        asset_set_error_once(error_buffer, error_capacity, "Family tree is missing person records");
        return false;
    }
    bool success = true;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!person)
        {
            if (stats)
            {
                stats->integrity_failures += 1U;
            }
            asset_set_error_once(error_buffer, error_capacity, "Encountered null person while collecting assets");
            success = false;
            continue;
        }
        if (!asset_collect_person_assets(person, list, stats, error_buffer, error_capacity))
        {
            success = false;
        }
    }
    return success;
}

static bool asset_verify_references(const char *asset_root, const AssetPathList *references,
                                    AssetCleanupStats *stats, char *error_buffer, size_t error_capacity)
{
    if (!asset_root || !references || !stats)
    {
        asset_set_error_once(error_buffer, error_capacity, "Invalid parameters for asset verification");
        return false;
    }
    bool success = true;
    for (size_t index = 0U; index < references->count; ++index)
    {
        const char *relative = references->items[index];
        char absolute[ASSET_PATH_MAX];
        if (!asset_join_path(absolute, sizeof(absolute), asset_root, relative))
        {
            stats->integrity_failures += 1U;
            char message[ASSET_PATH_MAX + 64U];
            (void)snprintf(message, sizeof(message), "Asset path too long: %s", relative);
            asset_set_error_once(error_buffer, error_capacity, message);
            success = false;
            continue;
        }
        asset_normalise_path(absolute);
        size_t file_size = 0U;
        if (!asset_get_file_size(absolute, &file_size))
        {
            stats->missing_files += 1U;
            char message[ASSET_PATH_MAX + 64U];
            (void)snprintf(message, sizeof(message), "Missing asset: %s", relative);
            asset_set_error_once(error_buffer, error_capacity, message);
            success = false;
            continue;
        }
        if (file_size == 0U)
        {
            stats->integrity_failures += 1U;
            char message[ASSET_PATH_MAX + 64U];
            (void)snprintf(message, sizeof(message), "Asset is empty: %s", relative);
            asset_set_error_once(error_buffer, error_capacity, message);
            success = false;
        }
    }
    return success && stats->missing_files == 0U && stats->integrity_failures == 0U;
}

#if defined(_WIN32)
static bool asset_cleanup_directory_win(const char *absolute_dir, const char *relative_prefix,
                                        const AssetPathList *references, AssetCleanupStats *stats,
                                        char *error_buffer, size_t error_capacity, bool is_root)
{
    char search[ASSET_PATH_MAX];
    if (!asset_join_path(search, sizeof(search), absolute_dir, "*"))
    {
        asset_set_error_once(error_buffer, error_capacity, "Asset directory path too long");
        return false;
    }
    asset_convert_to_windows_separators(search);
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(search, &data);
    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        {
            return true;
        }
        char message[ASSET_PATH_MAX + 64U];
        (void)snprintf(message, sizeof(message), "Failed to enumerate assets: %s", absolute_dir);
        asset_set_error_once(error_buffer, error_capacity, message);
        return false;
    }
    bool success = true;
    do
    {
        const char *name = data.cFileName;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue;
        }
        char child_absolute[ASSET_PATH_MAX];
        if (!asset_join_path(child_absolute, sizeof(child_absolute), absolute_dir, name))
        {
            char message[ASSET_PATH_MAX + 64U];
            (void)snprintf(message, sizeof(message), "Path too long: %s/%s", absolute_dir, name);
            asset_set_error_once(error_buffer, error_capacity, message);
            success = false;
            continue;
        }
        bool is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_directory)
        {
            char child_relative[ASSET_PATH_MAX];
            if (!asset_build_relative_path(child_relative, sizeof(child_relative), relative_prefix, name))
            {
                asset_set_error_once(error_buffer, error_capacity, "Relative asset path too long");
                success = false;
                continue;
            }
            if (!asset_cleanup_directory_win(child_absolute, child_relative, references, stats, error_buffer,
                                             error_capacity, false))
            {
                success = false;
            }
            asset_remove_directory_native(child_absolute);
        }
        else
        {
            char relative_path[ASSET_PATH_MAX];
            if (!asset_build_relative_path(relative_path, sizeof(relative_path), relative_prefix, name))
            {
                asset_set_error_once(error_buffer, error_capacity, "Relative asset path too long");
                success = false;
                continue;
            }
            if (!asset_path_list_contains(references, relative_path))
            {
                if (!asset_remove_file_native(child_absolute))
                {
                    char message[ASSET_PATH_MAX + 64U];
                    (void)snprintf(message, sizeof(message), "Failed to remove asset: %s", child_absolute);
                    asset_set_error_once(error_buffer, error_capacity, message);
                    success = false;
                }
                else
                {
                    stats->removed_files += 1U;
                }
            }
        }
    } while (FindNextFileA(handle, &data));

    DWORD final_error = GetLastError();
    FindClose(handle);
    if (final_error != ERROR_NO_MORE_FILES)
    {
        char message[ASSET_PATH_MAX + 64U];
        (void)snprintf(message, sizeof(message), "Error walking assets: %s", absolute_dir);
        asset_set_error_once(error_buffer, error_capacity, message);
        success = false;
    }

    if (!is_root)
    {
        asset_remove_directory_native(absolute_dir);
    }
    return success;
}
#else
static bool asset_cleanup_directory_posix(const char *absolute_dir, const char *relative_prefix,
                                          const AssetPathList *references, AssetCleanupStats *stats,
                                          char *error_buffer, size_t error_capacity, bool is_root)
{
    DIR *dir = opendir(absolute_dir);
    if (!dir)
    {
        if (errno == ENOENT)
        {
            return true;
        }
        char message[ASSET_PATH_MAX + 64U];
        (void)snprintf(message, sizeof(message), "Failed to open directory: %s", absolute_dir);
        asset_set_error_once(error_buffer, error_capacity, message);
        return false;
    }
    bool success = true;
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL)
    {
        const char *name = entry->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        {
            continue;
        }
        char child_absolute[ASSET_PATH_MAX];
        if (!asset_join_path(child_absolute, sizeof(child_absolute), absolute_dir, name))
        {
            char message[ASSET_PATH_MAX + 64U];
            (void)snprintf(message, sizeof(message), "Path too long: %s/%s", absolute_dir, name);
            asset_set_error_once(error_buffer, error_capacity, message);
            success = false;
            continue;
        }
        struct stat file_info;
        bool is_directory = false;
        if (stat(child_absolute, &file_info) == 0)
        {
            is_directory = S_ISDIR(file_info.st_mode);
        }
        if (is_directory)
        {
            char child_relative[ASSET_PATH_MAX];
            if (!asset_build_relative_path(child_relative, sizeof(child_relative), relative_prefix, name))
            {
                asset_set_error_once(error_buffer, error_capacity, "Relative asset path too long");
                success = false;
                continue;
            }
            if (!asset_cleanup_directory_posix(child_absolute, child_relative, references, stats, error_buffer,
                                               error_capacity, false))
            {
                success = false;
            }
            asset_remove_directory_native(child_absolute);
        }
        else
        {
            char relative_path[ASSET_PATH_MAX];
            if (!asset_build_relative_path(relative_path, sizeof(relative_path), relative_prefix, name))
            {
                asset_set_error_once(error_buffer, error_capacity, "Relative asset path too long");
                success = false;
                continue;
            }
            if (!asset_path_list_contains(references, relative_path))
            {
                if (!asset_remove_file_native(child_absolute))
                {
                    char message[ASSET_PATH_MAX + 64U];
                    (void)snprintf(message, sizeof(message), "Failed to remove asset: %s", child_absolute);
                    asset_set_error_once(error_buffer, error_capacity, message);
                    success = false;
                }
                else
                {
                    stats->removed_files += 1U;
                }
            }
        }
    }
    if (closedir(dir) != 0)
    {
        char message[ASSET_PATH_MAX + 64U];
        (void)snprintf(message, sizeof(message), "Failed to close directory: %s", absolute_dir);
        asset_set_error_once(error_buffer, error_capacity, message);
        success = false;
    }
    if (!is_root)
    {
        asset_remove_directory_native(absolute_dir);
    }
    return success;
}
#endif

static bool asset_remove_unreferenced(const char *asset_root, const char *normalized_subdir,
                                      const AssetPathList *references, AssetCleanupStats *stats,
                                      char *error_buffer, size_t error_capacity)
{
    if (!asset_root || !references || !stats)
    {
        asset_set_error_once(error_buffer, error_capacity, "Invalid parameters for asset cleanup");
        return false;
    }
    char base_path[ASSET_PATH_MAX];
    int written;
    if (normalized_subdir && normalized_subdir[0] != '\0')
    {
        if (!asset_join_path(base_path, sizeof(base_path), asset_root, normalized_subdir))
        {
            asset_set_error_once(error_buffer, error_capacity, "Asset cleanup path too long");
            return false;
        }
    }
    else
    {
        written = snprintf(base_path, sizeof(base_path), "%s", asset_root);
        if (written < 0 || (size_t)written >= sizeof(base_path))
        {
            asset_set_error_once(error_buffer, error_capacity, "Asset root path too long");
            return false;
        }
    }
    asset_normalise_path(base_path);
    const char *relative_prefix = (normalized_subdir && normalized_subdir[0] != '\0') ? normalized_subdir : "";

#if defined(_WIN32)
    return asset_cleanup_directory_win(base_path, relative_prefix, references, stats, error_buffer, error_capacity,
                                       true);
#else
    return asset_cleanup_directory_posix(base_path, relative_prefix, references, stats, error_buffer, error_capacity,
                                         true);
#endif
}

bool asset_cleanup(const FamilyTree *tree, const char *asset_root, const char *import_subdirectory,
                   AssetCleanupStats *stats, char *error_buffer, size_t error_capacity)
{
    AssetCleanupStats local_stats;
    local_stats.referenced_files = 0U;
    local_stats.removed_files = 0U;
    local_stats.missing_files = 0U;
    local_stats.integrity_failures = 0U;

    if (stats)
    {
        *stats = local_stats;
    }

    if (!tree)
    {
        asset_set_error(error_buffer, error_capacity, "Family tree is required for asset cleanup");
        return false;
    }

    if (!asset_root || asset_root[0] == '\0')
    {
        asset_set_error(error_buffer, error_capacity, "Asset root path missing");
        return false;
    }

    asset_set_error(error_buffer, error_capacity, "");

    char normalized_subdir[ASSET_PATH_MAX];
    normalized_subdir[0] = '\0';
    const char *raw_subdir = (import_subdirectory && import_subdirectory[0] != '\0') ? import_subdirectory : "";
    if (raw_subdir[0] != '\0')
    {
        if (!asset_prepare_relative_path(raw_subdir, normalized_subdir, sizeof(normalized_subdir)))
        {
            local_stats.integrity_failures += 1U;
            asset_set_error_once(error_buffer, error_capacity, "Invalid asset cleanup subdirectory");
            if (stats)
            {
                *stats = local_stats;
            }
            return false;
        }
    }

    AssetPathList references;
    asset_path_list_init(&references);

    bool success = true;
    bool collection_ok = asset_collect_tree_assets(tree, &references, &local_stats, error_buffer, error_capacity);
    if (!collection_ok)
    {
        success = false;
    }

    local_stats.referenced_files = references.count;

    if (collection_ok)
    {
        bool verify_ok = asset_verify_references(asset_root, &references, &local_stats, error_buffer, error_capacity);
        if (!verify_ok)
        {
            success = false;
        }
        else if (!asset_remove_unreferenced(asset_root, normalized_subdir, &references, &local_stats, error_buffer,
                                            error_capacity))
        {
            success = false;
        }
    }

    if (stats)
    {
        *stats = local_stats;
    }

    asset_path_list_dispose(&references);

    if (local_stats.missing_files > 0U || local_stats.integrity_failures > 0U)
    {
        success = false;
    }

    return success;
}
