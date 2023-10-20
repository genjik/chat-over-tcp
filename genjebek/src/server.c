#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/server.h"
#include "../include/user.h"
#include "../include/cmds.h"

#include "../include/logger.h"

/* server responses */
static void refresh_response(struct user_list* user_list, int con_sd); 

/* cmds */
static void stats_cmd(struct user_list* user_list);

/* utilities */
static struct user* create_user(struct sockaddr_in* client_addr_in, int client_sd);
char* get_status(bool is_logged_in);

static char logged_in[] = {"logged-in"};
static char logged_out[] = {"logged-out"};

void server_start(char* server_ip) {
    struct user_list user_list;
    user_list_init(&user_list);

    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int listening_socket;
    int yes = 1;

    if (getaddrinfo(NULL, server_ip, &hints, &res) != 0)
        perror("error: server getaddrinfo()");

    if ((listening_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 1)
        perror("error: server socket()");

    setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(listening_socket, res->ai_addr, res->ai_addrlen) < 0)
        perror("error: server bind()");

    if (listen(listening_socket, 5) < 0)
        perror("error: server listen()");

    freeaddrinfo(res);

    fd_set master_list, rdy_list;
    FD_ZERO(&master_list);
    FD_ZERO(&rdy_list);

    FD_SET(listening_socket, &master_list);
    FD_SET(STDIN, &master_list); 

    int fds_ready;
    int highest_socket_num = listening_socket;

    int client_sd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    while (true) {
        memcpy(&rdy_list, &master_list, sizeof(master_list));

        fds_ready = select(highest_socket_num + 1, &rdy_list, NULL, NULL, NULL);
        if (fds_ready < 0)
            perror("error: server select()");

        if (fds_ready > 0) {
            for (int sd = 0; sd <= highest_socket_num; ++sd) {
                if (!FD_ISSET(sd, &rdy_list))
                    continue;

                if (sd == STDIN) {
                    char* cmd = (char*) calloc(CMD_SIZE, sizeof(char));

                    if (fgets(cmd, CMD_SIZE-1, stdin) == NULL)
                        exit(-1);

                    if (strcmp(cmd, "AUTHOR\n") == 0) author_cmd();
                    else if (strcmp(cmd, "IP\n") == 0) ip_cmd();
                    else if (strcmp(cmd, "PORT\n") == 0) port_cmd();
                    else if (strcmp(cmd, "LIST\n") == 0) list_cmd(&user_list, false, false);
                    else if (strcmp(cmd, "STATISTICS\n") == 0) stats_cmd(&user_list);
                    else if (strncmp(cmd, "BLOCKED ", 8) == 0) {}

                    free(cmd);
                }
                else if (sd == listening_socket) {
                    socklen_t client_addr_size = sizeof(client_addr);
                    if ((client_sd = accept(sd, (struct sockaddr*) &client_addr, &client_addr_size)) < 0)
                        perror("error: server accept()");

                    FD_SET(client_sd, &master_list);
                    if (client_sd > highest_socket_num)
                        highest_socket_num = client_sd;

                    /* SEND list of connected users to user */
                    if (client_addr.ss_family == AF_INET) {
                        struct sockaddr_in* user_addr_in = (struct sockaddr_in*) &client_addr;

                        /* Extract ip and port from new socket connection */
                        char ip[18];
                        int port = htons(user_addr_in->sin_port);
                        if (inet_ntop(AF_INET, &user_addr_in->sin_addr, ip, 18) == NULL) {
                            printf("ERROR inet_ntop() in sd == listening_socket \n");
                        };
                        
                        /* Find if user (ip + port) has previously connected and logged out */
                        struct user* old_user = user_list_find_by_ip_and_port(&user_list, ip, port);
                        if (old_user != NULL) {
                            old_user->is_logged_in = true;
                            old_user->sd = client_sd;
                        }
                        else {
                            struct user* new_user = create_user((struct sockaddr_in*) &client_addr, client_sd);
                            user_list_insert(&user_list, new_user);
                        }

                        refresh_response(&user_list, client_sd); 
                    }
                }
                else {
                    char* buf = (char*) calloc(BUFF_SIZE, sizeof(char));

                    if (recv(sd, buf, BUFF_SIZE, 0) <= 0) {
                        /* Find user by its socket and mark user as "logged_out" */
                        struct user* user = user_list_find_by_sd(&user_list, sd);
                        if (user == NULL)
                            perror("error happened");
                        user->is_logged_in = false;

                        close(sd);
                        FD_CLR(sd, &master_list);
                        continue;
                    }

                    int type = *(int*) buf;

                    if (type == TYPE_REFRESH) refresh_response(&user_list, sd); 
                    else if (type == TYPE_SEND) {}
                    else if (type == TYPE_BROADCAST) {}
                    else if (type == TYPE_BLOCK) {}
                    else if (type == TYPE_UNBLOCK) {}
                    else if (type == TYPE_EXIT) user_list_remove_by_search(&user_list, (struct user_stripped*) (buf + 8));

                    free(buf);
                }
            }
        }
    }
}

/* Server to client response functions */
static void refresh_response(struct user_list* user_list, int con_sd) {
    void* buf = calloc(BUFF_SIZE, sizeof(char));

    *(int*) buf = TYPE_REFRESH;
    *(int*) (buf+4) = user_list->size;

    int i = 8;
    for (struct user* cur = user_list->head->next; cur != user_list->tail; cur = cur->next) {
       memcpy(buf+i, cur, sizeof(struct user_stripped)); 
       i += sizeof(struct user_stripped);
    }

    int bytes_send;
    if ((bytes_send = send(con_sd, buf, BUFF_SIZE, 0)) == -1)
        perror("error: server send()");

    free(buf);
}


/* misc functions */
static struct user* create_user(struct sockaddr_in* user_addr_in, int client_sd) {
    /* Allocate space for new client */
    struct user* new_user = (struct user*) calloc(1, sizeof(struct user));

    /* Get hostname using getnameinfo() */
    char* hostname = (char*) malloc(32);
    getnameinfo((struct sockaddr*) user_addr_in, sizeof(struct sockaddr_in), hostname, 32, NULL, 0, 0); 

    /* Add hostname, ip, port to struct user */
    memcpy(new_user->hostname, hostname, 32);
    if (inet_ntop(AF_INET, &user_addr_in->sin_addr, new_user->ip, 18) == NULL)
        perror("error: server inet_ntop()");
    new_user->port = htons(user_addr_in->sin_port);
    new_user->sd = client_sd;
    new_user->is_logged_in = true;
    new_user->num_msg_sent = 0;
    new_user->num_msg_rcv = 0;

    return new_user;
}

static void stats_cmd(struct user_list* user_list) {
    struct user* cur = user_list->head->next;
    int list_id = 1;
    while (cur != user_list->tail) {
        printf("%-5d%-35s%-8d%-8d%-8s\n", list_id, cur->hostname, cur->num_msg_sent, cur->num_msg_rcv, get_status(cur->is_logged_in));
        cur = cur->next;
        ++list_id;
    }
}

char* get_status(bool is_logged_in) {
    if (is_logged_in)
        return logged_in;
    return logged_out;
}
