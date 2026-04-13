#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct {
    int fd;
    char id[64];
    int registered;
} Client;

void get_current_time(char *buffer, size_t size) {
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, size, "%Y/%m/%d %I:%M:%S%p", timeinfo);
}

int main() {
    int listener;
    struct sockaddr_in server_addr;
    Client clients[MAX_CLIENTS];
    fd_set readfds;
    int max_fd;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
    }

    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(listener, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        max_fd = listener;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &readfds);
            }
            if (clients[i].fd > max_fd) {
                max_fd = clients[i].fd;
            }
        }

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(listener, &readfds)) {
            int new_socket;
            struct sockaddr_in client_addr;
            socklen_t addrlen = sizeof(client_addr);

            if ((new_socket = accept(listener, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
                perror("Accept failed");
                continue;
            }
            
            char *welcome_msg = "Vui long nhap ten theo cu phap: client_id: client_name\n";
            send(new_socket, welcome_msg, strlen(welcome_msg), 0);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].registered = 0;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[BUF_SIZE];
                memset(buffer, 0, BUF_SIZE);
                
                int valread = recv(sd, buffer, BUF_SIZE - 1, 0);

                if (valread == 0) {
                    close(sd);
                    clients[i].fd = 0;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0; 

                    if (clients[i].registered == 0) {
                        char id[64], name[64];
                        if (sscanf(buffer, "%[^:]: %s", id, name) == 2) {
                            clients[i].registered = 1;
                            strncpy(clients[i].id, id, sizeof(clients[i].id) - 1);
                            
                            char success_msg[128];
                            snprintf(success_msg, sizeof(success_msg), "Da ghi nhan ID: %s. Ban co the bat dau chat!\n", id);
                            send(sd, success_msg, strlen(success_msg), 0);
                        } else {
                            char *err_msg = "Sai cu phap. Vui long nhap lai (vd: abc: NguyenVanA)\n";
                            send(sd, err_msg, strlen(err_msg), 0);
                        }
                    } 
                    else {
                        char time_str[80];
                        get_current_time(time_str, sizeof(time_str));

                        char send_buffer[BUF_SIZE + 256];
                        snprintf(send_buffer, sizeof(send_buffer), "%s %s: %s\n", time_str, clients[i].id, buffer);

                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].fd != 0 && clients[j].fd != sd && clients[j].registered == 1) {
                                send(clients[j].fd, send_buffer, strlen(send_buffer), 0);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
