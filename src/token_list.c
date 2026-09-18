#include <stdlib.h>
#include "token_list.h"

#define INITIAL_CAPACITY 64

/* Initializes empty dynamic list for accumulated tokens */
void token_list_init(TokenList *list)
{
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* Appends token to dynamic list, doubling capacity upon exhaustion */
int token_list_add(TokenList *list, Token token)
{
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? INITIAL_CAPACITY : list->capacity * 2;
        Token *new_items = realloc(list->items, new_capacity * sizeof(Token));
        if (new_items == NULL) return 0;
        list->items = new_items;
        list->capacity = new_capacity;
    }

    list->items[list->count++] = token;
    return 1;
}

/* Frees dynamic memory allocated for stored tokens and their lexemes */
void token_list_free(TokenList *list)
{
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].lexeme);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}