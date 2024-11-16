#include "ip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <resolv.h>

#include "utils.h"

void get_local_ip_address(char* ip_addr)
{
    FILE* file = popen("hostname -I", "r");
    if (file == NULL)
    {
        strcpy(ip_addr, " ");
        return;
    }
    char buffer[BUFFER_SIZE];
    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        strcpy(ip_addr, " ");
        pclose(file);
        return;
    }
    pclose(file);
    char* newline = strchr(buffer, '\n');
    if (newline != NULL)
        *newline = '\0';
    strcpy(ip_addr, buffer);
}

void get_remote_ip_address(char* ip_addr)
{
    FILE* file = popen("curl -s ifconfig.me", "r");
    if (file == NULL)
    {
        strcpy(ip_addr, " ");
        return;
    }
    char buffer[BUFFER_SIZE];
    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        strcpy(ip_addr, " ");
        pclose(file);
        return;
    }
    pclose(file);
    char* newline = strchr(buffer, '\n');
    if (newline != NULL)
        *newline = '\0';
    strcpy(ip_addr, buffer);
}

int lookup_address(const char* lookup_addr, char* ip_addr)
{
    struct addrinfo hints, * res, * p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];

    memset(ip_addr, 0, MAX_IP_ADDR_LEN);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(lookup_addr, NULL, &hints, &res)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        ip_addr[0] = '\0';
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        void* addr = NULL;
        if (p->ai_family == AF_INET)
        {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
            if (addr != NULL)
            {
                if (strcmp(inet_ntoa(ipv4->sin_addr), "0.0.0.0") == 0)
                    log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Failed to resolve %s. Appending 0.0.0.0", lookup_addr);
                inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
                strcat(ip_addr, ipstr);
                strcat(ip_addr, " ");
            }
        }
    }

    if (strlen(ip_addr) > 0)
        ip_addr[strlen(ip_addr) - 1] = '\0';

    freeaddrinfo(res);
    return 0;
}

int is_in_private_network(const char* ip_addr)
{
    if (!is_numeric_address(ip_addr))
    {
        char ip[BUFFER_SIZE];
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "IP address is not numeric, performing lookup of %s whether it is in private prefix", ip_addr);
        lookup_address(ip_addr, ip);
        return is_in_private_network(ip);
    }
    unsigned int b1, b2, b3, b4;
    if (ip_addr == NULL)
        return 0;
    if (sscanf(ip_addr, "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4)
        return 0;
    if ((b1 == 192 && b2 == 168) ||
        (b1 == 10) ||
        (b1 == 172 && b2 >= 16 && b2 <= 31))
        return 1;
    return 0;
}

int is_numeric_address(const char* ip_addr)
{
    if (ip_addr == NULL)
        return 0;
    int dots = 0;
    for (size_t i = 0; i < strlen(ip_addr); ++i)
    {
        if (ip_addr[i] == '.')
            dots++;
        else if (ip_addr[i] < '0' || ip_addr[i] > '9')
            return 0;
        if (dots > 3)
            return 0;
    }
    return 1;
}

int is_valid_domain_address(const char* domain_addr)
{
    if (domain_addr == NULL)
        return 0;
    if (is_numeric_address(domain_addr))
        return 0;
    if (strstr(domain_addr, ".") == NULL)
        return 0;
    char ip[BUFFER_SIZE];
    lookup_address(domain_addr, ip);
    if (strlen(ip) == 0)
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Although %s is a valid domain, it does not resolve", domain_addr);
    return 1;
}
