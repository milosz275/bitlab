#ifndef __PEER_CONNECTION_H
#define __PEER_CONNECTION_H

/**
 * \brief Lists all connected nodes and their details.
 *
 * This function iterates through the list of nodes and prints the details
 * of each node that is currently connected.
 */
void list_connected_nodes();

void send_getaddr_and_wait(int idx);

int connect_to_peer(const char* ip_addr);

#endif // __PEER_CONNECTION_H
