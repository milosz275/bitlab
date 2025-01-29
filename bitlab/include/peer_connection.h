#ifndef __PEER_CONNECTION_H
#define __PEER_CONNECTION_H

#include <stdint.h>
#include <pthread.h>

// Maximum number of peers to track
#define MAX_NODES 100

// Bitcoin mainnet magic bytes:
#define BITCOIN_MAINNET_MAGIC 0xD9B4BEF9

// Default Bitcoin mainnet port:
#define BITCOIN_MAINNET_PORT 8333

// Structure for Bitcoin P2P message header (24 bytes).
// For reference: https://en.bitcoin.it/wiki/Protocol_documentation#Message_structure
#pragma pack(push, 1)
typedef struct
{
    unsigned int magic; // Magic value indicating message origin network
    char command[12]; // ASCII command (null-padded)
    unsigned int length; // Payload size (little-endian)
    unsigned int checksum; // First 4 bytes of double SHA-256 of the payload
} bitcoin_msg_header;
#pragma pack(pop)

// Node structure representing a peer connection
typedef struct
{
    char ip_address[64]; // IP address of the peer
    uint16_t port; // Port of the peer
    int socket_fd; // Socket file descriptor for communication
    pthread_t thread; // Thread assigned to this connection
    int is_connected; // Connection status
    int operation_in_progress; // is operation in progress
    uint64_t compact_blocks; // Does peer want to use compact blocks
    uint64_t fee_rate; // Min fee rate in sat/kB of transaction that peer allows
} Node;

// Declare the global array of nodes
extern Node nodes[MAX_NODES];

/**
 * @brief Lists all connected nodes and their details.
 *
 * This function iterates through the list of nodes and prints the details
 * of each node that is currently connected.
 */
void list_connected_nodes();

/**
 * @brief Sends a 'getaddr' message to the peer and waits for a response.
 *
 * This function sends a 'getaddr' message to the peer identified by the given index
 * and waits for a response. It is used to request a list of known peers from the connected peer.
 *
 * @param idx The index of the peer in the nodes array.
 */
void send_getaddr_and_wait(int idx);

/**
 * @brief Connects to a peer using the specified IP address.
 *
 * This function attempts to establish a connection to a peer using the given IP address.
 * It returns a socket file descriptor if the connection is successful, or -1 if it fails.
 *
 * @param ip_addr The IP address of the peer to connect to.
 */
int connect_to_peer(const char* ip_addr);

/**
 * @brief Disconnects from the node specified by the node ID.
 *
 * This function disconnects from the node specified by the given node ID. It closes the socket,
 * terminates the thread, and logs the disconnection.
 *
 * @param node_id The ID of the node in the nodes array to disconnect from.
 */
void disconnect(int node_id);

#endif // __PEER_CONNECTION_H
