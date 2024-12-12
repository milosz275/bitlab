#ifndef __FILE_H
#define __FILE_H

#include <stdio.h>
#include <stdlib.h>

/**
 * Read the file.
 *
 * @param filename The filename to read.
 * @param buffer The buffer to store the file content.
 * @param size The size of the file content.
 */
void read_file(const char* filename, char** buffer, size_t* size);

/**
 * Write the file.
 *
 * @param filename The filename to write.
 * @param buffer The buffer to write.
 * @param size The size of the buffer.
 */
void write_file(const char* filename, const char* buffer, size_t size);

/**
 * Append the file.
 *
 * @param filename The filename to append.
 * @param buffer The buffer to append.
 * @param size The size of the buffer.
 */
void append_file(const char* filename, const char* buffer, size_t size);

#endif // __FILE_H
