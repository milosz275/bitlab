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

void get_ip_of_url(const char* url, char* ip_address)
{
    char command[256];
    snprintf(command, sizeof(command), "dig +short %s", url);
    FILE* file = popen(command, "r");
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
    if (is_in_private_network(buffer))
        log_message(LOG_WARN, BITLAB_LOG, __FILE__, "Searching remote of %s: fetched IP address %s is in the private network", url, buffer);
    strcpy(ip_address, buffer);
}

int is_in_private_network(const char* ip_address)
{
    if (ip_address == NULL)
        return 0;
    if (strstr(ip_address, "192.168.") == ip_address)
        return 1;
    else if (strstr(ip_address, "10.") == ip_address)
        return 1;
    else if (strstr(ip_address, "172.") == ip_address)
        return 1;
    return 0;
}
