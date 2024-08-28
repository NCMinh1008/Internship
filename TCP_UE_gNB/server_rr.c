#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8080
#define MAX_CLIENTS 12
#define BUFFER_SIZE 1024
#define TTI_DURATION 2000000
#define MAX_UE_PER_TTI 4
#define RB_PER_TTI 100

typedef struct {
    int socket;
    struct sockaddr_in address;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int current_client_index = 0;
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

    printf("Client connected: %d\n", cli->socket - 3);

    while ((nbytes = recv(cli->socket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[nbytes] = '\0';
        printf("Client %d: %s\n", cli->socket, buffer);
    }

    close(cli->socket);
    printf("Client %d disconnected\n", cli->socket - 3);
    remove_client(cli->socket);
    free(cli);

    return NULL;
}

void allocate_resources() {
    pthread_mutex_lock(&clients_mutex);

    int allocated_clients = 0;

    if (connected_clients == 0) {
        pthread_mutex_unlock(&clients_mutex);
        return;
    }
    int base_rb_per_ue = 0;
    if(connected_clients < MAX_UE_PER_TTI){ 
        base_rb_per_ue = RB_PER_TTI / connected_clients;
    }else{
        base_rb_per_ue = RB_PER_TTI / MAX_UE_PER_TTI;
    }

    for (int i = 0; i < MAX_CLIENTS && allocated_clients < MAX_UE_PER_TTI; ++i) {
        int client_index = (current_client_index + i) % MAX_CLIENTS;
        if (clients[client_index]) {
            printf("Allocating %d RBs to Client %d\n", base_rb_per_ue, clients[client_index]->socket - 3);
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "Allocated %d RBs", base_rb_per_ue);
            send(clients[client_index]->socket, message, strlen(message) + 1, 0); 
            allocated_clients++;
        }
    }

    current_client_index = (current_client_index + allocated_clients) % connected_clients;

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

    // Start TTI scheduler thread
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
