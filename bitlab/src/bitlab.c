#include "bitlab.h"

#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "state.h"
#include "cli.h"
#include "utils.h"
#include "thread.h"
#include "peer_discovery.h"

bitlab_result run_bitlab(int argc, char* argv[])
{
    // init
    init_config_dir();
    init_logging(BITLAB_LOG);
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, LOG_BITLAB_STARTED);
    init_program_state(&state);
    init_program_operation(&operation);
    pthread_t cli_thread = thread_runner(handle_cli, "CLI", NULL);
    pthread_t peer_discovery_thread = thread_runner(handle_peer_discovery, "Peer discovery", NULL);

    // execute command line arguments
    if (argc > 1 && argv != NULL)
    {
        mark_started_with_parameters();
        int total_length = 0;
        for (int i = 1; i < argc; ++i)
            total_length += strlen(argv[i]) + 1;

        char* line = (char*)malloc(total_length * sizeof(char));
        if (line == NULL)
            log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Failed to allocate memory for command line arguments");
        else
        {
            line[0] = '\0';
            for (int i = 1; i < argc; ++i)
            {
                strcat(line, argv[i]);
                if (i < argc - 1)
                    strcat(line, " ");
            }

            if (argv[1] && strcmp(argv[1], "exit") == 0)
                log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Starting BitLab with \"%s\" parameter", line);

            cli_exec_line(line);
            free(line);
        }
        guarded_print_line("Close BitLab using \"exit\"");
    }

    // main loop
    while (!get_exit_flag(&state))
        usleep(100000); // 100 ms

    // cleanup
    pthread_join(cli_thread, NULL);
    pthread_join(peer_discovery_thread, NULL);
    destroy_program_state(&state);
    destroy_program_operation(&operation);
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, LOG_BITLAB_FINISHED);
    finish_logging();
    return BITLAB_RESULT_SUCCESS;
}
