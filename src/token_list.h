#ifndef TOKEN_LIST_H
#define TOKEN_LIST_H

#include <stddef.h>

#include "token.h"

typedef struct {
    Token *items;
    size_t count;
    size_t capacity;
} TokenList;

void token_list_init(TokenList *list);
int token_list_add(TokenList *list, Token token);
void token_list_free(TokenList *list);

#endif