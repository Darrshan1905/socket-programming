#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    struct ifaddrs *ifaddr, *ifa;

    // Get the list of all network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Iterate through the list
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        printf("Interface Name: %s\n", ifa->ifa_name);

        // Check if it is an IPv4 or IPv6 interface
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
            printf("    IPv4 Address: %s\n", ip);

            // Print Netmask if available
            if (ifa->ifa_netmask != NULL) {
                struct sockaddr_in *netmask = (struct sockaddr_in *)ifa->ifa_netmask;
                printf("    Netmask: %s\n", inet_ntoa(netmask->sin_addr));
            }

            // Print Broadcast Address if available
            if (ifa->ifa_broadaddr != NULL) {
                struct sockaddr_in *broadaddr = (struct sockaddr_in *)ifa->ifa_broadaddr;
                printf("    Broadcast Address: %s\n", inet_ntoa(broadaddr->sin_addr));
            }
        } else if (ifa->ifa_addr->sa_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)ifa->ifa_addr;
            char ip6[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &(addr6->sin6_addr), ip6, INET6_ADDRSTRLEN);
            printf("    IPv6 Address: %s\n", ip6);

            // Print Netmask if available
            if (ifa->ifa_netmask != NULL) {
                struct sockaddr_in6 *netmask6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
                // Handle IPv6 netmask appropriately (e.g., using inet_ntop)
                // Example: printf("    Netmask: %s\n", inet_ntop(AF_INET6, &(netmask6->sin6_addr), ip6, INET6_ADDRSTRLEN));
            }

            // Print Broadcast Address if available
            if (ifa->ifa_broadaddr != NULL) {
                struct sockaddr_in6 *broadaddr6 = (struct sockaddr_in6 *)ifa->ifa_broadaddr;
                // Handle IPv6 broadcast address appropriately (e.g., using inet_ntop)
                // Example: printf("    Broadcast Address: %s\n", inet_ntop(AF_INET6, &(broadaddr6->sin6_addr), ip6, INET6_ADDRSTRLEN));
            }
        }

        // Print Data Length if available
        printf("    Data Length: %ld\n", ifa->ifa_data == NULL ? 0 : strlen(ifa->ifa_data));
        printf("\n");
    }

    // Free the memory allocated by getifaddrs
    freeifaddrs(ifaddr);

    return 0;
}
