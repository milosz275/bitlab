#ifndef __PEER_CONNECTION_H
#define __PEER_CONNECTION_H

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

#endif // __PEER_CONNECTION_H
