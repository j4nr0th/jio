//
// Created by jan on 30.6.2023.
//

#ifndef JIO_IOBASE_H
#define JIO_IOBASE_H
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include "ioerr.h"

typedef struct jio_memory_file_T jio_memory_file;

typedef struct jio_string_segment_T jio_string_segment;
struct jio_string_segment_T
{
    const char* begin;
    size_t len;
};

typedef struct jio_allocator_callbacks_T jio_allocator_callbacks;
struct jio_allocator_callbacks_T
{
    void* (*alloc)(void* param, size_t size);
    void (*free)(void* param, void* ptr);
    void* (*realloc)(void* param, void* ptr, size_t new_size);
    void* param;
};

typedef struct jio_error_callbacks_T jio_error_callbacks;
struct jio_error_callbacks_T
{
    void (*report)(void* state, const char* msg, const char* file, int line, const char* function);
    void* state;
};

typedef struct jio_context_T jio_context;

typedef struct jio_context_create_info_T jio_context_create_info;
struct jio_context_create_info_T
{
    const jio_allocator_callbacks*  allocator_callbacks;
    const jio_allocator_callbacks*  stack_allocator_callbacks;
    const jio_error_callbacks*      error_callbacks;
};

typedef struct jio_memory_file_info_T jio_memory_file_info;
struct jio_memory_file_info_T
{
    const char* full_name;
    unsigned char* memory;
    int can_write;
    size_t size;
};

jio_result jio_context_create(const jio_context_create_info* create_info, jio_context** p_context);

void jio_context_destroy(jio_context* ctx);

jio_result jio_memory_file_create(
        const jio_context* ctx, const char* filename, jio_memory_file** p_file_out, int write, int can_create, size_t size);

jio_result jio_memory_file_sync(const jio_memory_file* file, int sync);

unsigned jio_memory_file_count_lines(const jio_memory_file* file);

unsigned jio_memory_file_count_non_empty_lines(const jio_memory_file* file);

void jio_memory_file_destroy(jio_memory_file* mem_file);

jio_memory_file_info jio_memory_file_get_info(const jio_memory_file* file);

#endif //JIO_IOBASE_H
