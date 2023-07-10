//
// Created by jan on 30.6.2023.
//

#ifndef JIO_IOBASE_H
#define JIO_IOBASE_H
#include <stddef.h>
#include <limits.h>
#include <jdm.h>
#include <stdbool.h>
#include "ioerr.h"

typedef struct jio_memory_file_struct jio_memory_file;

struct jio_memory_file_struct
{
    uint32_t can_write:1;
    void* ptr;
    size_t file_size;
#ifndef _WIN32
    char name[PATH_MAX];
#else
    char name[4096];
#endif
};

typedef struct jio_string_segment_struct jio_string_segment;
struct jio_string_segment_struct
{
    const char* begin;
    size_t len;
};

typedef struct jio_allocator_callbacks_struct jio_allocator_callbacks;
struct jio_allocator_callbacks_struct
{
    void* (*alloc)(void* param, uint64_t size);
    void (*free)(void* param, void* ptr);
    void* (*realloc)(void* param, void* ptr, uint64_t new_size);
    void* param;
};

typedef struct jio_stack_allocator_callbacks_struct jio_stack_allocator_callbacks;
struct jio_stack_allocator_callbacks_struct
{
    void* (*alloc)(void* param, uint64_t size);
    void (*free)(void* param, void* ptr);
    void* (*realloc)(void* param, void* ptr, uint64_t new_size);
    void* (*save)(void* param);
    void (*restore)(void* param, void* state);
    void* param;
};

bool iswhitespace(unsigned c);

bool string_segment_equal(const jio_string_segment* first, const jio_string_segment* second);

bool string_segment_equal_case(const jio_string_segment* first, const jio_string_segment* second);

bool string_segment_equal_str(const jio_string_segment* first, const char* str);

bool string_segment_equal_str_case(const jio_string_segment* first, const char* str);

jio_result jio_memory_file_create(
        const char* filename, jio_memory_file* p_file_out, int write, int can_create, size_t size);

jio_result jio_memory_file_sync(const jio_memory_file* file, int sync);

jio_result jio_memory_file_count_lines(const jio_memory_file* file, uint32_t* p_out);

jio_result jio_memory_file_count_non_empty_lines(const jio_memory_file* file, uint32_t* p_out);

void jio_memory_file_destroy(jio_memory_file* p_file_out);

#endif //JIO_IOBASE_H
