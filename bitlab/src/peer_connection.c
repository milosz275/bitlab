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

// Bitcoin mainnet magic bytes:
#define BITCOIN_MAINNET_MAGIC 0xD9B4BEF9

// Default Bitcoin mainnet port:
#define BITCOIN_MAINNET_PORT 8333

// Structure for Bitcoin P2P message header (24 bytes).
// For reference: https://en.bitcoin.it/wiki/Protocol_documentation#Message_structure
#pragma pack(push, 1)
typedef struct {
    unsigned int magic;       // Magic value indicating message origin network
    char command[12];         // ASCII command (null-padded)
    unsigned int length;      // Payload size (little-endian)
    unsigned int checksum;    // First 4 bytes of double SHA-256 of the payload
} bitcoin_msg_header;
#pragma pack(pop)

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
    if (buf_size < 86) {
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
    memset(buf + offset, 0, 8);   // services
    offset += 8;
    memset(buf + offset, 0, 16);  // IP
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
    if (buf_size < (sizeof(bitcoin_msg_header) + payload_len)) {
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
        if (cmd_len > sizeof(header.command)) {
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

/**
 * send_verack:
 *   Constructs a verack message with an **empty** payload (length = 0)
 *   and a valid 4-byte checksum for an empty payload.
 */
static ssize_t send_verack(int sockfd)
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
        if (cmd_len > sizeof(verack_header.command)) {
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
    if (msg_len == 0) {
        fprintf(stderr, "[Error] build_message failed for 'pong'.\n");
        return -1;
    }

    return send(sockfd, pong_msg, msg_len, 0);
}

int connect_to_peer(const char* ip_addr)
{
    // Create the socket
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "[Error] socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    // Prepare the sockaddr_in
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(BITCOIN_MAINNET_PORT);

    if (inet_pton(AF_INET, ip_addr, &servaddr.sin_addr) <= 0) {
        fprintf(stderr, "[Error] inet_pton failed (IP '%s'): %s\n",
            ip_addr, strerror(errno));
        close(sockfd);
        return -1;
    }

    // Connect
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        fprintf(stderr, "[Error] connect to %s:%d failed: %s\n",
            ip_addr, BITCOIN_MAINNET_PORT, strerror(errno));
        close(sockfd);
        return -1;
    }

    printf("[+] Connected to peer %s:%d\n", ip_addr, BITCOIN_MAINNET_PORT);

    // Build the 'version' payload
    unsigned char version_payload[200];
    size_t version_payload_len = build_version_payload(version_payload, sizeof(version_payload));
    if (version_payload_len == 0) {
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
    if (version_msg_len == 0) {
        fprintf(stderr, "[Error] build_message() failed for 'version'.\n");
        close(sockfd);
        return -1;
    }

    // Send the 'version' message
    ssize_t bytes_sent = send(sockfd, version_msg, version_msg_len, 0);
    if (bytes_sent < 0) {
        fprintf(stderr, "[Error] send() of 'version' failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    printf("[+] Sent 'version' message (%zd bytes).\n", bytes_sent);

    // Read loop - up to 10 messages
    unsigned char recv_buf[2048];
    for (int i = 0; i < 10; i++) {
        ssize_t n = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (n < 0) {
            fprintf(stderr, "[Error] recv() failed: %s\n", strerror(errno));
            break;
        }
        if (n == 0) {
            // Peer closed the connection
            printf("[i] Peer closed the connection.\n");
            break;
        }

        // If we have at least a full header, parse it
        if (n < (ssize_t)sizeof(bitcoin_msg_header)) {
            printf("[!] Received %zd bytes (less than header size 24).\n", n);
            continue;
        }

        printf("[<] Received %zd bytes.\n", n);

        // Cast to a header
        bitcoin_msg_header* hdr = (bitcoin_msg_header*)recv_buf;

        // Check the magic
        if (hdr->magic == BITCOIN_MAINNET_MAGIC) {
            char cmd_name[13];
            memset(cmd_name, 0, sizeof(cmd_name));
            memcpy(cmd_name, hdr->command, 12);

            printf("[<] Received command: '%s'\n", cmd_name);

            // Determine the payload length & pointer
            size_t payload_len = hdr->length;
            const unsigned char* payload_data = recv_buf + sizeof(bitcoin_msg_header);

            // In real code, you’d handle partial messages if n < header+payload
            if (n < (ssize_t)(sizeof(bitcoin_msg_header) + payload_len)) {
                printf("[!] Incomplete message; got %zd bytes, expected %zu.\n",
                    n, sizeof(bitcoin_msg_header) + payload_len);
                continue;
            }

            // Handle known commands
            if (strcmp(cmd_name, "version") == 0) {
                // Send verack once we get their version
                ssize_t verack_sent = send_verack(sockfd);
                if (verack_sent < 0) {
                    fprintf(stderr, "[Error] sending verack: %s\n", strerror(errno));
                }
                else {
                    printf("[+] Sent 'verack' message.\n");
                }
            }
            else if (strcmp(cmd_name, "verack") == 0) {
                printf("[i] Peer acknowledged us with verack.\n");
            }
            else if (strcmp(cmd_name, "ping") == 0) {
                // Typically 8-byte payload
                if (payload_len == 8) {
                    ssize_t s = send_pong(sockfd, payload_data);
                    if (s < 0) {
                        fprintf(stderr, "[Error] sending pong: %s\n", strerror(errno));
                    }
                    else {
                        printf("[+] Sent 'pong' message.\n");
                    }
                }
            }
            else if (strcmp(cmd_name, "sendcmpct") == 0) {
                // Usually 9 bytes: fAnnounce(1 byte) + version(8 bytes)
                if (payload_len == 9) {
                    unsigned char fAnnounce = payload_data[0];
                    uint64_t cmpctVersion;
                    memcpy(&cmpctVersion, payload_data + 1, 8);
                    printf("[i] Peer wants to use compact blocks. "
                        "fAnnounce=%u, version=%" PRIu64 "\n",
                        fAnnounce, (uint64_t)cmpctVersion);
                }
                else {
                    printf("[!] 'sendcmpct' payload_len=%zu (expected 9)\n", payload_len);
                }
            }
            else if (strcmp(cmd_name, "feefilter") == 0) {
                // 8 bytes: a uint64_t in little-endian indicating min fee rate in sat/kB
                if (payload_len == 8) {
                    uint64_t fee_rate;
                    memcpy(&fee_rate, payload_data, 8);
                    printf("[i] Peer set feefilter: min fee rate = %" PRIu64 " sat/kB\n",
                        (uint64_t)fee_rate);
                }
                else {
                    printf("[!] 'feefilter' payload_len=%zu (expected 8)\n", payload_len);
                }
            }
            else {
                // Unhandled command
                printf("[!] Unhandled command: '%s' (payload size=%u)\n",
                    cmd_name, hdr->length);
            }
        }
        else {
            printf("[!] Unexpected magic bytes (0x%08X).\n", hdr->magic);
        }
    }

    // Close the socket
    close(sockfd);
    printf("[+] Connection closed.\n");
    return 0;
}
