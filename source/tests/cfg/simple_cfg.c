//
// Created by jan on 4.7.2023.
//
#include "../../../include/jio/iocfg.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

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

    jio_memory_file* cfg_file;
    res = jio_memory_file_create(ctx, "ini_test_simple.ini", &cfg_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);

    jio_cfg_section* root;
    res = jio_cfg_parse(ctx, cfg_file, &root);
    ASSERT(res == JIO_RESULT_SUCCESS);

    size_t size_needed = 0, actual_size = 0;
    size_needed = jio_cfg_print_size(root, 1, true, false);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Space needed: %zu bytes\n", size_needed);
    jio_memory_file* f_out;
    res = jio_memory_file_create(ctx, "out.ini", &f_out, 1, 1, size_needed);
    ASSERT(res == JIO_RESULT_SUCCESS);
    const jio_memory_file_info info = jio_memory_file_get_info(f_out);
    ASSERT(info.can_write);
    ASSERT(info.size >= size_needed);
//    char big_buffer[4096] = {0};
    actual_size = jio_cfg_print(root, (char*)info.memory, "=", true, false, false);
    ASSERT(res == JIO_RESULT_SUCCESS);
//    fwrite(big_buffer, 1, actual_size, stdout);
    fflush(stdout);
    ASSERT(actual_size <= size_needed);
    jio_memory_file_sync(f_out, 1);
    jio_memory_file_destroy(f_out);

    jio_cfg_section_destroy(ctx, root);
    jio_memory_file_destroy(cfg_file);

    jio_context_destroy(ctx);
    return 0;
}
