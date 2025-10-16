#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct AssetCopyRequest
    {
        const char *source_path;  /* Absolute or working-directory relative path to the input file. */
        const char *asset_root;   /* Destination root directory (e.g., "assets"). */
        const char *subdirectory; /* Optional subdirectory beneath asset_root; defaults to "imports". */
        const char *name_prefix;  /* Optional filename prefix used before the timestamp/identifier. */
    } AssetCopyRequest;

    bool asset_copy(const AssetCopyRequest *request, char *out_relative_path, size_t relative_capacity,
                    char *error_buffer, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif /* ASSETS_H */
