//
// Created by jan on 1.7.2023.
//
#include "../../../include/jio/iocsv.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef NDEBUG
#define DBG_STOP __builtin_trap()
#else
#define DBG_STOP (void)0;
#endif
#define ASSERT(x) if (!(x)) {fputs("Failed assertion \"" #x "\"\n", stderr); DBG_STOP; exit(EXIT_FAILURE);} (void)0


static inline void print_csv(const jio_csv_data* data)
{
    jio_result res;
    uint32_t rows, cols;
    jio_csv_shape(data, &rows, &cols);
    for (uint32_t i = 0; i < cols; ++i)
    {
        const jio_csv_column* p_column;
        res = jio_csv_get_column(data, i, &p_column);
        ASSERT(res == JIO_RESULT_SUCCESS);
        printf("Column %u has a header \"%.*s\" and length of %u:\n", i, (int)p_column->header.len, p_column->header.begin, p_column->count);
        for (uint32_t j = 0; j < rows; ++j)
        {
            jio_string_segment* segment = p_column->elements + j;
            printf("\telement %u: \"%.*s\"\n", j, (int)segment->len, segment->begin);
        }
    }
}

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
    //  Test correct version
    res = jio_parse_csv(ctx, csv_file, ",", true, true, &csv_data);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("\nCsv contents before editing:\n\n");
    print_csv(csv_data);

    jio_csv_column extra_column =
            {
            .header = {.begin = "extra:)", .len = sizeof("extra:)") - 1},
            .count = 4,
            .capacity = 4,
            .elements = malloc(sizeof(*extra_column.elements) * 4),
            };
    ASSERT(extra_column.elements != NULL);
    extra_column.elements[0].begin = "Ass", extra_column.elements[0].len = 3;
    extra_column.elements[1].begin = "We", extra_column.elements[1].len = 2;
    extra_column.elements[2].begin = "Can", extra_column.elements[2].len = 3;
    extra_column.elements[3].begin = "!", extra_column.elements[3].len = 1;


    //  Test correct version
    res = jio_csv_add_cols(ctx, csv_data, 2, 1, &extra_column);
    ASSERT(res == JIO_RESULT_SUCCESS);

    printf("\nCsv contents after adding column:\n\n");
    print_csv(csv_data);

    //  Test incorrect version
    //      Position out of bounds
    res = jio_csv_remove_cols(ctx, csv_data, 1212312312, 2);
    ASSERT(res == JIO_RESULT_BAD_INDEX);

    //  Test correct version
    res = jio_csv_remove_cols(ctx, csv_data, 0, 2);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Csv contents after removing columns:\n");
    print_csv(csv_data);

    jio_string_segment** row_array = malloc(sizeof(jio_string_segment*) * 10);
    ASSERT(row_array);
    for (uint32_t i = 0; i < 10; ++i)
    {
        row_array[i] = malloc(sizeof(*row_array[i]) * 4);
        ASSERT(row_array[i]);
        for (uint32_t j = 0; j < 4; ++j)
        {
            row_array[i][j] = (jio_string_segment){.begin = "Funni", .len = 5};
        }
    }

    //  Test the incorrect versions
    //      Position out of bounds
    res = jio_csv_add_rows(ctx, csv_data, 112412412, 10, (const jio_string_segment**) row_array);
    ASSERT(res == JIO_RESULT_BAD_INDEX);

    //  Test getting the same column by both index, name, and string segment
    const char* extra_name = "extra:)";
    const jio_csv_column*  p_column1, *p_column2, *p_column3;

    res = jio_csv_get_column(csv_data, 0, &p_column1);
    ASSERT(res == JIO_RESULT_SUCCESS);

    res = jio_csv_get_column_by_name(ctx, csv_data, extra_name, &p_column2);
    ASSERT(res == JIO_RESULT_SUCCESS);
    //  Test the incorrect versions
    res = jio_csv_get_column_by_name(ctx, csv_data, "gamer god", &p_column2);
    ASSERT(res == JIO_RESULT_BAD_CSV_HEADER);

    const jio_string_segment ss = (jio_string_segment){ .begin = extra_name, .len = 7 };
    res = jio_csv_get_column_by_name_segment(ctx, csv_data, &ss, &p_column3);
    ASSERT(res == JIO_RESULT_SUCCESS);
    const jio_string_segment bad_ss = (jio_string_segment){ .begin = extra_name, .len = 6 };
    res = jio_csv_get_column_by_name_segment(ctx, csv_data, &bad_ss, &p_column2);
    ASSERT(res == JIO_RESULT_BAD_CSV_HEADER);


    ASSERT(p_column1 == p_column2);
    ASSERT(p_column2 == p_column3);



    //  Test the correct version
    res = jio_csv_add_rows(ctx, csv_data, 1, 10, (const jio_string_segment**) row_array);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("\nCsv contents after adding rows:\n\n");
    print_csv(csv_data);

    //  Test the incorrect versions
    res = jio_csv_remove_rows(ctx, csv_data, 212412512, 5);
    ASSERT(res == JIO_RESULT_BAD_INDEX);

    //  Test the correct version
    res = jio_csv_remove_rows(ctx, csv_data, 3, 5);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Csv contents after removing rows:\n");
    print_csv(csv_data);


    //  Test replacing rows
    for (uint32_t i = 0; i < 10; ++i)
    {
        for (uint32_t j = 0; j < 4; ++j)
        {
            row_array[i][j] = (jio_string_segment){.begin = "Not funne", .len = 9};
        }
    }

    //  Test the incorrect version
    res = jio_csv_replace_rows(ctx, csv_data, 13, 4, 6, (const jio_string_segment* const*) row_array);
    ASSERT(res == JIO_RESULT_BAD_INDEX);
    res = jio_csv_replace_rows(ctx, csv_data, 3, 27123, 6, (const jio_string_segment* const*) row_array);
    ASSERT(res == JIO_RESULT_BAD_INDEX);
    //  Test the correct version
    res = jio_csv_replace_rows(ctx, csv_data, 3, 4, 6, (const jio_string_segment* const*) row_array);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Csv contents after replacing rows:\n");
    print_csv(csv_data);

    size_t data_size = 0;
    const char* new_sep = "( ͡° ͜ʖ ͡°)";
    //  Incorrect versions
    //  Correct version
    res = jio_csv_print_size(csv_data, &data_size, strlen(new_sep), 1, true);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Space needed to print the data: %zu bytes\n", data_size);

    jio_memory_file* f_out;
    res = jio_memory_file_create(ctx, "out.csv", &f_out, 1, 1, data_size);
    ASSERT(res == JIO_RESULT_SUCCESS);
    const jio_memory_file_info info = jio_memory_file_get_info(f_out);
    ASSERT(info.size >= data_size);
    ASSERT(info.can_write == 1);
    size_t real_usage = 0;
    res = jio_csv_print(csv_data, &real_usage, (char*)info.memory, new_sep, 1, true, true);
    ASSERT(res == JIO_RESULT_SUCCESS);
    printf("Real usage: %zu bytes\n", real_usage);

    res = jio_memory_file_sync(f_out, 1);
    ASSERT(res == JIO_RESULT_SUCCESS);
    jio_memory_file_destroy(f_out);



    for (uint32_t i = 0; i < 10; ++i)
    {
        free(row_array[i]);
    }
    free(row_array);

    jio_csv_release(ctx, csv_data);
    jio_memory_file_destroy(csv_file);

    jio_context_destroy(ctx);
    return 0;
}
