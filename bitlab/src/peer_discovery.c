#include "peer_discovery.h"

#include <string.h>

#include "state.h"
#include "utils.h"
#include "cli.h"
#include "ip.h"

void* handle_peer_discovery(void* arg)
{
    usleep(1000000); // 1 s
    while (!get_exit_flag())
    {
        if (get_peer_discovery())
        {
            start_peer_discovery_progress();
            char ip_address[BUFFER_SIZE];
            lookup_address("seed.bitcoin.sipa.be.", ip_address);
            char* token = strtok(ip_address, " ");
            int peer_count = 0;
            while (token != NULL && *token != '\0')
            {
                if (strcmp(token, "0.0.0.0") == 0)
                {
                    log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Peer discovery failed: resolved IP address is 0.0.0.0");
                    finish_peer_discovery_progress(false);
                    break;
                }
                guarded_print_line("Found peer: %s", token);
                token = strtok(NULL, " ");
                peer_count++;
            }
            if (peer_count > 0)
                finish_peer_discovery_progress(true);
            else
            {
                log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Peer discovery failed: no peers found");
                finish_peer_discovery_progress(false);
            }
        }
        usleep(100000); // 100 ms
    }
    if (arg) {} // dummy code to avoid unused parameter warning
    log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Exiting peer discovery thread");
    pthread_exit(NULL);
}
