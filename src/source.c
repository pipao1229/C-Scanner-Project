#include <stdio.h>
#include <stdlib.h>

#include "source.h"

int source_load(const char *filename, Source *source)
{
    FILE *file;
    long file_size;
    size_t bytes_read;

    source->content = NULL;
    source->length = 0;

    file = fopen(filename, "rb");

    if (file == NULL) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    file_size = ftell(file);

    if (file_size < 0) {
        fclose(file);
        return 0;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }

    source->content = malloc((size_t)file_size + 1);

    if (source->content == NULL) {
        fclose(file);
        return 0;
    }

    bytes_read = fread(source->content, 1, (size_t)file_size, file);

    if (bytes_read != (size_t)file_size && ferror(file)) {
        free(source->content);
        source->content = NULL;
        fclose(file);
        return 0;
    }

    source->content[bytes_read] = '\0';
    source->length = bytes_read;

    fclose(file);

    return 1;
}

void source_free(Source *source)
{
    free(source->content);

    source->content = NULL;
    source->length = 0;
}