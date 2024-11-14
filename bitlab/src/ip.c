#include "ip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void get_local_ip_address(char* ip_address)
{
    FILE* file = popen("hostname -I", "r");
    if (file == NULL)
    {
        strcpy(ip_address, " ");
        return;
    }
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        strcpy(ip_address, " ");
        pclose(file);
        return;
    }
    pclose(file);
    char* newline = strchr(buffer, '\n');
    if (newline != NULL)
        *newline = '\0';
    strcpy(ip_address, buffer);
}

void get_remote_ip_address(char* ip_address)
{
    FILE* file = popen("curl -s ifconfig.me", "r");
    if (file == NULL)
    {
        strcpy(ip_address, " ");
        return;
    }
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), file) == NULL)
    {
        strcpy(ip_address, " ");
        pclose(file);
        return;
    }
    pclose(file);
    char* newline = strchr(buffer, '\n');
    if (newline != NULL)
        *newline = '\0';
    strcpy(ip_address, buffer);
}

void get_ip_of_address(const char* addr, char* ip_address)
{
    char command[256];
    char buffer[256];
    char current_addr[240];
    strncpy(current_addr, addr, sizeof(current_addr) - 1);
    current_addr[sizeof(current_addr) - 1] = '\0';

    while (1)
    {
        snprintf(command, sizeof(command), "dig +short %s", current_addr);

        FILE* file = popen(command, "r");
        if (file == NULL)
        {
            strcpy(ip_address, " ");
            return;
        }

        if (fgets(buffer, sizeof(buffer), file) == NULL)
        {
            strcpy(ip_address, " ");
            pclose(file);
            return;
        }
        pclose(file);

        char* newline = strchr(buffer, '\n');
        if (newline != NULL)
            *newline = '\0';

        if (is_numeric_address(buffer))
        {
            if (is_in_private_network(buffer))
                log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Searching remote of %s: fetched IP address %s is in the private network", addr, buffer);

            strcpy(ip_address, buffer);
            return;
        }
        strncpy(current_addr, buffer, sizeof(current_addr) - 1);
        current_addr[sizeof(current_addr) - 1] = '\0';
    }
}

int is_in_private_network(const char* ip_address)
{
    unsigned int b1, b2, b3, b4;
    if (ip_address == NULL)
        return 0;
    if (sscanf(ip_address, "%u.%u.%u.%u", &b1, &b2, &b3, &b4) != 4)
        return 0;
    if ((b1 == 192 && b2 == 168) ||
        (b1 == 10) ||
        (b1 == 172 && b2 >= 16 && b2 <= 31))
        return 1;
    return 0;
}

int is_numeric_address(const char* ip_address)
{
    if (ip_address == NULL)
        return 0;
    for (size_t i = 0; i < strlen(ip_address); ++i)
    {
        if (ip_address[i] != '.' && (ip_address[i] < '0' || ip_address[i] > '9'))
            return 0;
    }
    return 1;
}

int is_valid_domain_address(const char* ip_address)
{
    if (ip_address == NULL)
        return 0;
    if (strstr(ip_address, ".") == NULL)
        return 0;
    return 1;
}
