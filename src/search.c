#include "search.h"

#include "person.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEARCH_BOOLEAN_MAX_TOKENS 64U
#define SEARCH_PERSON_TEXT_CAPACITY 2048U

typedef enum SearchTokenType
{
    SEARCH_TOKEN_TERM = 0,
    SEARCH_TOKEN_AND,
    SEARCH_TOKEN_OR,
    SEARCH_TOKEN_NOT,
    SEARCH_TOKEN_LPAREN,
    SEARCH_TOKEN_RPAREN
} SearchTokenType;

typedef struct SearchToken
{
    SearchTokenType type;
    char text[SEARCH_QUERY_MAX_TERM_LENGTH];
} SearchToken;

typedef struct SearchBooleanProgram
{
    SearchToken items[SEARCH_BOOLEAN_MAX_TOKENS];
    size_t count;
} SearchBooleanProgram;

typedef struct SearchEvaluationContext
{
    SearchQueryMode mode;
    bool has_expression;
    char substring[SEARCH_QUERY_MAX_EXPRESSION_LENGTH];
    SearchBooleanProgram boolean_program;
    char regex[SEARCH_QUERY_MAX_EXPRESSION_LENGTH];
} SearchEvaluationContext;

typedef struct SearchPersonText
{
    bool built;
    char name[160];
    char birth_date[64];
    char birth_location[96];
    char death_date[64];
    char death_location[96];
    char locations[256];
    char metadata[512];
    char timeline[640];
    char combined[SEARCH_PERSON_TEXT_CAPACITY];
} SearchPersonText;

static void lowercase_copy(char *destination, size_t capacity, const char *source)
{
    if (!destination || capacity == 0U)
    {
        return;
    }
    if (!source)
    {
        destination[0] = '\0';
        return;
    }
    size_t index = 0U;
    while (index + 1U < capacity && source[index] != '\0')
    {
        destination[index] = (char)tolower((unsigned char)source[index]);
        index++;
    }
    destination[index] = '\0';
}

static bool extract_year(const char *value, int *out_year)
{
    if (!value)
    {
        return false;
    }
    int digits = 0;
    int year   = 0;
    for (size_t index = 0U; value[index] != '\0'; ++index)
    {
        unsigned char ch = (unsigned char)value[index];
        if (isdigit(ch))
        {
            year = year * 10 + (ch - '0');
            digits++;
            if (digits == 4)
            {
                if (out_year)
                {
                    *out_year = year;
                }
                return true;
            }
        }
        else if (digits > 0)
        {
            break;
        }
    }
    return false;
}

static int string_casefold_compare(const char *lhs, const char *rhs)
{
    size_t index = 0U;
    while (lhs[index] != '\0' && rhs[index] != '\0')
    {
        int diff = tolower((unsigned char)lhs[index]) - tolower((unsigned char)rhs[index]);
        if (diff != 0)
        {
            return diff;
        }
        index++;
    }
    return tolower((unsigned char)lhs[index]) - tolower((unsigned char)rhs[index]);
}

static bool filter_allows_alive(const SearchFilter *filter)
{
    if (!filter)
    {
        return true;
    }
    if (filter->include_alive || filter->include_deceased)
    {
        return filter->include_alive;
    }
    return true;
}

static bool filter_allows_deceased(const SearchFilter *filter)
{
    if (!filter)
    {
        return true;
    }
    if (filter->include_alive || filter->include_deceased)
    {
        return filter->include_deceased;
    }
    return true;
}

static void search_person_text_init(SearchPersonText *text)
{
    if (!text)
    {
        return;
    }
    memset(text, 0, sizeof(*text));
    text->built = false;
}

static void search_person_text_append(char *buffer, size_t capacity, const char *value)
{
    if (!buffer || capacity == 0U || !value || value[0] == '\0')
    {
        return;
    }
    size_t length = strnlen(buffer, capacity);
    if (length > 0U && length + 1U < capacity)
    {
        buffer[length++] = ' ';
    }
    size_t index = 0U;
    while (value[index] != '\0' && length + 1U < capacity)
    {
        buffer[length++] = (char)tolower((unsigned char)value[index]);
        index++;
    }
    buffer[length] = '\0';
}

static void search_person_text_build(const Person *person, SearchPersonText *text)
{
    if (!person || !text || text->built)
    {
        return;
    }
    text->built             = true;
    text->name[0]           = '\0';
    text->birth_date[0]     = '\0';
    text->birth_location[0] = '\0';
    text->death_date[0]     = '\0';
    text->death_location[0] = '\0';
    text->locations[0]      = '\0';
    text->metadata[0]       = '\0';
    text->timeline[0]       = '\0';
    text->combined[0]       = '\0';

    char name_buffer[160];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }
    lowercase_copy(text->name, sizeof(text->name), name_buffer);
    search_person_text_append(text->combined, sizeof(text->combined), text->name);

    lowercase_copy(text->birth_date, sizeof(text->birth_date), person->dates.birth_date);
    lowercase_copy(text->birth_location, sizeof(text->birth_location),
                   person->dates.birth_location);
    lowercase_copy(text->death_date, sizeof(text->death_date), person->dates.death_date);
    lowercase_copy(text->death_location, sizeof(text->death_location),
                   person->dates.death_location);

    search_person_text_append(text->locations, sizeof(text->locations), text->birth_location);
    search_person_text_append(text->locations, sizeof(text->locations), text->death_location);

    search_person_text_append(text->combined, sizeof(text->combined), text->birth_date);
    search_person_text_append(text->combined, sizeof(text->combined), text->birth_location);
    search_person_text_append(text->combined, sizeof(text->combined), text->death_date);
    search_person_text_append(text->combined, sizeof(text->combined), text->death_location);

    for (size_t index = 0U; index < person->metadata_count; ++index)
    {
        const PersonMetadataEntry *entry = &person->metadata[index];
        search_person_text_append(text->metadata, sizeof(text->metadata), entry->key);
        search_person_text_append(text->metadata, sizeof(text->metadata), entry->value);
    }
    search_person_text_append(text->combined, sizeof(text->combined), text->metadata);

    for (size_t index = 0U; index < person->timeline_count; ++index)
    {
        const TimelineEntry *entry = &person->timeline_entries[index];
        search_person_text_append(text->timeline, sizeof(text->timeline), entry->date);
        search_person_text_append(text->timeline, sizeof(text->timeline), entry->description);
        search_person_text_append(text->timeline, sizeof(text->timeline), entry->location);
    }
    search_person_text_append(text->combined, sizeof(text->combined), text->timeline);
}

static bool search_match_substring(const Person *person, const SearchEvaluationContext *context)
{
    if (!context || !context->has_expression)
    {
        return true;
    }
    char name_buffer[160];
    if (!person_format_display_name(person, name_buffer, sizeof(name_buffer)))
    {
        (void)snprintf(name_buffer, sizeof(name_buffer), "Person %u", person->id);
    }
    char lowered[160];
    lowercase_copy(lowered, sizeof(lowered), name_buffer);
    return strstr(lowered, context->substring) != NULL;
}

static void search_set_error(char *buffer, size_t capacity, const char *message)
{
    if (!buffer || capacity == 0U)
    {
        return;
    }
#if defined(_MSC_VER)
    if (message)
    {
        (void)strncpy_s(buffer, capacity, message, _TRUNCATE);
    }
    else
    {
        buffer[0] = '\0';
    }
#else
    if (!message)
    {
        buffer[0] = '\0';
        return;
    }
    (void)snprintf(buffer, capacity, "%s", message);
#endif
}

static bool search_boolean_tokenize(const char *expression, SearchToken *tokens, size_t *out_count,
                                    char *error_message, size_t error_capacity)
{
    if (!expression || !tokens || !out_count)
    {
        search_set_error(error_message, error_capacity, "Boolean parser received null input.");
        return false;
    }
    size_t count  = 0U;
    size_t cursor = 0U;
    while (expression[cursor] != '\0')
    {
        while (isspace((unsigned char)expression[cursor]))
        {
            cursor++;
        }
        char ch = expression[cursor];
        if (ch == '\0')
        {
            break;
        }
        if (count >= SEARCH_BOOLEAN_MAX_TOKENS)
        {
            search_set_error(error_message, error_capacity,
                             "Expression contains too many terms or operators.");
            return false;
        }
        if (ch == '(')
        {
            tokens[count].type    = SEARCH_TOKEN_LPAREN;
            tokens[count].text[0] = '\0';
            count++;
            cursor++;
            continue;
        }
        if (ch == ')')
        {
            tokens[count].type    = SEARCH_TOKEN_RPAREN;
            tokens[count].text[0] = '\0';
            count++;
            cursor++;
            continue;
        }
        bool in_quotes = false;
        size_t write   = 0U;
        char buffer[SEARCH_QUERY_MAX_TERM_LENGTH];
        memset(buffer, 0, sizeof(buffer));
        while (expression[cursor] != '\0')
        {
            ch = expression[cursor];
            if (ch == '"')
            {
                in_quotes = !in_quotes;
                cursor++;
                continue;
            }
            if (!in_quotes && (isspace((unsigned char)ch) || ch == '(' || ch == ')'))
            {
                break;
            }
            if (write + 1U >= sizeof(buffer))
            {
                search_set_error(error_message, error_capacity,
                                 "A search term exceeds the 64 character limit.");
                return false;
            }
            buffer[write++] = (char)tolower((unsigned char)ch);
            cursor++;
        }
        if (in_quotes)
        {
            search_set_error(error_message, error_capacity,
                             "Mismatched quotes detected in expression.");
            return false;
        }
        buffer[write] = '\0';
        if (buffer[0] == '\0')
        {
            continue;
        }
        if (strcmp(buffer, "and") == 0)
        {
            tokens[count].type    = SEARCH_TOKEN_AND;
            tokens[count].text[0] = '\0';
        }
        else if (strcmp(buffer, "or") == 0)
        {
            tokens[count].type    = SEARCH_TOKEN_OR;
            tokens[count].text[0] = '\0';
        }
        else if (strcmp(buffer, "not") == 0)
        {
            tokens[count].type    = SEARCH_TOKEN_NOT;
            tokens[count].text[0] = '\0';
        }
        else
        {
            tokens[count].type = SEARCH_TOKEN_TERM;
            (void)snprintf(tokens[count].text, sizeof(tokens[count].text), "%s", buffer);
        }
        count++;
    }
    *out_count = count;
    search_set_error(error_message, error_capacity, NULL);
    return true;
}

static int search_boolean_precedence(SearchTokenType type)
{
    switch (type)
    {
    case SEARCH_TOKEN_NOT:
        return 3;
    case SEARCH_TOKEN_AND:
        return 2;
    case SEARCH_TOKEN_OR:
        return 1;
    default:
        return 0;
    }
}

static bool search_boolean_is_right_associative(SearchTokenType type)
{
    return type == SEARCH_TOKEN_NOT;
}

static bool search_boolean_validate_sequence(const SearchToken *tokens, size_t count,
                                             char *error_message, size_t error_capacity)
{
    if (!tokens && count > 0U)
    {
        search_set_error(error_message, error_capacity, "Boolean parser received null input.");
        return false;
    }
    int paren_balance   = 0;
    bool expect_operand = true;
    bool last_was_not   = false;
    for (size_t index = 0U; index < count; ++index)
    {
        SearchTokenType type = tokens[index].type;
        switch (type)
        {
        case SEARCH_TOKEN_TERM:
            if (!expect_operand && !last_was_not)
            {
                search_set_error(error_message, error_capacity,
                                 "Missing operator between search terms.");
                return false;
            }
            expect_operand = false;
            last_was_not   = false;
            break;
        case SEARCH_TOKEN_NOT:
            if (!expect_operand)
            {
                search_set_error(error_message, error_capacity,
                                 "NOT must precede a term or group.");
                return false;
            }
            expect_operand = true;
            last_was_not   = true;
            break;
        case SEARCH_TOKEN_AND:
        case SEARCH_TOKEN_OR:
            if (expect_operand)
            {
                search_set_error(error_message, error_capacity,
                                 "Operator requires a term before it.");
                return false;
            }
            expect_operand = true;
            last_was_not   = false;
            break;
        case SEARCH_TOKEN_LPAREN:
            if (!expect_operand && !last_was_not)
            {
                search_set_error(error_message, error_capacity,
                                 "Missing operator before parenthesis.");
                return false;
            }
            paren_balance++;
            expect_operand = true;
            last_was_not   = false;
            break;
        case SEARCH_TOKEN_RPAREN:
            if (expect_operand)
            {
                search_set_error(error_message, error_capacity,
                                 "Closing parenthesis cannot follow an operator.");
                return false;
            }
            paren_balance--;
            if (paren_balance < 0)
            {
                search_set_error(error_message, error_capacity, "Mismatched parentheses.");
                return false;
            }
            expect_operand = false;
            last_was_not   = false;
            break;
        default:
            break;
        }
    }
    if (paren_balance != 0)
    {
        search_set_error(error_message, error_capacity, "Mismatched parentheses.");
        return false;
    }
    if (expect_operand && count > 0U)
    {
        search_set_error(error_message, error_capacity, "Expression cannot end with an operator.");
        return false;
    }
    search_set_error(error_message, error_capacity, NULL);
    return true;
}

static bool search_boolean_compile(const char *expression, SearchBooleanProgram *program,
                                   char *error_message, size_t error_capacity)
{
    if (!expression || !program)
    {
        search_set_error(error_message, error_capacity, "Boolean parser received null input.");
        return false;
    }
    SearchToken tokens[SEARCH_BOOLEAN_MAX_TOKENS];
    size_t token_count = 0U;
    if (!search_boolean_tokenize(expression, tokens, &token_count, error_message, error_capacity))
    {
        return false;
    }
    if (token_count == 0U)
    {
        program->count = 0U;
        search_set_error(error_message, error_capacity, NULL);
        return true;
    }

    if (!search_boolean_validate_sequence(tokens, token_count, error_message, error_capacity))
    {
        return false;
    }

    SearchToken stack[SEARCH_BOOLEAN_MAX_TOKENS];
    size_t stack_count  = 0U;
    size_t output_count = 0U;
    for (size_t index = 0U; index < token_count; ++index)
    {
        SearchToken token = tokens[index];
        if (token.type == SEARCH_TOKEN_TERM)
        {
            if (output_count >= SEARCH_BOOLEAN_MAX_TOKENS)
            {
                search_set_error(error_message, error_capacity,
                                 "Expression exceeds complexity limit (too many terms).");
                return false;
            }
            program->items[output_count++] = token;
            continue;
        }
        if (token.type == SEARCH_TOKEN_AND || token.type == SEARCH_TOKEN_OR ||
            token.type == SEARCH_TOKEN_NOT)
        {
            int precedence = search_boolean_precedence(token.type);
            while (stack_count > 0U)
            {
                SearchTokenType top_type = stack[stack_count - 1U].type;
                if (top_type == SEARCH_TOKEN_LPAREN)
                {
                    break;
                }
                int top_precedence = search_boolean_precedence(top_type);
                if ((search_boolean_is_right_associative(token.type) &&
                     precedence < top_precedence) ||
                    (!search_boolean_is_right_associative(token.type) &&
                     precedence <= top_precedence))
                {
                    if (output_count >= SEARCH_BOOLEAN_MAX_TOKENS)
                    {
                        search_set_error(error_message, error_capacity,
                                         "Expression exceeds complexity limit (too many terms).");
                        return false;
                    }
                    program->items[output_count++] = stack[stack_count - 1U];
                    stack_count--;
                }
                else
                {
                    break;
                }
            }
            stack[stack_count++] = token;
            continue;
        }
        if (token.type == SEARCH_TOKEN_LPAREN)
        {
            stack[stack_count++] = token;
            continue;
        }
        if (token.type == SEARCH_TOKEN_RPAREN)
        {
            bool matched = false;
            while (stack_count > 0U)
            {
                SearchToken top = stack[--stack_count];
                if (top.type == SEARCH_TOKEN_LPAREN)
                {
                    matched = true;
                    break;
                }
                if (output_count >= SEARCH_BOOLEAN_MAX_TOKENS)
                {
                    search_set_error(error_message, error_capacity,
                                     "Expression exceeds complexity limit (too many terms).");
                    return false;
                }
                program->items[output_count++] = top;
            }
            if (!matched)
            {
                search_set_error(error_message, error_capacity, "Mismatched parentheses.");
                return false;
            }
        }
    }
    while (stack_count > 0U)
    {
        SearchToken top = stack[--stack_count];
        if (top.type == SEARCH_TOKEN_LPAREN || top.type == SEARCH_TOKEN_RPAREN)
        {
            search_set_error(error_message, error_capacity, "Mismatched parentheses.");
            return false;
        }
        if (output_count >= SEARCH_BOOLEAN_MAX_TOKENS)
        {
            search_set_error(error_message, error_capacity,
                             "Expression exceeds complexity limit (too many terms).");
            return false;
        }
        program->items[output_count++] = top;
    }
    program->count = output_count;
    search_set_error(error_message, error_capacity, NULL);
    return true;
}

static bool search_boolean_match_term(const Person *person, SearchPersonText *text,
                                      const char *term)
{
    if (!person || !text || !term)
    {
        return false;
    }
    if (term[0] == '\0')
    {
        return true;
    }
    if (strcmp(term, "alive") == 0)
    {
        return person->is_alive;
    }
    if (strcmp(term, "deceased") == 0)
    {
        return !person->is_alive;
    }
    if (strncmp(term, "id:", 3) == 0)
    {
        const char *value    = term + 3;
        char *end            = NULL;
        unsigned long parsed = strtoul(value, &end, 10);
        if (value == end)
        {
            return false;
        }
        return person->id == (uint32_t)parsed;
    }

    search_person_text_build(person, text);

    if (strncmp(term, "name:", 5) == 0)
    {
        const char *value = term + 5;
        return strstr(text->name, value) != NULL;
    }
    if (strncmp(term, "birth:", 6) == 0)
    {
        const char *value = term + 6;
        if (strstr(text->birth_date, value) != NULL)
        {
            return true;
        }
        return strstr(text->birth_location, value) != NULL;
    }
    if (strncmp(term, "death:", 6) == 0)
    {
        const char *value = term + 6;
        if (strstr(text->death_date, value) != NULL)
        {
            return true;
        }
        return strstr(text->death_location, value) != NULL;
    }
    if (strncmp(term, "location:", 9) == 0)
    {
        const char *value = term + 9;
        return strstr(text->locations, value) != NULL;
    }
    if (strncmp(term, "timeline:", 9) == 0)
    {
        const char *value = term + 9;
        return strstr(text->timeline, value) != NULL;
    }
    if (strncmp(term, "metadata:", 9) == 0)
    {
        const char *value = term + 9;
        return strstr(text->metadata, value) != NULL;
    }
    return strstr(text->combined, term) != NULL;
}

static bool search_boolean_evaluate(const SearchBooleanProgram *program, const Person *person,
                                    SearchPersonText *text)
{
    if (!program || !person || !text)
    {
        return false;
    }
    if (program->count == 0U)
    {
        return true;
    }
    bool stack[SEARCH_BOOLEAN_MAX_TOKENS];
    size_t sp = 0U;

    for (size_t index = 0U; index < program->count; ++index)
    {
        SearchToken token = program->items[index];
        switch (token.type)
        {
        case SEARCH_TOKEN_TERM:
            if (sp >= SEARCH_BOOLEAN_MAX_TOKENS)
            {
                return false;
            }
            stack[sp++] = search_boolean_match_term(person, text, token.text);
            break;
        case SEARCH_TOKEN_NOT:
            if (sp == 0U)
            {
                return false;
            }
            stack[sp - 1U] = !stack[sp - 1U];
            break;
        case SEARCH_TOKEN_AND:
        case SEARCH_TOKEN_OR:
            if (sp < 2U)
            {
                return false;
            }
            bool rhs       = stack[--sp];
            bool lhs       = stack[sp - 1U];
            stack[sp - 1U] = (token.type == SEARCH_TOKEN_AND) ? (lhs && rhs) : (lhs || rhs);
            break;
        default:
            return false;
        }
    }
    return (sp == 1U) ? stack[0U] : false;
}

static bool search_regex_match_here(const char *pattern, const char *text);

static bool search_regex_match_star(char preceding, const char *pattern, const char *text)
{
    do
    {
        if (search_regex_match_here(pattern, text))
        {
            return true;
        }
        if (*text == '\0')
        {
            break;
        }
        if (preceding != '.' && preceding != *text)
        {
            break;
        }
        text++;
    } while (*text != '\0');
    return false;
}

static bool search_regex_match_here(const char *pattern, const char *text)
{
    if (pattern[0] == '\0')
    {
        return true;
    }
    if (pattern[1] == '*')
    {
        return search_regex_match_star(pattern[0], pattern + 2, text);
    }
    if (pattern[0] == '$' && pattern[1] == '\0')
    {
        return *text == '\0';
    }
    if (*text != '\0' && (pattern[0] == '.' || pattern[0] == *text))
    {
        return search_regex_match_here(pattern + 1, text + 1);
    }
    return false;
}

static bool search_regex_match(const char *pattern, const char *text)
{
    if (!pattern || !text)
    {
        return false;
    }
    if (pattern[0] == '^')
    {
        return search_regex_match_here(pattern + 1, text);
    }
    do
    {
        if (search_regex_match_here(pattern, text))
        {
            return true;
        }
    } while (*text++ != '\0');
    return false;
}

static bool search_prepare_context(SearchEvaluationContext *context, const SearchFilter *filter)
{
    if (!context || !filter)
    {
        return false;
    }
    context->mode = filter->query_mode;
    if (context->mode < SEARCH_QUERY_MODE_SUBSTRING || context->mode > SEARCH_QUERY_MODE_REGEX)
    {
        context->mode = SEARCH_QUERY_MODE_SUBSTRING;
    }

    const char *expression = filter->query_expression;
    if (!expression || expression[0] == '\0')
    {
        context->has_expression        = false;
        context->substring[0]          = '\0';
        context->regex[0]              = '\0';
        context->boolean_program.count = 0U;
        return true;
    }

    context->has_expression = true;
    switch (context->mode)
    {
    case SEARCH_QUERY_MODE_SUBSTRING:
        lowercase_copy(context->substring, sizeof(context->substring), expression);
        break;
    case SEARCH_QUERY_MODE_BOOLEAN:
        if (!search_boolean_compile(expression, &context->boolean_program, NULL, 0U))
        {
            return false;
        }
        break;
    case SEARCH_QUERY_MODE_REGEX:
        lowercase_copy(context->regex, sizeof(context->regex), expression);
        break;
    default:
        context->has_expression = false;
        break;
    }
    return true;
}

static bool search_matches_person_advanced(const Person *person, const SearchFilter *filter,
                                           const SearchEvaluationContext *context)
{
    if (!context->has_expression)
    {
        return true;
    }

    switch (context->mode)
    {
    case SEARCH_QUERY_MODE_SUBSTRING:
        return search_match_substring(person, context);
    case SEARCH_QUERY_MODE_BOOLEAN:
    {
        SearchPersonText text;
        search_person_text_init(&text);
        return search_boolean_evaluate(&context->boolean_program, person, &text);
    }
    case SEARCH_QUERY_MODE_REGEX:
    {
        SearchPersonText text;
        search_person_text_init(&text);
        search_person_text_build(person, &text);
        return search_regex_match(context->regex, text.combined);
    }
    default:
        (void)filter;
        return true;
    }
}

static bool search_matches_person(const Person *person, const SearchFilter *filter,
                                  const SearchEvaluationContext *context)
{
    if (!person || !filter || !context)
    {
        return false;
    }

    bool is_alive = person->is_alive;
    if (is_alive && !filter_allows_alive(filter))
    {
        return false;
    }
    if (!is_alive && !filter_allows_deceased(filter))
    {
        return false;
    }

    if (filter->use_birth_year_range)
    {
        int birth_year = 0;
        if (!extract_year(person->dates.birth_date, &birth_year))
        {
            return false;
        }
        int min_year = filter->birth_year_min;
        int max_year = filter->birth_year_max;
        if (min_year > max_year)
        {
            int swap = min_year;
            min_year = max_year;
            max_year = swap;
        }
        if (birth_year < min_year || birth_year > max_year)
        {
            return false;
        }
    }

    return search_matches_person_advanced(person, filter, context);
}

static int search_compare_persons(const void *lhs, const void *rhs)
{
    const Person *const *left  = (const Person *const *)lhs;
    const Person *const *right = (const Person *const *)rhs;
    const Person *left_person  = *left;
    const Person *right_person = *right;

    char left_name[160];
    char right_name[160];
    if (!person_format_display_name(left_person, left_name, sizeof(left_name)))
    {
        (void)snprintf(left_name, sizeof(left_name), "Person %u", left_person->id);
    }
    if (!person_format_display_name(right_person, right_name, sizeof(right_name)))
    {
        (void)snprintf(right_name, sizeof(right_name), "Person %u", right_person->id);
    }
    int diff = string_casefold_compare(left_name, right_name);
    if (diff != 0)
    {
        return diff;
    }
    if (left_person->id < right_person->id)
    {
        return -1;
    }
    if (left_person->id > right_person->id)
    {
        return 1;
    }
    return 0;
}

size_t search_execute(const FamilyTree *tree, const SearchFilter *filter,
                      const Person **out_results, size_t capacity)
{
    if (!tree || !out_results || capacity == 0U)
    {
        return 0U;
    }

    SearchFilter resolved;
    if (!filter)
    {
        resolved.name_substring       = NULL;
        resolved.include_alive        = true;
        resolved.include_deceased     = true;
        resolved.use_birth_year_range = false;
        resolved.birth_year_min       = 0;
        resolved.birth_year_max       = 0;
        resolved.query_mode           = SEARCH_QUERY_MODE_SUBSTRING;
        resolved.query_expression     = NULL;
        filter                        = &resolved;
    }

    SearchFilter normalized = *filter;
    if (normalized.query_mode < SEARCH_QUERY_MODE_SUBSTRING ||
        normalized.query_mode > SEARCH_QUERY_MODE_REGEX)
    {
        normalized.query_mode = SEARCH_QUERY_MODE_SUBSTRING;
    }
    if (!normalized.query_expression || normalized.query_expression[0] == '\0')
    {
        normalized.query_expression = normalized.name_substring;
    }

    SearchEvaluationContext context;
    if (!search_prepare_context(&context, &normalized))
    {
        return 0U;
    }

    size_t stored = 0U;
    for (size_t index = 0U; index < tree->person_count; ++index)
    {
        const Person *person = tree->persons[index];
        if (!search_matches_person(person, &normalized, &context))
        {
            continue;
        }
        if (stored < capacity)
        {
            out_results[stored] = person;
        }
        stored++;
    }

    size_t count_to_sort = (stored < capacity) ? stored : capacity;
    if (count_to_sort > 1U)
    {
        qsort(out_results, count_to_sort, sizeof(out_results[0]), search_compare_persons);
    }
    return count_to_sort;
}

bool search_validate_expression(SearchQueryMode mode, const char *expression, char *error_message,
                                size_t error_capacity)
{
    if (!expression || expression[0] == '\0')
    {
        search_set_error(error_message, error_capacity, NULL);
        return true;
    }
    switch (mode)
    {
    case SEARCH_QUERY_MODE_BOOLEAN:
    {
        SearchBooleanProgram program;
        return search_boolean_compile(expression, &program, error_message, error_capacity);
    }
    case SEARCH_QUERY_MODE_REGEX:
        /* Current lightweight regex engine treats all patterns as literals except '.','*','^','$'.
         * No structural validation needed beyond length checks performed by callers. */
        search_set_error(error_message, error_capacity, NULL);
        return true;
    case SEARCH_QUERY_MODE_SUBSTRING:
    default:
        search_set_error(error_message, error_capacity, NULL);
        return true;
    }
}
