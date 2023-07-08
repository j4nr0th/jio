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

static const jio_allocator_callbacks jio_callbacks =
        {
                .param = (void*)banana_string,
                .alloc = wrap_alloc,
                .realloc = wrap_realloc,
                .free = wrap_free,
        };

static const jio_stack_allocator_callbacks jio_stack_callbacks =
        {
                .param = (void*)banana_string,
                .alloc = wrap_alloc,
                .realloc = wrap_realloc,
                .free = wrap_free,
        };

static const jdm_allocator_callbacks jdm_callbacks =
        {
                .param = (void*)banana_string,
                .alloc = wrap_alloc,
                .free = wrap_free,
        };


int main()
{
    jdm_init_thread("master", JDM_MESSAGE_LEVEL_NONE, 32, 32, &jdm_callbacks);
    JDM_ENTER_FUNCTION;
    jdm_set_hook(error_hook_fn, NULL);

    jio_memory_file cfg_file;
    jio_result res = jio_memory_file_create("ini_test_simple.ini", &cfg_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);

    jio_cfg_section* root;
    res = jio_cfg_parse(&cfg_file, &root, &jio_callbacks);
    ASSERT(res == JIO_RESULT_SUCCESS);

    size_t size_needed = 0, actual_size = 0;
    res = jio_cfg_print_size(root, 1, &size_needed, true, false);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Space needed: %zu bytes\n", size_needed);
    jio_memory_file f_out;
    res = jio_memory_file_create("..out.ini", &f_out, 1, 1, size_needed);
    ASSERT(res == JIO_RESULT_SUCCESS);
    ASSERT(f_out.file_size >= size_needed);
    char big_buffer[4096] = {0};
    res = jio_cfg_print(root, big_buffer, "=", &actual_size, true, false, false);
    ASSERT(res == JIO_RESULT_SUCCESS);
    fwrite(big_buffer, 1, actual_size, stdout);
    fflush(stdout);
    ASSERT(actual_size <= size_needed);
    jio_memory_file_sync(&f_out, 1);
    jio_memory_file_destroy(&f_out);

    jio_cfg_section_destroy(root);
    jio_memory_file_destroy(&cfg_file);

    JDM_LEAVE_FUNCTION;
    jdm_cleanup_thread();
    return 0;
}
