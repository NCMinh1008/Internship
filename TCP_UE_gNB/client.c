#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;
    char server_reply[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created\n");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed");
        close(sock);
        return 1;
    }
    printf("Connected to server\n");

    // Receive server replies
    while (1) {
        int recv_len = recv(sock, server_reply, BUFFER_SIZE, 0);
        if (recv_len > 0) {
            server_reply[recv_len] = '\0';
            printf("Server reply: %s\n", server_reply);
        } else if (recv_len == 0) {
            printf("Server disconnected\n");
            break;
        } else {
            perror("Receive failed");
            break;
        }
    }

    close(sock);
    return 0;
}
