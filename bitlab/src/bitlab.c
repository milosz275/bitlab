#include "bitlab.h"

#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "state.h"
#include "log.h"
#include "cli.h"
#include "utils.h"

bitlab_result run_bitlab(int argc, char* argv[])
{
    // init
    init_config_dir();
    init_logging(BITLAB_LOG);
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, LOG_BITLAB_STARTED);
    init_program_state(&state);

    pthread_t cli_thread;
    if (pthread_create(&cli_thread, NULL, handle_cli, (void*)NULL) != 0)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "CLI thread creation failed: %s", strerror(errno));
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "CLI thread started");

    // execute command line arguments
    if (argc > 1 && argv != NULL)
    {
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
                {
                    strcat(line, " ");
                }
            }
            cli_exec_line(line);
            free(line);
        }
    }

    // main loop
    while (!get_exit_flag(&state))
        usleep(100000); // 100 ms

    // cleanup
    pthread_join(cli_thread, NULL);
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, LOG_BITLAB_FINISHED);
    finish_logging();
    return BITLAB_RESULT_SUCCESS;
}
