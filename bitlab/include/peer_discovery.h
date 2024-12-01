#ifndef __PEER_DISCOVERY_H
#define __PEER_DISCOVERY_H

#include <stdint.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_PEERS 10

typedef struct
{
    struct sockaddr_in address;
    int port;
    int is_active;
} peer_info;

/**
 * Initializes peer discovery based on configuration.
 */
void init_peer_discovery();

/**
 * Peer discovery handler thread.
 *
 * @param arg Not used. Write dummy code for no warning.
 */
void* handle_peer_discovery(void* arg);

/**
 * Send a "getaddr" message to a peer.
 */
void send_getaddr_request(int socket_fd);

/**
 * Process an "addr" message to update known peers.
 */
void process_addr_message(int socket_fd);

#endif
