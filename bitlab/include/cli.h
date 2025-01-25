#ifndef __CLI_H
#define __CLI_H

#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE_LEN 256
#define CLI_BUFSIZE 64
#define CLI_DELIM " "
#define CLI_COMMANDS_NUM (int) (sizeof(cli_commands) / sizeof(cli_command))
#define CLI_HISTORY_FILE "cli_history.txt"
#define CLI_PREFIX "\033[38;5;220mBitLab \033[0m"

/**
 * CLI command structure. Note that the only uninitialized field can be the cli_command_detailed_desc.
 *
 * @param cli_command The function pointer to executed function.
 * @param cli_command_name The name of command by which it is called.
 * @param cli_command_description The description of command printed by "help".
 * @param cli_command_detailed_desc The detailed description of command printed by "help <command>".
 * @param cli_command_usage The usage of command printed on wrong parameters.
 */
typedef struct
{
    int (*cli_command)(char** args);
    const char* cli_command_name;
    const char* cli_command_brief_desc;
    const char* cli_command_detailed_desc;
    const char* cli_command_usage;
} cli_command;


//// PRINT FUNCTIONS ////

/**
 * Prints CLI command help.
 */
void print_help();

/**
 * Prints CLI command usage.
 *
 * @param command_name The name of the command.
 */
void print_usage(const char* command_name);

/**
 * Prints CLI commands.
 */
void print_commands();


//// HISTORY FUNCTIONS ////

/**
 * Create history directory.
 *
 * @return The history directory.
 */
const char* create_history_dir();


//// CLI COMMANDS ////

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
 * Echoes the input.
 *
 * @param args Input that will be echoed.
 * @return The exit code.
 */
int cli_echo(char** args);

/**
 * Prints the user name.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_whoami(char** args);

/**
 * Gets remote IP address on an URL or host machine if no URL is provided.
 *
 * @param args The URL.
 * @return The exit code.
 */
int cli_get_ip(char** args);

/**
 * Prints program information.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_info(char** args);

/**
 * Discovers Bitcoin peers.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_peer_discovery(char** args);

/**
 * Clears CLI window.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_clear(char** args);

/**
 * Prints CLI command help.
 *
 * @param args The arguments passed to this function should be empty.
 * @return The exit code.
 */
int cli_help(char** args);

/**
 * Pings the specified IP address.
 *
 * @param args The IP address.
 * @return The exit code.
 */
int cli_ping(char** args);

/**
 * Connects to the specified IP address.
 *
 * @param args The IP address.
 * @return The exit code.
 */
int cli_connect(char** args);

/**
 * Lists all connected nodes.
 *
 * @param args The arguments passed to the function should be empty.
 * @return The exit code.
 */
int cli_list(char** args);

/**
 * Sends a 'getaddr' message to the specified peer.
 *
 * @param args The index of the peer.
 * @return The exit code.
 */
int cli_getaddr(char** args);


//// LINE HANDLING FUNCTIONS ////

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


//// LINE COMPLETION FUNCTIONS ////

/**
 * CLI completion function.
 *
 * @param text The text to complete.
 * @param start The start index.
 * @param end The end index.
 * @return The completion.
 */
char** cli_completion(const char* text, int start, int end);

/**
 * CLI command generator.
 *
 * @param text The text to generate.
 * @param state The state of the generator.
 * @return The generated command.
 */
char* cli_command_generator(const char* text, int state);


//// CLI HANDLER ////

/**
 * CLI handler thread.
 *
 * @param arg Not used. Write dummy code for no warning.
 */
void* handle_cli(void* arg);

#endif // __CLI_H
