#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

void get_timestamp(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%Y%m%d%H%M%S", &tm_now);
}

void get_formatted_timestamp(char* buffer, size_t buffer_size)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

void clear_cli()
{
    guarded_print("\033[H\033[J");
}

int init_config_dir()
{
    const char* home = getenv("HOME");
    if (home == NULL)
        return 1;
    const char* suffix = "/.bitlab";
    char* init_dir = malloc(strlen(home) + strlen(suffix) + 1);
    if (init_dir == NULL)
        return 1;
    strcpy(init_dir, home);
    strcat(init_dir, suffix);

    mkdir(init_dir, 0700);
    free(init_dir);
    return 0;
}

void guarded_print(const char* format, ...)
{
    flockfile(stdout);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    funlockfile(stdout);
}

void guarded_print_line(const char* format, ...)
{
    flockfile(stdout);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    funlockfile(stdout);
}
