#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "log.h"

#define TIMESTAMP_LENGTH 20

void usleep(unsigned int usec);
char* strdup(const char* str1);
char* strndup(const char* src, size_t size);

int fileno(FILE* __stream);
void usleep(unsigned int usec);

struct tm* localtime_r(const time_t* timer, struct tm* buf);
void flockfile(FILE* filehandle);
void funlockfile(FILE* file);
FILE* popen(const char* command, const char* type);
int pclose(FILE* stream);

/**
 * Get the timestamp. This function is used to get the timestamp in a YYYYMMDDHHMMSS format.
 *
 * @param buffer The buffer to store the timestamp.
 * @param buffer_size The size of the buffer.
 */
void get_timestamp(char* buffer, size_t buffer_size);

/**
 * Get the formatted timestamp. This function is used to get the formatted timestamp in a YYYY-MM-DD HH:MM:SS format.
 *
 * @param buffer The buffer to store the formatted timestamp.
 * @param buffer_size The size of the buffer.
 */
void get_formatted_timestamp(char* buffer, size_t buffer_size);

/**
 * Clear the CLI window.
 */
void clear_cli();

/**
 * Initialize the configuration directory.
 */
int init_config_dir();

/**
 * Guarded print function. This function is used to lock the stdout file and print the formatted string.
 *
 * @param format The format string.
 */
void guarded_print(const char* format, ...);

/**
 * Guarded print line function. This function is used to lock the stdout file and print the formatted string.
 *
 * @param format The format string.
 */
void guarded_print_line(const char* format, ...);

#endif // __UTILS_H
