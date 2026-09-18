%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "scanner.h"
#include "token.h"

/* Tracks scanner cursor coordinates across scanned lines and columns. */
int scanner_line = 1;
int scanner_column = 1;

static Token current_token;

static Token make_token(TokenType type, const char *lexeme);
static char *copy_lexeme(const char *text);
%}

%option noyywrap
%option nodefault
%option nounput
%option noinput

DIGIT       [0-9]
HEX_DIGIT   [0-9a-fA-F]
OCT_DIGIT   [0-7]
LETTER      [a-zA-Z_]
IDENT       {LETTER}({LETTER}|{DIGIT})*

INT_SUFFIX  ([uU][lL]?|[uU](ll|LL)?|[lL][uU]?|(ll|LL)[uU]?)
FLOAT_EXP   ([eE][+-]?{DIGIT}+)
FLOAT_SUFF  [fFlL]

HEX_INT     0[xX]{HEX_DIGIT}+{INT_SUFFIX}?
OCT_INT     0{OCT_DIGIT}+{INT_SUFFIX}?
DEC_INT     {DIGIT}+{INT_SUFFIX}?

FLOAT_1     {DIGIT}+"."{DIGIT}*{FLOAT_EXP}?{FLOAT_SUFF}?
FLOAT_2     "."{DIGIT}+{FLOAT_EXP}?{FLOAT_SUFF}?
FLOAT_3     {DIGIT}+{FLOAT_EXP}{FLOAT_SUFF}?

%%

    /* Update line and column counters on whitespace. */
[ \t\r\n]+ {
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            scanner_line++;
            scanner_column = 1;
        } else if (yytext[i] == '\t') {
            scanner_column += 4;
        } else {
            scanner_column++;
        }
    }
}

    /* Fallback comment skipper to preserve correct column offsets. */
"//".* {
    scanner_column += yyleng;
}

    /* Fallback block comment skipper tracking interior lines. */
"/*"([^*]|\*+[^*/])*\*+"/" {
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            scanner_line++;
            scanner_column = 1;
        } else {
            scanner_column++;
        }
    }
}

    /* Standard C reserved keywords including modern C99 standard additions. */
"auto"|"break"|"case"|"char"|"const"|"continue"|"default"|"do"|"double"|"else"|"enum"|"extern"|"float"|"for"|"goto"|"if"|"int"|"long"|"register"|"return"|"short"|"signed"|"sizeof"|"static"|"struct"|"switch"|"typedef"|"union"|"unsigned"|"void"|"volatile"|"while"|"inline"|"restrict"|"_Bool"|"_Complex"|"_Imaginary" {
    current_token = make_token(TOKEN_KEYWORD, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Decimal, octal, and hexadecimal integer constants. */
{HEX_INT}|{OCT_INT}|{DEC_INT} {
    current_token = make_token(TOKEN_INTEGER_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Floating-point constants with standard decimals, exponents, or suffixes. */
{FLOAT_1}|{FLOAT_2}|{FLOAT_3} {
    current_token = make_token(TOKEN_FLOAT_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* User-defined identifiers matching [a-zA-Z_][a-zA-Z0-9_]*. */
{IDENT} {
    current_token = make_token(TOKEN_IDENTIFIER, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* String literals enclosed in double quotes handling escape sequences. */
\"([^\\\"]|\\.)*\" {
    current_token = make_token(TOKEN_STRING_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Character constants enclosed in single quotes handling escape sequences. */
\'([^\\\']|\\.)*\' {
    current_token = make_token(TOKEN_CHAR_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Variadic function delimiter (...). */
"..." {
    current_token = make_token(TOKEN_DELIMITER, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Structure pointer operator (->). */
"->" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Compound assignment, comparison, and bit-shift operators. */
"<<"|">>"|"<="|">="|"=="|"!="|"&&"|"||"|"++"|"--"|"+="|"-="|"*="|"/="|"%="|"<<="|">>="|"&="|"^="|"|=" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Single-character arithmetic, logical, and relational operators. */
"+"|"-"|"*"|"/"|"%"|"="|"<"|">"|"!"|"&"|"|"|"^"|"~"|"?" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Structural punctuation and grouping delimiters. */
"("|")"|"["|"]"|"{"|"}"|";"|","|"."|":" {
    current_token = make_token(TOKEN_DELIMITER, yytext);
    scanner_column += yyleng;
    return 1;
}

    /* Unrecognized characters classified as lexical errors. */
. {
    current_token = make_token(TOKEN_LEXICAL_ERROR, yytext);
    scanner_column += yyleng;
    return 1;
}

%%

/* Duplicates lexeme string using dynamic memory allocation. */
static char *copy_lexeme(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

/* Constructs a Token instance with classification and coordinate metadata. */
static Token make_token(TokenType type, const char *lexeme)
{
    Token token;

    token.type = type;
    token.lexeme = copy_lexeme(lexeme);
    token.line = scanner_line;
    token.column = scanner_column;

    return token;
}

/* Opens input file stream and initializes scanner state. */
int scanner_init(const char *filename)
{
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        return 0;
    }

    yyin = file;
    scanner_line = 1;
    scanner_column = 1;

    return 1;
}

/* Primary API function requested in specification to retrieve next token. */
Token Get_Token(void)
{
    current_token.type = TOKEN_EOF;
    current_token.lexeme = NULL;

    yylex();

    return current_token;
}

/* Closes input file stream and resets scanner pointer. */
void scanner_close(void)
{
    if (yyin != NULL) {
        fclose(yyin);
        yyin = NULL;
    }
}