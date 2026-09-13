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

static void print_usage(const char *prog_name)
{
    printf("Uso: %s [opciones] <archivo-fuente>\n", prog_name);
    printf("Opciones:\n");
    printf("  -o <dir>     Especificar directorio de salida (por defecto: output)\n");
    printf("  -n           No abrir el visor de PDF automaticamente al terminar\n");
    printf("  -h           Mostrar este mensaje de ayuda\n");
}

int main(int argc, char **argv)
{
    const char *output_dir = "output";
    const char *input_path = NULL;
    int auto_open = 1;
    int opt;

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

    if (optind >= argc) {
        fprintf(stderr, "Error: Falta especificar el archivo de entrada.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    input_path = argv[optind];

    /* Crear directorio de salida si no existe */
    struct stat st;
    if (stat(output_dir, &st) == -1) {
        mkdir(output_dir, 0755);
    }

    PreprocessedFile preprocessed;
    Source source;

    if (!preprocessor_run(input_path, &preprocessed)) {
        fprintf(stderr, "Error: el preprocesamiento fallo para el archivo '%s'\n", input_path);
        return EXIT_FAILURE;
    }

    /* Mostrar al evaluador donde esta el archivo temporal (requisito de evaluacion) */
    printf("[+] Archivo preprocesado generado en: %s\n", preprocessed.path);

    if (!source_load(preprocessed.path, &source)) {
        fprintf(stderr, "Error: no se pudo cargar el archivo preprocesado\n");
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    if (!scanner_init(preprocessed.path)) {
        fprintf(stderr, "Error: no se pudo inicializar el scanner\n");
        source_free(&source);
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    TokenStatistics stats;
    TokenList tokens;

    statistics_init(&stats);
    token_list_init(&tokens);

    Token token;

    do {
        token = Get_Token();

        statistics_add(&stats, token.type);

        if (!token_list_add(&tokens, token)) {
            fprintf(stderr, "Error: fallo de memoria al almacenar tokens\n");
            free(token.lexeme);
            token_list_free(&tokens);
            scanner_close();
            source_free(&source);
            preprocessor_cleanup(&preprocessed);
            return EXIT_FAILURE;
        }

    } while (token.type != TOKEN_EOF);

    scanner_close();

    printf("[+] Total de tokens escaneados: %lu\n",
           stats.keywords + stats.identifiers + stats.integer_literals +
           stats.float_literals + stats.string_literals + stats.char_literals +
           stats.operators + stats.delimiters + stats.lexical_errors);

    printf("[+] Generando presentacion Beamer en '%s'...\n", output_dir);

    if (!presentation_generate(output_dir,
                                input_path,
                                &source,
                                &tokens,
                                &stats)) {
        fprintf(stderr, "Error: fallo la generacion de la presentacion Beamer\n");
        token_list_free(&tokens);
        source_free(&source);
        preprocessor_cleanup(&preprocessed);
        return EXIT_FAILURE;
    }

    printf("[+] PDF generado exitosamente en: %s/presentation.pdf\n", output_dir);

    /* Despliegue automatico en modo presentacion si esta habilitado */
    if (auto_open) {
        char command[1024];
        printf("[+] Abriendo presentacion con evince en modo presentacion...\n");
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