//
// Created by jan on 4.9.2023.
//

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>
#include "internal.h"

#ifdef _WIN32
    #define strncasecmp(first, second, n) _strnicmp((first), (second), (n))
#endif

bool jio_string_segment_equal(const jio_string_segment* first, const jio_string_segment* second)
{
    return first->len == second->len && (memcmp(first->begin, second->begin, first->len) == 0);
}

bool jio_string_segment_equal_case(const jio_string_segment* first, const jio_string_segment* second)
{
    return first->len == second->len && (strncasecmp((const char*)first->begin, (const char*)second->begin, first->len) == 0);
}

bool jio_string_segment_equal_str(const jio_string_segment* first, const char* str)
{
    const size_t len = strlen(str);
    return first->len == len && (memcmp(first->begin, str, len) == 0);
}

bool jio_string_segment_equal_str_case(const jio_string_segment* first, const char* str)
{
    const size_t len = strlen(str);
    return first->len == len && (strncasecmp((const char*)first->begin, str, len) == 0);
}

bool jio_iswhitespace(unsigned c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void* jio_alloc(const jio_context* ctx, size_t size)
{
    return ctx->allocator_callbacks.alloc(ctx->allocator_callbacks.param, size);
}

void* jio_realloc(const jio_context* ctx, void* ptr, size_t new_size)
{
    return ctx->allocator_callbacks.realloc(ctx->allocator_callbacks.param, ptr, new_size);
}

void jio_free(const jio_context* ctx, void* ptr)
{
    ctx->stack_allocator_callbacks.free(ctx->stack_allocator_callbacks.param, ptr);
}

void* jio_alloc_stack(const jio_context* ctx, size_t size)
{
    return ctx->stack_allocator_callbacks.alloc(ctx->stack_allocator_callbacks.param, size);
}

void* jio_realloc_stack(const jio_context* ctx, void* ptr, size_t new_size)
{
    return ctx->stack_allocator_callbacks.realloc(ctx->stack_allocator_callbacks.param, ptr, new_size);
}

void jio_free_stack(const jio_context* ctx, void* ptr)
{
    ctx->stack_allocator_callbacks.free(ctx->stack_allocator_callbacks.param, ptr);
}

void jio_error_report(const jio_context* ctx, const char* fmt, const char* file, int line, const char* function, ...)
{
    if (!ctx->error_callbacks.report)
    {
        return;
    }

    va_list args, cpy;
    va_start(args, function);
    va_copy(cpy, args);
    const int len = vsnprintf(NULL, 0, fmt, cpy);
    va_end(cpy);
    if (len < 0)
    {
        va_end(args);
        return;
    }

    char* msg = jio_alloc_stack(ctx, len + 1);
    if (!msg)
    {
        msg = jio_alloc(ctx, len + 1);
        if (!msg)
        {
            va_end(args);
            return;
        }

        (void)vsnprintf(msg, len + 1, fmt, args);
        ctx->error_callbacks.report(ctx->error_callbacks.state, msg, file, line, function);
        va_end(args);
        jio_free(ctx, msg);
        return;
    }

    (void)vsnprintf(msg, len + 1, fmt, args);
    ctx->error_callbacks.report(ctx->error_callbacks.state, msg, file, line, function);
    va_end(args);
    jio_free_stack(ctx, msg);
}


