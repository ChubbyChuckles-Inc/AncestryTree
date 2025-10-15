#ifndef AT_MEMORY_H
#define AT_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

bool at_check_mul_overflow_size(size_t a, size_t b, size_t *out_result);
void *at_secure_realloc(void *ptr, size_t new_count, size_t element_size);

#endif /* AT_MEMORY_H */
