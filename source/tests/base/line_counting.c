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


static const char* const banana_string = "Banana";
static void* wrap_alloc(void* param, uint64_t size)
{
    ASSERT(param == banana_string);
    return malloc(size);
}
static void* wrap_realloc(void* param, void* ptr, uint64_t new_size)
{
    ASSERT(param == banana_string);
    return realloc(ptr, new_size);
}
static void wrap_free(void* param, void* ptr)
{
    ASSERT(param == banana_string);
    free(ptr);
}

static const jdm_allocator_callbacks jdm_callbacks =
        {
                .param = (void*)banana_string,
                .alloc = wrap_alloc,
                .free = wrap_free,
        };



int main()
{
    jdm_init_thread("master", JDM_MESSAGE_LEVEL_NONE, 32, 32, &jdm_callbacks);
    jdm_set_hook(error_hook_fn, NULL);
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
    return 0;
}
