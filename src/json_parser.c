#include "json_parser.h"

#include "persistence_internal.h"

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonArray
{
    JsonValue **items;
    size_t count;
    size_t capacity;
} JsonArray;

typedef struct JsonObject
{
    char **keys;
    JsonValue **values;
    size_t count;
    size_t capacity;
} JsonObject;

struct JsonValue
{
    JsonValueType type;
    union
    {
        bool boolean;
        double number;
        char *string;
        JsonArray array;
        JsonObject object;
    } data;
};

typedef struct JsonValueHandle
{
    JsonValue value;
    struct JsonValueHandle *next;
} JsonValueHandle;

typedef struct JsonValuePoolBlock
{
    JsonValueHandle *handles;
    size_t count;
    struct JsonValuePoolBlock *next;
} JsonValuePoolBlock;

typedef struct JsonValuePool
{
    JsonValueHandle *free_list;
    JsonValuePoolBlock *blocks;
    size_t block_size;
    size_t total_blocks;
} JsonValuePool;

static JsonValuePool g_json_value_pool = {NULL, NULL, 128U, 0U};

typedef struct StringBuilder
{
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

typedef struct JsonParser
{
    const char *cursor;
    int line;
    int column;
} JsonParser;

static void string_builder_init(StringBuilder *builder)
{
    builder->data     = NULL;
    builder->length   = 0U;
    builder->capacity = 0U;
}

static void string_builder_free(StringBuilder *builder)
{
    free(builder->data);
    builder->data     = NULL;
    builder->length   = 0U;
    builder->capacity = 0U;
}

static bool string_builder_reserve(StringBuilder *builder, size_t additional)
{
    if (builder->length + additional + 1U <= builder->capacity)
    {
        return true;
    }
    size_t new_capacity = builder->capacity == 0U ? 32U : builder->capacity;
    while (new_capacity < builder->length + additional + 1U)
    {
        new_capacity *= 2U;
    }
    char *buffer = realloc(builder->data, new_capacity);
    if (!buffer)
    {
        return false;
    }
    builder->data     = buffer;
    builder->capacity = new_capacity;
    return true;
}

static bool string_builder_append_byte(StringBuilder *builder, unsigned char value)
{
    if (!string_builder_reserve(builder, 1U))
    {
        return false;
    }
    builder->data[builder->length++] = (char)value;
    builder->data[builder->length]   = '\0';
    return true;
}

static bool string_builder_append_utf8(StringBuilder *builder, unsigned int codepoint)
{
    unsigned char buffer[4];
    size_t length = 0U;
    if (codepoint <= 0x7FU)
    {
        buffer[length++] = (unsigned char)codepoint;
    }
    else if (codepoint <= 0x7FFU)
    {
        buffer[length++] = (unsigned char)(0xC0U | ((codepoint >> 6U) & 0x1FU));
        buffer[length++] = (unsigned char)(0x80U | (codepoint & 0x3FU));
    }
    else if (codepoint <= 0xFFFFU)
    {
        buffer[length++] = (unsigned char)(0xE0U | ((codepoint >> 12U) & 0x0FU));
        buffer[length++] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        buffer[length++] = (unsigned char)(0x80U | (codepoint & 0x3FU));
    }
    else if (codepoint <= 0x10FFFFU)
    {
        buffer[length++] = (unsigned char)(0xF0U | ((codepoint >> 18U) & 0x07U));
        buffer[length++] = (unsigned char)(0x80U | ((codepoint >> 12U) & 0x3FU));
        buffer[length++] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        buffer[length++] = (unsigned char)(0x80U | (codepoint & 0x3FU));
    }
    else
    {
        return false;
    }
    if (!string_builder_reserve(builder, length))
    {
        return false;
    }
    for (size_t index = 0; index < length; ++index)
    {
        builder->data[builder->length++] = (char)buffer[index];
    }
    builder->data[builder->length] = '\0';
    return true;
}

static JsonValueHandle *json_value_handle_from_value(JsonValue *value)
{
    if (!value)
    {
        return NULL;
    }
    return (JsonValueHandle *)((unsigned char *)value - offsetof(JsonValueHandle, value));
}

static bool json_value_pool_grow(JsonValuePool *pool)
{
    if (!pool)
    {
        return false;
    }
    size_t count = pool->block_size;
    if (count == 0U)
    {
        count = 128U;
    }
    JsonValueHandle *handles = (JsonValueHandle *)calloc(count, sizeof(JsonValueHandle));
    if (!handles)
    {
        return false;
    }
    JsonValuePoolBlock *block = (JsonValuePoolBlock *)calloc(1U, sizeof(JsonValuePoolBlock));
    if (!block)
    {
        free(handles);
        return false;
    }
    block->handles = handles;
    block->count   = count;
    block->next    = pool->blocks;
    pool->blocks   = block;
    pool->total_blocks += 1U;

    for (size_t index = 0; index < count; ++index)
    {
        handles[index].next = pool->free_list;
        pool->free_list     = &handles[index];
    }

    if (pool->block_size < 2048U)
    {
        pool->block_size *= 2U;
    }
    return true;
}

static JsonValue *json_pool_acquire(void)
{
    JsonValuePool *pool = &g_json_value_pool;
    if (!pool->free_list)
    {
        if (!json_value_pool_grow(pool))
        {
            return NULL;
        }
    }
    JsonValueHandle *handle = pool->free_list;
    pool->free_list         = handle->next;
    memset(&handle->value, 0, sizeof(handle->value));
    handle->next = NULL;
    return &handle->value;
}

static void json_pool_release(JsonValue *value)
{
    if (!value)
    {
        return;
    }
    JsonValueHandle *handle = json_value_handle_from_value(value);
    if (!handle)
    {
        free(value);
        return;
    }
    handle->next                = g_json_value_pool.free_list;
    g_json_value_pool.free_list = handle;
}

static char parser_peek(const JsonParser *parser) { return parser->cursor[0]; }

static char parser_next(JsonParser *parser)
{
    char ch = parser->cursor[0];
    if (ch == '\n')
    {
        parser->line++;
        parser->column = 1;
    }
    else
    {
        parser->column++;
    }
    parser->cursor++;
    return ch;
}

static void parser_skip_whitespace(JsonParser *parser)
{
    while (isspace((unsigned char)parser_peek(parser)))
    {
        parser_next(parser);
    }
}

static bool parser_set_error(JsonParser *parser, char *buffer, size_t buffer_size,
                             const char *message)
{
    char formatted[256];
    (void)snprintf(formatted, sizeof(formatted), "line %d, column %d: %s", parser->line,
                   parser->column, message);
    return persistence_set_error_message(buffer, buffer_size, formatted);
}

static int parse_hex_digit(char ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return 10 + (ch - 'A');
    }
    return -1;
}

static bool parser_parse_unicode_escape(JsonParser *parser, unsigned int *out_codepoint,
                                        char *error_buffer, size_t error_buffer_size)
{
    unsigned int codepoint = 0U;
    for (int index = 0; index < 4; ++index)
    {
        char ch   = parser_next(parser);
        int digit = parse_hex_digit(ch);
        if (digit < 0)
        {
            return parser_set_error(parser, error_buffer, error_buffer_size,
                                    "invalid unicode escape");
        }
        codepoint = (codepoint << 4U) | (unsigned int)digit;
    }
    if (codepoint >= 0xD800U && codepoint <= 0xDBFFU)
    {
        if (parser_peek(parser) != '\\' || parser->cursor[1] != 'u')
        {
            return parser_set_error(parser, error_buffer, error_buffer_size,
                                    "unterminated surrogate pair");
        }
        parser_next(parser);
        parser_next(parser);
        unsigned int low = 0U;
        for (int index = 0; index < 4; ++index)
        {
            char ch   = parser_next(parser);
            int digit = parse_hex_digit(ch);
            if (digit < 0)
            {
                return parser_set_error(parser, error_buffer, error_buffer_size,
                                        "invalid unicode escape");
            }
            low = (low << 4U) | (unsigned int)digit;
        }
        if (low < 0xDC00U || low > 0xDFFFU)
        {
            return parser_set_error(parser, error_buffer, error_buffer_size,
                                    "invalid surrogate pair");
        }
        codepoint = 0x10000U + (((codepoint - 0xD800U) << 10U) | (low - 0xDC00U));
    }
    else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU)
    {
        return parser_set_error(parser, error_buffer, error_buffer_size, "unpaired low surrogate");
    }
    *out_codepoint = codepoint;
    return true;
}

static JsonValue *json_value_new(JsonValueType type)
{
    JsonValue *value = json_pool_acquire();
    if (!value)
    {
        return NULL;
    }
    value->type = type;
    return value;
}

static JsonValue *parser_parse_string(JsonParser *parser, char *error_buffer,
                                      size_t error_buffer_size)
{
    StringBuilder builder;
    string_builder_init(&builder);
    parser_next(parser);
    while (parser_peek(parser) != '\0')
    {
        char ch = parser_next(parser);
        if (ch == '"')
        {
            char *result = builder.data ? builder.data : strdup("");
            if (!result)
            {
                string_builder_free(&builder);
                return NULL;
            }
            JsonValue *value = json_value_new(JSON_VALUE_STRING);
            if (!value)
            {
                free(result);
                return NULL;
            }
            value->data.string = result;
            return value;
        }
        if ((unsigned char)ch < 0x20U)
        {
            string_builder_free(&builder);
            parser_set_error(parser, error_buffer, error_buffer_size,
                             "control character in string");
            return NULL;
        }
        if (ch == '\\')
        {
            char escape = parser_next(parser);
            switch (escape)
            {
            case '"':
            case '\\':
            case '/':
                if (!string_builder_append_byte(&builder, (unsigned char)escape))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 'b':
                if (!string_builder_append_byte(&builder, '\b'))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 'f':
                if (!string_builder_append_byte(&builder, '\f'))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 'n':
                if (!string_builder_append_byte(&builder, '\n'))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 'r':
                if (!string_builder_append_byte(&builder, '\r'))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 't':
                if (!string_builder_append_byte(&builder, '\t'))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            case 'u':
            {
                unsigned int codepoint = 0U;
                if (!parser_parse_unicode_escape(parser, &codepoint, error_buffer,
                                                 error_buffer_size) ||
                    !string_builder_append_utf8(&builder, codepoint))
                {
                    string_builder_free(&builder);
                    return NULL;
                }
                break;
            }
            default:
                string_builder_free(&builder);
                parser_set_error(parser, error_buffer, error_buffer_size,
                                 "invalid escape sequence");
                return NULL;
            }
        }
        else
        {
            if (!string_builder_append_byte(&builder, (unsigned char)ch))
            {
                string_builder_free(&builder);
                return NULL;
            }
        }
    }
    string_builder_free(&builder);
    parser_set_error(parser, error_buffer, error_buffer_size, "unterminated string");
    return NULL;
}

static JsonValue *parser_parse_literal(JsonParser *parser, const char *literal, JsonValueType type,
                                       bool bool_value, char *error_buffer,
                                       size_t error_buffer_size)
{
    size_t length = strlen(literal);
    for (size_t index = 0; index < length; ++index)
    {
        if (parser_peek(parser) != literal[index])
        {
            parser_set_error(parser, error_buffer, error_buffer_size, "invalid literal");
            return NULL;
        }
        parser_next(parser);
    }
    JsonValue *value = json_value_new(type);
    if (!value)
    {
        return NULL;
    }
    if (type == JSON_VALUE_BOOL)
    {
        value->data.boolean = bool_value;
    }
    return value;
}

static JsonValue *parser_parse_number(JsonParser *parser, char *error_buffer,
                                      size_t error_buffer_size)
{
    const char *start = parser->cursor;
    char *endptr      = NULL;
    double number     = strtod(parser->cursor, &endptr);
    if (endptr == start)
    {
        parser_set_error(parser, error_buffer, error_buffer_size, "invalid number");
        return NULL;
    }
    size_t consumed = (size_t)(endptr - start);
    for (size_t index = 0; index < consumed; ++index)
    {
        parser_next(parser);
    }
    JsonValue *value = json_value_new(JSON_VALUE_NUMBER);
    if (!value)
    {
        return NULL;
    }
    value->data.number = number;
    return value;
}

static bool array_push(JsonArray *array, JsonValue *value)
{
    if (array->count == array->capacity)
    {
        size_t new_capacity = array->capacity == 0U ? 4U : array->capacity * 2U;
        JsonValue **items   = realloc(array->items, new_capacity * sizeof(JsonValue *));
        if (!items)
        {
            return false;
        }
        array->items    = items;
        array->capacity = new_capacity;
    }
    array->items[array->count++] = value;
    return true;
}

static bool object_put(JsonObject *object, char *key, JsonValue *value)
{
    if (object->count == object->capacity)
    {
        size_t new_capacity = object->capacity == 0U ? 4U : object->capacity * 2U;
        char **keys         = realloc(object->keys, new_capacity * sizeof(char *));
        if (!keys)
        {
            return false;
        }
        JsonValue **values = realloc(object->values, new_capacity * sizeof(JsonValue *));
        if (!values)
        {
            free(keys);
            return false;
        }
        object->keys     = keys;
        object->values   = values;
        object->capacity = new_capacity;
    }
    object->keys[object->count]   = key;
    object->values[object->count] = value;
    object->count++;
    return true;
}

static JsonValue *parser_parse_value(JsonParser *parser, char *error_buffer,
                                     size_t error_buffer_size);

static JsonValue *parser_parse_array(JsonParser *parser, char *error_buffer,
                                     size_t error_buffer_size)
{
    JsonValue *value = json_value_new(JSON_VALUE_ARRAY);
    if (!value)
    {
        return NULL;
    }
    parser_next(parser);
    parser_skip_whitespace(parser);
    if (parser_peek(parser) == ']')
    {
        parser_next(parser);
        return value;
    }
    while (true)
    {
        JsonValue *element = parser_parse_value(parser, error_buffer, error_buffer_size);
        if (!element)
        {
            json_value_destroy(value);
            return NULL;
        }
        if (!array_push(&value->data.array, element))
        {
            json_value_destroy(element);
            json_value_destroy(value);
            return NULL;
        }
        parser_skip_whitespace(parser);
        char ch = parser_peek(parser);
        if (ch == ',')
        {
            parser_next(parser);
            parser_skip_whitespace(parser);
            continue;
        }
        if (ch == ']')
        {
            parser_next(parser);
            break;
        }
        parser_set_error(parser, error_buffer, error_buffer_size,
                         "expected comma or closing bracket");
        json_value_destroy(value);
        return NULL;
    }
    return value;
}

static JsonValue *parser_parse_object(JsonParser *parser, char *error_buffer,
                                      size_t error_buffer_size)
{
    JsonValue *value = json_value_new(JSON_VALUE_OBJECT);
    if (!value)
    {
        return NULL;
    }
    parser_next(parser);
    parser_skip_whitespace(parser);
    if (parser_peek(parser) == '}')
    {
        parser_next(parser);
        return value;
    }
    while (true)
    {
        if (parser_peek(parser) != '"')
        {
            parser_set_error(parser, error_buffer, error_buffer_size, "object key must be string");
            json_value_destroy(value);
            return NULL;
        }
        JsonValue *key_value = parser_parse_string(parser, error_buffer, error_buffer_size);
        if (!key_value)
        {
            json_value_destroy(value);
            return NULL;
        }
        char *key = key_value->data.string;
        json_pool_release(key_value);
        parser_skip_whitespace(parser);
        if (parser_peek(parser) != ':')
        {
            free(key);
            parser_set_error(parser, error_buffer, error_buffer_size, "expected colon after key");
            json_value_destroy(value);
            return NULL;
        }
        parser_next(parser);
        parser_skip_whitespace(parser);
        JsonValue *element = parser_parse_value(parser, error_buffer, error_buffer_size);
        if (!element)
        {
            free(key);
            json_value_destroy(value);
            return NULL;
        }
        if (!object_put(&value->data.object, key, element))
        {
            free(key);
            json_value_destroy(element);
            json_value_destroy(value);
            return NULL;
        }
        parser_skip_whitespace(parser);
        char ch = parser_peek(parser);
        if (ch == ',')
        {
            parser_next(parser);
            parser_skip_whitespace(parser);
            continue;
        }
        if (ch == '}')
        {
            parser_next(parser);
            break;
        }
        parser_set_error(parser, error_buffer, error_buffer_size,
                         "expected comma or closing brace");
        json_value_destroy(value);
        return NULL;
    }
    return value;
}

static JsonValue *parser_parse_value(JsonParser *parser, char *error_buffer,
                                     size_t error_buffer_size)
{
    parser_skip_whitespace(parser);
    char ch = parser_peek(parser);
    if (ch == '"')
    {
        return parser_parse_string(parser, error_buffer, error_buffer_size);
    }
    if (ch == '{')
    {
        return parser_parse_object(parser, error_buffer, error_buffer_size);
    }
    if (ch == '[')
    {
        return parser_parse_array(parser, error_buffer, error_buffer_size);
    }
    if (ch == 't')
    {
        return parser_parse_literal(parser, "true", JSON_VALUE_BOOL, true, error_buffer,
                                    error_buffer_size);
    }
    if (ch == 'f')
    {
        return parser_parse_literal(parser, "false", JSON_VALUE_BOOL, false, error_buffer,
                                    error_buffer_size);
    }
    if (ch == 'n')
    {
        return parser_parse_literal(parser, "null", JSON_VALUE_NULL, false, error_buffer,
                                    error_buffer_size);
    }
    if (ch == '-' || ch == '+' || isdigit((unsigned char)ch))
    {
        return parser_parse_number(parser, error_buffer, error_buffer_size);
    }
    parser_set_error(parser, error_buffer, error_buffer_size, "unexpected character");
    return NULL;
}

JsonValue *json_parse(const char *text, char *error_buffer, size_t error_buffer_size,
                      int *error_line, int *error_column)
{
    if (!text)
    {
        persistence_set_error_message(error_buffer, error_buffer_size, "input text is NULL");
        return NULL;
    }
    JsonParser parser;
    parser.cursor   = text;
    parser.line     = 1;
    parser.column   = 1;
    JsonValue *root = parser_parse_value(&parser, error_buffer, error_buffer_size);
    if (!root)
    {
        if (error_line)
        {
            *error_line = parser.line;
        }
        if (error_column)
        {
            *error_column = parser.column;
        }
        return NULL;
    }
    parser_skip_whitespace(&parser);
    if (parser_peek(&parser) != '\0')
    {
        json_value_destroy(root);
        parser_set_error(&parser, error_buffer, error_buffer_size, "unexpected trailing data");
        if (error_line)
        {
            *error_line = parser.line;
        }
        if (error_column)
        {
            *error_column = parser.column;
        }
        return NULL;
    }
    if (error_line)
    {
        *error_line = 0;
    }
    if (error_column)
    {
        *error_column = 0;
    }
    return root;
}

void json_value_destroy(JsonValue *value)
{
    if (!value)
    {
        return;
    }
    switch (value->type)
    {
    case JSON_VALUE_STRING:
        free(value->data.string);
        break;
    case JSON_VALUE_ARRAY:
        for (size_t index = 0; index < value->data.array.count; ++index)
        {
            json_value_destroy(value->data.array.items[index]);
        }
        free(value->data.array.items);
        break;
    case JSON_VALUE_OBJECT:
        for (size_t index = 0; index < value->data.object.count; ++index)
        {
            free(value->data.object.keys[index]);
            json_value_destroy(value->data.object.values[index]);
        }
        free(value->data.object.keys);
        free(value->data.object.values);
        break;
    default:
        break;
    }
    json_pool_release(value);
}

JsonAllocatorStats json_parser_allocator_stats(void)
{
    JsonAllocatorStats stats  = {0};
    const JsonValuePool *pool = &g_json_value_pool;
    stats.total_blocks        = pool->total_blocks;

    size_t capacity = 0;
    for (const JsonValuePoolBlock *block = pool->blocks; block; block = block->next)
    {
        capacity += block->count;
    }
    stats.block_capacity = capacity;

    size_t free_count = 0;
    for (const JsonValueHandle *handle = pool->free_list; handle; handle = handle->next)
    {
        free_count += 1U;
    }
    stats.free_values = free_count;
    return stats;
}

void json_parser_allocator_reset(void)
{
    JsonValuePool *pool       = &g_json_value_pool;
    JsonValuePoolBlock *block = pool->blocks;
    while (block)
    {
        JsonValuePoolBlock *next = block->next;
        free(block->handles);
        free(block);
        block = next;
    }
    pool->blocks       = NULL;
    pool->free_list    = NULL;
    pool->total_blocks = 0U;
    pool->block_size   = 128U;
}

JsonValueType json_value_type(const JsonValue *value)
{
    if (!value)
    {
        return JSON_VALUE_NULL;
    }
    return value->type;
}

bool json_value_get_bool(const JsonValue *value, bool *out_value)
{
    if (!value || value->type != JSON_VALUE_BOOL)
    {
        return false;
    }
    if (out_value)
    {
        *out_value = value->data.boolean;
    }
    return true;
}

bool json_value_get_number(const JsonValue *value, double *out_value)
{
    if (!value || value->type != JSON_VALUE_NUMBER)
    {
        return false;
    }
    if (out_value)
    {
        *out_value = value->data.number;
    }
    return true;
}

const char *json_value_get_string(const JsonValue *value)
{
    if (!value || value->type != JSON_VALUE_STRING)
    {
        return NULL;
    }
    return value->data.string;
}

size_t json_value_array_size(const JsonValue *value)
{
    if (!value || value->type != JSON_VALUE_ARRAY)
    {
        return 0U;
    }
    return value->data.array.count;
}

JsonValue *json_value_array_get(const JsonValue *value, size_t index)
{
    if (!value || value->type != JSON_VALUE_ARRAY)
    {
        return NULL;
    }
    if (index >= value->data.array.count)
    {
        return NULL;
    }
    return value->data.array.items[index];
}

JsonValue *json_value_object_get(const JsonValue *value, const char *key)
{
    if (!value || value->type != JSON_VALUE_OBJECT || !key)
    {
        return NULL;
    }
    for (size_t index = 0; index < value->data.object.count; ++index)
    {
        if (strcmp(value->data.object.keys[index], key) == 0)
        {
            return value->data.object.values[index];
        }
    }
    return NULL;
}

size_t json_value_object_size(const JsonValue *value)
{
    if (!value || value->type != JSON_VALUE_OBJECT)
    {
        return 0U;
    }
    return value->data.object.count;
}

const char *json_value_object_key(const JsonValue *value, size_t index)
{
    if (!value || value->type != JSON_VALUE_OBJECT)
    {
        return NULL;
    }
    if (index >= value->data.object.count)
    {
        return NULL;
    }
    return value->data.object.keys[index];
}

JsonValue *json_value_object_value(const JsonValue *value, size_t index)
{
    if (!value || value->type != JSON_VALUE_OBJECT)
    {
        return NULL;
    }
    if (index >= value->data.object.count)
    {
        return NULL;
    }
    return value->data.object.values[index];
}
