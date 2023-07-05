//
// Created by jan on 5.7.2023.
//
#include "../../../include/jio/iobase.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#ifndef NDEBUG
#define DBG_STOP __builtin_trap()
#else
#define DBG_STOP (void)0
#endif
#define ASSERT(x) if (!(x)) {fputs("Failed assertion \"" #x "\"\n", stderr); DBG_STOP; exit(EXIT_FAILURE);} (void)0

int error_hook_fn(const char* thread_name, uint32_t stack_trace_count, const char*const* stack_trace, jdm_message_level level, uint32_t line, const char* file, const char* function, const char* message, void* param)
{
    const char* err_level_str;
    FILE* f_out;
    switch (level)
    {
    default:
        err_level_str = "unknown";
        f_out = stderr;
        break;
    case JDM_MESSAGE_LEVEL_CRIT:
        err_level_str = "critical error";
        f_out = stderr;
        break;
    case JDM_MESSAGE_LEVEL_ERR:
        err_level_str = "error";
        f_out = stderr;
        break;
    case JDM_MESSAGE_LEVEL_WARN:
        err_level_str = "warning";
        f_out = stderr;
        break;
    case JDM_MESSAGE_LEVEL_TRACE:
        err_level_str = "trace";
        f_out = stdout;
        break;
    case JDM_MESSAGE_LEVEL_DEBUG:
        err_level_str = "debug";
        f_out = stdout;
        break;
    case JDM_MESSAGE_LEVEL_INFO:
        err_level_str = "info";
        f_out = stdout;
        break;
    }
    fprintf(f_out, "%s from \"%s\" at line %u in file \"%s\", message: %s\n", err_level_str, function, line, file, message);
    return 0;
}



int main()
{
    jallocator* allocator = jallocator_create((1 << 20), 1);
    ASSERT(allocator);
    jdm_init_thread("master", JDM_MESSAGE_LEVEL_NONE, 32, 32, allocator);
    jdm_set_hook(error_hook_fn, NULL);
    linear_jallocator* lin_allocator = lin_jallocator_create(1 << 20);
    ASSERT(lin_allocator);
    void* const base = lin_jalloc_get_current(lin_allocator);
    JDM_ENTER_FUNCTION;



    jio_memory_file completely_full_file, spaced_file;
    jio_result res = jio_memory_file_create("full_lines.txt", &completely_full_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);
    res = jio_memory_file_create("with_spaces.txt", &spaced_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);

    uint32_t lines_full, lines_spaced;
    res = jio_memory_file_count_lines(&completely_full_file, &lines_full);
    ASSERT(res == JIO_RESULT_SUCCESS);
    res = jio_memory_file_count_non_empty_lines(&spaced_file, &lines_spaced);
    ASSERT(res == JIO_RESULT_SUCCESS);

    ASSERT(lines_full == lines_spaced);


    jio_memory_file_destroy(&spaced_file);
    jio_memory_file_destroy(&completely_full_file);




    JDM_LEAVE_FUNCTION;
    jdm_cleanup_thread();
    int_fast32_t i_pool, i_block;
    ASSERT(jallocator_verify(allocator, &i_pool, &i_block) == 0);
    uint_fast32_t leaked_array[128];
    uint_fast32_t count_leaked = jallocator_count_used_blocks(allocator, 128, leaked_array);
    for (uint_fast32_t i = 0; i < count_leaked; ++i)
    {
        fprintf(stderr, "Leaked block %"PRIuFAST32"\n", leaked_array[i]);
    }
    ASSERT(count_leaked == 0);
    ASSERT(base == lin_jalloc_get_current(lin_allocator));
    lin_jallocator_destroy(lin_allocator);

    return 0;
}
