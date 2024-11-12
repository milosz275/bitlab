#ifndef __IP_H
#define __IP_H



/**
 * Get the local IP address of the machine.
 *
 * @param ip_address The buffer to store the IP address.
 */
void get_local_ip_address(char* ip_address);

/**
 * Get the remote IP address of the machine.
 *
 * @param ip_address The buffer to store the IP address.
 */
void get_remote_ip_address(char* ip_address);

/**
 * Get the IP address of the URL.
 *
 * @param url The URL to get the IP address.
 * @param ip_address The buffer to store the IP address.
 */
void get_ip_of_url(const char* url, char* ip_address);

/**
 * Check if the IP address is in the private network.
 *
 * @param ip_address The IP address to check.
 * @return 1 if the IP address is in the private network, 0 otherwise.
 */
int is_in_private_network(const char* ip_address);

#endif // __IP_H
