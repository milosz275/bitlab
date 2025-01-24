#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>

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

void log_to_file(const char* filename, const char* format, ...)
{
    FILE* log_file = fopen(filename, "a");
    if (log_file == NULL) {
        perror("[Error] Failed to open log file");
        return;
    }

    char timestamp[20];
    get_formatted_timestamp(timestamp, sizeof(timestamp));

    flockfile(log_file);
    fprintf(log_file, "[%s] ", timestamp);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    funlockfile(log_file);

    fclose(log_file);
}

uint64_t ntohll(uint64_t value)
{
    if (__BYTE_ORDER == __LITTLE_ENDIAN)
    {
        return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
    }
    else
    {
        return value;
    }
}

uint64_t read_var_int(const unsigned char* data, size_t* offset)
{
    uint64_t result = 0;
    unsigned char first_byte = data[*offset];
    *offset += 1;

    if (first_byte < 0xfd)
    {
        result = first_byte;
    }
    else if (first_byte == 0xfd)
    {
        result = *(uint16_t*)(data + *offset);
        *offset += 2;
    }
    else if (first_byte == 0xfe)
    {
        result = *(uint32_t*)(data + *offset);
        *offset += 4;
    }
    else if (first_byte == 0xff)
    {
        result = *(uint64_t*)(data + *offset);
        *offset += 8;
    }
    return result;
}

int is_valid_ipv4(const char* ip_str)
{
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1;
}
