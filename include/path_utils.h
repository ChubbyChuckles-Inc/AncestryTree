#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    bool path_join_relative(const char *base_dir, unsigned int levels_up, const char *relative_path, char *out,
                            size_t capacity);

#ifdef __cplusplus
}
#endif

#endif /* PATH_UTILS_H */
