#include "token.h"

const char *token_type_name(TokenType type)
{
    switch (type) {
        case TOKEN_KEYWORD:
            return "KEYWORD";
        case TOKEN_IDENTIFIER:
            return "IDENTIFIER";
        case TOKEN_INTEGER_LITERAL:
            return "INTEGER_LITERAL";
        case TOKEN_FLOAT_LITERAL:
            return "FLOAT_LITERAL";
        case TOKEN_STRING_LITERAL:
            return "STRING_LITERAL";
        case TOKEN_CHAR_LITERAL:
            return "CHAR_LITERAL";
        case TOKEN_OPERATOR:
            return "OPERATOR";
        case TOKEN_DELIMITER:
            return "DELIMITER";
        case TOKEN_LEXICAL_ERROR:
            return "LEXICAL_ERROR";
        case TOKEN_EOF:
            return "EOF";
        default:
            return "UNKNOWN";
    }
}