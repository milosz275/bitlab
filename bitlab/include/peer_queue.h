#ifndef __PEER_QUEUE_H
#define __PEER_QUEUE_H

#define MAX_PEERS 10000

#include <stdbool.h>
#include <stddef.h>

/**
 * The peer structure used to store the peer information.
 *
 * @param ip The IP address of the peer.
 * @param port The port of the peer.
 */
typedef struct
{
    char ip[256];
    int port;
} Peer;

/**
 * Add a peer to the queue.
 *
 * @param ip The IP address of the peer.
 * @param port The port of the peer.
 */
void add_peer_to_queue(const char* ip, int port);

/**
 * Check if the peer queue is empty.
 *
 * @return True if the peer queue is empty, false otherwise.
 */
bool is_peer_queue_empty();

/**
 * Get a peer from the queue.
 *
 * @param buffer The buffer to store the peer.
 * @param buffer_size The size of the buffer.
 * @return True if the peer was successfully retrieved, false otherwise.
 */
bool get_peer_from_queue(char* buffer, size_t buffer_size);

/**
 * Clear the peer queue.
 */
void clear_peer_queue();

/**
 * Print the peer queue.
 */
void print_peer_queue();

#endif // __PEER_QUEUE_H
