#ifndef TOKEN_H
#define TOKEN_H

/* Classification of lexical token categories for standard C syntax */
typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    TOKEN_OPERATOR,
    TOKEN_DELIMITER,
    TOKEN_LEXICAL_ERROR,
    TOKEN_EOF
} TokenType;

/* Represents a scanned token instance with its lexeme string and source coordinates */
typedef struct {
    TokenType type;
    char *lexeme;
    int line;
    int column;
} Token;

/* Returns string representation of a token type enum */
const char *token_type_name(TokenType type);

#endif