#include "peer_discovery.h"

#include "state.h"
#include "utils.h"
#include "cli.h"

void* handle_peer_discovery(void* arg)
{
    usleep(1000000); // 1 s
    while (!get_exit_flag())
    {
        if (get_peer_discovery())
        {
            // peer discovery logic
        }
        usleep(100000); // 100 ms
    }
    if (arg) {} // dummy code to avoid unused parameter warning
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Exiting peer discovery thread");
    pthread_exit(NULL);
}
