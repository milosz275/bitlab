#ifndef __UTILS_H
#define __UTILS_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "log.h"

#define TIMESTAMP_LENGTH 20
#define BUFFER_SIZE 8096

void usleep(unsigned int usec);
char* strdup(const char* str1);
char* strndup(const char* src, size_t size);
char* strtok(char* str, const char* delimiters);

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


/**
 * Log to file function. This function is used to log messages to a specified file.
 *
 * @param filename The name of the log file.
 * @param format The format string.
 */
void log_to_file(const char* filename, const char* format, ...);

/**
 * Convert a 64-bit integer from host byte order to network byte order.
 *
 * @param value The 64-bit integer in host byte order.
 * @return The 64-bit integer in network byte order.
 */
uint64_t ntohll(uint64_t value);

/**
 * Convert a 64-bit integer from network byte order to host byte order.
 *
 * @param value The 64-bit integer in network byte order.
 * @return The 64-bit integer in host byte order.
 */
size_t read_var_int(const unsigned char* data, uint64_t* value);

/**
 * Check if the IP address is valid.
 *
 * @param ip_str The IP address string.
 * @return True if the IP address is valid, false otherwise.
 */
int is_valid_ipv4(const char* ip_str);

#endif // __UTILS_H
