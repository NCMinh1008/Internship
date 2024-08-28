#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define TTI_DURATION 2000000
#define MAX_UE_PER_TTI 4
#define RB_PER_TTI 100
#define MAX_TTI_DELAY 10
#define MAX_MCS 27

typedef struct {
    int socket;
    struct sockaddr_in address;
    int mcs;
    int delay;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int connected_clients = 0;

void add_client(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (!clients[i]) {
            clients[i] = cl;
            connected_clients++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            if (clients[i]->socket == socket) {
                clients[i] = NULL;
                connected_clients--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buffer[BUFFER_SIZE];
    int nbytes;
    client_t *cli = (client_t *)arg;

    // Assign a random MCS value between 0 and 27
    cli->mcs = rand() % (MAX_MCS + 1);
    cli->delay = 0;

    printf("Client connected with MCS %d\n", cli->mcs);

    while ((nbytes = recv(cli->socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[nbytes] = '\0';
        printf("Client %d: %s\n", cli->socket, buffer);
    }

    close(cli->socket);
    printf("Client disconnected\n");
    remove_client(cli->socket);
    free(cli);

    return NULL;
}

void allocate_resources() {
    pthread_mutex_lock(&clients_mutex);

    if (connected_clients == 0) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i]) {
            clients[i]->delay++;
        }
    }

    int max_mcs = -1;
    int selected_client = -1;

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] && clients[i]->delay >= MAX_TTI_DELAY) {
            if (clients[i]->mcs > max_mcs) {
                max_mcs = clients[i]->mcs;
                selected_client = i;
            }
        }
    }

    if (selected_client == -1) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (clients[i] && clients[i]->mcs > max_mcs) {
                max_mcs = clients[i]->mcs;
                selected_client = i;
            }
        }
    }

    clients[selected_client]->delay = 0;
    printf("Allocating %d RBs to Client %d with MCS %d\n", RB_PER_TTI, clients[selected_client]->socket, clients[selected_client]->mcs);
    char message[BUFFER_SIZE];
    snprintf(message, sizeof(message), "Allocated %d RBs", RB_PER_TTI);
    send(clients[selected_client]->socket, message, strlen(message) + 1, 0);

    pthread_mutex_unlock(&clients_mutex);
}


void *tti_scheduler(void *arg) {
    while (1) {
        usleep(TTI_DURATION);
        printf("Starting new TTI ...\n");
        allocate_resources();
    }
    return NULL;
}

int main() {
    int server_socket, new_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t tid, tti_tid;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Socket bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("Socket listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    pthread_create(&tti_tid, NULL, tti_scheduler, NULL);
    pthread_detach(tti_tid);

    while (1) {
        new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (new_socket == -1) {
            perror("Socket accept failed");
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->socket = new_socket;
        cli->address = client_addr;

        add_client(cli);
        pthread_create(&tid, NULL, handle_client, (void *)cli);
        pthread_detach(tid);
    }

    close(server_socket);
    return 0;
}
