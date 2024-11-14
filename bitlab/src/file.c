#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void read_file(const char* filename, char** buffer, size_t* size)
{
    FILE* file = fopen(filename, "r");
    if (file == NULL)
    {
        *buffer = NULL;
        *size = 0;
        return;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    *buffer = (char*)malloc(*size + 1);
    if (*buffer == NULL)
    {
        *size = 0;
        fclose(file);
        return;
    }

    fread(*buffer, 1, *size, file);
    (*buffer)[*size] = '\0';

    fclose(file);
}

void write_file(const char* filename, const char* buffer, size_t size)
{
    FILE* file = fopen(filename, "w");
    if (file == NULL)
        return;

    fwrite(buffer, 1, size, file);

    fclose(file);
}

void append_file(const char* filename, const char* buffer, size_t size)
{
    FILE* file = fopen(filename, "a");
    if (file == NULL)
        return;

    fwrite(buffer, 1, size, file);

    fclose(file);
}
