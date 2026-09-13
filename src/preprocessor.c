#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "preprocessor.h"

#define TEMP_TEMPLATE "/tmp/scanner_preprocessed_XXXXXX"
#define INITIAL_MACROS 32
#define INITIAL_ACTIVE 16

typedef struct {
    char *name;
    char *value;
} Macro;

typedef struct {
    Macro *items;
    size_t count;
    size_t capacity;
} MacroTable;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} ActiveMacros;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} IncludeStack;

static char *copy_string(const char *text);

static void macro_table_init(MacroTable *table);
static void macro_table_free(MacroTable *table);
static int macro_define(MacroTable *table,
                        const char *name,
                        const char *value);
static const char *macro_find(const MacroTable *table,
                              const char *name);

static void active_init(ActiveMacros *active);
static void active_free(ActiveMacros *active);
static int active_contains(const ActiveMacros *active, const char *name);
static int active_push(ActiveMacros *active, const char *name);
static void active_pop(ActiveMacros *active);

static void include_stack_init(IncludeStack *stack);
static void include_stack_free(IncludeStack *stack);
static int include_stack_contains(const IncludeStack *stack, const char *filename);
static int include_stack_push(IncludeStack *stack, const char *filename);
static void include_stack_pop(IncludeStack *stack);

static int is_identifier_start(char c);
static int is_identifier_char(char c);

static int remove_comments(const char *input,
                           size_t length,
                           char **output,
                           size_t *output_length);

static int process_file(FILE *output,
                        const char *filename,
                        MacroTable *macros,
                        ActiveMacros *active,
                        IncludeStack *inc_stack);

static int process_buffer(FILE *output,
                          const char *filename,
                          const char *buffer,
                          size_t length,
                          MacroTable *macros,
                          ActiveMacros *active,
                          IncludeStack *inc_stack);

static int expand_text(FILE *output,
                       const char *text,
                       size_t length,
                       MacroTable *macros,
                       ActiveMacros *active);

static int write_macro_value(FILE *output,
                             const char *value,
                             MacroTable *macros,
                             ActiveMacros *active);

static int parse_directive(const char *line,
                           size_t length,
                           char **directive,
                           char **argument);

static char *trim_copy(const char *text, size_t length);

/* Implementaciones */

static char *copy_string(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);

    return copy;
}

static void macro_table_init(MacroTable *table)
{
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

static void macro_table_free(MacroTable *table)
{
    size_t i;

    for (i = 0; i < table->count; i++) {
        free(table->items[i].name);
        free(table->items[i].value);
    }

    free(table->items);

    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

static const char *macro_find(const MacroTable *table, const char *name)
{
    size_t i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            return table->items[i].value;
        }
    }

    return NULL;
}

static int macro_define(MacroTable *table,
                        const char *name,
                        const char *value)
{
    size_t i;

    for (i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            char *new_value = copy_string(value);

            if (new_value == NULL) {
                return 0;
            }

            free(table->items[i].value);
            table->items[i].value = new_value;

            return 1;
        }
    }

    if (table->count == table->capacity) {
        size_t new_capacity;
        Macro *new_items;

        if (table->capacity == 0) {
            new_capacity = INITIAL_MACROS;
        } else {
            new_capacity = table->capacity * 2;
        }

        new_items = realloc(table->items,
                            new_capacity * sizeof(Macro));

        if (new_items == NULL) {
            return 0;
        }

        table->items = new_items;
        table->capacity = new_capacity;
    }

    table->items[table->count].name = copy_string(name);
    table->items[table->count].value = copy_string(value);

    if (table->items[table->count].name == NULL ||
        table->items[table->count].value == NULL) {
        free(table->items[table->count].name);
        free(table->items[table->count].value);
        return 0;
    }

    table->count++;

    return 1;
}

static void active_init(ActiveMacros *active)
{
    active->items = NULL;
    active->count = 0;
    active->capacity = 0;
}

static void active_free(ActiveMacros *active)
{
    size_t i;

    for (i = 0; i < active->count; i++) {
        free(active->items[i]);
    }

    free(active->items);

    active->items = NULL;
    active->count = 0;
    active->capacity = 0;
}

static int active_contains(const ActiveMacros *active, const char *name)
{
    size_t i;

    for (i = 0; i < active->count; i++) {
        if (strcmp(active->items[i], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int active_push(ActiveMacros *active, const char *name)
{
    char *copy;

    if (active->count == active->capacity) {
        size_t new_capacity;
        char **new_items;

        if (active->capacity == 0) {
            new_capacity = INITIAL_ACTIVE;
        } else {
            new_capacity = active->capacity * 2;
        }

        new_items = realloc(active->items,
                            new_capacity * sizeof(char *));

        if (new_items == NULL) {
            return 0;
        }

        active->items = new_items;
        active->capacity = new_capacity;
    }

    copy = copy_string(name);

    if (copy == NULL) {
        return 0;
    }

    active->items[active->count] = copy;
    active->count++;

    return 1;
}

static void active_pop(ActiveMacros *active)
{
    if (active->count == 0) {
        return;
    }

    active->count--;

    free(active->items[active->count]);
    active->items[active->count] = NULL;
}

static void include_stack_init(IncludeStack *stack)
{
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static void include_stack_free(IncludeStack *stack)
{
    size_t i;

    for (i = 0; i < stack->count; i++) {
        free(stack->items[i]);
    }

    free(stack->items);

    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

static int include_stack_contains(const IncludeStack *stack, const char *filename)
{
    size_t i;

    for (i = 0; i < stack->count; i++) {
        if (strcmp(stack->items[i], filename) == 0) {
            return 1;
        }
    }

    return 0;
}

static int include_stack_push(IncludeStack *stack, const char *filename)
{
    char *copy;

    if (stack->count == stack->capacity) {
        size_t new_capacity = (stack->capacity == 0) ? 8 : stack->capacity * 2;
        char **new_items = realloc(stack->items, new_capacity * sizeof(char *));

        if (new_items == NULL) {
            return 0;
        }

        stack->items = new_items;
        stack->capacity = new_capacity;
    }

    copy = copy_string(filename);

    if (copy == NULL) {
        return 0;
    }

    stack->items[stack->count] = copy;
    stack->count++;

    return 1;
}

static void include_stack_pop(IncludeStack *stack)
{
    if (stack->count == 0) {
        return;
    }

    stack->count--;
    free(stack->items[stack->count]);
    stack->items[stack->count] = NULL;
}

static int is_identifier_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_identifier_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static int remove_comments(const char *input,
                           size_t length,
                           char **output,
                           size_t *output_length)
{
    char *result;
    size_t i = 0;
    size_t position = 0;
    int in_string = 0;
    int in_char = 0;

    result = malloc(length + 1);

    if (result == NULL) {
        return 0;
    }

    while (i < length) {
        if (in_string) {
            result[position++] = input[i];

            if (input[i] == '\\' && i + 1 < length) {
                result[position++] = input[i + 1];
                i += 2;
                continue;
            }

            if (input[i] == '"') {
                in_string = 0;
            }

            i++;
            continue;
        }

        if (in_char) {
            result[position++] = input[i];

            if (input[i] == '\\' && i + 1 < length) {
                result[position++] = input[i + 1];
                i += 2;
                continue;
            }

            if (input[i] == '\'') {
                in_char = 0;
            }

            i++;
            continue;
        }

        if (input[i] == '"') {
            in_string = 1;
            result[position++] = input[i];
            i++;
            continue;
        }

        if (input[i] == '\'') {
            in_char = 1;
            result[position++] = input[i];
            i++;
            continue;
        }

        if (input[i] == '/' && i + 1 < length && input[i + 1] == '/') {
            i += 2;

            while (i < length && input[i] != '\n') {
                i++;
            }

            continue;
        }

        if (input[i] == '/' && i + 1 < length && input[i + 1] == '*') {
            i += 2;

            while (i + 1 < length &&
                   !(input[i] == '*' && input[i + 1] == '/')) {
                if (input[i] == '\n') {
                    result[position++] = '\n';
                }

                i++;
            }

            if (i + 1 < length) {
                i += 2;
            }

            continue;
        }

        result[position++] = input[i];
        i++;
    }

    result[position] = '\0';

    *output = result;
    *output_length = position;

    return 1;
}

static char *trim_copy(const char *text, size_t length)
{
    size_t start = 0;
    size_t end = length;

    while (start < end && isspace((unsigned char)text[start])) {
        start++;
    }

    while (end > start && isspace((unsigned char)text[end - 1])) {
        end--;
    }

    {
        size_t result_length = end - start;
        char *result = malloc(result_length + 1);

        if (result == NULL) {
            return NULL;
        }

        memcpy(result, text + start, result_length);
        result[result_length] = '\0';

        return result;
    }
}

static int parse_directive(const char *line,
                           size_t length,
                           char **directive,
                           char **argument)
{
    size_t position = 0;
    size_t start;

    *directive = NULL;
    *argument = NULL;

    while (position < length && isspace((unsigned char)line[position])) {
        position++;
    }

    if (position >= length || line[position] != '#') {
        return 0;
    }

    position++;

    while (position < length && isspace((unsigned char)line[position])) {
        position++;
    }

    start = position;

    while (position < length && isalpha((unsigned char)line[position])) {
        position++;
    }

    if (position == start) {
        return 0;
    }

    *directive = trim_copy(line + start, position - start);

    if (*directive == NULL) {
        return -1;
    }

    *argument = trim_copy(line + position, length - position);

    if (*argument == NULL) {
        free(*directive);
        *directive = NULL;
        return -1;
    }

    return 1;
}

static int write_macro_value(FILE *output,
                             const char *value,
                             MacroTable *macros,
                             ActiveMacros *active)
{
    return expand_text(output,
                       value,
                       strlen(value),
                       macros,
                       active);
}

static int expand_text(FILE *output,
                       const char *text,
                       size_t length,
                       MacroTable *macros,
                       ActiveMacros *active)
{
    size_t i = 0;
    int in_string = 0;
    int in_char = 0;

    while (i < length) {
        if (in_string) {
            if (fputc(text[i], output) == EOF) {
                return 0;
            }

            if (text[i] == '\\' && i + 1 < length) {
                if (fputc(text[i + 1], output) == EOF) {
                    return 0;
                }

                i += 2;
                continue;
            }

            if (text[i] == '"') {
                in_string = 0;
            }

            i++;
            continue;
        }

        if (in_char) {
            if (fputc(text[i], output) == EOF) {
                return 0;
            }

            if (text[i] == '\\' && i + 1 < length) {
                if (fputc(text[i + 1], output) == EOF) {
                    return 0;
                }

                i += 2;
                continue;
            }

            if (text[i] == '\'') {
                in_char = 0;
            }

            i++;
            continue;
        }

        if (text[i] == '"') {
            in_string = 1;

            if (fputc(text[i], output) == EOF) {
                return 0;
            }

            i++;
            continue;
        }

        if (text[i] == '\'') {
            in_char = 1;

            if (fputc(text[i], output) == EOF) {
                return 0;
            }

            i++;
            continue;
        }

        if (is_identifier_start(text[i])) {
            size_t start = i;
            size_t identifier_length;
            char *identifier;
            const char *value;

            i++;

            while (i < length && is_identifier_char(text[i])) {
                i++;
            }

            identifier_length = i - start;

            identifier = malloc(identifier_length + 1);

            if (identifier == NULL) {
                return 0;
            }

            memcpy(identifier, text + start, identifier_length);
            identifier[identifier_length] = '\0';

            value = macro_find(macros, identifier);

            if (value == NULL || active_contains(active, identifier)) {
                if (fwrite(identifier, 1, identifier_length, output) != identifier_length) {
                    free(identifier);
                    return 0;
                }

                free(identifier);
                continue;
            }

            if (!active_push(active, identifier)) {
                free(identifier);
                return 0;
            }

            if (!write_macro_value(output, value, macros, active)) {
                active_pop(active);
                free(identifier);
                return 0;
            }

            active_pop(active);
            free(identifier);

            continue;
        }

        if (fputc(text[i], output) == EOF) {
            return 0;
        }

        i++;
    }

    return 1;
}

static int process_buffer(FILE *output,
                          const char *filename,
                          const char *buffer,
                          size_t length,
                          MacroTable *macros,
                          ActiveMacros *active,
                          IncludeStack *inc_stack)
{
    char *without_comments;
    size_t clean_length;
    size_t position = 0;

    (void)filename;

    if (!remove_comments(buffer, length, &without_comments, &clean_length)) {
        return 0;
    }

    while (position < clean_length) {
        size_t line_start = position;
        size_t line_length;
        size_t line_end;
        char *directive = NULL;
        char *argument = NULL;
        int directive_result;

        while (position < clean_length && without_comments[position] != '\n') {
            position++;
        }

        line_end = position;

        if (position < clean_length && without_comments[position] == '\n') {
            position++;
        }

        line_length = line_end - line_start;

        directive_result = parse_directive(
            without_comments + line_start,
            line_length,
            &directive,
            &argument
        );

        if (directive_result == -1) {
            free(without_comments);
            return 0;
        }

        if (directive_result == 1) {
            if (strcmp(directive, "define") == 0) {
                size_t name_position = 0;
                size_t argument_length = strlen(argument);
                char *name;
                char *value;

                while (name_position < argument_length &&
                       isspace((unsigned char)argument[name_position])) {
                    name_position++;
                }

                if (name_position >= argument_length ||
                    !is_identifier_start(argument[name_position])) {
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                {
                    size_t name_start = name_position;

                    name_position++;

                    while (name_position < argument_length &&
                           is_identifier_char(argument[name_position])) {
                        name_position++;
                    }

                    name = trim_copy(argument + name_start, name_position - name_start);

                    if (name == NULL) {
                        free(directive);
                        free(argument);
                        free(without_comments);
                        return 0;
                    }
                }

                if (name_position < argument_length &&
                    argument[name_position] == '(') {
                    free(name);
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                value = trim_copy(argument + name_position, argument_length - name_position);

                if (value == NULL || !macro_define(macros, name, value)) {
                    free(name);
                    free(value);
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                free(name);
                free(value);
            } else if (strcmp(directive, "include") == 0) {
                size_t argument_length = strlen(argument);
                char *include_name = NULL;

                if (argument_length < 3 ||
                    (argument[0] != '"' && argument[0] != '<')) {
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                if ((argument[0] == '"' && argument[argument_length - 1] != '"') ||
                    (argument[0] == '<' && argument[argument_length - 1] != '>')) {
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                include_name = trim_copy(argument + 1, argument_length - 2);

                if (include_name == NULL) {
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                /* Deteccion de recursion/inclusion circular */
                if (include_stack_contains(inc_stack, include_name)) {
                    fprintf(stderr, "Error: Inclusion circular detectada en '%s'\n", include_name);
                    free(include_name);
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                if (!process_file(output, include_name, macros, active, inc_stack)) {
                    free(include_name);
                    free(directive);
                    free(argument);
                    free(without_comments);
                    return 0;
                }

                free(include_name);
            } else {
                free(directive);
                free(argument);
                free(without_comments);
                return 0;
            }

            free(directive);
            free(argument);

            continue;
        }

        if (!expand_text(output,
                         without_comments + line_start,
                         line_length,
                         macros,
                         active)) {
            free(without_comments);
            return 0;
        }

        if (position > line_end && without_comments[line_end] == '\n') {
            if (fputc('\n', output) == EOF) {
                free(without_comments);
                return 0;
            }
        }
    }

    free(without_comments);

    return 1;
}

static int process_file(FILE *output,
                        const char *filename,
                        MacroTable *macros,
                        ActiveMacros *active,
                        IncludeStack *inc_stack)
{
    FILE *input;
    char *buffer;
    long file_size;
    size_t bytes_read;
    int result;

    if (!include_stack_push(inc_stack, filename)) {
        return 0;
    }

    input = fopen(filename, "rb");

    if (input == NULL) {
        include_stack_pop(inc_stack);
        return 0;
    }

    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    file_size = ftell(input);

    if (file_size < 0) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    if (fseek(input, 0, SEEK_SET) != 0) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    buffer = malloc((size_t)file_size + 1);

    if (buffer == NULL) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    bytes_read = fread(buffer, 1, (size_t)file_size, input);

    if (bytes_read != (size_t)file_size && ferror(input)) {
        free(buffer);
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    buffer[bytes_read] = '\0';
    fclose(input);

    result = process_buffer(output,
                            filename,
                            buffer,
                            bytes_read,
                            macros,
                            active,
                            inc_stack);

    free(buffer);
    include_stack_pop(inc_stack);

    return result;
}

int preprocessor_run(const char *input_path, PreprocessedFile *output)
{
    char template[] = TEMP_TEMPLATE;
    MacroTable macros;
    ActiveMacros active;
    IncludeStack inc_stack;
    FILE *temp;
    int fd;
    int result;

    output->path = NULL;

    fd = mkstemp(template);

    if (fd == -1) {
        return 0;
    }

    temp = fdopen(fd, "wb");

    if (temp == NULL) {
        close(fd);
        return 0;
    }

    macro_table_init(&macros);
    active_init(&active);
    include_stack_init(&inc_stack);

    result = process_file(temp,
                          input_path,
                          &macros,
                          &active,
                          &inc_stack);

    if (fclose(temp) != 0) {
        result = 0;
    }

    macro_table_free(&macros);
    active_free(&active);
    include_stack_free(&inc_stack);

    if (!result) {
        free(output->path);
        output->path = NULL;
        return 0;
    }

    output->path = copy_string(template);

    if (output->path == NULL) {
        return 0;
    }

    return 1;
}

void preprocessor_cleanup(PreprocessedFile *output)
{
    free(output->path);
    output->path = NULL;
}