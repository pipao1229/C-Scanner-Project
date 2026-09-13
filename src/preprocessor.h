#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

typedef struct {
    char *path;
} PreprocessedFile;

int preprocessor_run(const char *input_path, PreprocessedFile *output);
void preprocessor_cleanup(PreprocessedFile *output);

#endif