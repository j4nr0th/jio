//
// Created by jan on 1.7.2023.
//
#include "../../../include/jio/iocsv.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#ifndef NDEBUG
#define DBG_STOP __builtin_trap()
#else
#define DBG_STOP (void)0;
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

static const char* const potato_string = "Potato";
static void* wrap_save(void* param)
{
    ASSERT(param == banana_string);
    return (void*)potato_string;
}
static void wrap_restore(void* param, void* ptr)
{
    ASSERT(param == banana_string);
    ASSERT(ptr == potato_string);
}
static const jio_stack_allocator_callbacks jio_stack_callbacks =
        {
                .param = (void*)banana_string,
                .alloc = wrap_alloc,
                .realloc = wrap_realloc,
                .free = wrap_free,
                .save = wrap_save,
                .restore = wrap_restore,
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

    {
        jio_memory_file csv_file;
        jio_result res = jio_memory_file_create("csv_test_ugly.csv", &csv_file, 0, 0, 0);
        ASSERT(res == JIO_RESULT_SUCCESS);
        jio_csv_data* csv_data;
        res = jio_parse_csv(&csv_file, ",", true, true, &csv_data, &jio_callbacks, &jio_stack_callbacks);
        ASSERT(res == JIO_RESULT_SUCCESS);
        uint32_t rows, cols;
        res = jio_csv_shape(csv_data, &rows, &cols);
        ASSERT(res == JIO_RESULT_SUCCESS);
        for (uint32_t i = 0; i < cols; ++i)
        {
            const jio_csv_column* p_column;
            res = jio_csv_get_column(csv_data, i, &p_column);
            ASSERT(res == JIO_RESULT_SUCCESS);
            printf(
                    "Column %u has a header \"%.*s\" and length of %u:\n", i, (int) p_column->header.len,
                    p_column->header.begin, p_column->count);
            for (uint32_t j = 0; j < rows; ++j)
            {
                jio_string_segment* segment = p_column->elements + j;
                printf("\telement %u: \"%.*s\"\n", j, (int) segment->len, segment->begin);
            }
        }
        jio_csv_release(csv_data);
        jio_memory_file_destroy(&csv_file);
    }
    {
        jio_memory_file csv_file;
        jio_result res = jio_memory_file_create("csv_test_ugly2.csv", &csv_file, 0, 0, 0);
        ASSERT(res == JIO_RESULT_SUCCESS);
        jio_csv_data* csv_data;
        res = jio_parse_csv(&csv_file, "( ͡° ͜ʖ ͡°)", true, true, &csv_data, &jio_callbacks, &jio_stack_callbacks);
        ASSERT(res == JIO_RESULT_SUCCESS);
        uint32_t rows, cols;
        res = jio_csv_shape(csv_data, &rows, &cols);
        ASSERT(res == JIO_RESULT_SUCCESS);
        for (uint32_t i = 0; i < cols; ++i)
        {
            const jio_csv_column* p_column;
            res = jio_csv_get_column(csv_data, i, &p_column);
            ASSERT(res == JIO_RESULT_SUCCESS);
            printf(
                    "Column %u has a header \"%.*s\" and length of %u:\n", i, (int) p_column->header.len,
                    p_column->header.begin, p_column->count);
            for (uint32_t j = 0; j < rows; ++j)
            {
                jio_string_segment* segment = p_column->elements + j;
                printf("\telement %u: \"%.*s\"\n", j, (int) segment->len, segment->begin);
            }
        }
        jio_csv_release(csv_data);
        jio_memory_file_destroy(&csv_file);
    }

    JDM_LEAVE_FUNCTION;
    jdm_cleanup_thread();
    return 0;
}
