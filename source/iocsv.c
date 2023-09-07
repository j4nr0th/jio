//
// Created by jan on 30.6.2023.
//

#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include "../include/jio/iocsv.h"
#include "internal.h"


struct jio_csv_data_struct
{
    uint32_t column_capacity;               //  Max size of columns before resizing the array
    uint32_t column_count;                  //  Number of columns used
    uint32_t column_length;                 //  Length of each column
    jio_csv_column* columns;                //  Columns themselves
};

static inline jio_string_segment extract_string_segment(const char* ptr, const char* const row_end, const char** p_end, const char* restrict separator)
{
    jio_string_segment ret = {.begin = NULL, .len = 0};
    const char* entry_end = strstr(ptr, separator);
    if (entry_end > row_end || !entry_end)
    {
        entry_end = row_end;
    }

    ret.begin = ptr;
    ret.len = entry_end - ptr;
    *p_end = entry_end;
    return ret;
}

static inline uint32_t count_row_entries(const char* const csv, const char* const end_of_row, const char* restrict sep, size_t sep_len)
{
    uint32_t so_far = 0;
    for (const char* ptr = strstr(csv, sep); ptr && ptr < end_of_row; ++so_far)
    {
        ptr = strstr(ptr + sep_len, sep);
    }
    return so_far + 1;
}

static inline jio_result extract_row_entries(
        const jio_context* ctx, const uint32_t expected_elements, const char* const row_begin,
        const char* const row_end, const char* sep, size_t sep_len, const bool trim, jio_string_segment* const p_out)
{
    uint32_t i;
    const char* end = NULL, *begin = row_begin;
    for (i = 0; i < expected_elements && end < row_end; ++i)
    {
        p_out[i] = extract_string_segment(begin, row_end, &end, sep);
        begin = end + sep_len;
    }
    
    if (i != expected_elements)
    {
        JIO_ERROR(ctx, "Row contained only %u elements instead of %u", i, expected_elements);
        return JIO_RESULT_BAD_CSV_FORMAT;
    }
    if (end < row_end)
    {
        JIO_ERROR(ctx, "Row contained %u elements instead of %u", count_row_entries(row_begin, row_end, sep, sep_len), expected_elements);
        return JIO_RESULT_BAD_CSV_FORMAT;
    }


    if (trim)
    {
        for (i = 0; i < expected_elements; ++i)
        {
            jio_string_segment segment = p_out[i];
            //  Trim front whitespace
            while (jio_iswhitespace(*segment.begin) && segment.len)
            {
                segment.begin += 1;
                segment.len -= 1;
            }
            //  Trim back whitespace
            while (jio_iswhitespace(*(segment.begin + segment.len - 1)) && segment.len)
            {
                segment.len -= 1;
            }
            p_out[i] = segment;
        }
    }

    return JIO_RESULT_SUCCESS;
}

jio_result jio_parse_csv(
        const jio_context* ctx, const jio_memory_file* mem_file, const char* separator, bool trim_whitespace,
        bool has_headers, jio_csv_data** pp_csv)
{

    jio_result res;
    jio_string_segment* segments = NULL;
    jio_csv_data* const csv = jio_alloc(ctx, sizeof(*csv));
    if (!csv)
    {
        res = JIO_RESULT_BAD_ALLOC;
        goto end;
    }

    memset(csv, 0, sizeof(*csv));

    //  Parse the first row
    const char* row_begin = mem_file->ptr;
    const char* row_end = strchr(row_begin, '\n');

    //  Count columns in the csv file
    size_t sep_len = strlen(separator);
    const uint32_t column_count = count_row_entries(row_begin, row_end ? row_end : (const char*)mem_file->ptr + mem_file->file_size, separator,
                                               sep_len);
    uint32_t row_capacity = 128;
    uint32_t row_count = 0;
    segments = jio_alloc_stack(ctx, sizeof(*segments) * row_capacity * column_count);
    if (!segments)
    {
        res = JIO_RESULT_BAD_ALLOC;
        JIO_ERROR(ctx, "Could not allocate memory for csv parsing");
        goto end;
    }

    for (;;)
    {
        //  Check if more space is needed to parse the row
        if (row_capacity == row_count)
        {
            const uint32_t new_capacity = row_capacity + 128;
            jio_string_segment* const new_ptr = jio_realloc_stack(ctx, segments, sizeof(*segments) * new_capacity * column_count);
            if (!new_ptr)
            {
                res = JIO_RESULT_BAD_ALLOC;
                JIO_ERROR(ctx, "Could not reallocate memory for csv parsing");
                goto end;
            }
            segments = new_ptr;
            row_capacity = new_capacity;
        }

        if ((res = extract_row_entries(ctx, column_count, row_begin, row_end, separator, sep_len, trim_whitespace,
                                       segments + row_count * column_count)))
        {
            JIO_ERROR(ctx, "Failed parsing row %u of CSV file \"%s\", reason: %s", row_count + 1, mem_file->name, jio_result_to_str(res));
            goto end;
        }

        row_count += 1;
        //  Move to the next row
        if (row_end == (const char*)mem_file->ptr + mem_file->file_size)
        {
            break;
        }
        row_begin = row_end + 1;
        row_end = strchr(row_end + 1, '\n');
        if (!row_end)
        {
            row_end = strchr(row_begin, 0);
        }
        if (row_end - row_begin < 2)
        {
            break;
        }
    }

    //  Convert the data into appropriate format
    csv->column_count = column_count;
    csv->column_length = row_count - (has_headers ? 1 : 0);

    jio_csv_column* const columns = jio_alloc(ctx, sizeof(*columns) * column_count);
    if (!columns)
    {
        res = JIO_RESULT_BAD_ALLOC;
        JIO_ERROR(ctx, "Could not allocate memory for csv column array");
        goto end;
    }
    for (uint32_t i = 0; i < column_count; ++i)
    {
        jio_csv_column* const p_column = columns + i;
        p_column->capacity = (p_column->count = csv->column_length);
        jio_string_segment* const elements = jio_alloc(ctx, sizeof(*elements) * p_column->count);
        if (!elements)
        {
            for (uint32_t j = 0; j < i; ++j)
            {
                jio_free(ctx, columns[j].elements);
            }
            JIO_ERROR(ctx, "Could not allocate memory for csv column elements");
            goto end;
        }
        if (has_headers)
        {
            for (uint32_t j = 0; j < row_count - 1; ++j)
            {
                elements[j] = segments[i + j * column_count + column_count];
            }
        }
        else
        {
            for (uint32_t j = 0; j < row_count; ++j)
            {
                elements[j] = segments[i + j * column_count];
            }
        }
        p_column->elements = elements;
        p_column->header = has_headers ? segments[i] : (jio_string_segment){ .begin = NULL, .len = 0 };

    }
    jio_free_stack(ctx, segments);
    csv->columns = columns;

    *pp_csv = csv;
    return JIO_RESULT_SUCCESS;

end:
    jio_free_stack(ctx, segments);
    jio_free(ctx, csv);
    return res;
}

void jio_csv_release(const jio_context* ctx, jio_csv_data* data)
{
    for (unsigned i = 0; i < data->column_count; ++i)
    {
        jio_free(ctx, data->columns[i].elements);
    }

    jio_free(ctx, data->columns);
    jio_free(ctx, data);
}

void jio_csv_shape(const jio_csv_data* data, uint32_t* p_rows, uint32_t* p_cols)
{
    if (p_rows)
    {
        *p_rows = data->column_length;
    }
    if (p_cols)
    {
        *p_cols = data->column_count;
    }
}

jio_result jio_csv_get_column(const jio_csv_data* data, uint32_t index, const jio_csv_column** pp_column)
{
    if (data->column_count <= index)
    {
        return JIO_RESULT_BAD_INDEX;
    }
    *pp_column = data->columns + index;
    return JIO_RESULT_SUCCESS;
}

jio_result jio_csv_get_column_by_name(
        const jio_context* ctx, const jio_csv_data* data, const char* name, const jio_csv_column** pp_column)
{
    jio_result res;
    uint32_t idx = UINT32_MAX;
    for (uint32_t i = 0; i < data->column_count; ++i)
    {
        const jio_csv_column* column = data->columns + i;
        if (jio_string_segment_equal_str(&column->header, name))
        {
#ifndef NDEBUG
            assert(idx == UINT32_MAX);
            idx = i;
#else
            idx = i;
            break;
#endif
        }
    }

    if (idx == UINT32_MAX)
    {
        JIO_ERROR(ctx, "Csv file has no header that matches \"%s\"", name);
        res = JIO_RESULT_BAD_CSV_HEADER;
        goto end;
    }

    res = jio_csv_get_column(data, idx, pp_column);
end:
    return res;
}

jio_result jio_csv_get_column_by_name_segment(
        const jio_context* ctx, const jio_csv_data* data, const jio_string_segment* name,
        const jio_csv_column** pp_column)
{
    jio_result res;

    uint32_t idx = UINT32_MAX;
    for (uint32_t i = 0; i < data->column_count; ++i)
    {
        const jio_csv_column* column = data->columns + i;
        if (jio_string_segment_equal(&column->header, name))
        {
#ifndef NDEBUG
            assert(idx == UINT32_MAX);
            idx = i;
#else
            idx = i;
            break;
#endif
        }
    }

    if (idx == UINT32_MAX)
    {
        JIO_ERROR(ctx, "Csv file has no header that matches \"%.*s\"", (int)name->len, name->begin);
        res = JIO_RESULT_BAD_CSV_HEADER;
        goto end;
    }

    res = jio_csv_get_column(data, idx, pp_column);

end:
    return res;
}

jio_result
jio_csv_add_rows(
        const jio_context* ctx, jio_csv_data* data, uint32_t position, uint32_t row_count,
        const jio_string_segment* const* rows)
{
    jio_result res = JIO_RESULT_SUCCESS;

    if (position > data->column_length && position != UINT32_MAX)
    {
        JIO_ERROR(ctx, "Rows were to be inserted at position %"PRIu32", but csv data has only %u rows", position, data->column_length);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }

    if (data->columns[0].count + row_count >= data->columns[0].capacity)
    {
        //  Need to resize all columns to fit all columns
        const uint32_t new_capacity = data->columns[0].capacity + (row_count < 64 ? 64 : row_count);
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            jio_string_segment* const new_ptr = jio_realloc(ctx, data->columns[i].elements, new_capacity * sizeof(*new_ptr));
            if (!new_ptr)
            {
                JIO_ERROR(ctx, "Could not reallocate column %u to fit additional %u row elements", i, row_count);
                res = JIO_RESULT_BAD_ALLOC;
                goto end;
            }
            data->columns[i].elements = new_ptr;
            data->columns[i].capacity = new_capacity;
        }
    }

    //  Now move the old data out of the way of the new if needed
    if (position == UINT32_MAX)
    {
        //  Appending
        position = data->column_length;
    }
    else if (position != data->column_length)
    {
        //  Moving
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            const jio_csv_column* const column = data->columns + i;
            jio_string_segment* const elements = column->elements;
            memmove(elements + position + row_count, elements + position, sizeof(*elements) * (column->count - position));
        }
    }

    //  Inserting the new elements and correcting the column lengths
    for (uint32_t i = 0; i < data->column_count; ++i)
    {
        jio_csv_column* const column = data->columns + i;
        jio_string_segment* const elements = column->elements;
        for (uint32_t j = 0; j < row_count; ++j)
        {
            elements[position + j] = rows[j][i];
        }
        column->count += row_count;
    }
    data->column_length += row_count;



end:
    return res;
}

jio_result jio_csv_add_cols(
        const jio_context* ctx, jio_csv_data* data, uint32_t position, uint32_t col_count, const jio_csv_column* cols)
{
    jio_result res = JIO_RESULT_SUCCESS;

    if (position > data->column_count&& position != UINT32_MAX)
    {
        JIO_ERROR(ctx, "Columns were to be inserted at position %"PRIu32", but csv data has only %u columns", position, data->column_count);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }
    bool bad = false;
    for (uint32_t i = 0; i < col_count; ++i)
    {
        if (cols[i].count != data->column_length)
        {
            JIO_ERROR(ctx, "Column %u to be inserted had a length of %"PRIu32", but others have the length of %u", i, cols[i].count, data->column_length);
            bad = true;
        }
    }
    if (bad)
    {
        res = JIO_RESULT_BAD_CSV_COLUMN;
        goto end;
    }

    //  Resize the columns array if needed
    if (data->column_count + col_count >= data->column_capacity)
    {
        const uint32_t new_capacity = data->column_capacity + (col_count < 8 ? 8 :  col_count);
        jio_csv_column* const new_ptr = jio_realloc(ctx, data->columns, sizeof(*new_ptr) * new_capacity);
        if (!new_ptr)
        {
            JIO_ERROR(ctx, "Could not reallocate memory for column array");
            res = JIO_RESULT_BAD_ALLOC;
            goto end;
        }
        data->columns = new_ptr;
        data->column_capacity = new_capacity;
    }

    //  Now move the old data out of the way of the new if needed
    if (position == UINT32_MAX)
    {
        //  Appending
        position = data->column_count;
    }
    else if (position != data->column_count)
    {
        //  Moving
        memmove(data->columns + position + col_count, data->columns + position, sizeof(*data->columns) * (data->column_count - position));
    }

    //  Now insert the new data
    memcpy(data->columns + position, cols, sizeof(*cols) * col_count);

    data->column_count += col_count;
end:
    return res;
}

jio_result jio_csv_remove_rows(const jio_context* ctx, jio_csv_data* data, uint32_t position, uint32_t row_count)
{
    //  Done already?
    if (!row_count || (data && position == data->column_length)) return JIO_RESULT_SUCCESS;
    jio_result res = JIO_RESULT_SUCCESS;
    //  Check that the range to remove lies within the list of available rows
    const uint32_t begin = position, end = position + row_count;
    if (begin > data->column_length || end > data->column_length)
    {
        JIO_ERROR(ctx, "Range of rows to remove is [%"PRIu32", %"PRIu32"), but the csv file only has %"PRIu32" rows", begin, end, data->column_length);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }

    for (uint32_t i = 0; i < data->column_count; ++i)
    {
        jio_csv_column* const column = data->columns + i;
        memmove(column->elements + begin, column->elements + end, sizeof(*column->elements) * (column->count - end));
        assert(column->count >= row_count);
        column->count -= row_count;
    }
    data->column_length -= row_count;
end:
    return res;
}

jio_result jio_csv_remove_cols(const jio_context* ctx, jio_csv_data* data, uint32_t position, uint32_t col_count)
{
    //  Done already?
    jio_result res = JIO_RESULT_SUCCESS;
    //  Check that the range to remove lies within the list of available rows
    const uint32_t begin = position, end = position + col_count;
    if (begin > data->column_count || end > data->column_count)
    {
        JIO_ERROR(ctx, "Range of columns to remove is [%"PRIu32", %"PRIu32"), but the csv file only has %"PRIu32" columns", begin, end, data->column_count);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }

    for (uint32_t i = begin; i < end; ++i)
    {
        jio_csv_column* const column = data->columns + i;
        jio_free(ctx, column->elements);
    }
    memmove(data->columns + begin, data->columns + end, sizeof(*data->columns) * (data->column_count - end));
    data->column_count -= col_count;
end:
    return res;
}

jio_result jio_csv_replace_cols(
        const jio_context* ctx, jio_csv_data* data, uint32_t begin, uint32_t count, uint32_t col_count,
        const jio_csv_column* cols)
{
    jio_result res = JIO_RESULT_SUCCESS;


    const uint32_t end = begin + count;
    if (begin >= data->column_count || end >= data->column_count)
    {
        JIO_ERROR(ctx, "Columns to be replaced were on the interval [%"PRIu32", %"PRIu32"), but only values on interval [0, %"PRIu32") can be given", begin, end, data->column_count);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }

    //  What is the overall change in column count
    const int32_t d_col = (int32_t)col_count - (int32_t)count;
    //  Reallocate the memory for all the new columns
    if (d_col + data->column_count >= data->column_capacity)
    {
        const uint32_t new_capacity = data->column_capacity + (d_col < 8 ? 8 : d_col);
        jio_csv_column* const columns = jio_realloc(ctx, data->columns, sizeof(*data->columns) * new_capacity);
        if (!columns)
        {
            JIO_ERROR(ctx, "Could not reallocate memory for columns");
            res = JIO_RESULT_BAD_ALLOC;
            goto end;
        }
        data->columns = columns;
        data->column_capacity = new_capacity;
    }

    for (uint32_t i = begin; i < end; ++i)
    {
        jio_free(ctx, data->columns[i].elements);
    }
    if (end != data->column_count)
    {
        //  This if is not really needed, I just don't want a call to memmove if not needed
        memmove(data->columns + begin + col_count, data->columns + end, sizeof(*data->columns) * (data->column_count - end));
    }
    //  Insert the new columns
    memcpy(data->columns + begin, cols, sizeof(*cols) * col_count);
    data->column_count += d_col;
end:
    return res;
}

jio_result jio_csv_replace_rows(
        const jio_context* ctx, jio_csv_data* data, uint32_t begin, uint32_t count, uint32_t row_count,
        const jio_string_segment* const* rows)
{
    jio_result res = JIO_RESULT_SUCCESS;

    const uint32_t end = begin + count;
    if (begin >= data->column_length || end >= data->column_length)
    {
        JIO_ERROR(ctx, "Rows to be replaced were on the interval [%"PRIu32", %"PRIu32"), but only values on interval [0, %"PRIu32") can be given", begin, end, data->column_count);
        res = JIO_RESULT_BAD_INDEX;
        goto end;
    }

    //  What is the overall change in column count
    const int32_t d_row = (int32_t)row_count - (int32_t)count;
    //  Reallocate the memory for all the new columns
    if (d_row + data->column_length >= data->columns[0].capacity)
    {
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            jio_csv_column* const column = data->columns + i;
            const uint32_t new_capacity = column->capacity + (d_row < 8 ? 8 : d_row);
            jio_string_segment* const elements = jio_realloc(ctx, column->elements, sizeof(*column->elements) * new_capacity);
            if (!elements)
            {
                JIO_ERROR(ctx, "Could not reallocate memory for column elements");
                res = JIO_RESULT_BAD_ALLOC;
                goto end;
            }
            column->elements = elements;
            column->capacity = new_capacity;
        }
    }

    for (uint32_t i = 0; i < data->column_count; ++i)
    {
        jio_csv_column* const column = data->columns + i;
        memmove(column->elements + begin + row_count, column->elements + end, sizeof(*column->elements) * (data->column_length - end));
        //  Insert the new elements for each row
        for (uint32_t j = 0; j < row_count; ++j)
        {
            column->elements[j + begin] = rows[j][i];
        }
        column->count += d_row;
    }
    data->column_length += d_row;

end:
    return res;
}

jio_result jio_csv_column_index(const jio_csv_data* data, const jio_csv_column* column, uint32_t* p_idx)
{
    jio_result res = JIO_RESULT_SUCCESS;

    const uintptr_t idx = column - data->columns;
    if (idx < data->column_count)
    {
        *p_idx = idx;
    }
    else
    {
        res = JIO_RESULT_BAD_PTR;
    }

    return res;
}

jio_result jio_process_csv_exact(
        const jio_context* ctx, const jio_memory_file* mem_file, const char* separator, uint32_t column_count,
        const jio_string_segment* headers, bool (** converter_array)(jio_string_segment*, void*), void** param_array)
{
    {
        bool converters_complete = true;
        for (uint32_t i = 0; i < column_count; ++i)
        {
            if (converter_array[i] == NULL)
            {
                converters_complete = false;
                JIO_ERROR(ctx, "Converter for column %u (%.*s) was not provided", i, (int)headers[i].len, headers[i].begin);
            }
        }
        if (!converters_complete)
        {
            return JIO_RESULT_BAD_CONVERTER;
        }
    }
    jio_result res;

    //  Parse the first row
    const char* row_begin = mem_file->ptr;
    const char* row_end = strchr(row_begin, '\n');
    jio_string_segment* segments = NULL;
    //  Count columns in the csv file
    size_t sep_len = strlen(separator);
    {
        const uint32_t real_column_count = count_row_entries(
                row_begin, row_end ? row_end : (const char*)mem_file->ptr + mem_file->file_size, separator, sep_len);
        if (real_column_count != column_count)
        {
            JIO_ERROR(ctx, "Csv file has %"PRIu32" columns, but %"PRIu32" were specified", real_column_count, column_count);
            res = JIO_RESULT_BAD_CSV_HEADER;
            goto end;
        }
    }
    segments = jio_alloc_stack(ctx, sizeof(*segments) * column_count);
    if (!segments)
    {
        res = JIO_RESULT_BAD_ALLOC;
        JIO_ERROR(ctx, "Could not allocate memory for csv parsing");
        goto end;
    }
    if ((res = extract_row_entries(NULL, column_count, row_begin, row_end, separator, sep_len, true, segments)) != JIO_RESULT_SUCCESS)
    {
        JIO_ERROR(ctx, "Failed extracting the headers from CSV file \"%s\", reason: %s", mem_file->name,
                  jio_result_to_str(res));
        goto end;
    }
    for (uint32_t i = 0; i < column_count; ++i)
    {
        if (!jio_string_segment_equal(segments + i, headers + i))
        {
            JIO_ERROR(ctx, "Column %u had a header \"%.*s\", but \"%.*s\" was expected", i + 1, (int)segments[i].len, segments[i].begin, (int)headers[i].len, headers[i].begin);
            goto end;
        }
    }

    uint32_t row_count = 0;
    while (row_end)
    {
        if ((res = extract_row_entries(NULL, column_count, row_begin, row_end, separator, sep_len, true, segments)))
        {
            JIO_ERROR(ctx, "Failed parsing row %u of CSV file \"%s\", reason: %s", row_count + 1, mem_file->name, jio_result_to_str(res));
            goto end;
        }
        for (uint32_t i = 0; row_count && i < column_count; ++i)
        {
            if (!converter_array[i](segments + i, param_array[i]))
            {
                JIO_ERROR(ctx, "Element %"PRIu32" in row %"PRIu32" could not be converted", i + 1, row_count + 1);
                res = JIO_RESULT_BAD_VALUE;
                goto end;
            }
        }

        row_count += 1;
        //  Move to the next row
        row_begin = row_end + 1;
        row_end = strchr(row_end + 1, '\n');
    }

end:
    jio_free_stack(ctx, segments);
    return res;
}

jio_result jio_csv_print_size(
        const jio_csv_data* const data, size_t* const p_size, const size_t separator_length, const uint32_t extra_padding, const bool same_width)
{
    jio_result res = JIO_RESULT_SUCCESS;


    size_t total_chars = 0;
    if (!same_width)
    {
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            const jio_csv_column* const column = data->columns + i;
            total_chars += column->header.len;

            for (uint32_t j = 0; j < column->count; ++j)
            {
                total_chars += column->elements[j].len;
            }
        }

        //  characters needed to pad entries
        total_chars += (extra_padding) * (data->column_length + 1) * data->column_count;
    }
    else
    {
        // Find the maximum width of any entry
        uint32_t width = 0;
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            const jio_csv_column* const column = data->columns + i;
            if (column->header.len > width)
            {
                width = column->header.len;
            }
            for (uint32_t j = 0; j < column->count; ++j)
            {
                if (column->elements[j].len > width)
                {
                    width = column->elements[j].len;
                }
            }
        }
        //  characters needed to fit entries
        total_chars = (width + extra_padding) * (data->column_length + 1) * data->column_count;
    }
    //  characters needed for separators
    total_chars += (data->column_count - 1) * (data->column_length + 1) * separator_length;
    //  Characters needed for new line characters
    total_chars += (data->column_length);

    *p_size = total_chars + 1;

    return res;
}

static size_t print_entry(char* restrict buffer, const char* restrict src, size_t n, uint32_t extra_padding, uint32_t min_width, bool align_left)
{
    uint32_t same_pad = 0;
    if (n < min_width)
    {
        same_pad = min_width - n;
    }
    if (align_left)
    {
        memcpy(buffer, src, n);
        memset(buffer + n, ' ', same_pad + extra_padding);
    }
    else
    {
        memset(buffer, ' ', same_pad + extra_padding);
        memcpy(buffer + same_pad + extra_padding, src, n);
    }
    return same_pad + extra_padding + n;
}

jio_result jio_csv_print(
        const jio_csv_data* data, size_t* p_usage, char* restrict buffer, const char* separator, uint32_t extra_padding, bool same_width,
        bool align_left)
{
    jio_result res = JIO_RESULT_SUCCESS;
    size_t min_width = 0;
    if (same_width)
    {
        for (uint32_t i = 0; i < data->column_count; ++i)
        {
            const jio_csv_column* const column = data->columns + i;
            if (column->header.len > min_width)
            {
                min_width = column->header.len;
            }
            for (uint32_t j = 0; j < column->count; ++j)
            {
                if (column->elements[j].len > min_width)
                {
                    min_width = column->elements[j].len;
                }
            }
        }
    }

    const size_t sep_len = strlen(separator);
    //  Print headers
    char* pos = buffer;
    pos += print_entry(pos, data->columns[0].header.begin, data->columns[0].header.len, extra_padding, min_width, align_left);
    for (uint32_t i = 1; i < data->column_count; ++i)
    {
        memcpy(pos, separator, sep_len); pos += sep_len;
        pos += print_entry(pos, data->columns[i].header.begin, data->columns[i].header.len, extra_padding, min_width, align_left);
    }
    *pos = '\n'; ++pos;

    //  Now to print all entries to the buffer
    for (uint32_t i = 0; i < data->column_length; ++i)
    {
        pos += print_entry(pos, data->columns[0].elements[i].begin, data->columns[0].elements[i].len, extra_padding, min_width, align_left);
        for (uint32_t j = 1; j < data->column_count; ++j)
        {
            const jio_string_segment segment = data->columns[j].elements[i];
            memcpy(pos, separator, sep_len); pos += sep_len;
            pos += print_entry(pos, segment.begin, segment.len, extra_padding, min_width, align_left);
        }
        *pos = '\n'; ++pos;
    }
    *pos = 0;

    if (p_usage)
    {
        *p_usage = pos - buffer;
    }

    return res;
}
