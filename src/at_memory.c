#include "at_memory.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct AtMemoryRecord
{
    void *pointer;
    size_t size;
    const char *file;
    int line;
    unsigned long long id;
} AtMemoryRecord;

#if AT_MEMORY_ENABLE_TRACKING
static AtMemoryRecord *g_records = NULL;
static size_t g_record_count = 0U;
static size_t g_record_capacity = 0U;
static AtMemoryStats g_stats = {0U, 0U, 0U, 0U, 0U};
static unsigned long long g_next_id = 0ULL;
static int g_tracking_initialized = 0;
static int g_suppress_tracking = 0;

static void at_memory_report_leaks_internal(void);

static void at_memory_tracking_initialize(void)
{
    if (!g_tracking_initialized)
    {
        g_tracking_initialized = 1;
        (void)atexit(at_memory_report_leaks_internal);
    }
}

static void at_memory_records_reserve(size_t required)
{
    if (g_record_capacity >= required)
    {
        return;
    }
    size_t new_capacity = g_record_capacity == 0U ? 32U : g_record_capacity * 2U;
    while (new_capacity < required)
    {
        new_capacity *= 2U;
    }
    g_suppress_tracking = 1;
    AtMemoryRecord *records = (AtMemoryRecord *)realloc(g_records, new_capacity * sizeof(AtMemoryRecord));
    g_suppress_tracking = 0;
    if (!records)
    {
        return;
    }
    g_records = records;
    g_record_capacity = new_capacity;
}

static void at_memory_add_record(void *pointer, size_t size, const char *file, int line)
{
    if (!pointer)
    {
        return;
    }
    at_memory_records_reserve(g_record_count + 1U);
    if (g_record_capacity == 0U)
    {
        return;
    }
    AtMemoryRecord record;
    record.pointer = pointer;
    record.size = size;
    record.file = file;
    record.line = line;
    record.id = ++g_next_id;
    g_records[g_record_count++] = record;
    g_stats.total_allocations++;
    g_stats.outstanding_allocations++;
    g_stats.outstanding_bytes += size;
    if (g_stats.outstanding_bytes > g_stats.peak_bytes)
    {
        g_stats.peak_bytes = g_stats.outstanding_bytes;
    }
}

static AtMemoryRecord *at_memory_find_record(void *pointer, size_t *out_index)
{
    if (!pointer)
    {
        return NULL;
    }
    for (size_t i = 0U; i < g_record_count; ++i)
    {
        if (g_records[i].pointer == pointer)
        {
            if (out_index)
            {
                *out_index = i;
            }
            return &g_records[i];
        }
    }
    return NULL;
}

static void at_memory_remove_index(size_t index)
{
    if (index >= g_record_count)
    {
        return;
    }
    AtMemoryRecord removed = g_records[index];
    g_records[index] = g_records[g_record_count - 1U];
    g_record_count -= 1U;
    g_stats.outstanding_allocations -= 1U;
    if (g_stats.outstanding_bytes >= removed.size)
    {
        g_stats.outstanding_bytes -= removed.size;
    }
    else
    {
        g_stats.outstanding_bytes = 0U;
    }
    g_stats.total_frees++;
}

static void at_memory_report_leaks_internal(void)
{
    if (g_record_count == 0U)
    {
        return;
    }
    fprintf(stderr, "at_memory: detected %zu leaked allocation(s) totalling %zu bytes\n", g_record_count,
            g_stats.outstanding_bytes);
    for (size_t i = 0U; i < g_record_count; ++i)
    {
        const AtMemoryRecord *record = &g_records[i];
        const char *file = record->file ? record->file : "<unknown>";
        fprintf(stderr, "  leak #%llu -> ptr=%p size=%zu at %s:%d\n", record->id, record->pointer, record->size, file,
                record->line);
    }
}
#else
static AtMemoryStats g_stats = {0U, 0U, 0U, 0U, 0U};
#endif

bool at_check_mul_overflow_size(size_t a, size_t b, size_t *out_result)
{
    if (a == 0U || b == 0U)
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

void *at_secure_realloc_impl(void *ptr, size_t new_count, size_t element_size, const char *file, int line)
{
    size_t total_size = 0U;
    if (at_check_mul_overflow_size(new_count, element_size, &total_size))
    {
        return NULL;
    }
    if (total_size == 0U)
    {
        at_memory_release(ptr, file, line);
        return NULL;
    }
    return at_memory_reallocate(ptr, total_size, file, line);
}

void *at_memory_allocate(size_t size, const char *file, int line)
{
    void *pointer = malloc(size);
#if AT_MEMORY_ENABLE_TRACKING
    if (pointer && !g_suppress_tracking)
    {
        at_memory_tracking_initialize();
        at_memory_add_record(pointer, size, file, line);
    }
#else
    (void)file;
    (void)line;
#endif
    return pointer;
}

void *at_memory_callocate(size_t count, size_t size, const char *file, int line)
{
    size_t total_size = 0U;
    if (at_check_mul_overflow_size(count, size, &total_size))
    {
        return NULL;
    }
    void *pointer = calloc(count, size);
#if AT_MEMORY_ENABLE_TRACKING
    if (pointer && !g_suppress_tracking)
    {
        at_memory_tracking_initialize();
        at_memory_add_record(pointer, total_size, file, line);
    }
#else
    (void)file;
    (void)line;
#endif
    return pointer;
}

void *at_memory_reallocate(void *ptr, size_t size, const char *file, int line)
{
    if (!ptr)
    {
        return at_memory_allocate(size, file, line);
    }
    if (size == 0U)
    {
        at_memory_release(ptr, file, line);
        return NULL;
    }

#if AT_MEMORY_ENABLE_TRACKING
    size_t index = 0U;
    AtMemoryRecord *record = NULL;
    if (!g_suppress_tracking)
    {
        at_memory_tracking_initialize();
        record = at_memory_find_record(ptr, &index);
    }
#endif

    void *result = realloc(ptr, size);
    if (!result)
    {
        return NULL;
    }

#if AT_MEMORY_ENABLE_TRACKING
    if (!g_suppress_tracking)
    {
        if (record)
        {
            if (result == ptr)
            {
                if (g_stats.outstanding_bytes >= record->size)
                {
                    g_stats.outstanding_bytes -= record->size;
                }
                else
                {
                    g_stats.outstanding_bytes = 0U;
                }
                record->size = size;
                record->file = file;
                record->line = line;
                g_stats.outstanding_bytes += size;
                if (g_stats.outstanding_bytes > g_stats.peak_bytes)
                {
                    g_stats.peak_bytes = g_stats.outstanding_bytes;
                }
            }
            else
            {
                at_memory_remove_index(index);
                at_memory_add_record(result, size, file, line);
            }
        }
        else
        {
            at_memory_add_record(result, size, file, line);
        }
    }
#else
    (void)file;
    (void)line;
#endif

    return result;
}

void at_memory_release(void *ptr, const char *file, int line)
{
    if (!ptr)
    {
        return;
    }
#if AT_MEMORY_ENABLE_TRACKING
    (void)file;
    (void)line;
    if (!g_suppress_tracking)
    {
        at_memory_tracking_initialize();
        size_t index = 0U;
        AtMemoryRecord *record = at_memory_find_record(ptr, &index);
        if (record)
        {
            at_memory_remove_index(index);
        }
        else
        {
            fprintf(stderr, "at_memory: free of untracked pointer %p requested at %s:%d\n", ptr,
                    file ? file : "<unknown>", line);
        }
    }
#else
    (void)file;
    (void)line;
#endif
    free(ptr);
}

void at_memory_get_stats(AtMemoryStats *out_stats)
{
    if (!out_stats)
    {
        return;
    }
    *out_stats = g_stats;
}

void at_memory_reset_tracking(void)
{
#if AT_MEMORY_ENABLE_TRACKING
    g_suppress_tracking = 1;
    free(g_records);
    g_records = NULL;
    g_suppress_tracking = 0;
    g_record_count = 0U;
    g_record_capacity = 0U;
    g_next_id = 0ULL;
#endif
    g_stats.total_allocations = 0U;
    g_stats.total_frees = 0U;
    g_stats.outstanding_allocations = 0U;
    g_stats.outstanding_bytes = 0U;
    g_stats.peak_bytes = 0U;
}

size_t at_memory_outstanding_allocations(void)
{
    return g_stats.outstanding_allocations;
}

size_t at_memory_outstanding_bytes(void)
{
    return g_stats.outstanding_bytes;
}

void at_memory_report_leaks(void)
{
#if AT_MEMORY_ENABLE_TRACKING
    at_memory_report_leaks_internal();
#endif
}
