#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stddef.h>

#define TIMESTAMP_LENGTH 20

void usleep(unsigned int usec);
char* strdup(const char* str1);
char* strndup(const char* src, size_t size);

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
 * Guarded printf function. This function is used to lock the stdout file and print the formatted string.
 *
 * @param format The format string.
 */
void guarded_printf(const char* format, ...);

#endif // __UTILS_H
