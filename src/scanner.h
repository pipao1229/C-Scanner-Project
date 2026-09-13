#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

int scanner_init(const char *filename);
Token Get_Token(void);
void scanner_close(void);

extern int scanner_line;
extern int scanner_column;

#endif