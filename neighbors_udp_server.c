#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PORT 5000
#define BUF_SIZE 1024
#define MAX_VOISINS 100
#define IP_LEN 16

// Lit le fichier neighbors et remplit buffer avec les IPs séparées par \n
// Retourne la taille du message écrit dans buffer ou -1 en cas d'erreur
int lire_voisins_reponse(char *buffer, size_t max_len) {
    FILE *f = fopen("neighbors", "r");
    if (!f) {
        perror("Erreur ouverture fichier neighbors");
        return -1;
    }

    buffer[0] = '\0';
    char ligne[IP_LEN];
    size_t longueur_totale = 0;

    while (fgets(ligne, sizeof(ligne), f)) {
        // Enlever saut de ligne
        ligne[strcspn(ligne, "\r\n")] = 0;

        size_t len_ligne = strlen(ligne);
        if (longueur_totale + len_ligne + 1 >= max_len) {
            fprintf(stderr, "Réponse trop longue pour le buffer\n");
            break;
        }

        strcat(buffer, ligne);
        strcat(buffer, "\n");
        longueur_totale += len_ligne + 1;
    }
    fclose(f);

    return (int)longueur_totale;
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUF_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY; // écoute sur toutes les interfaces
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Serveur UDP en écoute sur le port %d...\n", PORT);

    while (1) {
        socklen_t len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }
        buffer[n] = '\0';

        printf("Reçu de %s:%d -> %s\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port), buffer);

        if (strcmp(buffer, "GET_NEIGHBORS") == 0) {
            char reponse[BUF_SIZE];
            int taille_reponse = lire_voisins_reponse(reponse, BUF_SIZE);
            if (taille_reponse < 0) {
                // En cas d'erreur, on répond vide
                reponse[0] = '\0';
                taille_reponse = 0;
            }
            sendto(sockfd, reponse, taille_reponse, 0, (struct sockaddr *)&cliaddr, len);
            printf("Réponse envoyée (%d octets)\n", taille_reponse);
        } else {
            printf("Commande non reconnue, pas de réponse envoyée.\n");
        }
    }

    close(sockfd);
    return 0;
}
