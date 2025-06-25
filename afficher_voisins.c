#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define PORT 5000
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <ip_du_routeur>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip_cible = argv[1];
    int sock;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];
    ssize_t recv_len;

    // Création socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    // Préparation de l'adresse du routeur
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip_cible, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Adresse IP invalide : %s\n", ip_cible);
        close(sock);
        return EXIT_FAILURE;
    }

    const char *msg = "GET_NEIGHBORS";

    // Envoi de la requête
    if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return EXIT_FAILURE;
    }

    // Timeout de réception
    struct timeval tv = {2, 0}; // 2 secondes
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Réception de la réponse
    recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
    if (recv_len < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "Timeout : aucune réponse de %s\n", ip_cible);
        } else {
            perror("recvfrom");
        }
        close(sock);
        return EXIT_FAILURE;
    }

    buffer[recv_len] = '\0';
    printf("Voisins du routeur %s :\n", ip_cible);
    printf("----------------------------------\n");
    printf("%s", buffer);

    close(sock);
    return EXIT_SUCCESS;
}
