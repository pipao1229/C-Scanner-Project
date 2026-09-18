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

/* Stores a name-value pair for defined object-like macros */
typedef struct {
    char *name;
    char *value;
} Macro;

/* Dynamic table storing all active preprocessor definitions */
typedef struct {
    Macro *items;
    size_t count;
    size_t capacity;
} MacroTable;

/* Tracks macro expansion recursion to prevent infinite replacement loops */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} ActiveMacros;

/* Call stack tracking included files to detect circular dependencies */
typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} IncludeStack;

/* Function prototypes */
static char *copy_string(const char *text);

static void macro_table_init(MacroTable *table);
static void macro_table_free(MacroTable *table);
static int macro_define(MacroTable *table, const char *name, const char *value);
static const char *macro_find(const MacroTable *table, const char *name);

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

static int remove_comments(const char *input, size_t length, char **output, size_t *output_length);

static int process_file(FILE *output, const char *filename, MacroTable *macros,
                        ActiveMacros *active, IncludeStack *inc_stack);

static int process_buffer(FILE *output, const char *filename, const char *buffer,
                          size_t length, MacroTable *macros, ActiveMacros *active,
                          IncludeStack *inc_stack);

static int expand_text(FILE *output, const char *text, size_t length,
                       MacroTable *macros, ActiveMacros *active);

static int write_macro_value(FILE *output, const char *value, MacroTable *macros,
                             ActiveMacros *active);

static int parse_directive(const char *line, size_t length, char **directive, char **argument);

static char *trim_copy(const char *text, size_t length);

/* Allocates heap memory and duplicates a null-terminated string */
static char *copy_string(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);
    if (copy == NULL) return NULL;
    memcpy(copy, text, length + 1);
    return copy;
}

/* Initializes an empty macro symbol table */
static void macro_table_init(MacroTable *table)
{
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

/* Releases dynamic memory allocated for all macros in the table */
static void macro_table_free(MacroTable *table)
{
    for (size_t i = 0; i < table->count; i++) {
        free(table->items[i].name);
        free(table->items[i].value);
    }
    free(table->items);
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
}

/* Searches for a macro definition by name in the symbol table */
static const char *macro_find(const MacroTable *table, const char *name)
{
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            return table->items[i].value;
        }
    }
    return NULL;
}

/* Inserts or updates an object-like macro definition */
static int macro_define(MacroTable *table, const char *name, const char *value)
{
    for (size_t i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            char *new_value = copy_string(value);
            if (new_value == NULL) return 0;
            free(table->items[i].value);
            table->items[i].value = new_value;
            return 1;
        }
    }

    if (table->count == table->capacity) {
        size_t new_capacity = (table->capacity == 0) ? INITIAL_MACROS : table->capacity * 2;
        Macro *new_items = realloc(table->items, new_capacity * sizeof(Macro));
        if (new_items == NULL) return 0;
        table->items = new_items;
        table->capacity = new_capacity;
    }

    table->items[table->count].name = copy_string(name);
    table->items[table->count].value = copy_string(value);
    if (!table->items[table->count].name || !table->items[table->count].value) {
        free(table->items[table->count].name);
        free(table->items[table->count].value);
        return 0;
    }
    table->count++;
    return 1;
}

/* Initializes active macro recursion tracking list */
static void active_init(ActiveMacros *active)
{
    active->items = NULL;
    active->count = 0;
    active->capacity = 0;
}

/* Frees memory consumed by active macro expansion stack */
static void active_free(ActiveMacros *active)
{
    for (size_t i = 0; i < active->count; i++) {
        free(active->items[i]);
    }
    free(active->items);
    active->items = NULL;
    active->count = 0;
    active->capacity = 0;
}

/* Returns 1 if macro name is currently undergoing recursive expansion */
static int active_contains(const ActiveMacros *active, const char *name)
{
    for (size_t i = 0; i < active->count; i++) {
        if (strcmp(active->items[i], name) == 0) return 1;
    }
    return 0;
}

/* Pushes macro identifier to active expansion stack */
static int active_push(ActiveMacros *active, const char *name)
{
    if (active->count == active->capacity) {
        size_t new_capacity = (active->capacity == 0) ? INITIAL_ACTIVE : active->capacity * 2;
        char **new_items = realloc(active->items, new_capacity * sizeof(char *));
        if (new_items == NULL) return 0;
        active->items = new_items;
        active->capacity = new_capacity;
    }
    char *copy = copy_string(name);
    if (copy == NULL) return 0;
    active->items[active->count++] = copy;
    return 1;
}

/* Pops top macro identifier from active expansion stack */
static void active_pop(ActiveMacros *active)
{
    if (active->count == 0) return;
    active->count--;
    free(active->items[active->count]);
    active->items[active->count] = NULL;
}

/* Initializes stack used to trace nested file inclusions */
static void include_stack_init(IncludeStack *stack)
{
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

/* Frees resources allocated for file inclusion stack */
static void include_stack_free(IncludeStack *stack)
{
    for (size_t i = 0; i < stack->count; i++) {
        free(stack->items[i]);
    }
    free(stack->items);
    stack->items = NULL;
    stack->count = 0;
    stack->capacity = 0;
}

/* Checks if a file is already being processed to detect circular includes */
static int include_stack_contains(const IncludeStack *stack, const char *filename)
{
    for (size_t i = 0; i < stack->count; i++) {
        if (strcmp(stack->items[i], filename) == 0) return 1;
    }
    return 0;
}

/* Pushes included file path to call stack */
static int include_stack_push(IncludeStack *stack, const char *filename)
{
    if (stack->count == stack->capacity) {
        size_t new_capacity = (stack->capacity == 0) ? 8 : stack->capacity * 2;
        char **new_items = realloc(stack->items, new_capacity * sizeof(char *));
        if (new_items == NULL) return 0;
        stack->items = new_items;
        stack->capacity = new_capacity;
    }
    char *copy = copy_string(filename);
    if (copy == NULL) return 0;
    stack->items[stack->count++] = copy;
    return 1;
}

/* Pops finished include file from call stack */
static void include_stack_pop(IncludeStack *stack)
{
    if (stack->count == 0) return;
    stack->count--;
    free(stack->items[stack->count]);
    stack->items[stack->count] = NULL;
}

/* Checks if character can start an identifier ([a-zA-Z_]) */
static int is_identifier_start(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

/* Checks if character can continue an identifier ([a-zA-Z0-9_]) */
static int is_identifier_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Strips single-line and block comments while preserving newlines and string literals */
static int remove_comments(const char *input, size_t length, char **output, size_t *output_length)
{
    char *result = malloc(length + 1);
    size_t i = 0, position = 0;
    int in_string = 0, in_char = 0;

    if (result == NULL) return 0;

    while (i < length) {
        /* Preserve string literals without inspecting contents for comment symbols */
        if (in_string) {
            result[position++] = input[i];
            if (input[i] == '\\' && i + 1 < length) {
                result[position++] = input[i + 1];
                i += 2;
                continue;
            }
            if (input[i] == '"') in_string = 0;
            i++;
            continue;
        }

        /* Preserve character constants without comment processing */
        if (in_char) {
            result[position++] = input[i];
            if (input[i] == '\\' && i + 1 < length) {
                result[position++] = input[i + 1];
                i += 2;
                continue;
            }
            if (input[i] == '\'') in_char = 0;
            i++;
            continue;
        }

        if (input[i] == '"') {
            in_string = 1;
            result[position++] = input[i++];
            continue;
        }

        if (input[i] == '\'') {
            in_char = 1;
            result[position++] = input[i++];
            continue;
        }

        /* Skip single-line comment up to the newline character */
        if (input[i] == '/' && i + 1 < length && input[i + 1] == '/') {
            i += 2;
            while (i < length && input[i] != '\n') i++;
            continue;
        }

        /* Skip block comment while preserving interior newlines for line sync */
        if (input[i] == '/' && i + 1 < length && input[i + 1] == '*') {
            i += 2;
            while (i + 1 < length && !(input[i] == '*' && input[i + 1] == '/')) {
                if (input[i] == '\n') result[position++] = '\n';
                i++;
            }
            if (i + 1 < length) i += 2;
            continue;
        }

        result[position++] = input[i++];
    }

    result[position] = '\0';
    *output = result;
    *output_length = position;
    return 1;
}

/* Copies string slice trimming leading and trailing whitespace */
static char *trim_copy(const char *text, size_t length)
{
    size_t start = 0, end = length;

    while (start < end && isspace((unsigned char)text[start])) start++;
    while (end > start && isspace((unsigned char)text[end - 1])) end--;

    size_t result_length = end - start;
    char *result = malloc(result_length + 1);
    if (result == NULL) return NULL;

    memcpy(result, text + start, result_length);
    result[result_length] = '\0';
    return result;
}

/* Identifies and extracts preprocessor directive name and trailing argument */
static int parse_directive(const char *line, size_t length, char **directive, char **argument)
{
    size_t position = 0, start;
    *directive = NULL;
    *argument = NULL;

    while (position < length && isspace((unsigned char)line[position])) position++;
    if (position >= length || line[position] != '#') return 0;
    position++;

    while (position < length && isspace((unsigned char)line[position])) position++;
    start = position;

    while (position < length && isalpha((unsigned char)line[position])) position++;
    if (position == start) return 0;

    *directive = trim_copy(line + start, position - start);
    if (*directive == NULL) return -1;

    *argument = trim_copy(line + position, length - position);
    if (*argument == NULL) {
        free(*directive);
        *directive = NULL;
        return -1;
    }
    return 1;
}

/* Expands and writes macro value recursively to destination file */
static int write_macro_value(FILE *output, const char *value, MacroTable *macros, ActiveMacros *active)
{
    return expand_text(output, value, strlen(value), macros, active);
}

/* Substitutes known macro identifiers with their defined replacement strings */
static int expand_text(FILE *output, const char *text, size_t length,
                       MacroTable *macros, ActiveMacros *active)
{
    size_t i = 0;
    int in_string = 0, in_char = 0;

    while (i < length) {
        /* Write string literal characters without macro substitution */
        if (in_string) {
            if (fputc(text[i], output) == EOF) return 0;
            if (text[i] == '\\' && i + 1 < length) {
                if (fputc(text[i + 1], output) == EOF) return 0;
                i += 2;
                continue;
            }
            if (text[i] == '"') in_string = 0;
            i++;
            continue;
        }

        /* Write character constants without macro substitution */
        if (in_char) {
            if (fputc(text[i], output) == EOF) return 0;
            if (text[i] == '\\' && i + 1 < length) {
                if (fputc(text[i + 1], output) == EOF) return 0;
                i += 2;
                continue;
            }
            if (text[i] == '\'') in_char = 0;
            i++;
            continue;
        }

        if (text[i] == '"') {
            in_string = 1;
            if (fputc(text[i++], output) == EOF) return 0;
            continue;
        }

        if (text[i] == '\'') {
            in_char = 1;
            if (fputc(text[i++], output) == EOF) return 0;
            continue;
        }

        /* Recognize identifiers and attempt macro expansion */
        if (is_identifier_start(text[i])) {
            size_t start = i;
            while (i < length && is_identifier_char(text[i])) i++;
            size_t identifier_length = i - start;

            char *identifier = malloc(identifier_length + 1);
            if (identifier == NULL) return 0;
            memcpy(identifier, text + start, identifier_length);
            identifier[identifier_length] = '\0';

            const char *value = macro_find(macros, identifier);

            /* Do not expand if not defined or currently active in expansion call chain */
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

        if (fputc(text[i++], output) == EOF) return 0;
    }

    return 1;
}

/* Processes memory buffer line-by-line executing directives and macro expansion */
static int process_buffer(FILE *output, const char *filename, const char *buffer,
                          size_t length, MacroTable *macros, ActiveMacros *active,
                          IncludeStack *inc_stack)
{
    char *without_comments;
    size_t clean_length, position = 0;
    (void)filename;

    if (!remove_comments(buffer, length, &without_comments, &clean_length)) return 0;

    while (position < clean_length) {
        size_t line_start = position;
        while (position < clean_length && without_comments[position] != '\n') position++;
        size_t line_end = position;
        if (position < clean_length && without_comments[position] == '\n') position++;

        size_t line_length = line_end - line_start;
        char *directive = NULL, *argument = NULL;
        int directive_result = parse_directive(without_comments + line_start,
                                            line_length, &directive, &argument);

        if (directive_result == -1) {
            free(without_comments);
            return 0;
        }

        /* Execute recognized directives (#define, #include) */
        if (directive_result == 1) {
            if (strcmp(directive, "define") == 0) {
                size_t name_pos = 0, arg_len = strlen(argument);
                while (name_pos < arg_len && isspace((unsigned char)argument[name_pos])) name_pos++;
                if (name_pos >= arg_len || !is_identifier_start(argument[name_pos])) {
                    free(directive); free(argument); free(without_comments);
                    return 0;
                }

                size_t name_start = name_pos;
                while (name_pos < arg_len && is_identifier_char(argument[name_pos])) name_pos++;
                char *name = trim_copy(argument + name_start, name_pos - name_start);
                char *value = trim_copy(argument + name_pos, arg_len - name_pos);

                if (!name || !value || !macro_define(macros, name, value)) {
                    free(name); free(value); free(directive); free(argument); free(without_comments);
                    return 0;
                }
                free(name);
                free(value);
            } else if (strcmp(directive, "include") == 0) {
                size_t arg_len = strlen(argument);
                if (arg_len < 3 || (argument[0] != '"' && argument[0] != '<')) {
                    free(directive); free(argument); free(without_comments);
                    return 0;
                }

                char *include_name = trim_copy(argument + 1, arg_len - 2);
                if (!include_name) {
                    free(directive); free(argument); free(without_comments);
                    return 0;
                }

                /* Detect circular includes to prevent stack overflow and crash */
                if (include_stack_contains(inc_stack, include_name)) {
                    fprintf(stderr, "Error: Circular include detected for '%s'\n", include_name);
                    free(include_name); free(directive); free(argument); free(without_comments);
                    return 0;
                }

                if (!process_file(output, include_name, macros, active, inc_stack)) {
                    free(include_name); free(directive); free(argument); free(without_comments);
                    return 0;
                }
                free(include_name);
            }
            free(directive);
            free(argument);
            continue;
        }

        /* Expand regular source lines and write to output file */
        if (!expand_text(output, without_comments + line_start, line_length, macros, active)) {
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

/* Reads a source file into a memory buffer and dispatches it for processing */
static int process_file(FILE *output, const char *filename, MacroTable *macros,
                        ActiveMacros *active, IncludeStack *inc_stack)
{
    if (!include_stack_push(inc_stack, filename)) return 0;

    FILE *input = fopen(filename, "rb");
    if (input == NULL) {
        include_stack_pop(inc_stack);
        return 0;
    }

    fseek(input, 0, SEEK_END);
    long file_size = ftell(input);
    if (file_size < 0) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }
    fseek(input, 0, SEEK_SET);

    char *buffer = malloc((size_t)file_size + 1);
    if (buffer == NULL) {
        fclose(input);
        include_stack_pop(inc_stack);
        return 0;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)file_size, input);
    buffer[bytes_read] = '\0';
    fclose(input);

    int result = process_buffer(output, filename, buffer, bytes_read, macros, active, inc_stack);

    free(buffer);
    include_stack_pop(inc_stack);
    return result;
}

/* Entry point creating temporary file and orchestrating full preprocessing */
int preprocessor_run(const char *input_path, PreprocessedFile *output)
{
    char template[] = TEMP_TEMPLATE;
    MacroTable macros;
    ActiveMacros active;
    IncludeStack inc_stack;

    output->path = NULL;

    /* Create secure temporary file descriptor with non-hardcoded name */
    int fd = mkstemp(template);
    if (fd == -1) return 0;

    FILE *temp = fdopen(fd, "wb");
    if (temp == NULL) {
        close(fd);
        return 0;
    }

    macro_table_init(&macros);
    active_init(&active);
    include_stack_init(&inc_stack);

    int result = process_file(temp, input_path, &macros, &active, &inc_stack);
    if (fclose(temp) != 0) result = 0;

    macro_table_free(&macros);
    active_free(&active);
    include_stack_free(&inc_stack);

    if (!result) return 0;

    output->path = copy_string(template);
    return (output->path != NULL);
}

/* Frees heap-allocated path string without deleting the temporary file */
void preprocessor_cleanup(PreprocessedFile *output)
{
    free(output->path);
    output->path = NULL;
}