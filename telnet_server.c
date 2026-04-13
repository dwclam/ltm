#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int state; 
    char username[64];
} Client;

int authenticate(const char *user, const char *pass) {
    FILE *db = fopen("database.txt", "r");
    if (!db) return 0;
    
    char line[256], u[64], p[64];
    while (fgets(line, sizeof(line), db)) {
        if (sscanf(line, "%s %s", u, p) == 2) {
            if (strcmp(user, u) == 0 && strcmp(pass, p) == 0) {
                fclose(db);
                return 1;
            }
        }
    }
    fclose(db);
    return 0;
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

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 0;
                    memset(clients[i].username, 0, sizeof(clients[i].username));
                    
                    char *prompt = "Username: ";
                    send(new_socket, prompt, strlen(prompt), 0);
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
                    
                    if (strlen(buffer) == 0 && clients[i].state != 2) {
                        continue;
                    }

                    if (clients[i].state == 0) {
                        strncpy(clients[i].username, buffer, sizeof(clients[i].username) - 1);
                        clients[i].state = 1;
                        char *prompt = "Password: ";
                        send(sd, prompt, strlen(prompt), 0);
                    } 
                    else if (clients[i].state == 1) {
                        if (authenticate(clients[i].username, buffer)) {
                            clients[i].state = 2;
                            char *success_msg = "Login successful.\ntelnet> ";
                            send(sd, success_msg, strlen(success_msg), 0);
                        } else {
                            clients[i].state = 0;
                            char *fail_msg = "Login failed. Invalid username or password.\nUsername: ";
                            send(sd, fail_msg, strlen(fail_msg), 0);
                        }
                    } 
                    else if (clients[i].state == 2) {
                        if (strlen(buffer) > 0) {
                            char cmd[BUF_SIZE + 128];
                            char out_file[64];
                            snprintf(out_file, sizeof(out_file), "out_%d.txt", sd);
                            snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buffer, out_file);
                            
                            system(cmd);

                            FILE *fp = fopen(out_file, "r");
                            if (fp) {
                                char file_buf[BUF_SIZE];
                                size_t bytes_read;
                                while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), fp)) > 0) {
                                    send(sd, file_buf, bytes_read, 0);
                                }
                                fclose(fp);
                            }
                        }
                        char *prompt = "telnet> ";
                        send(sd, prompt, strlen(prompt), 0);
                    }
                }
            }
        }
    }
    return 0;
}
