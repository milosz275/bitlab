#include "cli.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#include "state.h"
#include "utils.h"
#include "ip.h"

// History directory initialized in CLI thread
static const char* cli_history_dir = NULL;

// CLI mutex protecting started command-line operation to avoid mixing outputs
pthread_mutex_t cli_mutex = PTHREAD_MUTEX_INITIALIZER;

// BitLab command array
static cli_command cli_commands[] =
{
    {
        .cli_command = &cli_clear,
        .cli_command_name = "clear",
        .cli_command_brief_desc = "Clears CLI screen.",
        .cli_command_detailed_desc = " * clear - Clears the command line interface screen for better readability.",
        .cli_command_usage = "clear"
    },
    {
        .cli_command = &cli_echo,
        .cli_command_name = "echo",
        .cli_command_brief_desc = "Echoes the input.",
        .cli_command_detailed_desc = " * echo - Outputs the text or arguments provided as input.",
        .cli_command_usage = "echo [This is an example text to be echoed.]"
    },
    {
        .cli_command = &cli_exit,
        .cli_command_name = "exit",
        .cli_command_brief_desc = "Stops the server.",
        .cli_command_detailed_desc = " * exit - Stops the server gracefully and terminates the session.",
        .cli_command_usage = "exit [-f | --force]"
    },
    {
        .cli_command = &cli_get_ip,
        .cli_command_name = "getip",
        .cli_command_brief_desc = "Obtains IP address of given URL.",
        .cli_command_detailed_desc = " * getip - Retrieves and displays the remote IP address of a specified URL or host if not specified.",
        .cli_command_usage = "getip [URL 1] [URL 2] ..."
    },
    {
        .cli_command = &cli_help,
        .cli_command_name = "help",
        .cli_command_brief_desc = "Prints command descriptions.",
        .cli_command_detailed_desc = " * help - Lists all available commands with their descriptions.",
        .cli_command_usage = "help [command]"
    },
    {
        .cli_command = &cli_history,
        .cli_command_name = "history",
        .cli_command_brief_desc = "Prints command history.",
        .cli_command_detailed_desc = " * history - Displays the history of all entered commands for reference.",
        .cli_command_usage = "history"
    },
    {
        .cli_command = &cli_peer_discovery,
        .cli_command_name = "peerdiscovery",
        .cli_command_brief_desc = "Starts peer discovery.",
        .cli_command_detailed_desc = " * peerdiscovery - Initiates the peer discovery process (currently a work in progress).",
        .cli_command_usage = "peerdiscovery [-d | --daemon]"
    },
    {
        .cli_command = &cli_whoami,
        .cli_command_name = "whoami",
        .cli_command_brief_desc = "Prints basic information about user.",
        .cli_command_detailed_desc = " * whoami - Displays username or full user information, including username, local IP, and public IP address when --full argument provided.",
        .cli_command_usage = "whoami [-f | --full]"
    },
}; // do not add NULLs at the end

void print_help()
{
    int longest_usage_length = 0;
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
    {
        int length = strlen(cli_commands[i].cli_command_usage);
        if (length > longest_usage_length)
            longest_usage_length = length;
    }

    int longest_description_length = 0;
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
    {
        int length = strlen(cli_commands[i].cli_command_brief_desc);
        if (length > longest_description_length)
            longest_description_length = length;
    }

    char* dashes_before = (char*)malloc(longest_usage_length + 1);
    memset(dashes_before, '-', longest_usage_length);
    dashes_before[longest_usage_length] = '\0';

    char* dashes_after = (char*)malloc(longest_description_length + 1);
    memset(dashes_after, '-', longest_description_length);
    dashes_after[longest_description_length] = '\0';

    guarded_print_line("\033[1m%-*s | %s\033[0m", longest_usage_length, "Command", "Description");
    guarded_print_line("%s-+-%s", dashes_before, dashes_after);

    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
        guarded_print_line("%-*s | %s", longest_usage_length, cli_commands[i].cli_command_usage, cli_commands[i].cli_command_brief_desc);

    free(dashes_before);
    free(dashes_after);
}

void print_usage(const char* command_name)
{
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
    {
        if (strcmp(command_name, cli_commands[i].cli_command_name) == 0)
        {
            guarded_print_line("Usage: %s", cli_commands[i].cli_command_usage);
            return;
        }
    }
}

void print_commands()
{
    for (int i = 0; i < CLI_COMMANDS_NUM; ++i)
        guarded_print_line("%s", cli_commands[i].cli_command_name);
}

int cli_exit(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    if (args[0] != NULL)
    {
        if (strcmp(args[0], "-f") == 0 || strcmp(args[0], "--force") == 0)
        {
            if (args[1] != NULL)
            {
                log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for exit command: \"%s\"", args[1]);
                print_usage("exit");
                pthread_mutex_unlock(&cli_mutex);
                return 1;
            }
            log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Force exiting BitLab");
            pthread_mutex_unlock(&cli_mutex);
            exit(0);
        }
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for exit command: \"%s\"", args[0]);
        print_usage("exit");
        pthread_mutex_unlock(&cli_mutex);
        return 1;
    }
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Server shutdown requested");
    set_exit_flag(1);
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_history(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    if (args[0] != NULL)
    {
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for history command: \"%s\"", args[0]);
        print_usage("history");
        pthread_mutex_unlock(&cli_mutex);
        return 1;
    }
    HIST_ENTRY** history_entries = history_list();
    if (history_entries)
    {
        for (int i = 0; history_entries[i] != NULL; ++i)
            guarded_print_line("%d: %s", i + 1, history_entries[i]->line);
    }
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_help(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    if (args[0] != NULL)
    {
        if (args[1] != NULL && args[2] == NULL)
        {
            log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Too many arguments for help command");
            print_usage("help");
            pthread_mutex_unlock(&cli_mutex);
            return 1;
        }
        for (long unsigned i = 0; i < sizeof(cli_commands) / sizeof(cli_command); ++i)
        {
            if (strcmp(args[0], cli_commands[i].cli_command_name) == 0)
            {
                if (cli_commands[i].cli_command_detailed_desc)
                    guarded_print_line(cli_commands[i].cli_command_detailed_desc);
                else
                    guarded_print_line(" * %s - Detailed information not included.", args[0]);
                pthread_mutex_unlock(&cli_mutex);
                return 0;
            }
        }
        guarded_print_line(" * %s - Unknown command.", args[0]);
    }
    else
        print_help();
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_echo(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    if (args[0] == NULL)
    {
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "No arguments provided for echo command");
        print_usage("echo");
        pthread_mutex_unlock(&cli_mutex);
        return 1;
    }
    for (int i = 0; args[i] != NULL; ++i)
        guarded_print("%s ", args[i]);
    guarded_print("\n");
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_whoami(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    bool full_information = false;
    if (args[0] != NULL)
    {
        if (strcmp(args[0], "-f") == 0 || strcmp(args[0], "--full") == 0)
        {
            if (args[1] != NULL)
            {
                log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for whoami command: \"%s\"", args[1]);
                print_usage("whoami");
                pthread_mutex_unlock(&cli_mutex);
                return 1;
            }
            full_information = true;
        }
        else
        {
            log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for whoami command: \"%s\"", args[0]);
            print_usage("whoami");
            pthread_mutex_unlock(&cli_mutex);
            return 1;
        }
    }

    if (!full_information)
        guarded_print_line("%s", getenv("USER"));
    else
    {
        char ip_address[256];
        get_local_ip_address(ip_address);

        if (getenv("USER") == NULL)
            guarded_print_line("Unknown user");
        else if (strcmp(getenv("USER"), "root") == 0)
            guarded_print_line("You are \033[1;31m%s\033[0m", getenv("USER"));
        else
            guarded_print_line("You are \033[1;34m%s\033[0m", getenv("USER"));

        guarded_print_line("Local IP address: %s", ip_address);

        get_remote_ip_address(ip_address);
        guarded_print_line("Public IP address: %s", ip_address);
    }

    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_get_ip(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    char ip_address[256];
    if (args[0] == NULL)
    {
        get_remote_ip_address(ip_address);
        guarded_print_line("Public IP address: %s", ip_address);
    }
    else
    {
        if (args[0] != NULL && args[1] == NULL)
        {
            if (is_valid_domain_address(args[0]))
            {
                lookup_address(args[0], ip_address);
                guarded_print_line("IP address of %s: %s", args[0], ip_address);
            }
            else
                guarded_print_line("Invalid domain address: %s", args[0]);
        }
        else
        {
            int i = 0;
            while (args[i] != NULL)
            {
                ip_address[0] = '\0';
                if (is_valid_domain_address(args[i]))
                {
                    lookup_address(args[i], ip_address);
                    guarded_print_line("%d: IP address of %s: %s", i + 1, args[i], ip_address);
                }
                else
                    guarded_print_line("%d: Invalid domain address: %s", i + 1, args[i]);
                i++;
            }
        }
    }
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_peer_discovery(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    bool daemon = false;

    if (args[0] != NULL)
    {
        for (int i = 0; args[i] != NULL; ++i)
        {
            if (strcmp(args[i], "-d") == 0 || strcmp(args[i], "--daemon") == 0)
                daemon = true;
            else
            {
                log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for peerdiscovery command: %s", args[i]);
                print_usage("peerdiscovery");
                pthread_mutex_unlock(&cli_mutex);
                return 1;
            }
        }
    }

    if (get_peer_discovery())
    {
        if (daemon)
        {
            log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Peer discovery already in progress");
            guarded_print_line("Peer discovery already in progress. Use \"peerdiscovery\" to check results later or \"info\" to see the status.");
            pthread_mutex_unlock(&cli_mutex);
            return 1;
        }
        else // wait for peer discovery to finish
        {
            guarded_print_line("Connected to peer discovery daemon. Waiting for results...");
            while (get_peer_discovery())
                usleep(100000); // 100 ms
        }
    }
    else
    {
        // [ ] Break if results are already set

        set_peer_discovery(true);

        if (daemon)
        {
            log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Peer discovery in background");
            guarded_print_line("Peer discovery started as daemon. Use \"peerdiscovery\" to check results or \"info\" to see the status.");
            pthread_mutex_unlock(&cli_mutex);
            return 0;
        }
        else // wait for peer discovery to finish
        {
            guarded_print_line("Peer discovery started. Waiting for results...");
            while (get_peer_discovery())
                usleep(100000); // 100 ms
        }
    }

    // [ ] Add peer discovery results
    
    pthread_mutex_unlock(&cli_mutex);
    return 0;
}

int cli_clear(char** args)
{
    pthread_mutex_lock(&cli_mutex);
    if (args[0] != NULL)
    {
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Unknown argument for clear command: %s", args[0]);
        print_usage("clear");
        pthread_mutex_unlock(&cli_mutex);
        return 1;
    }
    clear_cli();
    pthread_mutex_unlock(&cli_mutex);
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
    pthread_mutex_lock(&cli_mutex);
    char* line = readline(CLI_PREFIX);
    if (line && *line)
        add_history(line);
    pthread_mutex_unlock(&cli_mutex);
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
    guarded_print_line("Command not found! Type \"help\" to see available commands.");
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
        guarded_print("\n");
        print_help();
        guarded_print(CLI_PREFIX);
        return NULL;
    }

    // count spaces
    for (long unsigned i = 0; i < strlen(line); ++i)
        if (line[i] == ' ')
            spaces++;

    // check if the input starts with "help" and allow for one space
    if (((strncmp(line, "help", 4) == 0) && spaces <= 1))
        return rl_completion_matches(text, cli_command_generator);

    // complete first word
    if (spaces == 0)
        return rl_completion_matches(text, cli_command_generator);

    return NULL;
}

char* cli_command_generator(const char* text, int state)
{
    static int list_index, len;
    const char* name;
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

    while (!get_exit_flag())
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
