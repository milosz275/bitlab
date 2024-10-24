#include "cli.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>

#include "state.h"
#include "log.h"
#include "utils.h"

// BitLab command array
static cli_command cli_commands[] =
{
    {.cli_command = &cli_exit, .cli_command_name = "exit", .cli_command_description = "Stops the server." },
    {.cli_command = &cli_history, .cli_command_name = "history", .cli_command_description = "Prints command history." },
    {.cli_command = &cli_clear, .cli_command_name = "clear", .cli_command_description = "Clears CLI screen." },
    {.cli_command = &cli_help, .cli_command_name = "help", .cli_command_description = "Prints command descriptions." },
};

int cli_exit(char** args)
{
    if (args[0] != NULL)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Arguments provided for exit command ignored");
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Server shutdown requested");
    set_exit_flag(&state, 1);
    return 0;
}

int cli_history(char** args)
{
    if (args[0] != NULL)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Arguments provided for history command ignored");
    HIST_ENTRY** history_entries = history_list();
    if (history_entries)
    {
        for (int i = 0; history_entries[i] != NULL; ++i)
            printf("%d: %s\n", i + 1, history_entries[i]->line);
    }
    return 0;
}

int cli_help(char** args)
{
    if (args[0] != NULL)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Arguments provided for help command ignored"); // [ ] Add help for each command
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
        printf("%-11s- %s\n", cli_commands[i].cli_command_name, cli_commands[i].cli_command_description);
    return 0;
}

int cli_clear(char** args)
{
    if (args[0] != NULL)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Arguments provided for clear command ignored");
    clear_cli();
    return 0;
}

int cli_get_line(char** lineptr, size_t* n, FILE* stream)
{
    static char line[MAX_LINE_LEN];
    char* ptr;
    unsigned int len;

    if (lineptr == NULL || n == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (ferror(stream))
        return -1;

    if (feof(stream))
        return -1;

    (void)fgets(line, 256, stream);

    ptr = strchr(line, '\n');
    if (ptr)
        *ptr = '\0';

    len = strlen(line);

    if ((len + 1) < 256)
    {
        ptr = realloc(*lineptr, 256);
        if (ptr == NULL)
            return(-1);
        *lineptr = ptr;
        *n = 256;
    }

    strcpy(*lineptr, line);
    return(len);
}

char* cli_read_line(void)
{
    char* line = readline("BitLab> ");
    if (line && *line)
        add_history(line);
    return line;
}

int cli_exec_line(char* line)
{
    int i = 0;
    int buffer_size = CLI_BUFSIZE;
    char** tokens = malloc(buffer_size * sizeof(char*));
    char* token;

    char* command;
    char** args;

    if (!tokens)
    {
        log_message(LOG_FATAL, BITLAB_LOG, __FILE__, "cli_exec_line: malloc error");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, CLI_DELIM);
    while (token != NULL)
    {
        tokens[i] = token;
        i++;

        if (i >= buffer_size)
        {
            buffer_size += CLI_BUFSIZE;
            tokens = realloc(tokens, buffer_size * sizeof(char*));
            if (!tokens)
            {
                log_message(LOG_FATAL, BITLAB_LOG, __FILE__, "cli_exec_line: malloc error");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, CLI_DELIM);
    }
    tokens[i] = NULL;

    command = tokens[0];
    args = tokens + 1;

    if (command == NULL)
    {
        free(tokens);
        return 1;
    }

    for (i = 0; i < CLI_COMMANDS_NUM; ++i)
    {
        if (!strcmp(command, cli_commands[i].cli_command_name))
        {
            int result = (cli_commands[i].cli_command)(args);
            free(tokens);
            return result;
        }
    }
    printf("Command not found! Type \"help\" to see available commands.\n");
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Command not found: %s", command);
    free(tokens);
    return 1;
}

void* handle_cli(void* arg)
{
    char* line = NULL;
    read_history(CLI_HISTORY_FILE);
    usleep(50000); // 50 ms
    while (!get_exit_flag(&state))
    {
        line = cli_read_line();
        if (line && *line)
        {
            cli_exec_line(line);
            write_history(CLI_HISTORY_FILE);
        }
        if (line)
        {
            free(line);
            line = NULL;
        }
    }
    if (arg) {} // dummy code to avoid unused parameter warning
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Exiting CLI thread");
    pthread_exit(NULL);
}
