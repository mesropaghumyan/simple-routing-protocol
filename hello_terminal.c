#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#define PORT 50000
#define HELLO_MSG "HELLO_FROM_TERMINAL"

int main() {
    int sock;
    struct sockaddr_in broadcastAddr;
    int broadcastEnable = 1;

    // Création socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return 1;
    }

    // Activer le broadcast
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("setsockopt");
        close(sock);
        return 1;
    }

    // Récupération dynamique de l'adresse de broadcast
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        close(sock);
        return 1;
    }

    char broadcastIP[INET_ADDRSTRLEN] = {0};
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;

        // On ne prend que les interfaces IPv4, actives et non loopback
        if (ifa->ifa_addr->sa_family == AF_INET &&
            (ifa->ifa_flags & IFF_BROADCAST) &&
            !(ifa->ifa_flags & IFF_LOOPBACK)) {
            
            struct sockaddr_in *broad = (struct sockaddr_in *)ifa->ifa_broadaddr;
            if (broad != NULL) {
                inet_ntop(AF_INET, &broad->sin_addr, broadcastIP, INET_ADDRSTRLEN);
                break;
            }
        }
    }

    freeifaddrs(ifaddr);

    if (strlen(broadcastIP) == 0) {
        fprintf(stderr, "Impossible de déterminer l'adresse de broadcast\n");
        close(sock);
        return 1;
    }

    // Préparation de l'adresse de destination
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, broadcastIP, &broadcastAddr.sin_addr);

    printf("Envoi des HELLO à l'adresse de broadcast : %s:%d\n", broadcastIP, PORT);

    while (1) {
        if (sendto(sock, HELLO_MSG, strlen(HELLO_MSG), 0,
                   (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
            perror("sendto");
        } else {
            printf("HELLO envoyé en broadcast\n");
        }

        sleep(5);
    }

    close(sock);
    return 0;
}
