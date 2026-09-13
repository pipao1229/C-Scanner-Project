%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "scanner.h"
#include "token.h"

int scanner_line = 1;
int scanner_column = 1;

static Token current_token;

static Token make_token(TokenType type, const char *lexeme);
static char *copy_lexeme(const char *text);
%}

%option noyywrap
%option nodefault

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

[ \t\r\n]+ {
    for (int i = 0; yytext[i] != '\0'; i++) {
        if (yytext[i] == '\n') {
            scanner_line++;
            scanner_column = 1;
        } else if (yytext[i] == '\t') {
            scanner_column += 4; /* Avance estandar de tab */
        } else {
            scanner_column++;
        }
    }
}

"//".* {
    scanner_column += yyleng;
}

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

"auto"|"break"|"case"|"char"|"const"|"continue"|"default"|"do"|"double"|"else"|"enum"|"extern"|"float"|"for"|"goto"|"if"|"int"|"long"|"register"|"return"|"short"|"signed"|"sizeof"|"static"|"struct"|"switch"|"typedef"|"union"|"unsigned"|"void"|"volatile"|"while"|"inline"|"restrict"|"_Bool"|"_Complex"|"_Imaginary" {
    current_token = make_token(TOKEN_KEYWORD, yytext);
    scanner_column += yyleng;
    return 1;
}

{HEX_INT}|{OCT_INT}|{DEC_INT} {
    current_token = make_token(TOKEN_INTEGER_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

{FLOAT_1}|{FLOAT_2}|{FLOAT_3} {
    current_token = make_token(TOKEN_FLOAT_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

{IDENT} {
    current_token = make_token(TOKEN_IDENTIFIER, yytext);
    scanner_column += yyleng;
    return 1;
}

\"([^\\\"]|\\.)*\" {
    current_token = make_token(TOKEN_STRING_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

\'([^\\\']|\\.)*\' {
    current_token = make_token(TOKEN_CHAR_LITERAL, yytext);
    scanner_column += yyleng;
    return 1;
}

"..." {
    current_token = make_token(TOKEN_DELIMITER, yytext);
    scanner_column += yyleng;
    return 1;
}

"->Composite"|"->" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

"<<"|">>"|"<="|">="|"=="|"!="|"&&"|"||"|"++"|"--"|"+="|"-="|"*="|"/="|"%="|"<<="|">>="|"&="|"^="|"|=" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

"+"|"-"|"*"|"/"|"%"|"="|"<"|">"|"!"|"&"|"|"|"^"|"~"|"?" {
    current_token = make_token(TOKEN_OPERATOR, yytext);
    scanner_column += yyleng;
    return 1;
}

"("|")"|"["|"]"|"{"|"}"|";"|","|"."|":" {
    current_token = make_token(TOKEN_DELIMITER, yytext);
    scanner_column += yyleng;
    return 1;
}

. {
    current_token = make_token(TOKEN_LEXICAL_ERROR, yytext);
    scanner_column += yyleng;
    return 1;
}

%%

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

static Token make_token(TokenType type, const char *lexeme)
{
    Token token;

    token.type = type;
    token.lexeme = copy_lexeme(lexeme);
    token.line = scanner_line;
    token.column = scanner_column;

    return token;
}

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

Token Get_Token(void)
{
    current_token.type = TOKEN_EOF;
    current_token.lexeme = NULL;

    yylex();

    return current_token;
}

void scanner_close(void)
{
    if (yyin != NULL) {
        fclose(yyin);
        yyin = NULL;
    }
}