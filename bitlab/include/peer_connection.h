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

#define htole16(x) ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))
#define htole32(x) ((uint32_t)((((x) & 0xFF) << 24) | (((x) >> 8) & 0xFF00) | (((x) >> 16) & 0xFF) | (((x) >> 24) & 0xFF000000)))
#define htole64(x) ((uint64_t)((((x) & 0xFF) << 56) | (((x) >> 8) & 0xFF00) | (((x) >> 16) & 0xFF0000) | (((x) >> 24) & 0xFF000000) | (((x) >> 32) & 0xFF00000000) | (((x) >> 40) & 0xFF0000000000) | (((x) >> 48) & 0xFF000000000000) | (((x) >> 56) & 0xFF00000000000000)))

#define MAX_LOCATOR_COUNT 10
#define GENESIS_BLOCK_HASH "0000000000000000000000000000000000000000000000000000000000000000"
#define HEADERS_FILE "headers.dat"
#define MAX_HEADERS_COUNT 2000

// Structure for Bitcoin P2P message header (24 bytes).
// For reference: https://en.bitcoin.it/wiki/Protocol_documentation#Message_structure
#pragma pack(push, 1)
typedef struct
{
    unsigned int magic; // Magic value indicating message origin network
    char command[12]; // ASCII command (null-padded)
    unsigned int length; // Payload size (little-endian)
    unsigned char checksum[4]; // First 4 bytes of double SHA-256 of the payload
} bitcoin_msg_header;
#pragma pack(pop)

/**
 * @brief The structure to store information about a connected peer.
 *
 * @param ip_address The IP address of the peer.
 * @param port The port of the peer.
 * @param socket_fd The socket file descriptor for communication.
 * @param thread The thread assigned to this connection.
 * @param is_connected The connection status.
 * @param operation_in_progress The operation status.
 * @param compact_blocks Does peer want to use compact blocks.
 * @param fee_rate Min fee rate in sat/kB of transaction that peer allows.
 */
typedef struct
{
    char ip_address[64];
    uint16_t port;
    int socket_fd;
    pthread_t thread;
    int is_connected;
    int operation_in_progress;
    uint64_t compact_blocks;
    uint64_t fee_rate;
} Node;

// global array of nodes
extern Node nodes[MAX_NODES];

/**
 * @brief Lists all connected nodes and their details.
 *
 * This function iterates through the list of nodes and prints the details
 * of each node that is currently connected.
 */
void list_connected_nodes();

/**
 * @brief Get the index of the node with the given IP address.
 *
 * @param ip_address The IP address of the node.
 * @return The index of the node in the global array.
 */
int get_idx(const char* ip_address);

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


unsigned char* load_blocks_from_file(const char* filename, size_t* payload_len);

/**
 * @brief Sends a 'getheaders' message to the peer and waits for a response.
 *
 * This function sends a 'getheaders' message to the peer identified by the given index
 * and waits for a response. It is used to request a list of known peers from the connected peer.
 *
 * @param idx The index of the peer in the nodes array.
 */
void send_getheaders_and_wait(int idx);

/**
 * @brief Sends a 'headers' message to the peer.
 *
 * This function sends a 'headers' message to the peer identified by the given index.
 * It retrieves the block headers from the local storage starting from the specified
 * start hash up to the stop hash or the maximum number of headers allowed.
 *
 * @param idx The index of the peer in the nodes array.
 * @param start_hash The hash of the first block header to send.
 * @param stop_hash The hash of the last block header to send.
 */
void send_headers(int idx, const unsigned char* start_hash, const unsigned char* stop_hash);

/**
 * @brief Sends a 'getblocks' message to the peer and waits for a response.
 *
 * This function sends a 'getblocks' message to the peer identified by the given index
 * and waits for a response. It is used to request a list of blocks from the connected peer.
 * The response is processed and the blocks are saved to a file.
 *
 * @param idx The index of the peer in the nodes array.
 */
void send_getblocks_and_wait(int idx);

/**
 * @brief Sends a 'getdata' message to the peer and waits for a response.
 *
 * This function sends a 'getdata' message to the peer identified by the given index
 * and waits for a response. It is used to request specific blocks or transactions
 * from the connected peer based on the provided hashes. The response is saved to a file
 * and logged to the Bitlab logs.
 *
 * @param idx The index of the peer in the nodes array.
 * @param hashes An array of hashes representing the blocks or transactions to request.
 * @param hash_count The number of hashes in the array.
 */
void send_getdata_and_wait(int idx, const unsigned char* hashes, size_t hash_count);

/**
 * @brief Sends an 'inv' message to the peer and waits for a response.
 *
 * This function sends an 'inv' message to the peer identified by the given index
 * and waits for a response. It is used to advertise the knowledge of one or more objects
 * (blocks or transactions). The inventory data is provided as input to the function.
 *
 * @param idx The index of the peer in the nodes array.
 * @param inv_data An array of inventory vectors (type + hash).
 * @param inv_count The number of inventory vectors in the array.
 */
void send_inv_and_wait(int idx, const unsigned char* inv_data, size_t inv_count);

#endif // __PEER_CONNECTION_H
