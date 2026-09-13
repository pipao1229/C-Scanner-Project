#ifndef PRESENTATION_H
#define PRESENTATION_H

#include "source.h"
#include "statistics.h"
#include "token_list.h"

int presentation_generate(const char *output_dir,
                           const char *input_name,
                           const Source *source,
                           const TokenList *tokens,
                           const TokenStatistics *stats);

#endif