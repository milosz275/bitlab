#include "peer_discovery.h"

#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>

#include "peer_queue.h"
#include "state.h"
#include "utils.h"
#include "cli.h"
#include "ip.h"

static const char* dns_seeds[] =
{
    "seed.bitcoin.sipa.be.",
    "seed.btc.petertodd.org.",
    "dnsseed.emzy.de.",
    NULL
};

static const char* hardcoded_peers[] =
{
    "23.84.108.213:8333",
    "87.207.45.218:8333",
    "35.207.115.204:8333",
    "65.108.202.25:8333",
    NULL
};

void* handle_peer_discovery(void* arg)
{
    usleep(1000000); // 1 s
    while (!get_exit_flag())
    {
        if (get_peer_discovery() && !get_peer_discovery_in_progress() && !get_peer_discovery_succeeded())
        {
            start_peer_discovery_progress();
            int peer_count = 0;
            bool search = false;

            if (get_peer_discovery_hardcoded_seeds())
            {
                // query hardcoded seeds
                search = true;
                for (int i = 0; hardcoded_peers[i] != NULL; ++i)
                {
                    add_peer_to_queue(hardcoded_peers[i], 0);
                    guarded_print_line("Added hardcoded peer: %s", hardcoded_peers[i]);
                    peer_count++;
                }
            }
            else if (get_peer_discovery_dns_lookup())
            {
                // query DNS seeds
                search = true;

                if (get_peer_discovery_dns_domain() == NULL)
                {
                    for (int i = 0; dns_seeds[i] != NULL; ++i)
                    {
                        char ip_address[BUFFER_SIZE];
                        lookup_address(dns_seeds[i], ip_address);
                        char* token = strtok(ip_address, " ");
                        while (token != NULL && *token != '\0')
                        {
                            if (strcmp(token, "0.0.0.0") == 0)
                            {
                                log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Invalid IP from DNS seed: %s", dns_seeds[i]);
                                break;
                            }
                            add_peer_to_queue(token, 8333);
                            token = strtok(NULL, " ");
                            peer_count++;
                        }
                    }
                }
                else
                {
                    char ip_address[BUFFER_SIZE];
                    lookup_address(get_peer_discovery_dns_domain(), ip_address);
                    char* token = strtok(ip_address, " ");
                    while (token != NULL && *token != '\0')
                    {
                        if (strcmp(token, "0.0.0.0") == 0)
                        {
                            log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Invalid IP from DNS seed: %s", get_peer_discovery_dns_domain());
                            break;
                        }
                        add_peer_to_queue(token, 8333);
                        token = strtok(NULL, " ");
                        peer_count++;
                    }
                }
            }
            else
            {
                log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Peer discovery operation or no valid state set");
                finish_peer_discovery_progress(false);
            }

            // [ ] Implement peer processing using the Bitcoin P2P protocol

            if (peer_count > 0)
            {
                log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Peer discovery succeeded: found %d peers", peer_count);
                finish_peer_discovery_progress(true);
            }
            else if (!search)
            {
                log_message(LOG_ERROR, BITLAB_LOG, __FILE__, "Peer discovery failed: no valid state set");
                finish_peer_discovery_progress(false);
            }
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
