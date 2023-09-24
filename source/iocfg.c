#include "../include/jio/iocfg.h"
#include "internal.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

struct jio_cfg_section_T
{
    jio_string_segment name;
    unsigned value_count;
    unsigned value_capacity;
    jio_cfg_element* value_array;
    unsigned subsection_count;
    unsigned subsection_capacity;
    jio_cfg_section** subsection_array;
};

static void destroy_array(const jio_context* ctx, jio_cfg_array* array)
{
    for (uint32_t i = 0; i < array->count; ++i)
    {
        if (array->values[i].type == JIO_CFG_TYPE_ARRAY)
        {
            destroy_array(ctx, &array->values[i].value.value_array);
        }
    }
    jio_free(ctx, array->values);
}


jio_result jio_cfg_section_create(const jio_context* ctx, jio_string_segment name, jio_cfg_section** pp_out)
{
    jio_cfg_section* section = jio_alloc(ctx, sizeof(*section));
    if (!section)
    {
        JIO_ERROR(ctx, "Could not allocate memory for section");
        return JIO_RESULT_BAD_ALLOC;
    }

    section->name = name;
    section->value_count = 0;
    section->value_capacity = 8;
    section->value_array = jio_alloc(ctx, sizeof(*section->value_array) * section->value_capacity);
    if (!section->value_array)
    {
        jio_free(ctx, section);
        JIO_ERROR(ctx, "Could not allocate memory for section values");
        return JIO_RESULT_BAD_ALLOC;
    }
    section->subsection_capacity = 4;
    section->subsection_count = 0;
    section->subsection_array = jio_alloc(ctx, sizeof(*section->subsection_array) * section->subsection_capacity);
    if (!section->subsection_array)
    {
        jio_free(ctx, section);
        jio_free(ctx, section->value_array);
        JIO_ERROR(ctx, "Could not allocate memory for root section subsections");
        return JIO_RESULT_BAD_ALLOC;
    }
    *pp_out = section;

    return JIO_RESULT_SUCCESS;
}

static jio_result parse_string_segment_to_cfg_element_value(const jio_context* ctx, jio_string_segment segment, jio_cfg_value* value)
{
    if (*segment.begin == '{')
    {
        //  Array
        //  skip the {
        segment.begin += 1;
        segment.len -= 1;
        jio_cfg_array array;
        array.count = 0;
        array.capacity = 8;
        array.values = jio_alloc(ctx, sizeof(*array.values) * array.capacity);
        if (!array.values)
        {
            JIO_ERROR(ctx, "Could not allocate memory for an array");
            return JIO_RESULT_BAD_ALLOC;
        }

        jio_cfg_value element_value;
        const char* val_begin = segment.begin;
        const char* val_end = val_begin;
        uint32_t level = 1;
        while (level)
        {
            if (array.capacity == array.count)
            {
                const uint32_t new_capacity = array.capacity << 1;
                jio_cfg_value* const new_ptr = jio_realloc(ctx, array.values, sizeof(*array.values) * new_capacity);
                if (!new_ptr)
                {
                    JIO_ERROR(ctx, "Could not reallocate array values array");
                    destroy_array(ctx, &array);
                    return JIO_RESULT_BAD_ALLOC;
                }
                array.values = new_ptr;
                array.capacity = new_capacity;
            }
            if (val_end - segment.begin >= (ptrdiff_t)segment.len)
            {
                JIO_ERROR(ctx, "Unmatched pair(s) of array brackets {}");
                destroy_array(ctx, &array);
                return JIO_RESULT_BAD_CFG_FORMAT;
            }
            jio_result res;
            switch (*val_end)
            {
            case ',':
                if (level == 1)
                {
                    while (jio_iswhitespace(*val_begin) && val_begin != val_end)
                    {
                        val_begin += 1;
                    }
                    res = parse_string_segment_to_cfg_element_value(ctx, (jio_string_segment){.begin = val_begin, .len = val_end - val_begin}, &element_value);
                    if (res != JIO_RESULT_SUCCESS)
                    {
                        JIO_ERROR(ctx, "Could not convert value to array element");
                        destroy_array(ctx, &array);
                        return JIO_RESULT_BAD_CFG_FORMAT;
                    }
                    array.values[array.count++] = element_value;
                    val_begin = val_end + 1;
                }
                break;
            case '}':
                level -= 1;
                break;
            case '{':
                level += 1;
                break;
            default:break;
            }

            val_end += 1;
        }
        val_end -= 1;
        while (jio_iswhitespace(*val_begin) && val_begin != val_end)
        {
            val_begin += 1;
        }
        jio_result res = parse_string_segment_to_cfg_element_value(ctx, (jio_string_segment){.begin = val_begin, .len = val_end - val_begin}, &element_value);
        if (res != JIO_RESULT_SUCCESS)
        {
            JIO_ERROR(ctx, "Could not convert value to array element");
            destroy_array(ctx, &array);
            return JIO_RESULT_BAD_CFG_FORMAT;
        }
        array.values[array.count++] = element_value;

        value->type = JIO_CFG_TYPE_ARRAY;
        value->value.value_array = array;
    }
    else if (*segment.begin == '\"' || *segment.begin == '\'')
    {
        //  Quoted string literal
        const char q_type = *segment.begin;
        segment.begin += 1;
        segment.len -= 1;
        //  Search for next occurrence of the quote
        const char* end_q = memchr(segment.begin, q_type, segment.len);
        if (!end_q)
        {
            JIO_ERROR(ctx, "Quoted string had unmatched pair of quotes");
            return JIO_RESULT_BAD_CFG_FORMAT;
        }
        segment.len = end_q - segment.begin;
        value->type = JIO_CFG_TYPE_STRING;
        value->value.value_string = segment;
    }
    else
    {
        //  Not an array
        //      check if bool
        if (segment.len >= 4 && memcmp(segment.begin, "true", 4) == 0)
        {
            if (segment.len == 4 || (segment.begin[4] == '#' || segment.begin[4] == ';' || jio_iswhitespace(segment.begin[4]) || segment.begin[4] == ','))
            {
                value->type = JIO_CFG_TYPE_BOOLEAN;
                value->value.value_boolean = 1;
                goto end;
            }
        }
        else if (segment.len >= 5 && memcmp(segment.begin, "false", 5) == 0)
        {
            if (segment.len == 5 || (segment.begin[5] == '#' || segment.begin[5] == ';'|| jio_iswhitespace(segment.begin[5]) || segment.begin[5] == ','))
            {
                value->type = JIO_CFG_TYPE_BOOLEAN;
                value->value.value_boolean = 0;
                goto end;
            }
        }
        else
        {
            char* end;
            //  Check if int
            intmax_t v_int = strtoimax(segment.begin, &end, 0);
            if (end <= segment.begin + segment.len && end != segment.begin)
            {
                if (end == segment.begin + segment.len)
                {
                    value->type = JIO_CFG_TYPE_INT;
                    value->value.value_int = v_int;
                    goto end;
                }
            }
            //      check if real (double)
            double v_real = strtod(segment.begin, &end);
            if (end <= segment.begin + segment.len && end != segment.begin)
            {
                if (end == segment.begin + segment.len)
                {
                    value->type = JIO_CFG_TYPE_REAL;
                    value->value.value_real = v_real;
                    goto end;
                }
            }
            // it is a string
            value->type = JIO_CFG_TYPE_STRING;
            //  Remove any extra comments
            const char* new_end = memchr(segment.begin, ';', segment.len);
            if (!new_end)
            {
                new_end = memchr(segment.begin, '#', segment.len);
                if (!new_end)
                {
                    new_end = segment.begin + segment.len;
                }
            }
            while(jio_iswhitespace(*(new_end - 1)) && new_end != segment.begin)
            {
                new_end -= 1;
            }
            segment.len = new_end - segment.begin;
            value->value.value_string = segment;
        }
    }

end:
    return JIO_RESULT_SUCCESS;
}

jio_result jio_cfg_parse(const jio_context* ctx, const jio_memory_file* mem_file, jio_cfg_section** pp_root_section)
{
    jio_result res;

    //  First prepare the root section
    jio_cfg_section* root;
    res = jio_cfg_section_create(ctx, (jio_string_segment){.begin = NULL, .len = 0}, &root);
    if (res != JIO_RESULT_SUCCESS)
    {
        JIO_ERROR(ctx, "Could not create root section");
        goto failed;
    }

    jio_cfg_section* section = root;
    //  Begin parsing line by line
    unsigned line_count = 1;
    const char* row_begin = mem_file->ptr;
    for (;;)
    {
        const char* row_end = strchr(row_begin, '\n');
        if (!row_end)
        {
            row_end = strchr(row_begin, 0);
        }
        if (row_end == row_begin)
        {
            //  Check if there are any other lines
            if (*row_end == 0)
            {
                break;
            }
            //  Or empty line

            row_begin = row_end + 1;
            continue;
        }

        //  Trim whitespace
        while (jio_iswhitespace(*row_begin) && row_begin != row_end) { ++row_begin; }
        if (*row_begin == '#' || *row_begin == ';')
        {
            //  The line is a comment
            row_begin = *row_end == 0 ? row_end : row_end + 1;
            line_count += 1;
            continue;
        }

        while (jio_iswhitespace(*(row_end - 1)) && row_begin != row_end) { --row_end; }
        if (*row_begin == '[')
        {
            //  new subsection (potentially)
            row_begin += 1;
            const char* name_end = memchr(row_begin, ']', row_end - row_begin);
            if (!name_end)
            {
                JIO_ERROR(ctx, "Line %u begins with '[', which denotes a (sub-)section definition, but does not include the closing ']'", line_count);
                res = JIO_RESULT_BAD_CFG_SECTION_NAME;
                goto failed;
            }
            if (row_begin == name_end)
            {
                JIO_ERROR(ctx, "Line %u begins with '[', which denotes a (sub-)section definition, but has an empy section name", line_count);
                res = JIO_RESULT_BAD_CFG_SECTION_NAME;
                goto failed;
            }
            jio_string_segment name = {.begin = row_begin, .len = name_end - row_begin};
            if (*row_begin != '.')
            {
                 section = root;
            }
            else
            {
                row_begin += 1;
                name.begin += 1;
                name.len -= 1;
            }
            for (;;)
            {
                jio_string_segment sub_name = {.begin = name.begin};
                const char* sub_pos = memchr(name.begin, '.', name.len);
                bool end;

                if (sub_pos)
                {
                    sub_name.len = sub_pos - name.begin;
                    name.len = name.len - 1 - sub_name.len;
                    name.begin = sub_pos + 1;
                    end = false;
                }
                else
                {
                    sub_name = name;
                    end = true;
                }
                jio_cfg_section* subsection = NULL;
                jio_result search_res = jio_cfg_get_subsection_segment(section, sub_name, &subsection);
                if (search_res == JIO_RESULT_SUCCESS)
                {
                    //  Subsection already exists
                    assert(subsection);
                    section = subsection;
                }
                else if (search_res == JIO_RESULT_BAD_CFG_SECTION_NAME)
                {
                    //  Subsection does not exist already
                    res = jio_cfg_section_create(ctx, sub_name, &subsection);
                    if (res != JIO_RESULT_SUCCESS)
                    {
                        JIO_ERROR(ctx, "Could not create subsection, reason: %s", jio_result_to_str(res));
                        goto failed;
                    }
                    res = jio_cfg_section_insert(ctx, section, subsection);
                    if (res != JIO_RESULT_SUCCESS)
                    {
                        JIO_ERROR(ctx, "Could not insert subsection, reason: %s", jio_result_to_str(res));
                        goto failed;
                    }
                    section = subsection;
                }
                else
                {
                    //  Something else went wrong
                    JIO_ERROR(ctx, "Could not create subsection \"%.*s\", reason: %s", (int)(row_end - row_begin), row_begin,
                              jio_result_to_str(res));
                    goto failed;
                }
                if (end)
                {
                    break;
                }
            }
            //  Check what is after the name_end
            row_begin = name_end + 1;
            while (*row_begin != '\n' && jio_iswhitespace(*row_begin) && row_begin != row_end){ ++row_begin; }
#ifndef _WIN32
            if (*row_begin != '\n' && *row_begin != '#' && *row_begin != ';')
#else
            if ((*row_begin != '\n' && *row_begin != '\r') && *row_begin != '#' && *row_begin != ';')
#endif
            {
                JIO_ERROR(ctx, "Line %u contains a section name, but also contains other non-comment contents", line_count);
                res = JIO_RESULT_BAD_CFG_FORMAT;
                goto failed;
            }
        }
        else
        {
            //  Line contains a value
            const char* key_end = row_begin + 1;
            while (*key_end != '=' && *key_end != ':')
            {
                key_end += 1;
                if (key_end == row_end)
                {
                    JIO_ERROR(ctx, "Line %u does not contain neither a (sub-)section declaration, nor a key-value pair", line_count);
                    res = JIO_RESULT_BAD_CFG_FORMAT;
                    goto failed;
                }
            }
            const char* value_begin = key_end + 1;
            while (jio_iswhitespace(*(key_end - 1)) && key_end != row_begin)
            {
                key_end -= 1;
            }
            jio_string_segment key_name = {.begin = row_begin, .len = key_end - row_begin};
            while (jio_iswhitespace(*value_begin))
            {
                value_begin += 1;
                if (value_begin == row_end)
                {
                    JIO_ERROR(ctx, "Line %u contains a key and delimiter, but no value", line_count);
                    res = JIO_RESULT_BAD_CFG_FORMAT;
                    goto failed;
                }
            }
            jio_string_segment value_segment = {.begin = value_begin,  row_end - value_begin};
            jio_cfg_value val;
            res = parse_string_segment_to_cfg_element_value(ctx, value_segment, &val);
            if (res != JIO_RESULT_SUCCESS)
            {
                JIO_ERROR(ctx, "Could not convert \"%.*s\" to valid value", (int)value_segment.len, value_segment.begin);
                goto failed;
            }
            res = jio_cfg_element_insert(ctx, section, (jio_cfg_element) { .key = key_name, .value = val });
            if (res != JIO_RESULT_SUCCESS)
            {
                JIO_ERROR(ctx, "Could not insert key-element pair into subsection");
                if (val.type == JIO_CFG_TYPE_ARRAY)
                {
                    destroy_array(ctx, &val.value.value_array);
                }
                goto failed;
            }
        }


        row_begin = row_end + 1;
        line_count += 1;
    }



    *pp_root_section = root;
    return JIO_RESULT_SUCCESS;

failed:
    jio_cfg_section_destroy(ctx, root, 1);
    return res;
}

void jio_cfg_section_destroy(const jio_context* ctx, jio_cfg_section* section, bool free_contents)
{
    for (uint32_t i = 0; i < section->value_count; ++i)
    {
        if (section->value_array[i].value.type == JIO_CFG_TYPE_ARRAY && free_contents)
        {
            destroy_array(ctx, &section->value_array[i].value.value.value_array);
        }
    }
    jio_free(ctx, section->value_array);
    section->value_array = (void*)-1;
    for (uint32_t i = 0; i < section->subsection_count; ++i)
    {
        jio_cfg_section_destroy(ctx, section->subsection_array[i], free_contents);
    }
    jio_free(ctx, section->subsection_array);
    section->subsection_array = (void*)-1;
    jio_free(ctx, section);
}

jio_result jio_cfg_section_insert(const jio_context* ctx, jio_cfg_section* parent, jio_cfg_section* child)
{
    if (parent->subsection_count == parent->subsection_capacity)
    {
        const uint32_t new_capacity = parent->subsection_capacity << 1;
        jio_cfg_section** const new_ptr = jio_realloc(ctx, parent->subsection_array, sizeof(*parent->subsection_array) * new_capacity);
        if (!new_ptr)
        {
            JIO_ERROR(ctx, "Could not reallocate memory for parent's subsection array");
            return JIO_RESULT_BAD_ALLOC;
        }
        parent->subsection_capacity = new_capacity;
        parent->subsection_array = new_ptr;
    }
    parent->subsection_array[parent->subsection_count++] = child;

    return JIO_RESULT_SUCCESS;
}

jio_result jio_cfg_get_value_by_key(const jio_cfg_section* section, const char* key, jio_cfg_value* p_value)
{
    for (unsigned i = 0; i < section->value_count; ++i)
    {
        if (jio_string_segment_equal_str(&section->value_array[i].key, key))
        {
            *p_value = section->value_array[i].value;
            return JIO_RESULT_SUCCESS;
        }
    }

    return JIO_RESULT_BAD_CFG_KEY;
}

jio_result
jio_cfg_get_value_by_key_segment(const jio_cfg_section* section, jio_string_segment key, jio_cfg_value* p_value)
{
    for (unsigned i = 0; i < section->value_count; ++i)
    {
        if (jio_string_segment_equal(&section->value_array[i].key, &key))
        {
            *p_value = section->value_array[i].value;
            return JIO_RESULT_SUCCESS;
        }
    }

    return JIO_RESULT_BAD_CFG_KEY;

}

jio_result jio_cfg_get_subsection(const jio_cfg_section* section, const char* subsection_name, jio_cfg_section** pp_out)
{
    for (unsigned i = 0; i < section->subsection_count; ++i)
    {
        if (jio_string_segment_equal_str_case(&section->subsection_array[i]->name, subsection_name))
        {
            *pp_out = section->subsection_array[i];
            return JIO_RESULT_SUCCESS;
        }
    }

    return JIO_RESULT_BAD_CFG_SECTION_NAME;
}

jio_result jio_cfg_get_subsection_segment(
        const jio_cfg_section* section, jio_string_segment subsection_name, jio_cfg_section** pp_out)
{
    for (unsigned i = 0; i < section->subsection_count; ++i)
    {
        if (jio_string_segment_equal_case(&section->subsection_array[i]->name, &subsection_name))
        {
            *pp_out = section->subsection_array[i];
            return JIO_RESULT_SUCCESS;
        }
    }

    return JIO_RESULT_BAD_CFG_SECTION_NAME;

}

jio_result jio_cfg_element_insert(const jio_context* ctx, jio_cfg_section* section, jio_cfg_element element)
{
    if (section->value_capacity == section->value_count)
    {
        const uint32_t new_capacity = section->value_capacity << 1;
        jio_cfg_element* const new_ptr = jio_realloc(ctx, section->value_array, sizeof(*section->value_array) * new_capacity);
        if (!new_ptr)
        {
            JIO_ERROR(ctx, "Could not allocate memory for section's value array");
            return JIO_RESULT_BAD_ALLOC;
        }
        section->value_array = new_ptr;
        section->value_capacity = new_capacity;
    }
    section->value_array[section->value_count++] = element;

    return JIO_RESULT_SUCCESS;
}

static size_t print_value(char* const restrict base, const jio_cfg_value* const restrict value)
{
    char* pos = base;
    switch (value->type)
    {
    case JIO_CFG_TYPE_INT:
        pos += sprintf(pos, "%"PRIdMAX"", value->value.value_int);
        break;
    case JIO_CFG_TYPE_REAL:
        pos += sprintf(pos, "%g", value->value.value_real);
        break;
    case JIO_CFG_TYPE_BOOLEAN:
        if (value->value.value_boolean)
        {
            //  True
            memcpy(pos, "true", 4); pos += 4;
        }
        else
        {
            //  False
            memcpy(pos, "false", 5); pos += 5;
        }
        break;
    case JIO_CFG_TYPE_STRING:
        if (*(value->value.value_string.begin - 1) == '"' || *(value->value.value_string.begin - 1) == '\'')
        {
            *pos = '\"'; ++pos;
            memcpy(pos, value->value.value_string.begin, value->value.value_string.len); pos += value->value.value_string.len;
            *pos = '\"'; ++pos;
        }
        else
        {
            memcpy(pos, value->value.value_string.begin, value->value.value_string.len); pos += value->value.value_string.len;
        }
        break;
    case JIO_CFG_TYPE_ARRAY:
        *pos = '{'; ++pos;
        *pos = ' '; ++pos;
        {
            const jio_cfg_array* const array = &value->value.value_array;
            if (array->count)
            {
                //  Print first element
                pos += print_value(pos, array->values + 0);
            }
            for (uint32_t i = 1; i < array->count; ++i)
            {
                //  Print the following elements
                *pos = ','; ++pos;
                *pos = ' '; ++pos;
                pos += print_value(pos, array->values + i);
            }
        }
        *pos = ' '; ++pos;
        *pos = '}'; ++pos;
        break;
    default:break;
    }

    return pos - base;
}

static size_t print_section(
        const jio_string_segment parent_name, char* const buffer, const uint32_t level, const jio_cfg_section* section,
        const char* const delimiter, const size_t delim_len, const bool equalize_key_length_pad, const bool pad_left, const bool indent_subsections)
{
    const size_t pad_space = level * 4;
    char* pos = buffer;
    jio_string_segment section_name = {.begin = NULL, .len = 0};
    if (level > 0)
    {
        //  Print section name
        if (indent_subsections)
        {
            memset(pos, ' ', (level - 1) * 4); pos += (level - 1) * 4;
        }
        *pos = '['; ++pos;
        section_name.begin = pos;

        if (level > 1)
        {
            memcpy(pos, parent_name.begin, parent_name.len);
            pos += parent_name.len;
            *pos = '.'; ++pos;
        }

        memcpy(pos, section->name.begin, section->name.len);
        pos += section->name.len;
        section_name.len = pos - section_name.begin;
        *pos = ']'; ++pos;
        *pos = '\n'; ++pos;
    }
    size_t min_width = 0;
    if (equalize_key_length_pad)
    {
        for (uint32_t i = 0; i < section->value_count; ++i)
        {
            const jio_cfg_element* const restrict element = section->value_array + i;
            if (element->key.len > min_width)
            {
                min_width = element->key.len;
            }
        }
    }
    //  Print key-value pairs
    for (uint32_t i = 0; i < section->value_count; ++i)
    {
        memset(pos, ' ', pad_space); pos += pad_space;
        const jio_cfg_element* const restrict element = section->value_array + i;
        //  Print key
        if (min_width > element->key.len)
        {
            if (pad_left)
            {
                memset(pos, ' ', min_width - element->key.len); pos += min_width - element->key.len;
                memcpy(pos, element->key.begin, element->key.len); pos += element->key.len;
            }
            else
            {
                memcpy(pos, element->key.begin, element->key.len); pos += element->key.len;
                memset(pos, ' ', min_width - element->key.len); pos += min_width - element->key.len;
            }
        }
        else
        {
            memcpy(pos, element->key.begin, element->key.len); pos += element->key.len;
        }
        //  Delimiter
        *pos = ' '; ++pos;
        memcpy(pos, delimiter, delim_len); pos += delim_len;
        *pos = ' '; ++pos;
        //  Print value
        const jio_cfg_value* restrict const value = &element->value;
        pos += print_value(pos, value);
        //  New line
        *pos = '\n'; ++pos;
    }

    //  Print the subsections
    for (uint32_t i = 0; i < section->subsection_count; ++i)
    {
        pos += print_section(
                section_name, pos, level + 1, section->subsection_array[i], delimiter, delim_len,
                equalize_key_length_pad, pad_left, indent_subsections);
    }

    return pos - buffer;
}

size_t jio_cfg_print(
        const jio_cfg_section* section, char* const buffer, const char* const delimiter, const bool indent_subsections,
        const bool equalize_key_length_pad, const bool pad_left)
{

    const size_t delim_len = strlen(delimiter);
    const size_t used = print_section((jio_string_segment){.begin = NULL, .len = 0}, buffer, 0, section, delimiter, delim_len, equalize_key_length_pad, pad_left, indent_subsections);

    return used;
}

static size_t size_value(const jio_cfg_value* const restrict value)
{
    size_t pos = 0;
    switch (value->type)
    {
    case JIO_CFG_TYPE_INT:
        pos += snprintf(NULL, 0, "%"PRIdMAX"", value->value.value_int);
        break;
    case JIO_CFG_TYPE_REAL:
        pos += snprintf(NULL, 0, "%g", value->value.value_real);
        break;
    case JIO_CFG_TYPE_BOOLEAN:
        if (value->value.value_boolean)
        {
            //  True
            pos += 4;
        }
        else
        {
            //  False
            pos += 5;
        }
        break;
    case JIO_CFG_TYPE_STRING:
        if (*(value->value.value_string.begin - 1) == '"' || *(value->value.value_string.begin - 1) == '\'')
        {
            ++pos;
            ++pos;
        }
        pos += value->value.value_string.len;
        break;
    case JIO_CFG_TYPE_ARRAY:
        ++pos;
        ++pos;
        {
            const jio_cfg_array* const array = &value->value.value_array;
            if (array->count)
            {
                //  Print first element
                pos += size_value(array->values + 0);
            }
            for (uint32_t i = 1; i < array->count; ++i)
            {
                //  Print the following elements
                ++pos;
                ++pos;
                pos += size_value(array->values + i);
            }
        }
        ++pos;
        ++pos;
        break;
    default:break;
    }

    return pos;
}

static size_t size_section(const jio_string_segment parent_name, const uint32_t level, const jio_cfg_section* section, const size_t delim_len, const bool equalize_key_length_pad, const bool indent_subsections)
{
    const size_t pad_space = level * 4;
    size_t pos = 0;
    jio_string_segment section_name = {.begin = NULL, .len = 0};
    if (level > 0)
    {
        //  Print section name
        if (indent_subsections)
        {
            pos += (level - 1) * 4;
        }
        ++pos;
        section_name.begin = (const char*)0xB16B00B5BABE5;
        section_name.len = pos;

        if (level > 1)
        {
            pos += parent_name.len;
            ++pos;
        }

        pos += section->name.len;
        section_name.len = pos - section_name.len;
        ++pos;
        ++pos;
    }
    size_t min_width = 0;
    if (equalize_key_length_pad)
    {
        for (unsigned i = 0; i < section->value_count; ++i)
        {
            const jio_cfg_element* const restrict element = section->value_array + i;
            if (element->key.len > min_width)
            {
                min_width = element->key.len;
            }
        }
    }
    //  Print key-value pairs
    for (unsigned i = 0; i < section->value_count; ++i)
    {
        pos += pad_space;
        const jio_cfg_element* const restrict element = section->value_array + i;
        //  Print key
        if (min_width > element->key.len)
        {
            pos += min_width - element->key.len;
        }
        pos += element->key.len;
        //  Delimiter
        ++pos;
        pos += delim_len;
        ++pos;
        //  Print value
        const jio_cfg_value* restrict const value = &element->value;
        pos += size_value(value);
        //  New line
        ++pos;
    }

    //  Print the subsections
    for (unsigned i = 0; i < section->subsection_count; ++i)
    {
        pos += size_section(section_name, level + 1, section->subsection_array[i], delim_len,
                equalize_key_length_pad, indent_subsections);
    }

    return pos;
}
size_t jio_cfg_print_size(
        const jio_cfg_section* const section, const size_t delim_size, const bool indent_subsections,
        const bool equalize_key_length_pad)
{
    const size_t used = size_section((jio_string_segment){.begin = NULL, .len = 0},  0, section, delim_size, equalize_key_length_pad, indent_subsections);

    return used;
}

static const char* const type_names_array[JIO_CFG_TYPE_COUNT] =
        {
            [JIO_CFG_TYPE_NONE] = "Invalid",
            [JIO_CFG_TYPE_BOOLEAN] = "Boolean",
            [JIO_CFG_TYPE_INT] = "Integer",
            [JIO_CFG_TYPE_REAL] = "Real",
            [JIO_CFG_TYPE_STRING] = "String",
            [JIO_CFG_TYPE_ARRAY] = "Array",
        };

const char* jio_cfg_type_to_str(jio_cfg_type type)
{
    if (type >= JIO_CFG_TYPE_NONE && type < JIO_CFG_TYPE_COUNT)
    {
        return type_names_array[type];
    }
    return "Unknown";
}

jio_string_segment jio_cfg_section_get_name(const jio_cfg_section* section)
{
    return section->name;
}
