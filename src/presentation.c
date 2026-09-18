#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/wait.h>

#include "presentation.h"

#define SOURCE_LINES_PER_FRAME 16

/* Escapes LaTeX reserved characters and control codes safely for TeX parsing */
static void latex_escape_char(FILE *file, unsigned char c)
{
    switch (c) {
        case '\\': fprintf(file, "\\textbackslash{}"); break;
        case '{':  fprintf(file, "\\{"); break;
        case '}':  fprintf(file, "\\}"); break;
        case '#':  fprintf(file, "\\#"); break;
        case '$':  fprintf(file, "\\$"); break;
        case '%':  fprintf(file, "\\%%"); break;
        case '&':  fprintf(file, "\\&"); break;
        case '_':  fprintf(file, "\\_"); break;
        case '^':  fprintf(file, "\\textasciicircum{}"); break;
        case '~':  fprintf(file, "\\textasciitilde{}"); break;
        case '<':  fprintf(file, "\\textless{}"); break;
        case '>':  fprintf(file, "\\textgreater{}"); break;
        case '\t': fprintf(file, "\\hspace*{0.5cm}"); break;
        case ' ':  fprintf(file, "\\hspace*{0.18em}"); break;
        case '\r': break;
        default:
            if (c < 32 || c >= 127) {
                fprintf(file, "\\textcolor{errorcolor}{\\textbackslash{}x%02X}", c);
            } else {
                fputc(c, file);
            }
            break;
    }
}

/* Iterates and escapes an entire null-terminated text string for LaTeX */
static void latex_escape_text(FILE *file, const char *text)
{
    if (text == NULL) return;
    for (size_t i = 0; text[i] != '\0'; i++) {
        latex_escape_char(file, (unsigned char)text[i]);
    }
}

/* Maps token type enumeration to corresponding LaTeX visual styling macro */
static const char *token_macro(TokenType type)
{
    switch (type) {
        case TOKEN_KEYWORD:         return "KW";
        case TOKEN_IDENTIFIER:      return "ID";
        case TOKEN_INTEGER_LITERAL: return "INT";
        case TOKEN_FLOAT_LITERAL:   return "FLOAT";
        case TOKEN_STRING_LITERAL:  return "STR";
        case TOKEN_CHAR_LITERAL:    return "CHAR";
        case TOKEN_OPERATOR:        return "OP";
        case TOKEN_DELIMITER:       return "DELIM";
        case TOKEN_LEXICAL_ERROR:   return "ERR";
        default:                    return NULL;
    }
}

/* Writes highlighted token lexeme enclosed in its respective Beamer macro */
static void write_token(FILE *file, const Token *token)
{
    const char *macro = token_macro(token->type);
    if (macro == NULL || token->lexeme == NULL) return;

    fprintf(file, "\\%s{", macro);
    latex_escape_text(file, token->lexeme);
    fprintf(file, "}");
}

/* Renders a single source code line, matching character positions against recognized tokens */
static void write_source_line(FILE *file, const Source *source, size_t start,
                              size_t end, int line, const TokenList *tokens, size_t *token_index)
{
    size_t position = start, column = 1;

    while (position < end) {
        const Token *token = NULL;

        while (*token_index < tokens->count) {
            const Token *candidate = &tokens->items[*token_index];
            if (candidate->type == TOKEN_EOF || candidate->line < line) {
                (*token_index)++;
                continue;
            }
            if (candidate->line > line) break;
            token = candidate;
            break;
        }

        /* If cursor matches a token origin, render token and advance offset */
        if (token != NULL && token->line == line && token->column == (int)column) {
            size_t token_length = strlen(token->lexeme);
            write_token(file, token);
            position += token_length;
            column += token_length;
            (*token_index)++;
            continue;
        }

        latex_escape_char(file, (unsigned char)source->content[position++]);
        column++;
    }
    fputc('\n', file);
}

/* Computes total newline-delimited line count in source buffer */
static int source_line_count(const Source *source)
{
    int lines = 1;
    if (source->length == 0) return 0;

    for (size_t i = 0; i < source->length; i++) {
        if (source->content[i] == '\n') lines++;
    }
    if (source->content[source->length - 1] == '\n') lines--;
    return lines;
}

/* Renders a subset of source lines bounded by first_line and last_line */
static void write_source_frame(FILE *file, const Source *source, const TokenList *tokens,
                            int first_line, int last_line)
{
    size_t start = 0, position = 0, token_index = 0;
    int line = 1;

    while (line < first_line && position < source->length) {
        if (source->content[position] == '\n') line++;
        position++;
    }

    start = position;

    while (position < source->length && line <= last_line) {
        if (source->content[position] == '\n') {
            if (line >= first_line) {
                write_source_line(file, source, start, position, line, tokens, &token_index);
            }
            position++;
            start = position;
            line++;
        } else {
            position++;
        }
    }

    if (start < source->length && line >= first_line && line <= last_line) {
        write_source_line(file, source, start, source->length, line, tokens, &token_index);
    }
}

/* Helper that detects if a source line is a natural boundary (empty line or closing brace) */
static int is_natural_breakpoint(const Source *source, int line_num)
{
    size_t pos = 0;
    int cur_line = 1;

    while (pos < source->length && cur_line < line_num) {
        if (source->content[pos] == '\n') cur_line++;
        pos++;
    }

    /* Skip leading whitespace on the target line */
    while (pos < source->length && (source->content[pos] == ' ' || source->content[pos] == '\t')) {
        pos++;
    }

    if (pos >= source->length) return 1;

    /* Breakpoint if line is empty or closes a top-level block '}' */
    if (source->content[pos] == '\n' || source->content[pos] == '}') {
        return 1;
    }

    return 0;
}

/* Generates paginated Beamer slides containing syntax-highlighted preprocessed source */
static void write_source_frames(FILE *file,
                                const Source *source,
                                const TokenList *tokens)
{
    int total_lines = source_line_count(source);

    if (total_lines == 0) {
        fprintf(file,
            "\\begin{frame}[fragile]{Source Entering the Scanner}\n"
            "\\tiny\\ttfamily\\textit{Empty preprocessed source}\n"
            "\\end{frame}\n\n");
        return;
    }

    int first_line = 1;
    while (first_line <= total_lines) {
        int target_last = first_line + SOURCE_LINES_PER_FRAME - 1;
        if (target_last >= total_lines) {
            target_last = total_lines;
        } else {
            /* Look for a natural break (closing brace or empty line) between 12 and 18 */
            int natural_cut = -1;
            int search_start = first_line + 11;
            int search_end = first_line + 17;
            if (search_end > total_lines) search_end = total_lines;

            for (int l = search_end; l >= search_start; l--) {
                if (is_natural_breakpoint(source, l)) {
                    natural_cut = l;
                    break;
                }
            }

            if (natural_cut != -1) {
                target_last = natural_cut;
            }
        }

        fprintf(file,
            "\\begin{frame}[fragile]{Source Entering the Scanner (lines %d--%d)}\n"
            "\\tiny\n"
            "\\begin{block}{}\n"
            "\\ttfamily\\obeylines\n",
            first_line, target_last);

        write_source_frame(file, source, tokens, first_line, target_last);

        fprintf(file,
            "\\end{block}\n"
            "\\vspace{0.1cm}\n"
            "{\\tiny Lexemes are highlighted according to their lexical category.}\n"
            "\\end{frame}\n\n");

        first_line = target_last + 1;
    }
}

/* Emits Beamer preamble, theme configurations, colors, and title slide */
static int write_preamble(FILE *file)
{
    fprintf(file,
        "\\documentclass[aspectratio=169]{beamer}\n"
        "\\usepackage[utf8]{inputenc}\n"
        "\\usepackage[T1]{fontenc}\n"
        "\\usepackage{lmodern}\n"
        "\\usepackage{xcolor}\n"
        "\\usepackage{tikz}\n"
        "\\usetikzlibrary{positioning}\n"
        "\\usepackage{pgfplots}\n"
        "\\pgfplotsset{compat=1.18}\n"
        "\\usetheme{Madrid}\n"
        "\n"
        "\\title{Lexical Scanner Using Flex}\n"
        "\\subtitle{Compiladores e Interpretes -- Lexical Analysis}\n"
        "\\author[F. Benavides, M. Gaviria, M. Zamora]{Felipe Benavides \\\\ Matthew Gaviria Brenes \\\\ Marvin Zamora Mussio}\n"
        "\\institute[TEC]{Ingenieria en Computación}\n"
        "\\date{Semestre I -- 2026}\n"
        "\n"
        "\\definecolor{keywordcolor}{RGB}{25,75,180}\n"       /* Blue */
        "\\definecolor{identifiercolor}{RGB}{20,135,70}\n"    /* Emerald green */
        "\\definecolor{integercolor}{RGB}{215,90,15}\n"       /* Orange */
        "\\definecolor{floatcolor}{RGB}{0,150,165}\n"         /* Turquoise / Cyan */
        "\\definecolor{stringcolor}{RGB}{145,40,145}\n"       /* Magenta / Purple */
        "\\definecolor{charcolor}{RGB}{185,140,20}\n"         /* Amber / Gold */
        "\\definecolor{operatorcolor}{RGB}{85,25,160}\n"       /* Dark Violet */
        "\\definecolor{delimitercolor}{RGB}{75,80,90}\n"      /* Slate Gray */
        "\\definecolor{errorcolor}{RGB}{210,20,20}\n"         /* Red */
        "\\definecolor{errorbackground}{RGB}{255,220,220}\n"
        "\n"
        "\\newcommand{\\KW}[1]{\\textcolor{keywordcolor}{\\textbf{#1}}}\n"
        "\\newcommand{\\ID}[1]{\\textcolor{identifiercolor}{\\textit{#1}}}\n"
        "\\newcommand{\\INT}[1]{\\colorbox{integercolor!15}{\\textcolor{integercolor}{#1}}}\n"
        "\\newcommand{\\FLOAT}[1]{\\colorbox{floatcolor!15}{\\textcolor{floatcolor}{#1}}}\n"
        "\\newcommand{\\STR}[1]{\\colorbox{stringcolor!12}{\\textcolor{stringcolor}{#1}}}\n"
        "\\newcommand{\\CHAR}[1]{\\colorbox{charcolor!12}{\\textcolor{charcolor}{#1}}}\n"
        "\\newcommand{\\OP}[1]{\\textcolor{operatorcolor}{\\textbf{#1}}}\n"
        "\\newcommand{\\DELIM}[1]{\\textcolor{delimitercolor}{#1}}\n"
        "\\newcommand{\\ERR}[1]{\\colorbox{errorbackground}{\\textcolor{errorcolor}{\\textbf{#1}}}}\n"
        "\n"
        "\\setlength{\\fboxsep}{0.8pt}\n"
        "\n"
        "\\begin{document}\n"
        "\n"
        "\\begin{frame}\n"
        "\\titlepage\n"
        "\\end{frame}\n\n");
    return 1;
}

/* Writes introductory theoretical frames and TikZ architecture diagram */
static int write_overview(FILE *file)
{
    fprintf(file,
        "\\begin{frame}{Lexical Analysis}\n"
        "\\begin{itemize}\n"
        "\\item The scanner is the first phase of the compiler pipeline\n"
        "\\item It reads the input produced by the preprocessing phase\n"
        "\\item Whitespace and comments are ignored\n"
        "\\item The scanner recognizes lexemes and classifies them into token categories\n"
        "\\item Invalid characters are reported as lexical errors\n"
        "\\end{itemize}\n"
        "\\end{frame}\n\n"

        "\\begin{frame}{Scanner Architecture}\n"
        "\\begin{center}\n"
        "\\begin{tikzpicture}[node distance=1.4cm, auto]\n"
        "\\node[draw, rounded corners, fill=blue!10] (main) {\\texttt{main.c}};\n"
        "\\node[draw, rounded corners, fill=blue!10, right=1.2cm of main] (get) {\\texttt{Get\\_Token()}};\n"
        "\\node[draw, rounded corners, fill=blue!10, right=1.2cm of get] (flex) {\\texttt{yylex()}};\n"
        "\\node[draw, rounded corners, fill=blue!10, right=1.2cm of flex] (rules) {\\texttt{scanner.l}};\n"
        "\\draw[->, thick] (main) -- (get);\n"
        "\\draw[->, thick] (get) -- (flex);\n"
        "\\draw[->, thick] (flex) -- (rules);\n"
        "\\end{tikzpicture}\n"
        "\\end{center}\n"
        "\\par\\vspace{0.6cm}\n"
        "\\centering Each call to \\texttt{Get\\_Token()} requests the next recognized token from the Flex scanner.\n"
        "\\end{frame}\n\n"

        "\\begin{frame}{Flex}\n"
        "\\begin{itemize}\n"
        "\\item Flex generates a lexical analyzer from regular-expression rules\n"
        "\\item Rules combine patterns with C actions\n"
        "\\item The longest matching rule is selected\n"
        "\\item The generated scanner is compiled together with the C modules\n"
        "\\item The project uses Flex to implement lexical recognition\n"
        "\\end{itemize}\n"
        "\\end{frame}\n\n");
    return 1;
}

/* Writes visual color legend slide explaining token highlighting styles */
static int write_legend(FILE *file)
{
    fprintf(file,
        "\\begin{frame}{Lexeme Visualization}\n"
        "\\small\n"
        "\\begin{tabular}{ll}\n"
        "\\KW{keyword} & Keyword \\\\\n"
        "\\ID{identifier} & Identifier \\\\\n"
        "\\INT{123} & Integer literal \\\\\n"
        "\\FLOAT{45.67} & Floating-point literal \\\\\n"
        "\\STR{\"text\"} & String literal \\\\\n"
        "\\CHAR{'A'} & Character literal \\\\\n"
        "\\OP{+} & Operator \\\\\n"
        "\\DELIM{;} & Delimiter \\\\\n"
        "\\ERR{@} & Lexical error \\\\\n"
        "\\end{tabular}\n"
        "\\end{frame}\n\n");
    return 1;
}

/* Emits formal description table of all lexical token categories */
static int write_token_categories(FILE *file)
{
    fprintf(file,
        "\\begin{frame}{Token Categories}\n"
        "\\small\n"
        "\\begin{table}\n"
        "\\centering\n"
        "\\begin{tabular}{ll}\n"
        "\\textbf{Category} & \\textbf{Description} \\\\\n"
        "\\hline\n"
        "KEYWORD & C reserved word \\\\\n"
        "IDENTIFIER & User-defined name \\\\\n"
        "INTEGER\\_LITERAL & Integer constant \\\\\n"
        "FLOAT\\_LITERAL & Floating-point constant \\\\\n"
        "STRING\\_LITERAL & String constant \\\\\n"
        "CHAR\\_LITERAL & Character constant \\\\\n"
        "OPERATOR & Arithmetic, logical or relational operator \\\\\n"
        "DELIMITER & Punctuation and grouping symbols \\\\\n"
        "LEXICAL\\_ERROR & Unrecognized input character \\\\\n"
        "\\end{tabular}\n"
        "\\end{table}\n"
        "\\end{frame}\n\n");
    return 1;
}

/* Emits slide containing tabular breakdown of token frequencies */
static int write_statistics(FILE *file, const TokenStatistics *stats)
{
    fprintf(file,
        "\\begin{frame}{Token Statistics}\n"
        "\\begin{center}\n"
        "\\begin{tabular}{lr}\n"
        "\\textbf{Category} & \\textbf{Count} \\\\\n"
        "\\hline\n"
        "Keywords & %lu \\\\\n"
        "Identifiers & %lu \\\\\n"
        "Integer literals & %lu \\\\\n"
        "Float literals & %lu \\\\\n"
        "String literals & %lu \\\\\n"
        "Char literals & %lu \\\\\n"
        "Operators & %lu \\\\\n"
        "Delimiters & %lu \\\\\n"
        "Lexical errors & %lu \\\\\n"
        "\\end{tabular}\n"
        "\\end{center}\n"
        "\\end{frame}\n\n",
        stats->keywords, stats->identifiers, stats->integer_literals,
        stats->float_literals, stats->string_literals, stats->char_literals,
        stats->operators, stats->delimiters, stats->lexical_errors);
    return 1;
}

/* Returns the total aggregate count of all scanned tokens */
static unsigned long statistics_total(const TokenStatistics *stats)
{
    return stats->keywords + stats->identifiers + stats->integer_literals +
            stats->float_literals + stats->string_literals + stats->char_literals +
            stats->operators + stats->delimiters + stats->lexical_errors;
}

/* Generates PGFPlots vertical bar histogram slide showing token counts */
static int write_histogram(FILE *file, const TokenStatistics *stats)
{
    fprintf(file,
        "\\begin{frame}{Token Histogram}\n"
        "\\centering\n"
        "\\begin{tikzpicture}\n"
        "\\begin{axis}[\n"
        "ybar,\n"
        "bar width=10pt,\n"
        "width=0.95\\textwidth,\n"
        "height=0.65\\textheight,\n"
        "ylabel={Number of tokens},\n"
        "symbolic x coords={Keywords,Identifiers,Integer,Float,String,Char,Operators,Delimiters,Errors},\n"
        "xtick=data,\n"
        "x tick label style={rotate=35,anchor=east,font=\\scriptsize},\n"
        "ymin=0,\n"
        "enlarge x limits=0.08,\n"
        "nodes near coords,\n"
        "nodes near coords align={vertical},\n"
        "]\n"
        "\\addplot coordinates {\n"
        "(Keywords,%lu)(Identifiers,%lu)(Integer,%lu)(Float,%lu)(String,%lu)(Char,%lu)(Operators,%lu)(Delimiters,%lu)(Errors,%lu)\n"
        "};\n"
        "\\end{axis}\n"
        "\\end{tikzpicture}\n"
        "\\end{frame}\n\n",
        stats->keywords, stats->identifiers, stats->integer_literals,
        stats->float_literals, stats->string_literals, stats->char_literals,
        stats->operators, stats->delimiters, stats->lexical_errors);
    return 1;
}

/* Generates PGFPlots pie chart displaying proportional token distribution */
static int write_pie_chart(FILE *file, const TokenStatistics *stats)
{
    unsigned long total = statistics_total(stats);

    if (total == 0) {
        fprintf(file,
            "\\begin{frame}{Token Distribution}\n"
            "\\centering No tokens were produced by the scanner.\n"
            "\\end{frame}\n\n");
        return 1;
    }

    fprintf(file,
        "\\begin{frame}{Token Distribution}\n"
        "\\centering\n"
        "\\begin{tikzpicture}\n"
        "\\begin{axis}[\n"
        "hide axis, axis equal,\n"
        "width=0.72\\textwidth, height=0.72\\textheight,\n"
        "xmin=-1.25, xmax=1.25, ymin=-1.25, ymax=1.25\n"
        "]\n");

    struct PieSlice {
        unsigned long value;
        const char *name;
        const char *color;
    };

    const struct PieSlice slices[] = {
        {stats->keywords,         "Keywords",   "keywordcolor"},
        {stats->identifiers,      "Identifiers","identifiercolor"},
        {stats->integer_literals, "Integers",   "integercolor"},
        {stats->float_literals,   "Floats",     "floatcolor"},
        {stats->string_literals,  "Strings",    "stringcolor"},
        {stats->char_literals,    "Chars",      "charcolor"},
        {stats->operators,        "Operators",  "operatorcolor"},
        {stats->delimiters,       "Delimiters", "delimitercolor"},
        {stats->lexical_errors,   "Errors",     "errorcolor"}
    };

    double current_angle = 0.0;
    size_t count = sizeof(slices) / sizeof(slices[0]);

    /* Draw slices */
    for (size_t i = 0; i < count; i++) {
        if (slices[i].value == 0) continue;

        double fraction = (double)slices[i].value / (double)total;
        double next_angle = current_angle + fraction * 360.0;
        int segments = 25;

        fprintf(file, "\\addplot[draw=white, line width=0.6pt, fill=%s] coordinates {(0,0)", slices[i].color);
        for (int j = 0; j <= segments; j++) {
            double angle = current_angle + (next_angle - current_angle) * ((double)j / (double)segments);
            double radians = angle * 3.14159265358979323846 / 180.0;
            fprintf(file, " (%.5f,%.5f)", cos(radians), sin(radians));
        }
        fprintf(file, "};\n");

        /* Numeric label inside the slice if it is sufficiently large (>= 3.5%) */
        if ((next_angle - current_angle) >= 12.0) {
            double mid_angle = (current_angle + next_angle) / 2.0;
            double mid_rad = mid_angle * 3.14159265358979323846 / 180.0;
            double label_r = 0.68;
            fprintf(file,
                    "\\node[text=white, font=\\bfseries\\small] at (%.5f,%.5f) {%lu};\n",
                    label_r * cos(mid_rad), label_r * sin(mid_rad), slices[i].value);
        }

        current_angle = next_angle;
    }

    /* Bottom legend with colors and exact numeric count */
    fprintf(file,
        "\\end{axis}\n"
        "\\end{tikzpicture}\n"
        "\\vspace{-0.3cm}\n"
        "\\begin{center}\\scriptsize\n"
        "\\begin{tabular}{lll}\n"
        "\\textcolor{keywordcolor}{\\rule{0.25cm}{0.25cm}} Keywords (%lu) & "
        "\\textcolor{identifiercolor}{\\rule{0.25cm}{0.25cm}} Identifiers (%lu) & "
        "\\textcolor{integercolor}{\\rule{0.25cm}{0.25cm}} Integers (%lu) \\\\\n"
        "\\textcolor{floatcolor}{\\rule{0.25cm}{0.25cm}} Floats (%lu) & "
        "\\textcolor{stringcolor}{\\rule{0.25cm}{0.25cm}} Strings (%lu) & "
        "\\textcolor{charcolor}{\\rule{0.25cm}{0.25cm}} Chars (%lu) \\\\\n"
        "\\textcolor{operatorcolor}{\\rule{0.25cm}{0.25cm}} Operators (%lu) & "
        "\\textcolor{delimitercolor}{\\rule{0.25cm}{0.25cm}} Delimiters (%lu) & "
        "\\textcolor{errorcolor}{\\rule{0.25cm}{0.25cm}} Errors (%lu)\n"
        "\\end{tabular}\n"
        "\\end{center}\n"
        "\\end{frame}\n\n",
        stats->keywords, stats->identifiers, stats->integer_literals,
        stats->float_literals, stats->string_literals, stats->char_literals,
        stats->operators, stats->delimiters, stats->lexical_errors);

    return 1;
}

/* Writes final conclusion slide and closes document environment */
static int write_footer(FILE *file)
{
    fprintf(file,
        "\\begin{frame}{Conclusion}\n"
        "\\begin{itemize}\n"
        "\\item The preprocessor produces the clean source consumed by the scanner\n"
        "\\item Flex provides regular-expression based scanning and longest-match selection\n"
        "\\item The scanner categorizes valid lexemes and preserves syntax coordinates\n"
        "\\item Whitespace and comments are stripped while maintaining line synchronization\n"
        "\\item Invalid characters are preserved and reported as lexical errors\n"
        "\\item Token statistics provide quantitative distribution analysis\n"
        "\\end{itemize}\n"
        "\\end{frame}\n\n"
        "\\end{document}\n");
    return 1;
}

/* Orchestrates TeX file generation and executes double-pass pdflatex compilation */
int presentation_generate(const char *output_dir, const char *input_name,
                          const Source *source, const TokenList *tokens,
                          const TokenStatistics *stats)
{
    char tex_path[512], command[1024];
    (void)input_name;

    snprintf(tex_path, sizeof(tex_path), "%s/presentation.tex", output_dir);

    FILE *file = fopen(tex_path, "w");
    if (file == NULL) return 0;

    write_preamble(file);
    write_overview(file);
    write_legend(file);
    write_source_frames(file, source, tokens);
    write_token_categories(file);
    write_statistics(file, stats);
    write_histogram(file, stats);
    write_pie_chart(file, stats);
    write_footer(file);
    fclose(file);

    snprintf(command, sizeof(command),
            "pdflatex -interaction=nonstopmode -halt-on-error "
            "-output-directory=\"%s\" \"%s\" > /dev/null 2>&1",
            output_dir, tex_path);

    /* First pass compiles initial document layout */
    int result = system(command);
    if (result == -1 || !WIFEXITED(result) || WEXITSTATUS(result) != 0) return 0;

    /* Second pass resolves slide counters and cross references */
    result = system(command);
    if (result == -1 || !WIFEXITED(result) || WEXITSTATUS(result) != 0) return 0;

    return 1;
}