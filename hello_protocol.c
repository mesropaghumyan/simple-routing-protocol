#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ifaddrs.h>
#include <net/if.h>

#define PORT 50000
#define MAX_VOISINS 100
#define HELLO_INTERVAL 3        // secondes entre envois HELLO
#define VOISIN_TIMEOUT 10       // secondes avant d'oublier un voisin
#define MAX_IFACES 20

typedef struct {
    char ip[INET_ADDRSTRLEN];
    char broadcast[INET_ADDRSTRLEN];
} Interface;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    time_t dernier_hello;
} Voisin;

Interface interfaces[MAX_IFACES];
int nb_interfaces = 0;

Voisin voisins[MAX_VOISINS];
int nb_voisins = 0;

int running = 1;

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void afficher_voisins() {
    time_t now = time(NULL);
    printf("\n=== Voisins actifs ===\n");
    for (int i = 0; i < nb_voisins; i++) {
        if (now - voisins[i].dernier_hello <= VOISIN_TIMEOUT) {
            printf(" - %s (vu il y a %lds)\n", voisins[i].ip, now - voisins[i].dernier_hello);
        }
    }
    printf("=====================\n\n");
}

void maj_voisin(const char* ip) {
    time_t now = time(NULL);
    for (int i = 0; i < nb_voisins; i++) {
        if (strcmp(voisins[i].ip, ip) == 0) {
            voisins[i].dernier_hello = now;
            return;
        }
    }
    if (nb_voisins < MAX_VOISINS) {
        strcpy(voisins[nb_voisins].ip, ip);
        voisins[nb_voisins].dernier_hello = now;
        nb_voisins++;
        printf("[INFO] Nouveau voisin détecté : %s\n", ip);
    }
}

int recup_interfaces() {
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    nb_interfaces = 0;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (!(ifa->ifa_flags & IFF_UP)) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;

        if (!ifa->ifa_broadaddr) continue;

        if (nb_interfaces >= MAX_IFACES) {
            fprintf(stderr, "Nombre max d'interfaces atteint (%d)\n", MAX_IFACES);
            break;
        }

        struct sockaddr_in *sa_ip = (struct sockaddr_in*)ifa->ifa_addr;
        struct sockaddr_in *sa_bc = (struct sockaddr_in*)ifa->ifa_broadaddr;

        if (inet_ntop(AF_INET, &sa_ip->sin_addr, interfaces[nb_interfaces].ip, INET_ADDRSTRLEN) == NULL) continue;
        if (inet_ntop(AF_INET, &sa_bc->sin_addr, interfaces[nb_interfaces].broadcast, INET_ADDRSTRLEN) == NULL) continue;

        printf("[INFO] Interface détectée : %s, broadcast %s\n", interfaces[nb_interfaces].ip, interfaces[nb_interfaces].broadcast);
        nb_interfaces++;
    }

    freeifaddrs(ifaddr);
    return nb_interfaces > 0 ? 0 : -1;
}

int main() {
    signal(SIGINT, handle_sigint);

    if (recup_interfaces() < 0) {
        fprintf(stderr, "Erreur : aucune interface réseau active avec broadcast détectée\n");
        return EXIT_FAILURE;
    }

    // Création socket réception (INADDR_ANY)
    int sock_recv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_recv < 0) {
        perror("socket recv");
        return EXIT_FAILURE;
    }

    int reuseEnable = 1;
    if (setsockopt(sock_recv, SOL_SOCKET, SO_REUSEADDR, &reuseEnable, sizeof(reuseEnable)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(sock_recv);
        return EXIT_FAILURE;
    }

    struct sockaddr_in addr_recv;
    memset(&addr_recv, 0, sizeof(addr_recv));
    addr_recv.sin_family = AF_INET;
    addr_recv.sin_port = htons(PORT);
    addr_recv.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock_recv, (struct sockaddr*)&addr_recv, sizeof(addr_recv)) < 0) {
        perror("bind recv");
        close(sock_recv);
        return EXIT_FAILURE;
    }

    // Création des sockets d'envoi (une par interface)
    int sock_send[MAX_IFACES];
    for (int i = 0; i < nb_interfaces; i++) {
        sock_send[i] = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_send[i] < 0) {
            perror("socket send");
            // Fermer sockets déjà créés
            for (int j = 0; j < i; j++) close(sock_send[j]);
            close(sock_recv);
            return EXIT_FAILURE;
        }

        int broadcastEnable = 1;
        if (setsockopt(sock_send[i], SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
            perror("setsockopt SO_BROADCAST");
            for (int j = 0; j <= i; j++) close(sock_send[j]);
            close(sock_recv);
            return EXIT_FAILURE;
        }
    }

    fd_set read_fds;
    struct timeval tv;
    time_t last_hello = 0;

    printf("[INFO] Protocole HELLO démarré\n");
    printf("[INFO] Envoi des HELLO toutes les %d secondes sur toutes les interfaces\n", HELLO_INTERVAL);

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(sock_recv, &read_fds);
        int maxfd = sock_recv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(maxfd +1, &read_fds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        time_t now = time(NULL);
        if (now - last_hello >= HELLO_INTERVAL) {
            for (int i = 0; i < nb_interfaces; i++) {
                struct sockaddr_in addr_send;
                memset(&addr_send, 0, sizeof(addr_send));
                addr_send.sin_family = AF_INET;
                addr_send.sin_port = htons(PORT);
                if (inet_pton(AF_INET, interfaces[i].broadcast, &addr_send.sin_addr) != 1) {
                    fprintf(stderr, "Erreur inet_pton broadcast interface %s\n", interfaces[i].ip);
                    continue;
                }
                ssize_t sent = sendto(sock_send[i], interfaces[i].ip, strlen(interfaces[i].ip), 0,
                                      (struct sockaddr*)&addr_send, sizeof(addr_send));
                if (sent < 0) {
                    perror("sendto");
                } else {
                    printf("[DEBUG] HELLO envoyé depuis %s sur broadcast %s\n", interfaces[i].ip, interfaces[i].broadcast);
                }
            }
            last_hello = now;
        }

        if (ret > 0 && FD_ISSET(sock_recv, &read_fds)) {
            char buf[128];
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);

            ssize_t len = recvfrom(sock_recv, buf, sizeof(buf)-1, 0,
                                   (struct sockaddr*)&sender_addr, &sender_len);
            if (len < 0) {
                perror("recvfrom");
                continue;
            }
            buf[len] = '\0';

            char sender_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, sizeof(sender_ip));

            // Ignorer nos propres messages (par IP)
            int is_self = 0;
            for (int i = 0; i < nb_interfaces; i++) {
                if (strcmp(sender_ip, interfaces[i].ip) == 0) {
                    is_self = 1;
                    break;
                }
            }
            if (is_self) continue;

            printf("[DEBUG] HELLO reçu de %s : '%s'\n", sender_ip, buf);

            maj_voisin(sender_ip);
        }

        if (now % HELLO_INTERVAL == 0) {
            afficher_voisins();
        }
    }

    for (int i = 0; i < nb_interfaces; i++) {
        close(sock_send[i]);
    }
    close(sock_recv);

    printf("[INFO] Programme arrêté\n");

    return 0;
}
