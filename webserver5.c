#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096

void *handle_client(void *arg);
int is_palindrome(int num);
void send_ui(int client_sock);

int main() {
    int server_fd, client_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 50) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("🚀 Multithreaded Server listening on port %d...\n", PORT);

    while (1) {
        if ((client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }

        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}

void *handle_client(void *arg) {
    int client_sock = *((int *)arg);
    free(arg);

    char buffer[BUFFER_SIZE] = {0};
    read(client_sock, buffer, BUFFER_SIZE);

    if (strstr(buffer, "GET / ") != NULL || strstr(buffer, "GET /ui6.html") != NULL) {
        send_ui(client_sock);
    }
    else if (strstr(buffer, "GET /check-palindrome?") != NULL) {
        char *query = strstr(buffer, "/check-palindrome?") + strlen("/check-palindrome?");
        int clientId = -1, number = -1;
        sscanf(query, "clientId=%d&number=%d", &clientId, &number);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int result = is_palindrome(number);
        usleep(100000); // simulate server processing

        clock_gettime(CLOCK_MONOTONIC, &end);
        double timeTaken = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec)/1000000.0;

        printf("Client %d processed by thread %lu\n", clientId, pthread_self());

        char response[1024];
        snprintf(response, sizeof(response),
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
            "{\"clientId\": %d, \"number\": %d, \"isPalindrome\": %s, \"timeTaken\": %.2f, \"threadId\": %lu}",
            clientId, number, result ? "true":"false", timeTaken, pthread_self());

        send(client_sock, response, strlen(response), 0);
    }
    else {
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, not_found, strlen(not_found), 0);
    }

    close(client_sock);
    return NULL;
}

int is_palindrome(int num) {
    int original = num, reversed = 0;
    while(num > 0) {
        reversed = reversed*10 + (num%10);
        num /= 10;
    }
    return original == reversed;
}

void send_ui(int client_sock) {
    FILE *file = fopen("ui6.html", "r");
    if(!file) {
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, not_found, strlen(not_found), 0);
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);
    char *content = malloc(filesize+1);
    fread(content, 1, filesize, file);
    content[filesize] = '\0';
    fclose(file);

    char header[256];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %ld\r\n\r\n",
             filesize);

    send(client_sock, header, strlen(header), 0);
    send(client_sock, content, filesize, 0);
    free(content);
}
