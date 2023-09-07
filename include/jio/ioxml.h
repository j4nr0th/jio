//
// Created by jan on 4.6.2023.
//

#ifndef JIO_PARSING_BASE_H
#define JIO_PARSING_BASE_H
#include <stdio.h>
#include "iobase.h"

typedef struct jio_xml_element_struct jio_xml_element;
struct jio_xml_element_struct
{
    unsigned depth;
    jio_string_segment name;
    unsigned attrib_capacity;
    unsigned attrib_count;
    jio_string_segment* attribute_names;
    jio_string_segment* attribute_values;
    unsigned child_count;
    unsigned child_capacity;
    jio_xml_element* children;
    jio_string_segment value;
};

void jio_xml_release(const jio_context* ctx, jio_xml_element* root);

jio_result jio_xml_parse(const jio_context* ctx, const jio_memory_file* mem_file, jio_xml_element** p_root);

jio_result jio_serialize_xml(jio_xml_element* root, FILE* f_out);




#endif //JIO_PARSING_BASE_H
