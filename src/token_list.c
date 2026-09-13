#include <stdlib.h>

#include "token_list.h"

#define INITIAL_CAPACITY 64

void token_list_init(TokenList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int token_list_add(TokenList *list, Token token)
{
    if (list->count == list->capacity) {
        size_t new_capacity;
        Token *new_items;

        if (list->capacity == 0) {
            new_capacity = INITIAL_CAPACITY;
        } else {
            new_capacity = list->capacity * 2;
        }

        new_items = realloc(list->items,
                            new_capacity * sizeof(Token));

        if (new_items == NULL) {
            return 0;
        }

        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count] = token;
    list->count++;

    return 1;
}

void token_list_free(TokenList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++) {
        free(list->items[i].lexeme);
    }

    free(list->items);

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}