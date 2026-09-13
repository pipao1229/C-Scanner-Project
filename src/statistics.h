#ifndef STATISTICS_H
#define STATISTICS_H

#include "token.h"

typedef struct {
    unsigned long keywords;
    unsigned long identifiers;
    unsigned long integer_literals;
    unsigned long float_literals;
    unsigned long string_literals;
    unsigned long char_literals;
    unsigned long operators;
    unsigned long delimiters;
    unsigned long lexical_errors;
} TokenStatistics;

void statistics_init(TokenStatistics *stats);
void statistics_add(TokenStatistics *stats, TokenType type);

#endif