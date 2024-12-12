#ifndef __PEER_DISCOVERY_H
#define __PEER_DISCOVERY_H

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

#endif
