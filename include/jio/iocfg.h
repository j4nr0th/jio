//
// Created by jan on 4.6.2023.
//

#ifndef JIO_INI_PARSING_H
#define JIO_INI_PARSING_H
#include "iobase.h"

enum jio_cfg_type_enum
{
    JIO_CFG_TYPE_NONE,

    JIO_CFG_TYPE_BOOLEAN,
    JIO_CFG_TYPE_INT,
    JIO_CFG_TYPE_REAL,
    JIO_CFG_TYPE_STRING,
    JIO_CFG_TYPE_ARRAY,

    JIO_CFG_TYPE_COUNT,
};
typedef enum jio_cfg_type_enum jio_cfg_type;

typedef struct jio_cfg_value_struct jio_cfg_value;
typedef struct jio_cfg_array_struct jio_cfg_array;
struct jio_cfg_array_struct
{
    unsigned capacity;
    unsigned count;
    jio_cfg_value* values;
};

struct jio_cfg_value_struct
{
    jio_cfg_type type;
    union
    {
        bool value_boolean;
        intmax_t value_int;
        double value_real;
        jio_string_segment value_string;
        jio_cfg_array value_array;
    } value;
};

typedef struct jio_cfg_element_struct jio_cfg_element;
struct jio_cfg_element_struct
{
    jio_string_segment key;
    jio_cfg_value value;
};

typedef struct jio_cfg_section_struct jio_cfg_section;

jio_result jio_cfg_section_insert(const jio_context* ctx, jio_cfg_section* parent, jio_cfg_section* child);

jio_result jio_cfg_element_insert(const jio_context* ctx, jio_cfg_section* section, jio_cfg_element element);

jio_result jio_cfg_section_create(const jio_context* ctx, jio_string_segment name, jio_cfg_section** pp_out);

void jio_cfg_section_destroy(const jio_context* ctx, jio_cfg_section* section);

jio_result jio_cfg_parse(const jio_context* ctx, const jio_memory_file* mem_file, jio_cfg_section** pp_root_section);

jio_result jio_cfg_get_value_by_key(const jio_cfg_section* section, const char* key, jio_cfg_value* p_value);

jio_result jio_cfg_get_value_by_key_segment(const jio_cfg_section* section, jio_string_segment key, jio_cfg_value* p_value);

jio_result jio_cfg_get_subsection(const jio_cfg_section* section, const char* subsection_name, jio_cfg_section** pp_out);

jio_result jio_cfg_get_subsection_segment(
        const jio_cfg_section* section, jio_string_segment subsection_name, jio_cfg_section** pp_out);

size_t jio_cfg_print_size(
        const jio_cfg_section* section, size_t delim_size, bool indent_subsections,
        bool equalize_key_length_pad);

size_t
jio_cfg_print(
        const jio_cfg_section* section, char* buffer, const char* delimiter, bool indent_subsections,
        bool equalize_key_length_pad, bool pad_left);

const char* jio_cfg_type_to_str(jio_cfg_type type);

#endif //JIO_INI_PARSING_H
