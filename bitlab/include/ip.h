#ifndef __IP_H
#define __IP_H



/**
 * Get the local IP address of the machine (e.g. 192.168.1.10).
 *
 * @param ip_address The buffer to store the IP address.
 */
void get_local_ip_address(char* ip_address);

/**
 * Get the remote IP address of the machine (e.g. 1.1.1.1).
 *
 * @param ip_address The buffer to store the IP address.
 */
void get_remote_ip_address(char* ip_address);

/**
 * Get the IP address of the domain address (e.g. google.com).
 *
 * @param addr The domain address.
 * @param ip_address The buffer to store the IP address.
 */
void get_ip_of_address(const char* url, char* ip_address);

/**
 * Check if the IP address is in the private network (e.g. 192.168.1.10).
 *
 * @param ip_address The IP address to check.
 * @return 1 if the IP address is in the private network, 0 otherwise.
 */
int is_in_private_network(const char* ip_address);

/**
 * Check if the IP address is a numeric address (e.g. 1.1.1.1 is, example.com is not).
 *
 * @param ip_address The IP address to check.
 * @return 1 if the IP address is a numeric address, 0 otherwise.
 */
int is_numeric_address(const char* ip_address);

/**
 * Check if the IP address is a valid domain address (e.g. example.com is, example or 1.1.1.1 is not).
 *
 * @param ip_address The IP address to check.
 * @return 1 if the IP address is a valid domain address, 0 otherwise.
 */
int is_valid_domain_address(const char* ip_address);

#endif // __IP_H
