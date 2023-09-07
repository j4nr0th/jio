//
// Created by jan on 1.7.2023.
//
#include "../../../include/jio/iocsv.h"
#include <stdlib.h>
#include <stdio.h>

#ifndef NDEBUG
#define DBG_STOP __builtin_trap()
#else
#define DBG_STOP (void)0;
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

    jio_memory_file* csv_file;
    res = jio_memory_file_create(ctx, "csv_test_simple.csv", &csv_file, 0, 0, 0);
    ASSERT(res == JIO_RESULT_SUCCESS);


    jio_csv_data* csv_data;
    res = jio_parse_csv(ctx, csv_file, ",", true, true, &csv_data);
    ASSERT(res == JIO_RESULT_SUCCESS);
    uint32_t rows, cols;
    jio_csv_shape(csv_data, &rows, &cols);
    ASSERT(res == JIO_RESULT_SUCCESS);
    for (uint32_t i = 0; i < cols; ++i)
    {
        const jio_csv_column* p_column;
        res = jio_csv_get_column(csv_data, i, &p_column);
        ASSERT(res == JIO_RESULT_SUCCESS);
        printf("Column %u has a header \"%.*s\" and length of %u:\n", i, (int)p_column->header.len, p_column->header.begin, p_column->count);
        for (uint32_t j = 0; j < rows; ++j)
        {
            jio_string_segment* segment = p_column->elements + j;
            printf("\telement %u: \"%.*s\"\n", j, (int)segment->len, segment->begin);
        }
    }
    jio_csv_release(ctx, csv_data);
    jio_memory_file_destroy(csv_file);

    jio_context_destroy(ctx);

    return 0;
}
