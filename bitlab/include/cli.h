#ifndef __CLI_H
#define __CLI_H

#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE_LEN 256
#define CLI_BUFSIZE 64
#define CLI_DELIM " "
#define CLI_COMMANDS_NUM (int) (sizeof(cli_commands) / sizeof(cli_command))
#define CLI_HISTORY_FILE "cli_history.txt"
#define CLI_PREFIX "BitLab> "

/**
 * CLI command structure.
 *
 * @param cli_command The function pointer to executed function.
 * @param cli_command_name The name of command by which it is called.
 * @param cli_command_description The description of command printed by !help.
 */
typedef struct
{
    int (*cli_command)(char**);
    char* cli_command_name;
    char* cli_command_description;
} cli_command;

/**
 * Exits BitLab CLI command.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_exit(char** args);

/**
 * Prints CLI command history.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_history(char** args);

/**
 * Clears CLI window.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_clear(char** args);

void print_help();

/**
 * Prints CLI command help.
 *
 * @param args The arguments passed to this function should be empty.
 * @return The exit code.
 */
int cli_help(char** args);

/**
 * Gets the line from the file stream.
 *
 * @param lineptr The pointer to the line.
 * @param n The size of the line.
 * @param stream The file stream.
 * @return The number of characters read.
 */
int cli_get_line(char** lineptr, size_t* n, FILE* stream);

/**
 * Reads input from user.
 *
 * WARNING: char* returned from this function must be freed.
 *
 * @return The string containing input from user.
 */
char* cli_read_line(void);

/**
 * Execute command. This function executes a command from line.
 *
 * @param line The string containing input from user.
 * @return The exit code.
 */
int cli_exec_line(char* line);

char** cli_completion(const char* text, int start, int end);

char* cli_command_generator(const char* text, int state);

/**
 * CLI handler thread.
 *
 * @param arg Not used. Write dummy code for no warning.
 */
void* handle_cli(void* arg);

/**
 * Create history directory.
 *
 * @return The history directory.
 */
const char* create_history_dir();

#endif // __CLI_H
