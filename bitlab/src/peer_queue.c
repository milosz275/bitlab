#include "peer_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "utils.h"

static Peer peer_queue[MAX_PEERS];
static int peer_queue_start = 0;
static int peer_queue_end = 0;
static pthread_mutex_t peer_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void add_peer_to_queue(const char* ip, int port)
{
    pthread_mutex_lock(&peer_queue_mutex);

    // Check if the IP-port pair already exists in the queue
    for (int i = peer_queue_start; i != peer_queue_end; i = (i + 1) % MAX_PEERS)
    {
        if (strcmp(peer_queue[i].ip, ip) == 0 && peer_queue[i].port == port)
        {
            // IP-port pair already exists, don't add it again
            log_message(LOG_INFO, BITLAB_LOG, __FILE__, "Duplicate peer: %s:%d, not added", ip, port);
            pthread_mutex_unlock(&peer_queue_mutex);
            return;
        }
    }

    // Check if the queue is full
    if ((peer_queue_end + 1) % MAX_PEERS == peer_queue_start)
    {
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Peer queue is full, cannot add peer: %s:%d", ip, port);
        pthread_mutex_unlock(&peer_queue_mutex);
        return;
    }

    // Extract the port if not provided
    if (port == 0)
    {
        char* colon_pos = strrchr(ip, ':');
        if (colon_pos != NULL)
        {
            port = atoi(colon_pos + 1);
            strncpy(peer_queue[peer_queue_end].ip, ip, colon_pos - ip);
            peer_queue[peer_queue_end].ip[colon_pos - ip] = '\0';
        }
        else
        {
            log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Invalid IP format, cannot extract port: %s", ip);
            pthread_mutex_unlock(&peer_queue_mutex);
            return;
        }
    }
    else
    {
        strncpy(peer_queue[peer_queue_end].ip, ip, sizeof(peer_queue[peer_queue_end].ip));
    }

    // Add the IP-port pair to the queue
    peer_queue[peer_queue_end].port = port;
    peer_queue_end = (peer_queue_end + 1) % MAX_PEERS;

    pthread_mutex_unlock(&peer_queue_mutex);
}

bool is_peer_queue_empty()
{
    pthread_mutex_lock(&peer_queue_mutex);
    bool empty = (peer_queue_start == peer_queue_end);
    pthread_mutex_unlock(&peer_queue_mutex);
    return empty;
}

bool get_peer_from_queue(char* buffer, size_t buffer_size)
{
    pthread_mutex_lock(&peer_queue_mutex);
    if (peer_queue_start == peer_queue_end)
    {
        pthread_mutex_unlock(&peer_queue_mutex);
        return false;
    }
    snprintf(buffer, buffer_size, "%s:%d", peer_queue[peer_queue_start].ip, peer_queue[peer_queue_start].port);
    peer_queue_start = (peer_queue_start + 1) % MAX_PEERS;
    pthread_mutex_unlock(&peer_queue_mutex);
    return true;
}

void clear_peer_queue()
{
    pthread_mutex_lock(&peer_queue_mutex);
    peer_queue_start = 0;
    peer_queue_end = 0;
    pthread_mutex_unlock(&peer_queue_mutex);
}

void print_peer_queue()
{
    pthread_mutex_lock(&peer_queue_mutex);
    if (peer_queue_start == peer_queue_end)
    {
        guarded_print_line("Peer queue is empty");
        pthread_mutex_unlock(&peer_queue_mutex);
        return;
    }
    for (int i = peer_queue_start; i != peer_queue_end; i = (i + 1) % MAX_PEERS)
        guarded_print_line("%s:%d", peer_queue[i].ip, peer_queue[i].port);
    pthread_mutex_unlock(&peer_queue_mutex);
}
