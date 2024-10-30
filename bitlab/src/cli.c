#include "cli.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <sys/stat.h>

#include "state.h"
#include "log.h"
#include "utils.h"

static const char* cli_history_dir = NULL;

// BitLab command array
static cli_command cli_commands[] =
{
    {.cli_command = &cli_exit, .cli_command_name = "exit", .cli_command_description = "Stops the server." },
    {.cli_command = &cli_history, .cli_command_name = "history", .cli_command_description = "Prints command history." },
    {.cli_command = &cli_clear, .cli_command_name = "clear", .cli_command_description = "Clears CLI screen." },
    {.cli_command = &cli_help, .cli_command_name = "help", .cli_command_description = "Prints command descriptions." },
}; // do not add NULLs at the end

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

void print_help()
{
    int longest_length = 0;
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
    {
        int length = strlen(cli_commands[i].cli_command_name);
        if (length > longest_length)
            longest_length = length;
    }
    printf("\033[1m%-*s | %s\033[0m\n", longest_length, "Command", "Description");
    printf("%.*s-+-%.*s\n", longest_length, "--------------------", 50, "--------------------------------------------------");
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
        printf("%-*s | %s\n", longest_length, cli_commands[i].cli_command_name, cli_commands[i].cli_command_description);
}

int cli_help(char** args)
{
    if (args[0] != NULL)
    {
        if (strcmp(args[0], "exit") == 0)
            printf(" * Detailed information about exit command:\n * exit - Stops the server.\n");
        else if (strcmp(args[0], "history") == 0)
            printf(" * Detailed information about history command:\n * history - Prints command history.\n");
        else if (strcmp(args[0], "clear") == 0)
            printf(" * Detailed information about clear command:\n * clear - Clears CLI screen.\n");
        else if (strcmp(args[0], "help") == 0)
            printf(" * Detailed information about help command:\n * help - Prints command descriptions.\n");
        else
            printf("Unknown command: %s\n", args[0]);
    }
    else
        print_help();
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
    char* line = readline(CLI_PREFIX);
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

char** cli_completion(const char* text, int start, int end)
{
    rl_attempted_completion_over = 1;
    start = start; // unused
    end = end;     // unused
    char* line = rl_line_buffer;
    int spaces = 0;

    // print help for empty input
    if (strlen(line) == 0)
    {
        putchar('\n');
        print_help();
        printf(CLI_PREFIX);
        return NULL;
    }

    // count spaces
    for (long unsigned i = 0; i < strlen(line); ++i)
        if (line[i] == ' ')
            spaces++;

    // check if the input starts with "help" and allow for one space
    if (((strncmp(line, "help", 4) == 0) && (spaces <= 1)) || (spaces <= 0))
        return rl_completion_matches(text, cli_command_generator);
    return NULL;
}

char* cli_command_generator(const char* text, int state)
{
    static int list_index, len;
    char* name;
    if (!state)
    {
        list_index = 0;
        len = strlen(text);
    }
    while (list_index < CLI_COMMANDS_NUM)
    {
        name = cli_commands[list_index].cli_command_name;
        list_index++;

        if (strncmp(name, text, len) == 0)
            return strdup(name);
    }
    return NULL;
}

void* handle_cli(void* arg)
{
    char* line = NULL;
    cli_history_dir = create_history_dir();
    struct stat st = { 0 };

    if (stat(cli_history_dir, &st) == -1 && mkdir(cli_history_dir, 0700) != 0)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Failed to create history directory");

    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/%s", cli_history_dir, CLI_HISTORY_FILE);
    read_history(full_path);
    usleep(50000); // 50 ms

    rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";
    rl_completer_word_break_characters = " \t\n\"\\'`@$><=;|&{(";
    rl_completer_quote_characters = "\"\\'`";
    rl_completion_append_character = '\0';
    rl_attempted_completion_over = 1;
    rl_attempted_completion_function = cli_completion;
    // [ ] Find setting to disable inputting space after tabbing

    while (!get_exit_flag(&state))
    {
        line = cli_read_line();
        if (line && *line)
        {
            cli_exec_line(line);
            write_history(full_path);
        }
        if (line)
        {
            free(line);
            line = NULL;
        }
    }
    if (arg) {} // dummy code to avoid unused parameter warning
    if (cli_history_dir != NULL)
    {
        free((void*)cli_history_dir);
        cli_history_dir = NULL;
    }
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Exiting CLI thread");
    pthread_exit(NULL);
}

const char* create_history_dir()
{
    const char* home = getenv("HOME");
    if (home == NULL)
        return NULL;
    const char* suffix = "/.bitlab/history";
    char* logs_dir = malloc(strlen(home) + strlen(suffix) + 1);
    if (logs_dir == NULL)
        return NULL;
    strcpy(logs_dir, home);
    strcat(logs_dir, suffix);
    return logs_dir;
}
