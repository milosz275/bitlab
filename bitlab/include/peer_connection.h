#ifndef __PEER_CONNECTION_H
#define __PEER_CONNECTION_H


void send_getaddr_and_wait(int idx);

int connect_to_peer(const char* ip_addr);

#endif // __PEER_CONNECTION_H
