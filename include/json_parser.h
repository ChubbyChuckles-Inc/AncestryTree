#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum JsonValueType
{
    JSON_VALUE_NULL = 0,
    JSON_VALUE_BOOL,
    JSON_VALUE_NUMBER,
    JSON_VALUE_STRING,
    JSON_VALUE_ARRAY,
    JSON_VALUE_OBJECT
} JsonValueType;

typedef struct JsonValue JsonValue;

JsonValue *json_parse(const char *text, char *error_buffer, size_t error_buffer_size, int *error_line, int *error_column);
void json_value_destroy(JsonValue *value);

JsonValueType json_value_type(const JsonValue *value);
bool json_value_get_bool(const JsonValue *value, bool *out_value);
bool json_value_get_number(const JsonValue *value, double *out_value);
const char *json_value_get_string(const JsonValue *value);
size_t json_value_array_size(const JsonValue *value);
JsonValue *json_value_array_get(const JsonValue *value, size_t index);
JsonValue *json_value_object_get(const JsonValue *value, const char *key);
size_t json_value_object_size(const JsonValue *value);
const char *json_value_object_key(const JsonValue *value, size_t index);
JsonValue *json_value_object_value(const JsonValue *value, size_t index);

#endif /* JSON_PARSER_H */
