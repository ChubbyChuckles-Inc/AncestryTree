#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct FileDialogFilter
    {
        const char *label;
        const char *pattern;
    } FileDialogFilter;

    typedef struct FileDialogOptions
    {
        const char *title;
        const char *default_path;
        const FileDialogFilter *filters;
        size_t filter_count;
    } FileDialogOptions;

    bool file_dialog_open(const FileDialogOptions *options, char *out_path, size_t capacity, char *error_buffer,
                          size_t error_capacity);

    bool file_dialog_save(const FileDialogOptions *options, char *out_path, size_t capacity, char *error_buffer,
                          size_t error_capacity);

    bool file_dialog_ensure_extension(char *path, size_t capacity, const char *extension);

    /* Manual validation checklist:
     * 1. Trigger File -> Open. Confirm the native or zenity-backed dialog appears and selects JSON files.
     * 2. Save an edited tree via File -> Save As. Verify the dialog enforces the .json extension and writes the file.
     */

#ifdef __cplusplus
}
#endif

#endif /* FILE_DIALOG_H */
