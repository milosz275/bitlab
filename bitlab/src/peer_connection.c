#include "peer_connection.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <errno.h>
#include <stdint.h> // for uint64_t, etc.
#include <openssl/sha.h> // For SHA-256
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <bits/pthreadtypes.h>

#include "peer_queue.h"
#include "utils.h"
#include "log.h"
#include "ip.h"

// Global array to hold connected nodes
Node nodes[MAX_NODES];

void parse_inv_message(const unsigned char* payload, size_t payload_len);
void handle_inv_message(int idx, const unsigned char* payload, size_t payload_len);
size_t build_getblocks_message(unsigned char* buffer, size_t buffer_size, const unsigned char* block_locator, size_t locator_count);
size_t write_var_int(unsigned char* buf, uint64_t value);
void decode_transactions(const unsigned char* block_data, size_t block_len);

/**
 * compute_checksum:
 *   Calculate the double-SHA256 of the payload, then copy the first 4 bytes
 *   into `out[4]` as the checksum.
 */
static void compute_checksum(const unsigned char* payload, size_t payload_len,
    unsigned char out[4])
{
    unsigned char hash1[SHA256_DIGEST_LENGTH];
    unsigned char hash2[SHA256_DIGEST_LENGTH];

    // First SHA-256
    SHA256(payload, payload_len, hash1);
    // Second SHA-256
    SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);

    // Copy first 4 bytes to out
    memcpy(out, hash2, 4);
}

/**
 * build_version_payload:
 *   Build a minimal "version" message payload. This includes:
 *     protocol version, services, timestamp, addr_recv, addr_from, nonce,
 *     user agent, start_height, and relay.
 */
static size_t build_version_payload(unsigned char* buf, size_t buf_size)
{
    // We need at least 86 bytes for a minimal version message
    if (buf_size < 86)
    {
        return 0;
    }

    memset(buf, 0, buf_size);
    size_t offset = 0;

    // (1) protocol version (int32_t)
    unsigned int protocol_version = 70015;
    memcpy(buf + offset, &protocol_version, 4);
    offset += 4;

    // (2) services (uint64_t) - set to 0
    unsigned long long services = 0ULL;
    memcpy(buf + offset, &services, 8);
    offset += 8;

    // (3) timestamp (int64_t) - current epoch
    long long timestamp = (long long)time(NULL);
    memcpy(buf + offset, &timestamp, 8);
    offset += 8;

    // (4) addr_recv (26 bytes)
    memset(buf + offset, 0, 8); // services
    offset += 8;
    memset(buf + offset, 0, 16); // IP
    offset += 16;
    unsigned short port = htons(BITCOIN_MAINNET_PORT);
    memcpy(buf + offset, &port, 2);
    offset += 2;

    // (5) addr_from (26 bytes) - same approach
    memset(buf + offset, 0, 8);
    offset += 8;
    memset(buf + offset, 0, 16);
    offset += 16;
    memcpy(buf + offset, &port, 2);
    offset += 2;

    // (6) nonce (8 bytes) - random
    unsigned long long nonce = (((unsigned long long)rand()) << 32) | rand();
    memcpy(buf + offset, &nonce, 8);
    offset += 8;

    // (7) user agent (var_str)
    const char* user_agent = "/Satoshi:0.1.0/";
    unsigned char user_agent_len = (unsigned char)strlen(user_agent);
    buf[offset] = user_agent_len;
    offset += 1;
    memcpy(buf + offset, user_agent, user_agent_len);
    offset += user_agent_len;

    // (8) start_height (int32_t) - let’s set 0 for demo
    unsigned int start_height = 0;
    memcpy(buf + offset, &start_height, 4);
    offset += 4;

    // (9) relay (bool) for version >= 70001; set to 0
    unsigned char relay = 0;
    buf[offset] = relay;
    offset += 1;

    return offset; // total payload size
}

/**
 * build_message:
 *   Creates a Bitcoin P2P message (header + payload) in `buf`.
 *   The 'command' is zero-padded to 12 bytes. The payload is appended.
 *   We compute the 4-byte double-SHA256 checksum.
 */
static size_t build_message(
    unsigned char* buf,
    size_t buf_size,
    const char* command,
    const unsigned char* payload,
    size_t payload_len)
{
    if (buf_size < (sizeof(bitcoin_msg_header) + payload_len))
    {
        return 0;
    }

    // Prepare the header
    bitcoin_msg_header header;
    memset(&header, 0, sizeof(header));

    header.magic = BITCOIN_MAINNET_MAGIC;

    // Zero out the command field, then copy up to 12 bytes
    memset(header.command, 0, sizeof(header.command));
    {
        size_t cmd_len = strlen(command);
        if (cmd_len > sizeof(header.command))
        {
            cmd_len = sizeof(header.command);
        }
        memcpy(header.command, command, cmd_len);
    }

    header.length = payload_len;

    // Compute the checksum of the payload
    unsigned char csum[4];
    compute_checksum(payload, payload_len, csum);
    memcpy(&header.checksum, csum, 4);

    // Copy header into buf, then the payload
    memcpy(buf, &header, sizeof(header));
    memcpy(buf + sizeof(header), payload, payload_len);

    return sizeof(header) + payload_len;
}

size_t build_getheaders_message(unsigned char* buffer, size_t buffer_size, const unsigned char* block_locator, size_t locator_count)
{
    if (buffer_size < sizeof(bitcoin_msg_header) + 37)
        return 0;

    unsigned char payload[4 + 1 + (locator_count * 32) + 32];

    // Set protocol version (4 bytes)
    uint32_t version = htonl(70015);
    memcpy(payload, &version, 4);

    // Set block locator count (var_int encoding)
    size_t offset = 4;
    offset += write_var_int(payload + offset, locator_count);

    // Copy block locator hashes
    memcpy(payload + offset, block_locator, locator_count * 32);
    offset += locator_count * 32;

    // Set hash_stop (32 bytes of zeros for max 2000 headers)
    memset(payload + offset, 0, 32);
    offset += 32;

    // Ensure buffer is large enough
    if (offset > buffer_size)
        return 0;

    // Build final message
    return build_message(buffer, buffer_size, "getheaders", payload, offset);
}

void compute_block_hash(const unsigned char* block_header, unsigned char* output_hash)
{
    unsigned char hash1[32];
    SHA256(block_header, 80, hash1); // First SHA-256
    SHA256(hash1, 32, output_hash);  // Second SHA-256
}

size_t write_var_int(unsigned char* buffer, uint64_t value)
{
    if (buffer == NULL)
    {
        // Calculate the size of the var_int encoding
        if (value < 0xfd)
            return 1;
        else if (value <= 0xffff)
            return 3;
        else if (value <= 0xffffffff)
            return 5;
        else
            return 9;
    }

    log_message(LOG_DEBUG, BITLAB_LOG, __FILE__, "write_var_int: buffer=%p, value=%lu", (void*)buffer, value);

    if (value < 0xfd)
    {
        buffer[0] = (unsigned char)value;
        return 1;
    }
    else if (value <= 0xffff)
    {
        buffer[0] = 0xfd;
        buffer[1] = (unsigned char)(value & 0xff);
        buffer[2] = (unsigned char)((value >> 8) & 0xff);
        return 3;
    }
    else if (value <= 0xffffffff)
    {
        buffer[0] = 0xfe;
        buffer[1] = (unsigned char)(value & 0xff);
        buffer[2] = (unsigned char)((value >> 8) & 0xff);
        buffer[3] = (unsigned char)((value >> 16) & 0xff);
        buffer[4] = (unsigned char)((value >> 24) & 0xff);
        return 5;
    }
    else
    {
        buffer[0] = 0xff;
        buffer[1] = (unsigned char)(value & 0xff);
        buffer[2] = (unsigned char)((value >> 8) & 0xff);
        buffer[3] = (unsigned char)((value >> 16) & 0xff);
        buffer[4] = (unsigned char)((value >> 24) & 0xff);
        buffer[5] = (unsigned char)((value >> 32) & 0xff);
        buffer[6] = (unsigned char)((value >> 40) & 0xff);
        buffer[7] = (unsigned char)((value >> 48) & 0xff);
        buffer[8] = (unsigned char)((value >> 56) & 0xff);
        return 9;
    }
}

void list_connected_nodes()
{
    for (int i = 0; i < MAX_NODES; ++i)
    {
        if (nodes[i].is_connected == 1)
        {
            guarded_print_line("Node %d:", i);
            guarded_print_line(" IP Address: %s", nodes[i].ip_address);
            guarded_print_line(" Port: %u", nodes[i].port);
            guarded_print_line(" Socket FD: %d", nodes[i].socket_fd);
            guarded_print_line(" Thread ID: %lu", nodes[i].thread);
            guarded_print_line(" Is Connected: %d", nodes[i].is_connected);
            guarded_print_line(" Is operation in progress: %d",
                nodes[i].operation_in_progress);
            guarded_print_line(" Compact blocks: %lu",
                nodes[i].compact_blocks);
            guarded_print_line(" Fee_rate: %lu",
                nodes[i].fee_rate);
        }
    }
}

int get_idx(const char* ip_address)
{
    for (int i = 0; i < MAX_NODES; ++i)
    {
        if (nodes[i].is_connected == 1 && strcmp(nodes[i].ip_address, ip_address) == 0)
        {
            return i;
        }
    }
    return -1;
}

void send_getaddr_and_wait(int idx)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        node->ip_address);

    // Build the 'getaddr' message
    unsigned char getaddr_msg[sizeof(bitcoin_msg_header)];
    size_t msg_len = build_message(getaddr_msg, sizeof(getaddr_msg), "getaddr", NULL,
        0);
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'getaddr' message.\n");
        return;
    }

    // Send the 'getaddr' message
    ssize_t bytes_sent = send(node->socket_fd, getaddr_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'getaddr' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'getaddr' message.");
    node->operation_in_progress = 1;

    // Wait for 10 seconds for a response
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    char buffer[32768];
    size_t total_bytes_received = 0;
    ssize_t bytes_received;

    // Receive the message in parts
    while (total_bytes_received < sizeof(bitcoin_msg_header))
    {
        log_message(LOG_INFO, log_filename, __FILE__, "first while receiving");
        bytes_received = recv(node->socket_fd, buffer + total_bytes_received,
            sizeof(buffer) - total_bytes_received - 1, 0);
        if (bytes_received <= 0)
        {
            log_message(LOG_INFO, log_filename, __FILE__, "Recv 1 failed: %s",
                strerror(errno));
            node->is_connected = 0;
            node->operation_in_progress = 0;
            return;
        }
        total_bytes_received += bytes_received;
    }

    bitcoin_msg_header* hdr = (bitcoin_msg_header*)buffer;
    size_t payload_len = hdr->length;
    size_t message_size = sizeof(bitcoin_msg_header) + payload_len;
    int retry_count = 0;
    const int max_retries = 1;

    while (total_bytes_received < message_size)
    {
        log_message(LOG_INFO, log_filename, __FILE__, "second while receiving");
        bytes_received = recv(node->socket_fd, buffer + total_bytes_received,
            sizeof(buffer) - total_bytes_received - 1, 0);
        if (bytes_received <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                log_message(LOG_INFO, log_filename, __FILE__,
                    "second while receiving inside if errno");
                // Resource temporarily unavailable, continue receiving

                if (++retry_count >= max_retries)
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "Max retries reached, stopping recv.");
                    node->is_connected = 0;
                    node->operation_in_progress = 0;
                    break;
                }
                continue;
            }
            log_message(LOG_INFO, log_filename, __FILE__, "Recv failed: %s",
                strerror(errno));
            node->is_connected = 0;
            node->operation_in_progress = 0;
            return;
        }
        total_bytes_received += bytes_received;
    }

    buffer[total_bytes_received] = '\0'; // Null-terminate the received data

    if (hdr->magic == BITCOIN_MAINNET_MAGIC)
    {
        char cmd_name[13];
        memset(cmd_name, 0, sizeof(cmd_name));
        memcpy(cmd_name, hdr->command, 12);

        log_message(LOG_INFO, log_filename, __FILE__, "[!] Received %s command ",
            cmd_name);

        const unsigned char* payload_data = (unsigned char*)buffer + sizeof(
            bitcoin_msg_header);


        if (strcmp(cmd_name, "addr") == 0)
        {
            size_t offset = 0;

            // Check if the payload length is sufficient to read the count of address entries
            if (payload_len < 1)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "Insufficient payload length to read address count");
                return;
            }

            // Read the count of address entries (var_int)
            uint64_t count = read_var_int(payload_data + offset, &offset);

            // Log the count of address entries
            log_message(LOG_INFO, log_filename, __FILE__, "Address count: %llu", count);

            // Ensure count does not exceed the maximum allowed entries
            if (count > 1000)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "Address count exceeds maximum allowed: %llu", count);
                return;
            }

            for (uint64_t i = 0; i < count; i++)
            {
                if (offset + 30 > payload_len)
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "Insufficient payload length for address entry");
                    return;
                }

                // Read timestamp (4 bytes)
                uint32_t timestamp;
                memcpy(&timestamp, payload_data + offset, 4);
                timestamp = ntohl(timestamp);
                offset += 4;

                // Read services (8 bytes, skip for now)
                offset += 8;

                // Read IP address (16 bytes)
                struct in6_addr ip_addr;
                memcpy(&ip_addr, payload_data + offset, 16);
                offset += 16;

                // Read port (2 bytes)
                uint16_t port;
                memcpy(&port, payload_data + offset, 2);
                port = ntohs(port);
                offset += 2;

                // Convert IP to string
                char ip_str[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &ip_addr, ip_str, INET6_ADDRSTRLEN);

                // Determine if the address is IPv4-mapped or IPv6
                const char* ip_type = "IPv6";
                if (IN6_IS_ADDR_V4MAPPED(&ip_addr))
                {
                    struct in_addr ipv4_addr;
                    memcpy(&ipv4_addr, &ip_addr.s6_addr[12], 4);
                    inet_ntop(AF_INET, &ipv4_addr, ip_str, INET_ADDRSTRLEN);
                    ip_type = "IPv4";
                }

                // Check if it's a valid IPv4 address
                if (strcmp(ip_type, "IPv4") == 0 && is_valid_ipv4(ip_str) && !
                    is_in_private_network(ip_str))
                {
                    // Guarded print of the IP address and port
                    guarded_print_line("Valid IPv4 Peer: %s:%u (timestamp: %u)", ip_str,
                        port, timestamp);

                    // Add to peer queue
                    add_peer_to_queue(ip_str, port);

                    // Log the result if valid
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "Received valid IPv4 address: %s:%u (timestamp: %u)",
                        ip_str, port, timestamp);
                }
                else if (strcmp(ip_type, "IPv6") == 0)
                {
                    // Log the IPv6 addresses if needed
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "Received IPv6 address: %s:%u (timestamp: %u)",
                        ip_str, port, timestamp);
                }
            }

            if (offset != payload_len)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "Remaining bytes after processing: %zu",
                    payload_len - offset);
            }
        }
    }
    else
    {
        log_message(LOG_WARN, log_filename, __FILE__,
            "else hdr->magic ");
    }

    node->operation_in_progress = 0;
}

/**
 * send_addr:
 *   Sends the 'addr' message to the specified socket with the list of peers.
 *   The addresses are converted to IPv6-mapped IPv4 addresses.
 *
 * Parameters:
 *   sockfd - The socket file descriptor to send the message to.
 *   ip_addr - The IP address of the peer to log the message.
 *
 * Returns:
 *   The number of bytes sent on success, or -1 on failure.
 */
ssize_t send_addr(int sockfd, const char* ip_addr)
{
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", ip_addr);

    int peer_count;
    Peer* peers = get_peer_queue(&peer_count);

    if (peer_count == 0 || peers == NULL)
    {
        guarded_print_line("[Error] No peers available to send");
        return -1;
    }

    // Allocate memory for address payload (IPv4 to IPv6-mapped)
    size_t payload_size = peer_count * sizeof(struct sockaddr_in6);
    unsigned char* addr_payload = malloc(payload_size);
    if (!addr_payload)
    {
        perror("malloc failed");
        free(peers);
        return -1;
    }

    // Populate address payload with peer data (IPv6-mapped addresses for IPv4)
    for (int i = 0; i < peer_count; ++i)
    {
        struct sockaddr_in6* addr = (struct sockaddr_in6*)&addr_payload[i * sizeof(
            struct sockaddr_in6)];
        memset(addr, 0, sizeof(struct sockaddr_in6));
        addr->sin6_family = AF_INET6;
        addr->sin6_port = htons(peers[i].port);
        addr->sin6_addr.s6_addr[10] = 0xFF;
        addr->sin6_addr.s6_addr[11] = 0xFF;
        inet_pton(AF_INET, peers[i].ip, &addr->sin6_addr.s6_addr[12]);

        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr->sin6_addr, ip_str, INET6_ADDRSTRLEN);
        if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr))
        {
            struct in_addr ipv4_addr;
            memcpy(&ipv4_addr, &addr->sin6_addr.s6_addr[12], 4);
            inet_ntop(AF_INET, &ipv4_addr, ip_str, INET_ADDRSTRLEN);
        }
        log_message(LOG_INFO, log_filename, __FILE__, "Sending address: %s:%d", ip_str,
            peers[i].port);
    }

    // Prepare message buffer
    unsigned char addr_msg[sizeof(bitcoin_msg_header) + payload_size];
    size_t msg_len = build_message(addr_msg, sizeof(addr_msg), "addr", addr_payload,
        payload_size);

    if (msg_len == 0)
    {
        guarded_print_line("Failed to build 'addr' message.");
        free(peers);
        free(addr_payload);
        return -1;
    }

    // Send the 'addr' message
    ssize_t bytes_sent = send(sockfd, addr_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        perror("Sending addresses failed");
    }
    else
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "Successfully sent addresses");
    }

    free(peers);
    free(addr_payload);

    return bytes_sent;
}

/**
 * send_verack:
 *   Constructs a verack message with an **empty** payload (length = 0)
 *   and a valid 4-byte checksum for an empty payload.
 */
static ssize_t send_verack(int sockfd, const char* ip_addr)
{
    unsigned char verack_msg[sizeof(bitcoin_msg_header)];
    memset(verack_msg, 0, sizeof(verack_msg));

    bitcoin_msg_header verack_header;
    memset(&verack_header, 0, sizeof(verack_header));
    verack_header.magic = BITCOIN_MAINNET_MAGIC;

    // "verack" as a 12-byte command (zero-padded)
    {
        const char* cmd = "verack";
        memset(verack_header.command, 0, sizeof(verack_header.command));
        size_t cmd_len = strlen(cmd);
        if (cmd_len > sizeof(verack_header.command))
        {
            cmd_len = sizeof(verack_header.command);
        }
        memcpy(verack_header.command, cmd, cmd_len);
    }

    verack_header.length = 0; // no payload

    // Checksum for empty payload => double-SHA256("") => first 4 bytes
    unsigned char csum[4];
    compute_checksum(NULL, 0, csum);
    memcpy(&verack_header.checksum, csum, 4);

    // Copy to buffer
    memcpy(verack_msg, &verack_header, sizeof(verack_header));

    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", ip_addr);
    log_message(LOG_INFO, log_filename, __FILE__,
        "sent verack");
    // log_to_file('test.log', 'sent verack');
    // Send to peer
    return send(sockfd, verack_msg, sizeof(verack_header), 0);
}

/**
 * send_pong:
 *   Send a "pong" message echoing the same 8-byte nonce from a "ping" payload.
 */
static ssize_t send_pong(int sockfd, const unsigned char* nonce8)
{
    // Build an 8-byte payload containing the same nonce
    unsigned char pong_payload[8];
    memcpy(pong_payload, nonce8, 8);

    // Create the message buffer
    unsigned char pong_msg[sizeof(bitcoin_msg_header) + 8];
    size_t msg_len = build_message(
        pong_msg,
        sizeof(pong_msg),
        "pong",
        pong_payload,
        8
    );
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] build_message failed for 'pong'.\n");
        return -1;
    }

    return send(sockfd, pong_msg, msg_len, 0);
}

ssize_t send_ping(int sockfd, const char* ip_addr)
{
    // Generate an 8-byte nonce
    uint64_t nonce = ((uint64_t)rand() << 32) | rand();

    // Prepare the ping payload (8 bytes)
    unsigned char ping_payload[8];
    memcpy(ping_payload, &nonce, sizeof(nonce));

    // Prepare the complete message (header + payload)
    unsigned char ping_msg[sizeof(bitcoin_msg_header) + sizeof(ping_payload)];
    size_t msg_len = build_message(
        ping_msg,
        sizeof(ping_msg),
        "ping",
        ping_payload,
        sizeof(ping_payload)
    );

    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'ping' message.\n");
        return -1;
    }

    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        ip_addr);
    ssize_t bytes_sent = send(sockfd, ping_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'ping' message: %s", strerror(errno));
        return -1;
    }

    log_message(LOG_INFO, log_filename, __FILE__,
        "Sent 'ping' message with nonce: %llu", (unsigned long long)nonce);
    return bytes_sent;
}

void initialize_node(Node* node, const char* ip, uint16_t port, int socket_fd)
{
    snprintf(node->ip_address, sizeof(node->ip_address), "%s", ip);
    node->port = port;
    node->socket_fd = socket_fd;
    node->is_connected = 1; // Mark as connected
}

void* peer_communication(void* arg)
{
    Node* node = (Node*)arg;

    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        node->ip_address);
    log_message(LOG_INFO, log_filename, __FILE__,
        "started peer communication with node with ip: %s", node->ip_address);

    char buffer[2048];
    struct timeval tv;
    tv.tv_sec = 5; // 5 seconds timeout for recv
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    time_t last_ping_time = time(NULL);

    while (node->is_connected)
    {
        // Check the magic
        ssize_t bytes_received = recv(node->socket_fd, buffer, sizeof(buffer) - 1, 0);

        // Wait while other operation is in progress e.g. getaddr
        while (node->operation_in_progress)
        {
            sleep(1);
        }
        if (bytes_received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "recv() timed out, continuing...");
            }
            else
            {
                log_message(LOG_INFO, log_filename, __FILE__,
                    "Recv failed: %s", strerror((errno)));
            }
            time_t current_time = time(NULL);
            if (difftime(current_time, last_ping_time) >= 5)
            {
                send_ping(node->socket_fd, node->ip_address);
                last_ping_time = current_time;
            }

            continue;
        }
        if (bytes_received == 0)
        {
            log_message(LOG_INFO, log_filename, __FILE__,
                "Connection closed by peer %s", node->ip_address);
            node->is_connected = 0;
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the received data
        log_message(LOG_INFO, log_filename, __FILE__,
            "Received bytes: %s", buffer);
        // If we have at least a full header, parse it
        if (bytes_received < (ssize_t)sizeof(bitcoin_msg_header))
        {
            log_message(LOG_INFO, log_filename, __FILE__,
                "[!] Received %zd bytes (less than header size 24).",
                bytes_received);
            continue;
        }

        // Cast to a header
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)buffer;
        if (hdr->magic == BITCOIN_MAINNET_MAGIC)
        {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            log_message(LOG_INFO, log_filename, __FILE__,
                "[!] Received %s command ",
                cmd_name);

            // Determine the payload length & pointer
            size_t payload_len = hdr->length;
            const unsigned char* payload_data = (const unsigned char*)buffer + sizeof(
                bitcoin_msg_header);

            // In real code, you’d handle partial messages if n < header+payload
            if (bytes_received < (ssize_t)(sizeof(bitcoin_msg_header) + payload_len))
            {
                log_message(LOG_INFO, log_filename, __FILE__,
                    "Incomplete message; got %zd bytes, expected %zu.\n",
                    bytes_received, sizeof(bitcoin_msg_header) + payload_len);
                continue;
            }

            if (strcmp(cmd_name, "ping") == 0)
            {
                // Typically 8-byte payload
                if (payload_len == 8)
                {
                    ssize_t s = send_pong(node->socket_fd, payload_data);
                    if (s < 0)
                    {
                        log_message(LOG_ERROR, log_filename, __FILE__,
                            "Sending pong: %s\n", strerror(errno));
                    }
                    else
                    {
                        log_message(LOG_INFO, log_filename, __FILE__,
                            "Successfully sent pong");
                    }
                }
                else
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "Ping payload length is not 8 bytes, not sending pong");
                }
            }
            else if (strcmp(cmd_name, "getaddr") == 0)
            {
                if (payload_len == 0)
                {
                    ssize_t s = send_addr(node->socket_fd, node->ip_address);
                    if (s < 0)
                    {
                        log_message(LOG_ERROR, log_filename, __FILE__,
                            "Sending addr: %s\n", strerror(errno));
                    }
                    else
                    {
                        log_message(LOG_INFO, log_filename, __FILE__,
                            "Successfully sent addresses");
                    }
                }
                else
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "Invalid payload length for 'getaddr' command: %zu",
                        payload_len);
                }
            }
            else if (strcmp(cmd_name, "getheaders") == 0)
            {
                // Parse the getheaders message
                size_t offset = sizeof(bitcoin_msg_header);
                uint32_t version;
                memcpy(&version, payload_data + offset, 4);
                offset += 4;

                unsigned char start_hash[32];
                memcpy(start_hash, payload_data + offset, 32);
                offset += 32;

                unsigned char stop_hash[32];
                memcpy(stop_hash, payload_data + offset, 32);

                // Log the received getheaders message
                log_message(LOG_INFO, log_filename, __FILE__, "Received 'getheaders' message.");

                // Send the headers in response
                int idx = get_idx(node->ip_address);
                send_headers(idx, start_hash, stop_hash);
            }
            else if (strcmp(cmd_name, "getblocks") == 0)
            {
                // Log the received getblocks message
                log_message(LOG_INFO, log_filename, __FILE__, "Received 'getblocks' message.");

                // Handle the getblocks request
                size_t payload_len;
                unsigned char* payload = load_blocks_from_file("blocks.dat", &payload_len);
                if (!payload)
                {
                    log_message(LOG_ERROR, log_filename, __FILE__, "Failed to load blocks from file");
                }
                else
                {
                    ssize_t bytes_sent = send(node->socket_fd, payload, payload_len, 0);
                    if (bytes_sent < 0)
                    {
                        log_message(LOG_ERROR, log_filename, __FILE__, "Failed to send blocks: %s", strerror(errno));
                    }
                    else
                    {
                        log_message(LOG_INFO, log_filename, __FILE__, "Sent blocks to node %s", node->ip_address);
                    }
                    free(payload);
                }
            }
            else if (strcmp(cmd_name, "inv") == 0)
            {
                // Handle the inv message
                log_message(LOG_INFO, log_filename, __FILE__, "Received 'inv' message.");
                int idx = get_idx(node->ip_address);
                handle_inv_message(idx, payload_data, payload_len);
            }
            else if (strcmp(cmd_name, "getdata") == 0)
            {
                // Handle the getdata message
                log_message(LOG_INFO, log_filename, __FILE__, "Received 'getdata' message.");
                // Load the requested data from file and send it
                size_t data_len;
                unsigned char* data = load_blocks_from_file("data.dat", &data_len);
                if (!data)
                {
                    log_message(LOG_ERROR, log_filename, __FILE__, "Failed to load data from file");
                }
                else
                {
                    ssize_t bytes_sent = send(node->socket_fd, data, data_len, 0);
                    if (bytes_sent < 0)
                    {
                        log_message(LOG_ERROR, log_filename, __FILE__, "Failed to send data: %s", strerror(errno));
                    }
                    else
                    {
                        log_message(LOG_INFO, log_filename, __FILE__, "Sent data to node %s", node->ip_address);
                    }
                    free(data);
                }
            }
            // Info about compact blocks is saved but handling of compact blocks is not implemented
            else if (strcmp(cmd_name, "sendcmpct") == 0)
            {
                // usually 9 bytes: fannounce(1 byte) + version(8 bytes)
                if (payload_len == 9)
                {
                    unsigned char fannounce = payload_data[0];
                    uint64_t cmpctversion;
                    memcpy(&cmpctversion, payload_data + 1, 8);
                    node->compact_blocks = cmpctversion;
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "compactblocks set to: %lu, fannounce: %u",
                        cmpctversion, fannounce);
                }
                else
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "sendcmpct payload length is not 9 bytes, its: %zu",
                        payload_len);
                }
            }
            // Fee rate is saved, but filtering out transactions is not implemented
            else if (strcmp(cmd_name, "feefilter") == 0)
            {
                // 8 bytes: an uint64_t in little-endian indicating min fee rate in sat/kB
                if (payload_len == 8)
                {
                    uint64_t fee_rate;
                    memcpy(&fee_rate, payload_data, 8);
                    node->fee_rate = fee_rate;
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "fee rate set to: %lu", fee_rate);
                }
                else
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "feefilter payload length is not 8 bytes, its: %zu",
                        payload_len);
                }
            }
        }

        time_t current_time = time(NULL);
        if (difftime(current_time, last_ping_time) >= 5)
        {
            send_ping(node->socket_fd, node->ip_address);
            last_ping_time = current_time;
        }
    }

    close(node->socket_fd); // Close the socket once done
    return NULL;
}

void create_peer_thread(Node* node)
{
    if (pthread_create(&node->thread, NULL, peer_communication, (void*)node) != 0)
    {
        perror("Failed to create thread for peer");
        exit(1);
    }

    if (pthread_detach(node->thread) != 0)
    {
        perror("Failed to detach thread for peer");
        exit(1);
    }
}

int connect_to_peer(const char* ip_addr)
{
    // Create the socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "[Error] socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 3; // seconds
    timeout.tv_usec = 0; // microseconds

    // Set timeout for recv
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // Set timeout for connect()
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt failed");
        close(sockfd);
        return -1;
    }

    // Prepare the sockaddr_in
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(BITCOIN_MAINNET_PORT);

    if (inet_pton(AF_INET, ip_addr, &servaddr.sin_addr) <= 0)
    {
        fprintf(stderr, "[Error] inet_pton failed (IP '%s'): %s\n",
            ip_addr, strerror(errno));
        close(sockfd);
        return -1;
    }

    // Connect
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        fprintf(stderr, "[Error] connect to %s:%d failed: %s\n",
            ip_addr, BITCOIN_MAINNET_PORT, strerror(errno));
        close(sockfd);
        return -1;
    }

    printf("[+] Connected to peer %s:%d\n", ip_addr, BITCOIN_MAINNET_PORT);

    // Build the 'version' payload
    unsigned char version_payload[200];
    size_t version_payload_len = build_version_payload(
        version_payload, sizeof(version_payload));
    if (version_payload_len == 0)
    {
        fprintf(stderr, "[Error] build_version_payload() failed.\n");
        close(sockfd);
        return -1;
    }

    // Create the full 'version' message
    unsigned char version_msg[256];
    size_t version_msg_len = build_message(
        version_msg,
        sizeof(version_msg),
        "version",
        version_payload,
        version_payload_len
    );
    if (version_msg_len == 0)
    {
        fprintf(stderr, "[Error] build_message() failed for 'version'.\n");
        close(sockfd);
        return -1;
    }

    // Send the 'version' message
    ssize_t bytes_sent = send(sockfd, version_msg, version_msg_len, 0);
    if (bytes_sent < 0)
    {
        fprintf(stderr, "[Error] send() of 'version' failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    printf("[+] Sent 'version' message (%zd bytes).\n", bytes_sent);

    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        ip_addr);
    init_logging(log_filename);
    // Read loop - up to 10 messages
    unsigned char recv_buf[2048];
    bool connected = false;
    for (int i = 0; i < 4; ++i)
    {
        printf("[d] Executing %dth receive loop iteration\n", i);
        ssize_t n = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "recv() timed out, continuing...");
                continue;
            }
            fprintf(stderr, "[Error] recv() failed: %s\n", strerror(errno));
            break;
        }
        if (n == 0)
        {
            // Peer closed the connection
            printf("[i] Peer closed the connection.\n");
            break;
        }

        // If we have at least a full header, parse it
        if (n < (ssize_t)sizeof(bitcoin_msg_header))
        {
            printf("[!] Received %zd bytes (less than header size 24).\n", n);
            continue;
        }

        printf("[<] Received %zd bytes.\n", n);

        // Cast to a header
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)recv_buf;

        // Check the magic
        if (hdr->magic == BITCOIN_MAINNET_MAGIC)
        {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            printf("[<] Received command: '%s'\n", cmd_name);

            // Determine the payload length & pointer
            size_t payload_len = hdr->length;

            if (n < (ssize_t)(sizeof(bitcoin_msg_header) + payload_len))
            {
                printf("[!] Incomplete message; got %zd bytes, expected %zu.\n",
                    n, sizeof(bitcoin_msg_header) + payload_len);
                continue;
            }

            // Handle known commands
            if (strcmp(cmd_name, "version") == 0)
            {
                // Send verack once we get their version
                ssize_t verack_sent = send_verack(sockfd, ip_addr);
                if (verack_sent < 0)
                {
                    fprintf(stderr, "[Error] sending verack: %s\n", strerror(errno));
                }
                else
                {
                    printf("[+] Sent 'verack' message.\n");
                }
                connected = true;
            }
            else if (strcmp(cmd_name, "verack") == 0)
            {
                connected = true;
                break;
            }
            else
            {
                // Unhandled command
                printf("[!] Unhandled command: '%s' (payload size=%u)\n",
                    cmd_name, hdr->length);
            }
        }
        else
        {
            printf("[!] Unexpected magic bytes (0x%08X).\n", hdr->magic);
        }
    }
    if (connected == true)
    {
        for (int j = 0; j < MAX_NODES; ++j)
        {
            if (nodes[j].is_connected == 0)
            {
                guarded_print_line("connected to node: %s | %d.", ip_addr, j);
                initialize_node(&nodes[j], ip_addr, 8333, sockfd);
                create_peer_thread(&nodes[j]);
                break;
            }
        }
    }
    else
    {
        guarded_print_line("Couldn't connect to node\n");
        close(sockfd);
    }
    return 0;
}

void disconnect(int node_id)
{
    if (node_id < 0 || node_id >= MAX_NODES || !nodes[node_id].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node ID or node not connected.\n");
        return;
    }

    Node* node = &nodes[node_id];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        node->ip_address);

    log_message(LOG_INFO, log_filename, __FILE__, "Disconnecting from node %s:%u",
        node->ip_address, node->port);

    close(node->socket_fd);

    node->is_connected = 0;

    if (pthread_cancel(node->thread) != 0)
    {
        perror("Failed to cancel thread for peer");
    }

    log_message(LOG_INFO, log_filename, __FILE__,
        "Successfully disconnected from node %s:%u", node->ip_address,
        node->port);
}

void print_block_header(const unsigned char* header)
{
    uint32_t version;
    unsigned char prev_block[32];
    unsigned char merkle_root[32];
    uint32_t timestamp;
    uint32_t bits;
    uint32_t nonce;

    memcpy(&version, header, 4);
    memcpy(prev_block, header + 4, 32);
    memcpy(merkle_root, header + 36, 32);
    memcpy(&timestamp, header + 68, 4);
    memcpy(&bits, header + 72, 4);
    memcpy(&nonce, header + 76, 4);

    printf("Version: %u\n", version);
    printf("Previous Block Hash: ");
    for (int i = 0; i < 32; i++) printf("%02x", prev_block[i]);
    printf("\n");
    printf("Merkle Root: ");
    for (int i = 0; i < 32; i++) printf("%02x", merkle_root[i]);
    printf("\n");
    printf("Timestamp: %u\n", timestamp);
    printf("Bits: %u\n", bits);
    printf("Nonce: %u\n", nonce);
    printf("\n");
}

void parse_headers_message(const unsigned char* payload, size_t payload_len)
{
    size_t offset = 0;
    FILE* file = fopen(HEADERS_FILE, "ab");
    if (!file)
    {
        perror("Failed to open headers file");
        return;
    }

    while (offset + 80 <= payload_len)
    {
        print_block_header(payload + offset);
        fwrite(payload + offset, 80, 1, file);
        offset += 80;
    }

    fclose(file);
}

void load_latest_known_block_hash(unsigned char* block_hash)
{
    FILE* file = fopen(HEADERS_FILE, "rb");
    if (!file)
    {
        // No known blocks, use genesis block hash
        memset(block_hash, 0, 32);
        return;
    }

    fseek(file, -80, SEEK_END);
    fread(block_hash, 32, 1, file);
    fclose(file);
}

void send_getheaders_and_wait(int idx)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    // Define block locator and locator count
    unsigned char block_locator[MAX_LOCATOR_COUNT * 32];
    int locator_count = 1; // Set to 1 to request headers starting from the latest known block

    // Load the latest known block hash
    load_latest_known_block_hash(block_locator);

    // Build the 'getheaders' message
    unsigned char getheaders_msg[sizeof(bitcoin_msg_header) + 4 + 1 + (MAX_LOCATOR_COUNT * 32) + 32];
    size_t msg_len = build_getheaders_message(getheaders_msg, sizeof(getheaders_msg), block_locator, locator_count);
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'getheaders' message.\n");
        return;
    }

    // Send the 'getheaders' message
    ssize_t bytes_sent = send(node->socket_fd, getheaders_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'getheaders' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'getheaders' message.");
    node->operation_in_progress = 1;

    // Wait for 10 seconds for a response
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Receive the response
    unsigned char buffer[4096];
    ssize_t bytes_received = recv(node->socket_fd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to receive response: %s", strerror(errno));
        node->operation_in_progress = 0;
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Received response to 'getheaders' message.");
    node->operation_in_progress = 0;

    // Process the response and print to CLI
    guarded_print("Received response to 'getheaders' message:\n");
    parse_headers_message(buffer, bytes_received);
}

void send_headers(int idx, const unsigned char* start_hash, const unsigned char* stop_hash)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    // Open the headers file
    FILE* file = fopen(HEADERS_FILE, "rb");
    if (!file)
    {
        perror("Failed to open headers file");
        return;
    }

    // Find the starting block header
    unsigned char buffer[81];
    size_t headers_count = 0;
    int found_start = 0;
    while (fread(buffer, 80, 1, file) == 1)
    {
        if (memcmp(buffer, start_hash, 32) == 0)
        {
            found_start = 1;
            break;
        }
    }

    if (!found_start)
    {
        fclose(file);
        fprintf(stderr, "[Error] Start hash not found in headers file.\n");
        return;
    }

    // Build the 'headers' message
    unsigned char headers_msg[sizeof(bitcoin_msg_header) + 1 + (MAX_HEADERS_COUNT * 81)];
    size_t offset = sizeof(bitcoin_msg_header);
    headers_msg[offset++] = 0; // Placeholder for count

    while (headers_count < MAX_HEADERS_COUNT && fread(buffer, 80, 1, file) == 1)
    {
        memcpy(headers_msg + offset, buffer, 80);
        offset += 80;
        headers_msg[offset++] = 0; // Transaction count (var_int, 0 for headers only)
        headers_count++;

        if (memcmp(buffer, stop_hash, 32) == 0)
        {
            break;
        }
    }

    fclose(file);

    // Set the count
    headers_msg[sizeof(bitcoin_msg_header)] = headers_count;

    // Build the message header
    bitcoin_msg_header* header = (bitcoin_msg_header*)headers_msg;
    header->magic = htole32(BITCOIN_MAINNET_MAGIC);
    strncpy(header->command, "headers", 12);
    header->length = htole32(offset - sizeof(bitcoin_msg_header));
    compute_checksum(headers_msg + sizeof(bitcoin_msg_header), offset - sizeof(bitcoin_msg_header), header->checksum);

    // Send the 'headers' message
    ssize_t bytes_sent = send(node->socket_fd, headers_msg, offset, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'headers' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'headers' message.");
}

void save_blocks_to_file(const unsigned char* payload, size_t payload_len, const char* filename)
{
    FILE* file = fopen(filename, "wb");
    if (!file)
    {
        perror("Failed to open file for writing");
        return;
    }

    fwrite(payload, 1, payload_len, file);
    fclose(file);
    guarded_print("Blocks saved to file: %s\n", filename);
}

unsigned char* load_blocks_from_file(const char* filename, size_t* payload_len)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        perror("Failed to open file for reading");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *payload_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char* payload = (unsigned char*)malloc(*payload_len);
    if (!payload)
    {
        perror("Failed to allocate memory");
        fclose(file);
        return NULL;
    }

    fread(payload, 1, *payload_len, file);
    fclose(file);
    guarded_print("Blocks loaded from file: %s\n", filename);
    return payload;
}

void send_getblocks_and_wait(int idx)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    // Define block locator and locator count
    unsigned char block_locator[MAX_LOCATOR_COUNT * 32];
    int locator_count = 1; // Set to 1 to request blocks starting from the latest known block

    // Load the latest known block hash
    load_latest_known_block_hash(block_locator);

    // Build the 'getblocks' message
    unsigned char getblocks_msg[sizeof(bitcoin_msg_header) + 4 + 1 + (MAX_LOCATOR_COUNT * 32) + 32];
    size_t msg_len = build_getblocks_message(getblocks_msg, sizeof(getblocks_msg), block_locator, locator_count);
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'getblocks' message.\n");
        return;
    }

    // Send the 'getblocks' message
    ssize_t bytes_sent = send(node->socket_fd, getblocks_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'getblocks' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'getblocks' message.");
    node->operation_in_progress = 1;

    // Wait for 10 seconds for a response
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Increase buffer size to handle larger responses
    unsigned char buffer[32768];
    size_t total_bytes_received = 0;
    ssize_t bytes_received;

    // Receive the message in parts
    while ((bytes_received = recv(node->socket_fd, buffer + total_bytes_received, sizeof(buffer) - total_bytes_received - 1, 0)) > 0)
    {
        total_bytes_received += bytes_received;
    }

    if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to receive response: %s", strerror(errno));
        node->operation_in_progress = 0;
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Received response to 'getblocks' message.");
    node->operation_in_progress = 0;

    // Process the response and print to CLI
    guarded_print("Received response to 'getblocks' message:\n");
    parse_inv_message(buffer, total_bytes_received);

    // Save the blocks to a file
    save_blocks_to_file(buffer, total_bytes_received, "blocks.dat");
}

size_t build_getblocks_message(unsigned char* buffer, size_t buffer_size, const unsigned char* block_locator, size_t locator_count)
{
    if (buffer_size < sizeof(bitcoin_msg_header) + 37)
        return 0;

    unsigned char payload[4 + 1 + (locator_count * 32) + 32];

    // Set protocol version (4 bytes)
    uint32_t version = htonl(70015);
    memcpy(payload, &version, 4);

    // Set block locator count (var_int encoding)
    size_t offset = 4;
    offset += write_var_int(payload + offset, locator_count);

    // Copy block locator hashes
    memcpy(payload + offset, block_locator, locator_count * 32);
    offset += locator_count * 32;

    // Set hash_stop (32 bytes of zeros for max 500 blocks)
    memset(payload + offset, 0, 32);
    offset += 32;

    // Ensure buffer is large enough
    if (offset > buffer_size)
        return 0;

    // Build final message
    return build_message(buffer, buffer_size, "getblocks", payload, offset);
}

void parse_inv_message(const unsigned char* payload, size_t payload_len)
{
    size_t offset = 0;
    uint64_t count = read_var_int(payload + offset, &offset);

    guarded_print("Inventory count: %llu\n", count);

    for (uint64_t i = 0; i < count; i++)
    {
        if (offset + 36 > payload_len)
        {
            guarded_print("Insufficient payload length for inventory entry\n");
            return;
        }

        uint32_t type;
        memcpy(&type, payload + offset, 4);
        offset += 4;

        unsigned char hash[32];
        memcpy(hash, payload + offset, 32);
        offset += 32;

        guarded_print("Inventory item %llu: Type: %u, Hash: ", i + 1, type);
        for (int j = 0; j < 32; j++)
        {
            guarded_print("%02x", hash[j]);
        }
        guarded_print("\n");
    }
}

void handle_inv_message(int idx, const unsigned char* payload, size_t payload_len)
{
    size_t offset = 0;
    uint64_t count = read_var_int(payload + offset, &offset);

    if (count == 0 || count > 50000)
    {
        log_message(LOG_WARN, BITLAB_LOG, __FILE__,
            "Invalid inventory count in inv message: %llu", count);
        return;
    }

    unsigned char hashes[count * 32];
    size_t hash_count = 0;

    for (uint64_t i = 0; i < count; i++)
    {
        if (offset + 36 > payload_len)
        {
            log_message(LOG_WARN, BITLAB_LOG, __FILE__,
                "Insufficient payload length for inventory entry");
            return;
        }

        uint32_t type;
        memcpy(&type, payload + offset, 4);
        offset += 4;

        if (type == 2) // Type 2 for block (1 for transaction)
        {
            memcpy(hashes + (hash_count * 32), payload + offset, 32);
            hash_count++;
        }
        offset += 32;
    }

    if (hash_count > 0)
    {
        send_getdata_and_wait(idx, hashes, hash_count);
    }
}

size_t build_getdata_message(unsigned char* buffer, size_t buffer_size, const unsigned char* hashes, size_t hash_count)
{
    if (buffer_size < sizeof(bitcoin_msg_header) + 1 + (hash_count * 36))
        return 0;

    unsigned char payload[1 + (hash_count * 36)];

    // Set inventory count (var_int encoding)
    size_t offset = 0;
    offset += write_var_int(payload + offset, hash_count);

    // Copy inventory vectors (type + hash)
    for (size_t i = 0; i < hash_count; i++)
    {
        uint32_t type = htonl(2); // Assuming type 2 for block (1 for transaction)
        memcpy(payload + offset, &type, 4);
        offset += 4;
        memcpy(payload + offset, hashes + (i * 32), 32);
        offset += 32;
    }

    // Ensure buffer is large enough
    if (offset > buffer_size)
        return 0;

    // Build final message
    return build_message(buffer, buffer_size, "getdata", payload, offset);
}

void send_getdata_and_wait(int idx, const unsigned char* hashes, size_t hash_count)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    if (hash_count == 0 || hash_count > 50000)
    {
        fprintf(stderr, "[Error] Invalid hash count. Must be between 1 and 50000.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    // Build the 'getdata' message
    unsigned char getdata_msg[sizeof(bitcoin_msg_header) + 1 + (hash_count * 36)];
    size_t msg_len = build_getdata_message(getdata_msg, sizeof(getdata_msg), hashes, hash_count);
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'getdata' message.\n");
        return;
    }

    // Send the 'getdata' message
    ssize_t bytes_sent = send(node->socket_fd, getdata_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'getdata' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'getdata' message.");
    node->operation_in_progress = 1;

    // Wait for 20 seconds for a response (increased timeout)
    struct timeval tv;
    tv.tv_sec = 20;
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Receive the response
    unsigned char buffer[32768];
    ssize_t bytes_received;

    while ((bytes_received = recv(node->socket_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)buffer;
        if (hdr->magic == BITCOIN_MAINNET_MAGIC)
        {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            if (strcmp(cmd_name, "block") == 0)
            {
                log_message(LOG_INFO, log_filename, __FILE__, "Received 'block' message.");
                decode_transactions(buffer + sizeof(bitcoin_msg_header), hdr->length);
            }
        }
    }

    if (bytes_received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to receive block message: %s", strerror(errno));
    }

    node->operation_in_progress = 0;
}

/**
 * @brief Builds an 'inv' message.
 *
 * This function builds an 'inv' message with the given inventory data.
 *
 * @param buffer The buffer to store the built message.
 * @param buffer_size The size of the buffer.
 * @param inv_data An array of inventory vectors (type + hash).
 * @param inv_count The number of inventory vectors in the array.
 * @return The size of the built message, or 0 on failure.
 */
size_t build_inv_message(unsigned char* buffer, size_t buffer_size, const unsigned char* inv_data, size_t inv_count)
{
    size_t var_int_size = write_var_int(NULL, inv_count); // Get the size of the var_int encoding
    size_t payload_size = var_int_size + (inv_count * 36); // var_int size + 36 bytes per inventory vector

    printf("inv_count: %zu, payload_size: %zu, buffer_size: %zu\n", inv_count, payload_size, buffer_size);

    if (buffer_size < sizeof(bitcoin_msg_header) + payload_size)
    {
        printf("Buffer size is too small: buffer_size=%zu, required=%zu\n", buffer_size, sizeof(bitcoin_msg_header) + payload_size);
        return 0;
    }

    unsigned char* payload = (unsigned char*)malloc(payload_size);
    if (!payload)
    {
        printf("Failed to allocate memory for payload\n");
        return 0;
    }

    // Set inventory count (var_int encoding)
    size_t offset = 0;
    offset += write_var_int(payload + offset, inv_count);
    log_message(LOG_DEBUG, BITLAB_LOG, __FILE__, "After write_var_int: offset=%zu", offset);

    // Copy inventory vectors (type + hash)
    for (size_t i = 0; i < inv_count; i++)
    {
        memcpy(payload + offset, inv_data + (i * 36), 36);
        offset += 36;
    }
    printf("After copying inventory vectors: offset=%zu\n", offset);

    // Build final message
    size_t message_size = build_message(buffer, buffer_size, "inv", payload, payload_size);
    printf("Built message size: %zu\n", message_size);

    free(payload);
    return message_size;
}

/**
 * @brief Sends an 'inv' message to the peer.
 *
 * This function sends an 'inv' message to the peer identified by the given index.
 * It is used to advertise the knowledge of one or more objects (blocks or transactions).
 * The inventory data is provided as input to the function.
 *
 * @param idx The index of the peer in the nodes array.
 * @param inv_data An array of inventory vectors (type + hash).
 * @param inv_count The number of inventory vectors in the array.
 */
void send_inv_and_wait(int idx, const unsigned char* inv_data, size_t inv_count)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    if (inv_count == 0 || inv_count > 50000)
    {
        fprintf(stderr, "[Error] Invalid inventory count. Must be between 1 and 50000.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    size_t var_int_size = write_var_int(NULL, inv_count); // Get the size of the var_int encoding
    size_t payload_size = var_int_size + (inv_count * 36); // var_int size + 36 bytes per inventory vector
    size_t inv_msg_size = sizeof(bitcoin_msg_header) + payload_size;

    printf("inv_count: %zu, inv_msg_size: %zu\n", inv_count, inv_msg_size);

    unsigned char* inv_msg = (unsigned char*)malloc(inv_msg_size);
    if (!inv_msg)
    {
        fprintf(stderr, "[Error] Failed to allocate memory for 'inv' message.\n");
        return;
    }

    size_t msg_len = build_inv_message(inv_msg, inv_msg_size, inv_data, inv_count);
    printf("msg_len: %zu\n", msg_len);
    if (msg_len == 0)
    {
        fprintf(stderr, "[Error] Failed to build 'inv' message.\n");
        free(inv_msg);
        return;
    }

    // Send the 'inv' message
    ssize_t bytes_sent = send(node->socket_fd, inv_msg, msg_len, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'inv' message: %s", strerror(errno));
        free(inv_msg);
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'inv' message.");
    printf("Sent 'inv' message.\n");
    node->operation_in_progress = 1;

    // Wait for 10 seconds for a response
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    setsockopt(node->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // Receive the response
    unsigned char buffer[32768];
    ssize_t bytes_received = recv(node->socket_fd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to receive response: %s", strerror(errno));
        node->operation_in_progress = 0;
        free(inv_msg);
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Received response to 'inv' message.");
    printf("Received response to 'inv' message.\n");
    node->operation_in_progress = 0;

    // Process the response
    if (bytes_received >= (ssize_t)sizeof(bitcoin_msg_header))
    {
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)buffer;
        if (hdr->magic == BITCOIN_MAINNET_MAGIC)
        {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            printf("Received command: '%s'\n", cmd_name);

            if (strcmp(cmd_name, "inv") == 0)
            {
                printf("Received 'inv' response:\n");
                handle_inv_message(idx, buffer + sizeof(bitcoin_msg_header), bytes_received - sizeof(bitcoin_msg_header));
            }
            else
            {
                printf("Unhandled command: '%s'\n", cmd_name);
            }
        }
        else
        {
            printf("Unexpected magic bytes (0x%08X).\n", hdr->magic);
        }
    }
    else
    {
        printf("Received incomplete message.\n");
    }

    free(inv_msg);
}

void decode_transactions(const unsigned char* block_data, size_t block_len)
{
    (void)block_len; // Mark block_len as unused to avoid the warning

    size_t offset = 0;

    // Skip the block header (80 bytes)
    offset += 80;

    // Read the number of transactions (var_int)
    uint64_t tx_count = read_var_int(block_data + offset, &offset);

    printf("Number of transactions: %lu\n", tx_count);

    for (uint64_t i = 0; i < tx_count; i++)
    {
        // Decode each transaction (simplified for demonstration)
        printf("Transaction %lu:\n", i + 1);

        // Read transaction version (4 bytes)
        uint32_t tx_version;
        memcpy(&tx_version, block_data + offset, 4);
        offset += 4;
        printf("  Version: %u\n", tx_version);

        // Read the number of inputs (var_int)
        uint64_t input_count = read_var_int(block_data + offset, &offset);
        printf("  Number of inputs: %lu\n", input_count);

        // Read each input (simplified)
        for (uint64_t j = 0; j < input_count; j++)
        {
            // Skip previous output (32 bytes hash + 4 bytes index)
            offset += 36;

            // Read script length (var_int)
            uint64_t script_len = read_var_int(block_data + offset, &offset);

            // Skip script and sequence (script_len + 4 bytes)
            offset += script_len + 4;
        }

        // Read the number of outputs (var_int)
        uint64_t output_count = read_var_int(block_data + offset, &offset);
        printf("  Number of outputs: %lu\n", output_count);

        // Read each output (simplified)
        for (uint64_t j = 0; j < output_count; j++)
        {
            // Read value (8 bytes)
            uint64_t value;
            memcpy(&value, block_data + offset, 8);
            offset += 8;
            printf("    Value: %lu\n", value);

            // Read script length (var_int)
            uint64_t script_len = read_var_int(block_data + offset, &offset);

            // Skip script (script_len bytes)
            offset += script_len;
        }

        // Read lock time (4 bytes)
        uint32_t lock_time;
        memcpy(&lock_time, block_data + offset, 4);
        offset += 4;
        printf("  Lock time: %u\n", lock_time);
    }
}

void send_tx(int idx, const unsigned char* tx_data, size_t tx_size)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        fprintf(stderr, "[Error] Invalid node index or node not connected.\n");
        return;
    }

    Node* node = &nodes[idx];
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log", node->ip_address);

    if (!tx_data || tx_size == 0)
    {
        fprintf(stderr, "[Error] Invalid transaction data.\n");
        return;
    }

    // Build the 'tx' message with the appropriate header
    unsigned char tx_msg[sizeof(bitcoin_msg_header) + tx_size];

    // Build the message header
    bitcoin_msg_header* header = (bitcoin_msg_header*)tx_msg;
    header->magic = htole32(BITCOIN_MAINNET_MAGIC);
    strncpy(header->command, "tx", 12);
    header->length = htole32(tx_size);
    compute_checksum(tx_data, tx_size, header->checksum);

    // Copy the transaction data
    memcpy(tx_msg + sizeof(bitcoin_msg_header), tx_data, tx_size);

    // Send the 'tx' message
    ssize_t bytes_sent = send(node->socket_fd, tx_msg, sizeof(bitcoin_msg_header) + tx_size, 0);
    if (bytes_sent < 0)
    {
        log_message(LOG_INFO, log_filename, __FILE__,
            "[Error] Failed to send 'tx' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "Sent 'tx' message (%zu bytes).", tx_size);
}
