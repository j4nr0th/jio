//
// Created by jan on 5.7.2023.
//
#include "../../../include/jio/iobase.h"
#include <stdlib.h>
#include <stdio.h>

#ifndef NDEBUG
#define DBG_STOP __builtin_trap()
#else
#define DBG_STOP (void)0
#endif
#define ASSERT(x) if (!(x)) {fputs("Failed assertion \"" #x "\"\n", stderr); DBG_STOP; exit(EXIT_FAILURE);} (void)0


int main()
{
    jio_context* ctx;
    const jio_context_create_info create_info =
            {
            .error_callbacks = NULL,
            .allocator_callbacks = NULL,
            .stack_allocator_callbacks = NULL,
            };
    jio_result res = jio_context_create(&create_info, &ctx);
    ASSERT(res == JIO_RESULT_SUCCESS);

    jio_memory_file* completely_full_file, *spaced_file;
    res = jio_memory_file_create(ctx, "full_lines.txt", &completely_full_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);
    res = jio_memory_file_create(ctx, "with_spaces.txt", &spaced_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);

    uint32_t lines_full, lines_spaced;
    lines_full = jio_memory_file_count_lines(completely_full_file);
    ASSERT(res == JIO_RESULT_SUCCESS);
    lines_spaced = jio_memory_file_count_non_empty_lines(spaced_file);
    ASSERT(res == JIO_RESULT_SUCCESS);

    ASSERT(lines_full == lines_spaced);


    jio_memory_file_destroy(spaced_file);
    jio_memory_file_destroy(completely_full_file);

    jio_context_destroy(ctx);
    return 0;
}
