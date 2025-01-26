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
#include "ip.h"

// Global array to hold connected nodes
Node nodes[MAX_NODES];

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

void list_connected_nodes()
{
    int node_count = 0;
    for (int i = 0; i < MAX_NODES; ++i)
    {
        if (nodes[i].is_connected == 1)
        {
            node_count++;
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
    if (node_count == 0)
    {
        guarded_print_line("No nodes are currently connected.");
    }
}

void send_getaddr_and_wait(int idx)
{
    if (idx < 0 || idx >= MAX_NODES || !nodes[idx].is_connected)
    {
        guarded_print("Invalid node index or node not connected.\n");
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
        log_message(LOG_ERROR, log_filename, __FILE__,
            "getaddr: Failed to send 'getaddr' message: %s", strerror(errno));
        return;
    }

    log_message(LOG_INFO, log_filename, __FILE__, "getaddr: Sent 'getaddr' message.");
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
        log_message(LOG_INFO, log_filename, __FILE__, "getaddr: Constructing message header from chunks");
        bytes_received = recv(node->socket_fd, buffer + total_bytes_received,
            sizeof(buffer) - total_bytes_received - 1, 0);
        if (bytes_received <= 0)
        {
            log_message(LOG_WARN, log_filename, __FILE__, "getaddr: Invalid message header chunk received: %s",
                strerror(errno));
            // node->is_connected = 0;
            node->operation_in_progress = 0;
            return;
        }
        total_bytes_received += bytes_received;
    }

    bitcoin_msg_header* hdr = (bitcoin_msg_header*)buffer;
    size_t payload_len = hdr->length;
    size_t message_size = sizeof(bitcoin_msg_header) + payload_len;
    int retry_count = 0;
    const int max_retries = 4;

    while (total_bytes_received < message_size)
    {
        log_message(LOG_INFO, log_filename, __FILE__, "getaddr: Constructing message content from chunks");
        bytes_received = recv(node->socket_fd, buffer + total_bytes_received,
            sizeof(buffer) - total_bytes_received - 1, 0);
        if (bytes_received <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                log_message(LOG_INFO, log_filename, __FILE__,
                    "getaddr: Resource temporarily unavailable, continue receiving");

                if (++retry_count >= max_retries)
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "getaddr: Max retries reached, stopping recv.");
                    // node->is_connected = 0;
                    // node->operation_in_progress = 0;
                    break;
                }
                continue;
            }
            log_message(LOG_INFO, log_filename, __FILE__, "getaddr: Recv failed while constructing message contents: %s",
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
                    "addr: Insufficient payload length to read address count");
                return;
            }

            // Read the count of address entries (var_int)
            uint64_t count = read_var_int(payload_data + offset, &offset);

            // Log the count of address entries
            log_message(LOG_INFO, log_filename, __FILE__, "addr: Address count: %llu", count);

            // Ensure count does not exceed the maximum allowed entries
            if (count > 1000)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "addr: Address count exceeds maximum allowed: %llu", count);
                return;
            }

            int ipv4_count = 0;
            for (uint64_t i = 0; i < count; ++i)
            {
                if (offset + 30 > payload_len)
                {
                    log_message(LOG_WARN, log_filename, __FILE__,
                        "addr: Insufficient payload length for address entry");
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
                    // // Guarded print of the IP address and port
                    // guarded_print_line("Valid IPv4 Peer: %s:%u (timestamp: %u)", ip_str,
                    //     port, timestamp);

                    // Add to peer queue
                    add_peer_to_queue(ip_str, port);

                    // Log the result if valid
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "addr: Received valid IPv4 address: %s:%u (timestamp: %u)",
                        ip_str, port, timestamp);

                    ipv4_count++;
                }
                else if (strcmp(ip_type, "IPv6") == 0)
                {
                    // Log the IPv6 addresses if needed
                    log_message(LOG_INFO, log_filename, __FILE__,
                        "addr: Received IPv6 address: %s:%u (timestamp: %u)",
                        ip_str, port, timestamp);
                }
            }
            if (ipv4_count == 0)
            {
                guarded_print_line("addr: No valid IPv4 addresses received");
            }
            else
            {
                guarded_print_line("addr: Received and saved %d valid IPv4 addresses", ipv4_count);
            }

            if (offset != payload_len)
            {
                log_message(LOG_WARN, log_filename, __FILE__,
                    "addr: Remaining bytes after processing: %zu",
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

    // Limit the number of peers to 1000
    if (peer_count > 1000)
    {
        peer_count = 1000;
    }

    // Allocate memory for address payload
    size_t payload_size = 1 + peer_count * (sizeof(uint32_t) + sizeof(uint64_t) + sizeof(struct sockaddr_in6));
    unsigned char* addr_payload = malloc(payload_size);
    if (!addr_payload)
    {
        perror("malloc failed");
        free(peers);
        return -1;
    }

    // Populate address payload
    unsigned char* p = addr_payload;
    *p++ = (unsigned char)peer_count;

    for (int i = 0; i < peer_count; ++i)
    {
        // Add timestamp
        uint32_t timestamp = (uint32_t)time(NULL);
        memcpy(p, &timestamp, sizeof(uint32_t));
        p += sizeof(uint32_t);

        // Add service flags (NODE_NETWORK)
        uint64_t services = 0x01;
        memcpy(p, &services, sizeof(uint64_t));
        p += sizeof(uint64_t);

        // Convert IPv4 to IPv6-mapped IPv4 and add address
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(struct sockaddr_in6));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(peers[i].port);
        addr.sin6_addr.s6_addr[10] = 0xFF;
        addr.sin6_addr.s6_addr[11] = 0xFF;
        if (inet_pton(AF_INET, peers[i].ip, &addr.sin6_addr.s6_addr[12]) != 1)
        {
            log_message(LOG_INFO, log_filename, __FILE__, "Invalid IP address: %s", peers[i].ip);
            continue; // Skip invalid addresses
        }

        memcpy(p, &addr, sizeof(struct sockaddr_in6));
        p += sizeof(struct sockaddr_in6);

        // Log the address being added
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, INET6_ADDRSTRLEN);
        log_message(LOG_INFO, log_filename, __FILE__, "Adding address: %s:%d", ip_str, peers[i].port);
    }

    // Prepare message buffer
    unsigned char addr_msg[sizeof(bitcoin_msg_header) + payload_size];
    size_t msg_len = build_message(addr_msg, sizeof(addr_msg), "addr", addr_payload, payload_size);

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
        log_message(LOG_INFO, log_filename, __FILE__, "Successfully sent addresses");
    }

    // Free allocated memory
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

    guarded_print("[+] Connected to peer %s:%d\n", ip_addr, BITCOIN_MAINNET_PORT);

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
    guarded_print("[+] Sent 'version' message (%zd bytes).\n", bytes_sent);

    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "peer_connection_%s.log",
        ip_addr);
    init_logging(log_filename);
    // Read loop - up to 10 messages
    unsigned char recv_buf[2048];
    bool connected = false;
    for (int i = 0; i < 4; ++i)
    {
        guarded_print("for loop waiting for messages %d\n", i);
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
            guarded_print("[i] Peer closed the connection.\n");
            break;
        }

        // If we have at least a full header, parse it
        if (n < (ssize_t)sizeof(bitcoin_msg_header))
        {
            guarded_print("[!] Received %zd bytes (less than header size 24).\n", n);
            continue;
        }

        guarded_print("[<] Received %zd bytes.\n", n);

        // Cast to a header
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)recv_buf;

        // Check the magic
        if (hdr->magic == BITCOIN_MAINNET_MAGIC)
        {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            guarded_print("[<] Received command: '%s'\n", cmd_name);

            // Determine the payload length & pointer
            size_t payload_len = hdr->length;

            if (n < (ssize_t)(sizeof(bitcoin_msg_header) + payload_len))
            {
                guarded_print("[!] Incomplete message; got %zd bytes, expected %zu.\n",
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
                    guarded_print("[+] Sent 'verack' message.\n");
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
                guarded_print("[!] Unhandled command: '%s' (payload size=%u)\n",
                    cmd_name, hdr->length);
            }
        }
        else
        {
            guarded_print("[!] Unexpected magic bytes (0x%08X).\n", hdr->magic);
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

    //// This code is causing stack-buffer-overflow
    // if (pthread_cancel(node->thread) != 0)
    // {
    //     perror("Failed to cancel thread for peer");
    // }

    log_message(LOG_INFO, log_filename, __FILE__,
        "Successfully disconnected from node %s:%u", node->ip_address,
        node->port);
}
