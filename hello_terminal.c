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

#define PORT 50000
#define HELLO_MSG "HELLO_FROM_TERMINAL"

int main() {
    int sock;
    struct sockaddr_in broadcastAddr;
    int broadcastEnable = 1;
    char buffer[256];

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

    // Préparation adresse broadcast (on choisit ici broadcast 192.168.1.255)
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, "192.168.1.255", &broadcastAddr.sin_addr);

    while (1) {
        // Envoi message HELLO broadcast
        if (sendto(sock, HELLO_MSG, strlen(HELLO_MSG), 0,
                   (struct sockaddr *)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
            perror("sendto");
        } else {
            printf("HELLO envoyé en broadcast\n");
        }

        // Attente avant prochain envoi
        sleep(5);
    }

    close(sock);
    return 0;
}
