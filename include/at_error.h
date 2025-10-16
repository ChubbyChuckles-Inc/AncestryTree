#ifndef AT_ERROR_H
#define AT_ERROR_H

#include "at_log.h"

#include <setjmp.h>
#include <stddef.h>

typedef struct AtTryContext
{
    jmp_buf jump;
    const char *message;
    int code;
} AtTryContext;

static inline void at_try_context_init(AtTryContext *context)
{
    if (!context)
    {
        return;
    }
    context->message = NULL;
    context->code = 0;
}

static inline void at_throw(AtTryContext *context, int code, const char *message)
{
    if (!context)
    {
        return;
    }
    context->message = message;
    context->code = code;
    longjmp(context->jump, 1);
}

#define AT_TRY(context_ptr) if (setjmp((context_ptr)->jump) == 0)
#define AT_CATCH(context_ptr) else
#define AT_THROW(context_ptr, error_code, error_message) at_throw((context_ptr), (error_code), (error_message))

#define AT_CHECK_NULL(logger_ptr, value, action)                                     \
    do                                                                               \
    {                                                                                \
        if ((value) == NULL)                                                         \
        {                                                                            \
            AT_LOG((logger_ptr), AT_LOG_ERROR, "Null pointer detected: %s", #value); \
            action;                                                                  \
        }                                                                            \
    } while (0)

#define AT_CHECK_ALLOC(logger_ptr, value, action)                                \
    do                                                                           \
    {                                                                            \
        if ((value) == NULL)                                                     \
        {                                                                        \
            AT_LOG((logger_ptr), AT_LOG_ERROR, "Allocation failed: %s", #value); \
            action;                                                              \
        }                                                                        \
    } while (0)

#endif /* AT_ERROR_H */
