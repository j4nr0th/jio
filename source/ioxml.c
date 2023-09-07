//
// Created by jan on 4.6.2023.
//
#include "../include/jio/ioxml.h"
#include "internal.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>


#define COMPARE_STRING_SEGMENT_TO_LITERAL(literal, xml) compare_jio_jio_string_segment(sizeof(#literal) - 1, #literal, (xml))
#define COMPARE_CASE_STRING_SEGMENT_TO_LITERAL(literal, xml) compare_case_jio_jio_string_segment(sizeof(#literal) - 1, #literal, (xml))

static const char WHITESPACE[] = {
        0x20,   //  this is ' '
        0x9,    //  this is '\t'
        0xD,    //  this is '\r'
        0xA     //  this is '\n'
};
static bool is_whitespace(unsigned char c)
{
    return !(c != WHITESPACE[0] && c != WHITESPACE[1] && c != WHITESPACE[2] && c != WHITESPACE[3]);
}


#define IS_IN_RANGE(v, btm, top) ((v) >= (btm) && (v) <= (top))

static inline bool is_name_start_char(unsigned c)
{
    if (c == ':')
        return true;
    if (IS_IN_RANGE(c, 'A', 'Z'))
        return true;
    if (c == '_')
        return true;
    if (IS_IN_RANGE(c, 'a', 'z'))
        return true;
    if (IS_IN_RANGE(c, 0xC0, 0xD6))
        return true;
    if (IS_IN_RANGE(c, 0xD8, 0xF6))
        return  true;
    if (IS_IN_RANGE(c, 0xF8, 0x2FF))
        return true;
    if (IS_IN_RANGE(c, 0x370, 0x37D))
        return true;
    if (IS_IN_RANGE(c, 0x37F, 0x1FFF))
        return true;
    if (IS_IN_RANGE(c, 0x200C, 0x200D))
        return true;
    if (IS_IN_RANGE(c, 0x2070, 0x218F))
        return true;
    if (IS_IN_RANGE(c, 0x2C00, 0x2FEF))
        return true;
    if (IS_IN_RANGE(c, 0x3001, 0xD7FF))
        return true;
    if (IS_IN_RANGE(c, 0xF900, 0xFDCF))
        return true;
    if (IS_IN_RANGE(c, 0xFDF0, 0xFFFD))
        return true;
    if (IS_IN_RANGE(c, 0x10000, 0xEFFFF))
        return true;

    return false;
}

static inline bool is_name_char(unsigned c)
{
    if (is_name_start_char(c))
        return true;
    if (c == '-')
        return true;
    if (c == '.')
        return true;
    if (IS_IN_RANGE(c, '0', '9'))
        return true;
    if (c == 0xB7)
        return true;
    if (IS_IN_RANGE(c, 0x0300, 0x036F))
        return true;
    if (IS_IN_RANGE(c, 0x203F, 0x2040))
        return true;

    return false;
}

static inline bool parse_utf8_to_utf32(uint64_t max_chars, const uint8_t* ptr, uint64_t* p_length, unsigned* p_char)
{
    assert(max_chars);
    unsigned char c = *ptr;
    if (!c)
    {
        return false;
    }
    unsigned cp = 0;
    uint32_t len;
    if (c < 0x80)
    {
        //  ASCII character
        cp = c;
        len = 1;
    }
    else if ((c & 0xE0) == 0xC0 && max_chars > 1)
    {
        //  2 byte codepoint
        cp = ((c & 0x1F) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80 )
        {
            return false;
        }
        c &= 0x3F;
        cp |= c;
        len = 2;
    }
    else if ((c & 0xF0) == 0xE0 && max_chars > 2)
    {
        //  3 byte codepoint
        cp = ((c & 0x0F) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            return false;
        }
        c &= 0x3F;
        cp |= c;
        len = 3;
    }
    else if ((c & 0xF8) == 0xF0 && max_chars > 3)
    {
        //  4 byte codepoint -> at least U+10000, so don't support this
        cp = ((c & 0x07) << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x3F;
        cp |= c;
        cp = (cp << 6);
        c = *(ptr += 1);
        if ((c & 0xC0) != 0x80)
        {
            //  Invalid continuation byte mark
            return false;
        }
        c &= 0x2F;
        cp |= c;
        len = 4;
    }
    else
    {
        return false;
    }

    *p_char = cp;
    *p_length = len;
    return true;
}

static uint32_t count_new_lines(uint32_t length, const char* str)
{
    uint64_t i, c;
    for (i = 0, c = 0; i < length; ++i)
    {
        c += (str[i] == '\n');
    }
    return c;
}

static bool parse_name_from_string(const uint64_t max_len, const char* const str, jio_string_segment* out)
{
    const char* ptr = str;
    unsigned c;
    uint64_t c_len;
    if (!parse_utf8_to_utf32(max_len - (ptr - str), (const uint8_t*)ptr, &c_len, &c))
    {
        return false;
    }
    if (!is_name_start_char(c))
    {
        return false;
    }
    do
    {
        ptr += c_len;
        if (!parse_utf8_to_utf32(max_len - (ptr - str), (const uint8_t*)ptr, &c_len, &c))
        {
            return false;
        }
    } while (is_name_char(c));

    out->begin = str;
    out->len = ptr - str;

    return true;
}

static void xml_release(const jio_context* ctx, jio_xml_element* e)
{
    jio_free(ctx, e->attribute_values);
    jio_free(ctx, e->attribute_names);
    for (uint32_t i = 0; i < e->child_count; ++i)
    {
        jio_xml_release(ctx, e->children + i);
    }
    jio_free(ctx, e->children);
    memset(e, 0xCC, sizeof*e);
}

void jio_xml_release(const jio_context* ctx, jio_xml_element* root)
{
    xml_release(ctx, root);
    jio_free(ctx, root);
}


static void print_xml_element_to_file(const jio_xml_element * e, const uint32_t depth, FILE* const file)
{
    for (uint32_t i = 0; i < depth; ++i)
        putc('\t', file);
    fprintf(file, "<%.*s", (int)e->name.len, e->name.begin);
    for (uint32_t i = 0; i < e->attrib_count; ++i)
    {
        fprintf(file, " %.*s=\"%.*s\"", (int)e->attribute_names[i].len, e->attribute_names[i].begin, (int)e->attribute_values[i].len, e->attribute_values[i].begin);
    }
    putc('>', file);
    if (e->depth)
        putc('\n', file);


    if (e->value.len)
    {
        if (e->depth)
            for (uint32_t i = 0; i < depth + 1; ++i)
                putc('\t', file);
        fprintf(file, "%.*s", (int)e->value.len, e->value.begin);
        if (e->depth)
            putc('\n', file);

    }

    for (uint32_t i = 0; i < e->child_count; ++i)
    {
        print_xml_element_to_file(e->children + i, depth + 1, file);
    }

    if (e->depth)
        for (uint32_t i = 0; i < depth; ++i)
            putc('\t', file);
    fprintf(file, "</%.*s>\n", (int)e->name.len, e->name.begin);
}

jio_result jio_serialize_xml(jio_xml_element* root, FILE* f_out)
{
    fprintf(f_out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    print_xml_element_to_file(root, 0, f_out);
    return JIO_RESULT_SUCCESS;
}

jio_result jio_xml_parse(const jio_context* ctx, const jio_memory_file* mem_file, jio_xml_element** p_root)
{
    const char* const xml = mem_file->ptr;
    const uint64_t len = mem_file->file_size;
    jio_result res;
    const char* pos;
    //  Parse the xml prologue (if present)
    if ((pos = strstr(xml, "<?xml")))
    {
        pos = strstr(pos, "?>");
        if (!pos)
        {
            JIO_ERROR(ctx, "Prologue to xml file has to have a form of \"<?xml\" ... \"?>\"");
            res = JIO_RESULT_BAD_XML_FORMAT;
            goto failed;
        }
        pos += 2;
    }
    else
    {
        pos = xml;
    }
    //  Search until reaching the root element
    unsigned c;
    do
    {
        pos = strchr(pos, '<');
        if (!pos)
        {
            JIO_ERROR(ctx, "No root element was found");
            res = JIO_RESULT_BAD_XML_FORMAT;
            goto failed;
        }
        pos += 1;
        uint64_t c_len;
        if (!parse_utf8_to_utf32(len - (pos - xml), (const uint8_t*) pos, &c_len, &c))
        {
            JIO_ERROR(ctx, "Invalid character encountered on line %u", count_new_lines(pos - xml, xml));
            res = JIO_RESULT_BAD_XML_FORMAT;
            goto failed;
        }
    } while(!is_name_start_char(c));

    //  We have now arrived at the root element
    jio_xml_element* const root = jio_alloc(ctx, sizeof(*root));
    if (!root)
    {
        JIO_ERROR(ctx, "Could not allocate memory for root element");
        return JIO_RESULT_BAD_ALLOC;
    }
    //  Find the end of root's name
    if (!parse_name_from_string(len - (pos - xml), pos, &root->name))
    {
        JIO_ERROR(ctx, "Could not parse the name of the root tag");
        res = JIO_RESULT_BAD_XML_FORMAT;
        goto failed;
    }
    if (!(pos = strchr(pos, '>')))
    {
        JIO_ERROR(ctx, "Root element's start tag was not concluded");
        res = JIO_RESULT_BAD_XML_FORMAT;
        goto failed;
    }
    if (pos != root->name.begin + root->name.len)
    {
        JIO_ERROR(ctx, "Root tag should be only \"<rmod>\"");
        res = JIO_RESULT_BAD_XML_FORMAT;
        goto failed;
    }
    pos += 1;


    uint32_t stack_depth = 32;
    uint32_t stack_pos = 0;
    jio_xml_element** current_stack = jio_alloc(ctx, stack_depth * sizeof(*current_stack));
    if (!current_stack)
    {
        JIO_ERROR(ctx, "Failed jalloc (%zu)", stack_depth * sizeof(*current_stack));
        res = JIO_RESULT_BAD_ALLOC;
        goto failed;
    }
    memset(current_stack, 0, stack_depth * sizeof(*current_stack));
    jio_xml_element* current = root;
    current_stack[0] = current;
    //  Perform descent down the element tree
    for (;;)
    {
        //  Skip any whitespace
        while (is_whitespace(*pos))
        {
            pos += 1;
        }
        current = current_stack[stack_pos];
        const char* new_pos = strchr(pos, '<');
        if (!new_pos)
        {
            JIO_ERROR(ctx, "Tag \"%.*s\" on line %u is unclosed", (int)current->name.len, current->name.begin,
                       count_new_lines(current->name.begin - xml, xml));
            res = JIO_RESULT_BAD_XML_FORMAT;
            goto free_fail;
        }

        //  Skip any whitespace
        while (is_whitespace(*pos))
        {
            pos += 1;
        }
        //  We have some text before the next '<'
        jio_string_segment val = {.begin = pos, .len = new_pos - pos};
        //  Trim space after the end of text
        while (val.len > 0 && is_whitespace(val.begin[val.len - 1]))
        {
            val.len -= 1;
        }
        current->value = val;

        pos = new_pos + 1;
        if (*pos == '/')
        {
            //  End tag
            if (strncmp(pos + 1, current->name.begin, current->name.len) != 0)
            {
                JIO_ERROR(ctx, "Tag \"%.*s\" on line %u was not properly closed", (int)current->name.len, current->name.begin,
                           count_new_lines(current->name.begin - xml, xml));
                res = JIO_RESULT_BAD_XML_FORMAT;
                goto free_fail;
            }
            pos += 1 + current->name.len;
            if (*pos != '>')
            {
                JIO_ERROR(ctx, "Tag \"%.*s\" on line %u was not properly closed", (int)current->name.len, current->name.begin,
                           count_new_lines(current->name.begin - xml, xml));
                res = JIO_RESULT_BAD_XML_FORMAT;
                goto free_fail;
            }
            pos += 1;
            if (stack_pos)
            {
                stack_pos -= 1;
            }
            else
            {
                goto done;
            }
        }
        else if (*pos == '!' && *(pos + 1) == '-' && *(pos + 2) == '-')
        {
            //  This is a comment
            pos += 3;
            new_pos = strstr(pos, "-->");
            if (!new_pos)
            {
                JIO_ERROR(ctx, "Comment on line %u was not concluded", count_new_lines(pos - xml, xml));
                res = JIO_RESULT_BAD_XML_FORMAT;
                goto free_fail;
            }
            pos = new_pos + 3;
        }
        else
        {
            //  New child tag
            jio_string_segment name;
            if (!parse_name_from_string(len - (pos - xml), pos, &name))
            {
                JIO_ERROR(ctx, "Failed parsing tag name on line %u", count_new_lines(pos - xml, xml));
                res = JIO_RESULT_BAD_XML_FORMAT;
                goto free_fail;
            }
            pos += name.len;
            //  Add the child
            if (current->child_count == current->child_capacity)
            {
                const uint64_t new_capacity = current->child_capacity + 32;
                jio_xml_element* const new_ptr = jio_realloc(ctx, current->children, sizeof(*current->children) * new_capacity);
                if (!new_ptr)
                {
                    JIO_ERROR(ctx, "Failed jrealloc(%p, %zu)", (void*)current->children, sizeof(*current->children) * new_capacity);
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                memset(new_ptr + current->child_count, 0, sizeof(*new_ptr) * (new_capacity - current->child_capacity));
                current->child_capacity = new_capacity;
                current->children = new_ptr;
            }
            jio_xml_element* new_child = current->children + (current->child_count++);
            new_child->name = name;


            while (is_whitespace(*pos))
            {
                pos += 1;
            }
            while (*pos != '>')
            {
                //  There are attributes to add
                jio_string_segment attrib_name, attrib_val;
                if (!parse_name_from_string(len - (pos - xml), pos, &attrib_name))
                {
                    JIO_ERROR(ctx, "Failed parsing attribute name for block %.*s on line %u", (int)name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                //  Next is the '=', which could be surrounded by whitespace
                pos += attrib_name.len;
                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
                if (*pos != '=')
                {
                    JIO_ERROR(ctx, "Failed parsing attribute %.*s for block %.*s on line %u: attribute name and value must be separated by '='", (int)attrib_name.len, attrib_name.begin, (int)name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                pos += 1;
                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
                if (*pos != '\'' && *pos != '\"')
                {
                    JIO_ERROR(ctx, "Failed parsing attribute %.*s for block %.*s on line %u: attribute value must be quoted", (int)attrib_name.len, attrib_name.begin, (int)name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                new_pos = strchr(pos + 1, *pos);
                if (!new_pos)
                {
                    JIO_ERROR(ctx, "Failed parsing attribute %.*s for block %.*s on line %u: attribute value quotes are not closed", (int)attrib_name.len, attrib_name.begin, (int)name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                attrib_val.begin = pos + 1;
                attrib_val.len = new_pos - pos - 1;
                pos = new_pos + 1;
                if (!is_whitespace(*pos) && *pos != '>')
                {
                    JIO_ERROR(ctx, "Failed parsing attribute %.*s for block %.*s on line %u: attributes should be separated by whitespace", (int)attrib_name.len, attrib_name.begin, (int)name.len, name.begin,
                               count_new_lines(pos - xml, xml));
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                if (new_child->attrib_count == new_child->attrib_capacity)
                {
                    const uint32_t new_capacity = new_child->attrib_capacity + 8;
                    jio_string_segment* const new_ptr1 = jio_realloc(ctx, new_child->attribute_names, sizeof(*new_ptr1) * new_capacity);
                    if (!new_ptr1)
                    {
                        JIO_ERROR(ctx, "Failed calling jrealloc(%p, %zu)", (void*)new_child->attribute_names, sizeof(*new_ptr1) * new_capacity);
                        res = JIO_RESULT_BAD_XML_FORMAT;
                        goto free_fail;
                    }
                    memset(new_ptr1 + new_child->attrib_count, 0, sizeof(*new_ptr1) * (new_capacity - new_child->attrib_capacity));
                    new_child->attribute_names = new_ptr1;

                    jio_string_segment* const new_ptr2 = jio_realloc(ctx, new_child->attribute_values, sizeof(*new_ptr2) * new_capacity);
                    if (!new_ptr2)
                    {
                        JIO_ERROR(ctx, "Failed calling jrealloc(%p, %zu)", (void*)new_child->attribute_values, sizeof(*new_ptr2) * new_capacity);
                        res = JIO_RESULT_BAD_XML_FORMAT;
                        goto free_fail;
                    }
                    memset(new_ptr2 + new_child->attrib_count, 0, sizeof(*new_ptr2) * (new_capacity - new_child->attrib_capacity));
                    new_child->attribute_values = new_ptr2;

                    new_child->attrib_capacity = new_capacity;
                }
                new_child->attribute_names[new_child->attrib_count] = attrib_name;
                new_child->attribute_values[new_child->attrib_count] = attrib_val;
                new_child->attrib_count += 1;

                while (is_whitespace(*pos))
                {
                    pos += 1;
                }
            }
            pos += 1;


            //  Push the stack
            if (stack_pos == stack_depth)
            {
                const uint64_t new_depth = stack_depth + 32;
                jio_xml_element** const new_ptr = jio_realloc(ctx, current_stack, sizeof(*current_stack) * new_depth);
                if (!new_ptr)
                {
                    JIO_ERROR(ctx, "Failed jrealloc(%p, %zu)", (void*)current_stack, sizeof(*current_stack) * new_depth);
                    res = JIO_RESULT_BAD_XML_FORMAT;
                    goto free_fail;
                }
                memset(new_ptr + stack_pos, 0, sizeof(*new_ptr) * (new_depth - stack_depth));
                stack_depth = new_depth;
                current_stack = new_ptr;
            }
            current_stack[++stack_pos] = new_child;
            if (current->child_count == 1)
            {
                assert(current->depth == 0);
                current->depth = 1;
                //  First child increases the depth of the tree
                for (uint32_t i = 0; i < stack_pos - 1; ++i)
                {
                    //  If a child now has an equal depth as parent, parent should have a greater depth
                    if (current_stack[stack_pos - 1 - i]->depth == current_stack[stack_pos - i - 2]->depth)
                    {
                        current_stack[stack_pos - 2 - i]->depth += 1;
                    }
                }
            }
        }
    }

done:
    assert(stack_pos == 0);
    jio_free(ctx, current_stack);
    *p_root = root;

    return JIO_RESULT_SUCCESS;

free_fail:
    jio_xml_release(ctx, root);
    jio_free(ctx, current_stack);
failed:
     ;
    return res;
}
