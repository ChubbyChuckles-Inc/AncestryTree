#ifndef AT_STRING_H
#define AT_STRING_H

#include <stdbool.h>
#include <stddef.h>

bool at_string_copy(char *destination, size_t destination_size, const char *source);
char *at_string_dup(const char *input);
char *at_string_dup_range(const char *input, size_t length);
bool at_string_equals(const char *lhs, const char *rhs);
size_t at_string_length_safe(const char *value);

#endif /* AT_STRING_H */
