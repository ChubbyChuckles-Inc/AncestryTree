#include "at_memory.h"

#include <limits.h>
#include <stdlib.h>

bool at_check_mul_overflow_size(size_t a, size_t b, size_t *out_result)
{
    if (a == 0 || b == 0)
    {
        if (out_result)
        {
            *out_result = 0U;
        }
        return false;
    }
    if (a > SIZE_MAX / b)
    {
        return true;
    }
    if (out_result)
    {
        *out_result = a * b;
    }
    return false;
}

void *at_secure_realloc(void *ptr, size_t new_count, size_t element_size)
{
    size_t total_size = 0U;
    if (at_check_mul_overflow_size(new_count, element_size, &total_size))
    {
        return NULL;
    }
    if (total_size == 0U)
    {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, total_size);
}
