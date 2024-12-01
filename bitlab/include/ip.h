#ifndef __IP_H
#define __IP_H

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_IP_ADDR_LEN 1024

/**
 * Get the local IP address of the machine (e.g. 192.168.1.10).
 *
 * @param ip_addr The buffer to store the IP address.
 */
void get_local_ip_address(char* ip_addr);

/**
 * Get the remote IP address of the machine (e.g. 1.1.1.1).
 *
 * @param ip_addr The buffer to store the IP address.
 */
void get_remote_ip_address(char* ip_addr);

/**
 * Perform lookup of given IP address by the domain address (e.g. google.com).
 *
 * @param lookup_addr The current address to lookup.
 * @param ip_addr The buffer to store the IP address.
 * @return 0 if successful, otherwise -1 or 1.
 */
int lookup_address(const char* lookup_addr, char* ip_addr);

/**
 * Check if the IP address is in the private prefix (e.g. 192.168.1.10 is, 1.1.1.1 is not).
 *
 * @param ip_addr The IP address to check.
 * @return 1 if the IP address is in the private network, 0 otherwise.
 */
int is_in_private_network(const char* ip_addr);

/**
 * Check if the IP address is a numeric address (e.g. 1.1.1.1 is, example.com is not).
 *
 * @param ip_addr The IP address to check.
 * @return 1 if the IP address is a numeric address, 0 otherwise.
 */
int is_numeric_address(const char* ip_addr);

/**
 * Check if the IP address is a valid domain address (e.g. example.com is, example or 1.1.1.1 is not).
 *
 * @param domain_addr The domain address to check.
 * @return 1 if the IP address is a valid domain address, 0 otherwise.
 */
int is_valid_domain_address(const char* domain_addr);



//// Linux manual page ////

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr* ai_addr;
    char* ai_canonname;
    struct addrinfo* ai_next;
};

int getaddrinfo(const char* restrict node,
    const char* restrict service,
    const struct addrinfo* restrict hints,
    struct addrinfo** restrict res);

void freeaddrinfo(struct addrinfo* res);

const char* gai_strerror(int errcode);

#endif // __IP_H
