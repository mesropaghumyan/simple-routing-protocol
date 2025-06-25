#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

#define MAX_NODES 1000
#define MAX_NEIGHBORS 100
#define BUF_SIZE 1024
#define PORT 5000

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int parent_idx;  // index du parent dans visited, -1 si racine
} VisitedNode;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int metric; // inutilisé ici, mais on le lit quand même
} Node;

VisitedNode visited[MAX_NODES];
int visited_count = 0;

int find_in_visited(const char *ip) {
    for (int i = 0; i < visited_count; i++) {
        if (strcmp(visited[i].ip, ip) == 0)
            return i;
    }
    return -1;
}

void print_route_visited(int idx) {
    if (idx == -1) return;
    print_route_visited(visited[idx].parent_idx);
    printf("%s ", visited[idx].ip);
}

// Interroge un noeud en UDP port 5000, envoie "GET_NEIGHBORS", récupère la liste des voisins
// Remplit neighbors avec les IP + metric, renvoie nombre de voisins ou -1 en erreur
int interroger_voisin(const char *ip, Node neighbors[], int *count) {
    int sock;
    struct sockaddr_in addr;
    char buffer[BUF_SIZE];
    ssize_t recvd;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("[interroger_voisin] socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "[interroger_voisin] inet_pton failed for %s\n", ip);
        close(sock);
        return -1;
    }

    // Envoi de la requête
    const char *msg = "GET_NEIGHBORS";
    if (sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[interroger_voisin] sendto");
        close(sock);
        return -1;
    }

    // Réception réponse (timeout 2s)
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    recvd = recvfrom(sock, buffer, sizeof(buffer)-1, 0, NULL, NULL);
    if (recvd < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            fprintf(stderr, "[interroger_voisin] Timeout recvfrom sur %s\n", ip);
        } else {
            perror("[interroger_voisin] recvfrom");
        }
        close(sock);
        return -1;
    }

    buffer[recvd] = '\0';
    close(sock);

    // Parsing de la réponse ligne par ligne : "ip metric"
    *count = 0;
    char *line = strtok(buffer, "\n");
    while (line != NULL && *count < MAX_NEIGHBORS) {
        char ipbuf[INET_ADDRSTRLEN];
        int metric;
        if (sscanf(line, "%15s %d", ipbuf, &metric) == 2) {
            strcpy(neighbors[*count].ip, ipbuf);
            neighbors[*count].metric = metric;
            (*count)++;
        }
        line = strtok(NULL, "\n");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage : %s <ip_cible>\n", argv[0]);
        return 1;
    }

    const char *ip_target = argv[1];
    printf("[main] IP cible : %s\n", ip_target);

    // Queue BFS, stocke indices dans visited
    int queue[MAX_NODES];
    int front = 0, rear = 0;

    // Initialisation visited avec noeud local 127.0.0.1, parent -1
    strcpy(visited[0].ip, "127.0.0.1");
    visited[0].parent_idx = -1;
    visited_count = 1;

    queue[rear++] = 0;

    int found_idx = -1;

    while (front < rear) {
        int cur_idx = queue[front++];
        printf("[main] Traitement du noeud courant : %s\n", visited[cur_idx].ip);

        if (strcmp(visited[cur_idx].ip, ip_target) == 0) {
            printf("Route vers %s trouvee !\n", ip_target);
            found_idx = cur_idx;
            break;
        }

        Node neighbors[MAX_NEIGHBORS];
        int count = 0;
        if (interroger_voisin(visited[cur_idx].ip, neighbors, &count) < 0) {
            fprintf(stderr, "[main] Erreur en interrogeant %s\n", visited[cur_idx].ip);
            continue;
        }

        printf("[interroger_voisin] Reponse recue:\n");
        for (int i = 0; i < count; i++) {
            printf("%s %d\n", neighbors[i].ip, neighbors[i].metric);
        }
        printf("\n");

        for (int i = 0; i < count; i++) {
            if (find_in_visited(neighbors[i].ip) == -1) {
                if (visited_count >= MAX_NODES) {
                    fprintf(stderr, "Limite de noeuds atteinte\n");
                    break;
                }
                strcpy(visited[visited_count].ip, neighbors[i].ip);
                visited[visited_count].parent_idx = cur_idx;
                queue[rear++] = visited_count;
                visited_count++;
                printf("[enqueue] Ajoute %s %d en position %d\n", neighbors[i].ip, neighbors[i].metric, rear -1);
            }
        }
    }

    if (found_idx != -1) {
        printf("Chemin complet : ");
        print_route_visited(found_idx);
        printf("\n");
    } else {
        printf("Route vers %s introuvable.\n", ip_target);
    }

    return 0;
}
