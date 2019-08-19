#include <stdio.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main()
{
    struct ifaddrs *iflist, *iface;

    if (getifaddrs(&iflist) < 0) {
        perror("getifaddrs");
        return 1;
    }

    for (iface = iflist; iface; iface = iface->ifa_next) {
        int af = iface->ifa_addr->sa_family;
        const void *addr;
        char addrp[INET6_ADDRSTRLEN];

        switch (af) {
            case AF_INET:
                addr = &((struct sockaddr_in *)iface->ifa_addr)->sin_addr;
                break;
            case AF_INET6:
                addr = &((struct sockaddr_in6 *)iface->ifa_addr)->sin6_addr;
                break;
            default:
                addr = NULL;
        }

        if (addr) {
            if (inet_ntop(af, addr, addrp, sizeof addrp) == NULL) {
                perror("inet_ntop");
                continue;
            }

            printf("af value : %d Interface %s has address %s\n",af, iface->ifa_name, addrp);
        }
    }

    freeifaddrs(iflist);
    return 0;
}
