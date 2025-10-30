#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    struct FamilyTree;

    typedef struct AssetCopyRequest
    {
        const char
            *source_path;       /* Absolute or working-directory relative path to the input file. */
        const char *asset_root; /* Destination root directory (e.g., "assets"). */
        const char
            *subdirectory; /* Optional subdirectory beneath asset_root; defaults to "imports". */
        const char
            *name_prefix; /* Optional filename prefix used before the timestamp/identifier. */
    } AssetCopyRequest;

    typedef struct AssetCleanupStats
    {
        size_t referenced_files;   /* Unique referenced asset paths discovered in the tree. */
        size_t removed_files;      /* Files deleted because they were unreferenced. */
        size_t missing_files;      /* Referenced files that were absent on disk. */
        size_t integrity_failures; /* Referenced files that failed integrity checks (e.g., zero
                                      length). */
    } AssetCleanupStats;

    typedef struct AssetExportStats
    {
        size_t referenced_files; /* Referenced asset count included in the package (excludes the
                                    tree JSON). */
        size_t exported_files;   /* Total files written to the package, including the tree JSON. */
        size_t exported_bytes;   /* Aggregate bytes written across all packaged files. */
    } AssetExportStats;

    typedef struct AssetImportStats
    {
        size_t
            extracted_files; /* Total files extracted from the package, including the tree JSON. */
        size_t extracted_bytes; /* Aggregate bytes written to disk while unpacking. */
    } AssetImportStats;

    bool asset_copy(const AssetCopyRequest *request, char *out_relative_path,
                    size_t relative_capacity, char *error_buffer, size_t error_capacity);

    bool asset_cleanup(const struct FamilyTree *tree, const char *asset_root,
                       const char *import_subdirectory, AssetCleanupStats *stats,
                       char *error_buffer, size_t error_capacity);

    bool asset_export(const struct FamilyTree *tree, const char *asset_root,
                      const char *tree_json_path, const char *package_path, AssetExportStats *stats,
                      char *error_buffer, size_t error_capacity);

    bool asset_import_package(const char *package_path, const char *asset_root,
                              char *out_tree_json_path, size_t tree_json_capacity,
                              char *out_asset_prefix, size_t asset_prefix_capacity,
                              AssetImportStats *stats, char *error_buffer, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif /* ASSETS_H */
