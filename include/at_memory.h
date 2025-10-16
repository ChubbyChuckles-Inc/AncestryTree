#ifndef AT_MEMORY_H
#define AT_MEMORY_H

#include <stdbool.h>
#include <stddef.h>

#if !defined(AT_MEMORY_ENABLE_TRACKING)
#if !defined(NDEBUG)
#define AT_MEMORY_ENABLE_TRACKING 1
#else
#define AT_MEMORY_ENABLE_TRACKING 0
#endif
#endif

typedef struct AtMemoryStats
{
    size_t total_allocations;
    size_t total_frees;
    size_t outstanding_allocations;
    size_t outstanding_bytes;
    size_t peak_bytes;
} AtMemoryStats;

bool at_check_mul_overflow_size(size_t a, size_t b, size_t *out_result);
void *at_secure_realloc_impl(void *ptr, size_t new_count, size_t element_size, const char *file, int line);

void *at_memory_allocate(size_t size, const char *file, int line);
void *at_memory_callocate(size_t count, size_t size, const char *file, int line);
void *at_memory_reallocate(void *ptr, size_t size, const char *file, int line);
void at_memory_release(void *ptr, const char *file, int line);
void at_memory_get_stats(AtMemoryStats *out_stats);
void at_memory_reset_tracking(void);
size_t at_memory_outstanding_allocations(void);
size_t at_memory_outstanding_bytes(void);
void at_memory_report_leaks(void);

#define at_secure_realloc(ptr, new_count, element_size) \
    at_secure_realloc_impl((ptr), (new_count), (element_size), __FILE__, __LINE__)

#define AT_MALLOC(size) at_memory_allocate((size), __FILE__, __LINE__)
#define AT_CALLOC(count, size) at_memory_callocate((count), (size), __FILE__, __LINE__)
#define AT_REALLOC(ptr, size) at_memory_reallocate((ptr), (size), __FILE__, __LINE__)
#define AT_FREE(ptr) at_memory_release((ptr), __FILE__, __LINE__)

#endif /* AT_MEMORY_H */
