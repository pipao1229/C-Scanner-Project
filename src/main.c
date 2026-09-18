#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "preprocessor.h"
#include "scanner.h"
#include "statistics.h"
#include "token_list.h"
#include "source.h"
#include "presentation.h"

/* Prints standard command-line usage and available flags */
static void print_usage(const char *prog_name)
{
    printf("Usage: %s [options] <input-file>\n", prog_name);
    printf("Options:\n");
    printf("  -o <dir>     Output directory for generated files (default: output)\n");
    printf("  -n           Do not automatically open PDF viewer upon completion\n");
    printf("  -h           Show this help message\n");
}

int main(int argc, char **argv)
{
    const char *output_dir = "output";
    const char *input_path = NULL;
    int auto_open = 1;
    int opt;

    /* Parse standard UNIX command-line options */
    while ((opt = getopt(argc, argv, "o:nh")) != -1) {
        switch (opt) {
            case 'o':
                output_dir = optarg;
                break;
            case 'n':
                auto_open = 0;
                break;
            case 'h':
                print_usage(argv[0]);
                return EXIT_SUCCESS;
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    /* Ensure a positional input file argument is provided */
    if (optind >= argc) {
        fprintf(stderr, "Error: Missing input file.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    input_path = argv[optind];

    /* Create target output directory if it does not already exist */
    struct stat st;
    if (stat(output_dir, &st) == -1) {
        mkdir(output_dir, 0755);
    }

    PreprocessedFile preprocessed;
    Source source;

    /* Run preprocessor to resolve directives and clean comments */
    if (!preprocessor_run(input_path, &preprocessed)) {
        fprintf(stderr, "Error: Preprocessing failed for '%s'\n", input_path);
        return EXIT_FAILURE;
    }

    /* Display preprocessed temporary file path for evaluation inspection */
    printf("[+] Preprocessed file generated at: %s\n", preprocessed.path);

    /* Load preprocessed content into memory for presentation frames */
    if (!source_load(preprocessed.path, &source)) {
        fprintf(stderr, "Error: Cannot load preprocessed source file\n");
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    /* Initialize Flex scanner with preprocessed input stream */
    if (!scanner_init(preprocessed.path)) {
        fprintf(stderr, "Error: Cannot initialize scanner\n");
        source_free(&source);
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    TokenStatistics stats;
    TokenList tokens;

    statistics_init(&stats);
    token_list_init(&tokens);

    Token token;

    /* Read tokens sequentially until EOF is reached */
    do {
        token = Get_Token();

        statistics_add(&stats, token.type);

        if (!token_list_add(&tokens, token)) {
            fprintf(stderr, "Error: Memory allocation failure while storing tokens\n");
            free(token.lexeme);
            token_list_free(&tokens);
            scanner_close();
            source_free(&source);
            preprocessor_cleanup(&preprocessed);
            return EXIT_FAILURE;
        }

    } while (token.type != TOKEN_EOF);

    scanner_close();

    printf("[+] Total tokens scanned: %lu\n",
        stats.keywords + stats.identifiers + stats.integer_literals +
        stats.float_literals + stats.string_literals + stats.char_literals +
        stats.operators + stats.delimiters + stats.lexical_errors);

    printf("[+] Generating Beamer presentation in '%s'...\n", output_dir);

    /* Generate LaTeX file and compile PDF via pdflatex */
    if (!presentation_generate(output_dir,
                                input_path,
                                &source,
                                &tokens,
                                &stats)) {
        fprintf(stderr, "Error: Beamer presentation generation failed\n");
        token_list_free(&tokens);
        source_free(&source);
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    printf("[+] PDF generated successfully at: %s/presentation.pdf\n", output_dir);

    /* Launch evince viewer in presentation mode unless disabled by flag */
    if (auto_open) {
        char command[1024];
        printf("[+] Launching presentation with evince...\n");
        snprintf(command, sizeof(command),
                "evince -s \"%s/presentation.pdf\" > /dev/null 2>&1 &",
                output_dir);
        system(command);
    }

    token_list_free(&tokens);
    source_free(&source);
    preprocessor_cleanup(&preprocessed);

    return EXIT_SUCCESS;
}