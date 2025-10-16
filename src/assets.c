#include "assets.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
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
