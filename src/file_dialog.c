#include "file_dialog.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <commdlg.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#elif defined(__linux__)
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <stdlib.h>
#endif

static void file_dialog_clear_buffer(char *buffer, size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
    buffer[0] = '\0';
}

static void file_dialog_set_error(char *error_buffer, size_t error_capacity, const char *message)
{
    if (!error_buffer || error_capacity == 0U)
    {
        return;
    }
    if (message)
    {
#if defined(_MSC_VER)
        (void)strncpy_s(error_buffer, error_capacity, message, _TRUNCATE);
#else
        (void)snprintf(error_buffer, error_capacity, "%s", message);
#endif
    }
    else
    {
        file_dialog_clear_buffer(error_buffer, error_capacity);
    }
}

static size_t file_dialog_strnlen(const char *value, size_t max_length)
{
    if (!value)
    {
        return 0U;
    }
    size_t length = 0U;
    while (length < max_length && value[length] != '\0')
    {
        ++length;
    }
    return length;
}

bool file_dialog_ensure_extension(char *path, size_t capacity, const char *extension)
{
    if (!path || capacity == 0U)
    {
        return false;
    }
    if (!extension || extension[0] == '\0')
    {
        return false;
    }
    size_t path_length      = file_dialog_strnlen(path, capacity);
    size_t extension_length = strlen(extension);
    if (path_length < extension_length)
    {
        if (path_length + extension_length + 1U > capacity)
        {
            return false;
        }
#if defined(_MSC_VER)
        (void)strncpy_s(path + path_length, capacity - path_length, extension, _TRUNCATE);
#else
        (void)snprintf(path + path_length, capacity - path_length, "%s", extension);
#endif
        return true;
    }
    size_t offset = path_length - extension_length;
    bool matches  = true;
    for (size_t index = 0U; index < extension_length; ++index)
    {
        char a = (char)tolower((unsigned char)path[offset + index]);
        char b = (char)tolower((unsigned char)extension[index]);
        if (a != b)
        {
            matches = false;
            break;
        }
    }
    if (matches)
    {
        return true;
    }
    if (path_length + extension_length + 1U > capacity)
    {
        return false;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(path + path_length, capacity - path_length, extension, _TRUNCATE);
#else
    (void)snprintf(path + path_length, capacity - path_length, "%s", extension);
#endif
    return true;
}

#if defined(_WIN32)

static UINT_PTR CALLBACK file_dialog_hook_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    (void)wParam;
    (void)lParam;
    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetForegroundWindow(hwnd);
        break;
    case WM_DESTROY:
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        break;
    default:
        break;
    }
    return 0U;
}

static void file_dialog_split_path(const char *source, char *directory, size_t dir_capacity,
                                   char *filename, size_t file_capacity)
{
    if (directory && dir_capacity > 0U)
    {
        directory[0] = '\0';
    }
    if (filename && file_capacity > 0U)
    {
        filename[0] = '\0';
    }
    if (!source || source[0] == '\0')
    {
        return;
    }
    const char *separator = strrchr(source, '\\');
    if (!separator)
    {
        separator = strrchr(source, '/');
    }
    if (separator)
    {
        size_t dir_len = (size_t)(separator - source);
        if (directory && dir_capacity > 0U)
        {
            size_t copy_len = (dir_len < dir_capacity - 1U) ? dir_len : dir_capacity - 1U;
            memcpy(directory, source, copy_len);
            directory[copy_len] = '\0';
        }
        if (filename && file_capacity > 0U)
        {
            (void)snprintf(filename, file_capacity, "%s", separator + 1);
        }
    }
    else if (filename && file_capacity > 0U)
    {
        (void)snprintf(filename, file_capacity, "%s", source);
    }
}

static bool file_dialog_build_filter_string(const FileDialogOptions *options, char *buffer,
                                            size_t capacity)
{
    if (!buffer || capacity == 0U)
    {
        return false;
    }
    buffer[0]     = '\0';
    size_t cursor = 0U;
    if (!options || !options->filters || options->filter_count == 0U)
    {
        const char fallback[] = "All Files\0*.*\0";
        size_t fallback_len   = sizeof(fallback);
        if (fallback_len > capacity)
        {
            return false;
        }
        memcpy(buffer, fallback, fallback_len);
        return true;
    }
    for (size_t index = 0U; index < options->filter_count; ++index)
    {
        const FileDialogFilter *filter = &options->filters[index];
        if (!filter->label || !filter->pattern)
        {
            continue;
        }
        size_t label_len   = strlen(filter->label);
        size_t pattern_len = strlen(filter->pattern);
        if (cursor + label_len + 1U >= capacity)
        {
            return false;
        }
        memcpy(buffer + cursor, filter->label, label_len);
        cursor += label_len;
        buffer[cursor++] = '\0';
        if (cursor + pattern_len + 1U >= capacity)
        {
            return false;
        }
        memcpy(buffer + cursor, filter->pattern, pattern_len);
        cursor += pattern_len;
        buffer[cursor++] = '\0';
    }
    if (cursor + 1U >= capacity)
    {
        return false;
    }
    buffer[cursor++] = '\0';
    return true;
}

static bool file_dialog_win32_run(const FileDialogOptions *options, char *out_path, size_t capacity,
                                  char *error_buffer, size_t error_capacity, bool save_dialog)
{
    if (!out_path || capacity == 0U)
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "Invalid output buffer for file dialog.");
        return false;
    }
    file_dialog_clear_buffer(out_path, capacity);
    file_dialog_clear_buffer(error_buffer, error_capacity);

    char directory[MAX_PATH] = {0};
    char filename[MAX_PATH]  = {0};
    if (options && options->default_path)
    {
        file_dialog_split_path(options->default_path, directory, sizeof(directory), filename,
                               sizeof(filename));
    }
    else if (save_dialog)
    {
        (void)snprintf(filename, sizeof(filename), "ancestrytree.json");
    }

    char file_buffer[MAX_PATH];
    file_buffer[0] = '\0';
    if (filename[0] != '\0')
    {
        (void)snprintf(file_buffer, sizeof(file_buffer), "%s", filename);
    }

    char filter_buffer[256];
    if (!file_dialog_build_filter_string(options, filter_buffer, sizeof(filter_buffer)))
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "Unable to build filter list for file dialog.");
        return false;
    }

    HWND owner = GetActiveWindow();
    if (!owner)
    {
        owner = GetForegroundWindow();
    }

    OPENFILENAMEA dialog;
    ZeroMemory(&dialog, sizeof(dialog));
    dialog.lStructSize  = sizeof(dialog);
    dialog.lpstrFile    = file_buffer;
    dialog.nMaxFile     = (DWORD)sizeof(file_buffer);
    dialog.lpstrFilter  = filter_buffer;
    dialog.nFilterIndex = 1U;
    dialog.Flags        = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    if (save_dialog)
    {
        dialog.Flags |= OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    }
    else
    {
        dialog.Flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    }
    dialog.hwndOwner = owner;
    dialog.Flags |= OFN_ENABLEHOOK;
    dialog.lpfnHook = file_dialog_hook_proc;
    if (directory[0] != '\0')
    {
        dialog.lpstrInitialDir = directory;
    }
    if (options && options->title)
    {
        dialog.lpstrTitle = options->title;
    }

    BOOL accepted = save_dialog ? GetSaveFileNameA(&dialog) : GetOpenFileNameA(&dialog);
    if (!accepted)
    {
        DWORD error = CommDlgExtendedError();
        if (error != 0U)
        {
            char buffer[128];
            (void)snprintf(buffer, sizeof(buffer), "File dialog failed (code %lu).",
                           (unsigned long)error);
            file_dialog_set_error(error_buffer, error_capacity, buffer);
        }
        return false;
    }

#if defined(_MSC_VER)
    (void)strncpy_s(out_path, capacity, file_buffer, _TRUNCATE);
#else
    (void)snprintf(out_path, capacity, "%s", file_buffer);
#endif
    return true;
}

bool file_dialog_open(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_win32_run(options, out_path, capacity, error_buffer, error_capacity, false);
}

bool file_dialog_save(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_win32_run(options, out_path, capacity, error_buffer, error_capacity, true);
}

#elif defined(__APPLE__)

static bool file_dialog_run_osascript(const char *script, char *out_path, size_t capacity,
                                      char *error_buffer, size_t error_capacity)
{
    FILE *pipe = popen(script, "r");
    if (!pipe)
    {
        file_dialog_set_error(error_buffer, error_capacity, "Failed to spawn osascript.");
        return false;
    }
    file_dialog_clear_buffer(out_path, capacity);
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe) == NULL)
    {
        int status = pclose(pipe);
        if (status != 0)
        {
            file_dialog_set_error(error_buffer, error_capacity, "osascript returned an error.");
        }
        return false;
    }
    int status = pclose(pipe);
    if (status != 0)
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "osascript returned a non-zero status.");
        return false;
    }
    size_t length = file_dialog_strnlen(buffer, sizeof(buffer));
    while (length > 0U && (buffer[length - 1U] == '\n' || buffer[length - 1U] == '\r'))
    {
        buffer[length - 1U] = '\0';
        --length;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(out_path, capacity, buffer, _TRUNCATE);
#else
    (void)snprintf(out_path, capacity, "%s", buffer);
#endif
    return true;
}

static void file_dialog_escape_quotes(const char *input, char *output, size_t capacity)
{
    if (!output || capacity == 0U)
    {
        return;
    }
    size_t cursor = 0U;
    if (!input)
    {
        output[0] = '\0';
        return;
    }
    for (const char *ptr = input; *ptr != '\0' && cursor + 2U < capacity; ++ptr)
    {
        if (*ptr == '"')
        {
            output[cursor++] = '\\';
        }
        output[cursor++] = *ptr;
    }
    output[cursor] = '\0';
}

static bool file_dialog_mac_run(const FileDialogOptions *options, char *out_path, size_t capacity,
                                char *error_buffer, size_t error_capacity, bool save_dialog)
{
    char title_buffer[128];
    file_dialog_escape_quotes(options ? options->title : "", title_buffer, sizeof(title_buffer));
    char default_buffer[512];
    file_dialog_escape_quotes(options ? options->default_path : "", default_buffer,
                              sizeof(default_buffer));

    char script[1024];
    if (save_dialog)
    {
        if (default_buffer[0] != '\0')
        {
            (void)snprintf(script, sizeof(script),
                           "osascript -e 'set _path to POSIX path of (choose file name default "
                           "name \"%s\" with prompt "
                           "\"%s\")' -e 'print _path'",
                           default_buffer,
                           title_buffer[0] != '\0' ? title_buffer : "Save Family Tree");
        }
        else
        {
            (void)snprintf(script, sizeof(script),
                           "osascript -e 'set _path to POSIX path of (choose file name with prompt "
                           "\"%s\")' -e 'print _path'",
                           title_buffer[0] != '\0' ? title_buffer : "Save Family Tree");
        }
    }
    else
    {
        if (default_buffer[0] != '\0')
        {
            (void)snprintf(script, sizeof(script),
                           "osascript -e 'set _path to POSIX path of (choose file with prompt "
                           "\"%s\" default location "
                           "POSIX file \"%s\")' -e 'print _path'",
                           title_buffer[0] != '\0' ? title_buffer : "Open Family Tree",
                           default_buffer);
        }
        else
        {
            (void)snprintf(script, sizeof(script),
                           "osascript -e 'set _path to POSIX path of (choose file with prompt "
                           "\"%s\")' -e 'print _path'",
                           title_buffer[0] != '\0' ? title_buffer : "Open Family Tree");
        }
    }

    return file_dialog_run_osascript(script, out_path, capacity, error_buffer, error_capacity);
}

bool file_dialog_open(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_mac_run(options, out_path, capacity, error_buffer, error_capacity, false);
}

bool file_dialog_save(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_mac_run(options, out_path, capacity, error_buffer, error_capacity, true);
}

#elif defined(__linux__)

static bool file_dialog_command_available(const char *command)
{
    if (!command)
    {
        return false;
    }
    char probe[256];
    (void)snprintf(probe, sizeof(probe), "command -v %s >/dev/null 2>&1", command);
    int status = system(probe);
    return status == 0;
}

static bool file_dialog_run_capture(const char *command, char *out_path, size_t capacity,
                                    char *error_buffer, size_t error_capacity)
{
    FILE *pipe = popen(command, "r");
    if (!pipe)
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "Failed to spawn file selection command.");
        return false;
    }
    file_dialog_clear_buffer(out_path, capacity);
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe) == NULL)
    {
        int status = pclose(pipe);
        if (status != 0)
        {
            file_dialog_set_error(error_buffer, error_capacity,
                                  "File selection command returned an error.");
        }
        return false;
    }
    int status = pclose(pipe);
    if (status != 0)
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "File selection command returned non-zero status.");
        return false;
    }
    size_t length = file_dialog_strnlen(buffer, sizeof(buffer));
    while (length > 0U && (buffer[length - 1U] == '\n' || buffer[length - 1U] == '\r'))
    {
        buffer[length - 1U] = '\0';
        --length;
    }
#if defined(_MSC_VER)
    (void)strncpy_s(out_path, capacity, buffer, _TRUNCATE);
#else
    (void)snprintf(out_path, capacity, "%s", buffer);
#endif
    return true;
}

static void file_dialog_escape_shell(const char *input, char *output, size_t capacity)
{
    if (!output || capacity == 0U)
    {
        return;
    }
    size_t cursor = 0U;
    if (!input)
    {
        output[0] = '\0';
        return;
    }
    for (const char *ptr = input; *ptr != '\0' && cursor + 2U < capacity; ++ptr)
    {
        if (*ptr == '\'' || *ptr == '"' || *ptr == '\\')
        {
            output[cursor++] = '\\';
        }
        output[cursor++] = *ptr;
    }
    output[cursor] = '\0';
}

static bool file_dialog_append_format(char *destination, size_t capacity, const char *format, ...)
{
    if (!destination || capacity == 0U)
    {
        return false;
    }
    size_t length = file_dialog_strnlen(destination, capacity);
    if (length >= capacity)
    {
        return false;
    }
    va_list args;
    va_start(args, format);
    int written = vsnprintf(destination + length, capacity - length, format, args);
    va_end(args);
    if (written < 0)
    {
        destination[length] = '\0';
        return false;
    }
    if ((size_t)written >= capacity - length)
    {
        destination[capacity - 1U] = '\0';
        return false;
    }
    return true;
}

static bool file_dialog_build_linux_command(const FileDialogOptions *options, bool save_dialog,
                                            char *command, size_t capacity)
{
    const char *tool = NULL;
    if (file_dialog_command_available("zenity"))
    {
        tool = "zenity";
    }
    else if (file_dialog_command_available("kdialog"))
    {
        tool = "kdialog";
    }
    if (!tool)
    {
        return false;
    }
    char title[128];
    file_dialog_escape_shell(options ? options->title : "", title, sizeof(title));
    char def_path[512];
    file_dialog_escape_shell(options ? options->default_path : "", def_path, sizeof(def_path));

    command[0] = '\0';
    if (strcmp(tool, "zenity") == 0)
    {
        int written = 0;
        if (save_dialog)
        {
            written = snprintf(command, capacity,
                               "zenity --file-selection --save --confirm-overwrite%s%s%s",
                               title[0] != '\0' ? " --title='" : "", title[0] != '\0' ? title : "",
                               title[0] != '\0' ? "'" : "");
        }
        else
        {
            written = snprintf(command, capacity, "zenity --file-selection%s%s%s",
                               title[0] != '\0' ? " --title='" : "", title[0] != '\0' ? title : "",
                               title[0] != '\0' ? "'" : "");
        }
        if (written < 0 || (size_t)written >= capacity)
        {
            return false;
        }
        if (def_path[0] != '\0')
        {
            if (!file_dialog_append_format(command, capacity, " --filename='%s'", def_path))
            {
                return false;
            }
        }
        if (options && options->filters && options->filter_count > 0U)
        {
            for (size_t index = 0U; index < options->filter_count; ++index)
            {
                const FileDialogFilter *filter = &options->filters[index];
                if (!filter->label || !filter->pattern)
                {
                    continue;
                }
                char label[128];
                file_dialog_escape_shell(filter->label, label, sizeof(label));
                char pattern[128];
                file_dialog_escape_shell(filter->pattern, pattern, sizeof(pattern));
                if (!file_dialog_append_format(command, capacity, " --file-filter='%s | %s'", label,
                                               pattern))
                {
                    return false;
                }
            }
        }
    }
    else
    {
        int written = 0;
        if (save_dialog)
        {
            written = snprintf(command, capacity, "kdialog --getsavefilename %s '%s'",
                               def_path[0] != '\0' ? def_path : "",
                               options && options->filters && options->filter_count > 0U &&
                                       options->filters[0].pattern
                                   ? options->filters[0].pattern
                                   : "*.json");
        }
        else
        {
            written = snprintf(command, capacity, "kdialog --getopenfilename %s '%s'",
                               def_path[0] != '\0' ? def_path : "",
                               options && options->filters && options->filter_count > 0U &&
                                       options->filters[0].pattern
                                   ? options->filters[0].pattern
                                   : "*.json");
        }
        if (written < 0 || (size_t)written >= capacity)
        {
            return false;
        }
    }
    return true;
}

static bool file_dialog_linux_run(const FileDialogOptions *options, char *out_path, size_t capacity,
                                  char *error_buffer, size_t error_capacity, bool save_dialog)
{
    char command[1024];
    if (!file_dialog_build_linux_command(options, save_dialog, command, sizeof(command)))
    {
        file_dialog_set_error(error_buffer, error_capacity,
                              "Neither zenity nor kdialog is available for file selection.");
        return false;
    }
    return file_dialog_run_capture(command, out_path, capacity, error_buffer, error_capacity);
}

bool file_dialog_open(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_linux_run(options, out_path, capacity, error_buffer, error_capacity, false);
}

bool file_dialog_save(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    return file_dialog_linux_run(options, out_path, capacity, error_buffer, error_capacity, true);
}

#else

bool file_dialog_open(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    (void)options;
    file_dialog_clear_buffer(out_path, capacity);
    file_dialog_set_error(error_buffer, error_capacity,
                          "File dialog not supported on this platform.");
    return false;
}

bool file_dialog_save(const FileDialogOptions *options, char *out_path, size_t capacity,
                      char *error_buffer, size_t error_capacity)
{
    (void)options;
    file_dialog_clear_buffer(out_path, capacity);
    file_dialog_set_error(error_buffer, error_capacity,
                          "File dialog not supported on this platform.");
    return false;
}

#endif
