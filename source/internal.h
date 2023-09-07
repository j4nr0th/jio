//
// Created by jan on 4.9.2023.
//

#ifndef JIO_INTERNAL_H
#define JIO_INTERNAL_H
#include "../include/jio/iobase.h"

#ifdef _WIN32
#include <Windows.h>
#endif


struct jio_memory_file_T
{
    const jio_context* ctx;
    bool can_write;
    void* ptr;
    size_t file_size;
#ifndef _WIN32
    char name[PATH_MAX];
#else
    HANDLE view_handle;
    HANDLE file_handle;
    char name[4096];
#endif
};

struct jio_context_T
{
    jio_allocator_callbacks allocator_callbacks;
    jio_allocator_callbacks stack_allocator_callbacks;
    jio_error_callbacks error_callbacks;
};


void* jio_alloc(const jio_context* ctx, size_t size);

void* jio_realloc(const jio_context* ctx, void* ptr, size_t new_size);

void jio_free(const jio_context* ctx, void* ptr);

void* jio_alloc_stack(const jio_context* ctx, size_t size);

void* jio_realloc_stack(const jio_context* ctx, void* ptr, size_t new_size);

void jio_free_stack(const jio_context* ctx, void* ptr);

#ifdef __GNUC__
__attribute__((format(printf, 2, 6)))
#endif
void jio_error_report(const jio_context* ctx, const char* fmt, const char* file, int line, const char* function, ...);

#ifndef _WIN32
    #define JIO_ERROR(ctx, fmt, ...) jio_error_report((ctx), (fmt), __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__)
    #define JIO_ERROR_FN(ctx, fmt, fn, ...) jio_error_report((ctx), (fmt), __FILE__, __LINE__, (fn) __VA_OPT__(,) __VA_ARGS__)
#else
    #define JIO_ERROR(ctx, fmt, ...) jio_error_report((ctx), (fmt), __FILE__, __LINE__, __func__, __VA_ARGS__)
    #define JIO_ERROR_FN(ctx, fmt, fn, ...) jio_error_report((ctx), (fmt), __FILE__, __LINE__, (fn) , __VA_ARGS__)
#endif

bool jio_string_segment_equal(const jio_string_segment* first, const jio_string_segment* second);

bool jio_string_segment_equal_case(const jio_string_segment* first, const jio_string_segment* second);

bool jio_string_segment_equal_str(const jio_string_segment* first, const char* str);

bool jio_string_segment_equal_str_case(const jio_string_segment* first, const char* str);

bool jio_iswhitespace(unsigned c);

#endif //JIO_INTERNAL_H
