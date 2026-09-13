#include "statistics.h"

void statistics_init(TokenStatistics *stats)
{
    stats->keywords = 0;
    stats->identifiers = 0;
    stats->integer_literals = 0;
    stats->float_literals = 0;
    stats->string_literals = 0;
    stats->char_literals = 0;
    stats->operators = 0;
    stats->delimiters = 0;
    stats->lexical_errors = 0;
}

void statistics_add(TokenStatistics *stats, TokenType type)
{
    switch (type) {
        case TOKEN_KEYWORD:
            stats->keywords++;
            break;

        case TOKEN_IDENTIFIER:
            stats->identifiers++;
            break;

        case TOKEN_INTEGER_LITERAL:
            stats->integer_literals++;
            break;

        case TOKEN_FLOAT_LITERAL:
            stats->float_literals++;
            break;

        case TOKEN_STRING_LITERAL:
            stats->string_literals++;
            break;

        case TOKEN_CHAR_LITERAL:
            stats->char_literals++;
            break;

        case TOKEN_OPERATOR:
            stats->operators++;
            break;

        case TOKEN_DELIMITER:
            stats->delimiters++;
            break;

        case TOKEN_LEXICAL_ERROR:
            stats->lexical_errors++;
            break;

        case TOKEN_EOF:
            break;
    }
}