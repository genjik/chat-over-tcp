/**
 * @genjebek_assignment1
 * @author  Genjebek Matchanov <genjebek@buffalo.edu>
 * @version 1.0
 *
 * @section LICENSE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @section DESCRIPTION
 *
 * This contains the main function. Add further description here....
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/logger.h"

#define STDIN 0
#define CMD_SIZE 100
#define BUFF_SIZE 256

#define LIST_CMD 1

struct host {
    char hostname[32];
    char ip[18];
    unsigned short port;

    struct host* prev;
    struct host* next;

    bool is_head;
    bool is_tail;
};

struct host_serialized {
    char hostname[32];
    char ip[18];
    unsigned short port;
};

struct host_list {
    struct host* head;
    struct host* tail;
    int size;
};

/* list functions */
void list_init(struct host_list* list);
void list_insert(struct host_list* list, struct host* new_host); 
void list_remove(struct host_list* list, struct host* host); 
void list_debug(struct host_list* list);
void list_free(struct host_list* list); 

/* cmds */
void author_cmd();
void exit_cmd();
void ip_cmd();
void port_cmd();
int login_cmd(char*);
void list_cmd(struct host_list*);

char* my_ip;
char* my_port;
/* used only by client */
bool is_logged_in = false;

/* utility functions */
void get_ip();
bool is_valid_port(char* port);
void login_error();

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv) {
    /*Init. Logger*/
    cse4589_init_log(argv[2]);

    /*Clear LOGFILE*/
    fclose(fopen(LOGFILE, "w"));

    /*Start Here*/
    if (argc != 3) {
        printf("Usage:%s [c|s] [port]\n", argv[0]);
        exit(-1);
    }

    if (strlen(argv[1]) != 1) {
        printf("First argument must either be: c or s\n");
        exit(1);
    }

    get_ip();
    my_port = argv[2];
    
    /* Client */
    if (argv[1][0] == 'c') {
        struct host_list client_list;
        list_init(&client_list);

        fd_set master_list, rdy_list;
        FD_ZERO(&master_list);
        FD_ZERO(&rdy_list);
        FD_SET(STDIN, &master_list); 

        int sel_ret, server_sd;
        int highest_socket_num = STDIN;

        while (true) {
            printf("\n[PA1-Client@CSE489]$ ");
            fflush(stdout);

            memcpy(&rdy_list, &master_list, sizeof(master_list));

            sel_ret = select(highest_socket_num + 1, &rdy_list, NULL, NULL, NULL);
            if (sel_ret < 0)
                perror("error: server select()");

            if (sel_ret > 0) {
                for (int sd = 0; sd <= highest_socket_num; ++sd) {
                    if (!FD_ISSET(sd, &rdy_list))
                        continue;

                    if (sd == STDIN) {
                        char* cmd = (char*) calloc(CMD_SIZE, sizeof(char));

                        if (fgets(cmd, CMD_SIZE-1, stdin) == NULL)
                            exit(-1);

                        if (strcmp(cmd, "AUTHOR\n") == 0) 
                            author_cmd();
                        else if (strcmp(cmd, "IP\n") == 0)
                            ip_cmd();
                        else if (strcmp(cmd, "PORT\n") == 0)
                            port_cmd();
                        else if (strcmp(cmd, "LIST\n") == 0 && is_logged_in)
                            list_cmd(&client_list);
                        else if (strncmp(cmd, "LOGIN ", 6) == 0) {
                            server_sd = login_cmd(cmd+6);
                            FD_SET(server_sd, &master_list);
                            if (server_sd > highest_socket_num)
                                highest_socket_num = server_sd;
                        }
                        else if (strcmp(cmd, "REFRESH\n") == 0 && is_logged_in) {}
                        else if (strcmp(cmd, "LOGOUT\n") == 0 && is_logged_in) {}
                        else if (strcmp(cmd, "EXIT\n") == 0)
                            exit_cmd();

                        free(cmd);
                    }
                    else if (sd == server_sd) {
                        char* buf = (char*) calloc(BUFF_SIZE, sizeof(char));
                        void* orig_ptr = buf;

                        if (recv(server_sd, buf, BUFF_SIZE, 0) <= 0) {
                            printf("server closed connection\n");
                            close(server_sd);
                            is_logged_in = false;
                            FD_CLR(server_sd, &master_list);
                        }
                        else {
                            int cmd = *(int*) buf;
                            int size = *(int*) (buf+4);
                            buf += 8;
                            if (cmd == LIST_CMD) {
                                list_free(&client_list);
                                for (int i = 0; i < size; ++i) {
                                    struct host_serialized* host = (struct host_serialized*) buf; 
                                    struct host* full_data = (struct host*) calloc(1, sizeof(struct host));
                                    memcpy(full_data->hostname, host->hostname, 32);
                                    memcpy(full_data->ip, host->ip, 18);
                                    full_data->port = host->port;
                                    list_insert(&client_list, full_data);
                                    //printf("%d) ip: %s port: %d\n", i, host->ip, host->port);
                                    buf += sizeof(struct host_serialized);
                                }
                                //list_debug(&client_list);
                            }
                        }
                        free(orig_ptr);
                    }
                }
            }
        }
    }
    /* Server */
    else if (argv[1][0] == 's') {
        struct host_list client_list;
        list_init(&client_list);

        struct addrinfo hints;
        struct addrinfo* res;

        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = AI_PASSIVE;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int listening_socket;
        int yes = 1;

        if (getaddrinfo(NULL, argv[2], &hints, &res) != 0)
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

        int sel_ret;
        int highest_socket_num = listening_socket;

        int con_sd;
        struct sockaddr_storage host_addr;
        socklen_t host_addr_size = sizeof(host_addr);

        while (true) {
            memcpy(&rdy_list, &master_list, sizeof(master_list));

            sel_ret = select(highest_socket_num + 1, &rdy_list, NULL, NULL, NULL);
            if (sel_ret < 0)
                perror("error: server select()");

            if (sel_ret > 0) {
                for (int sd = 0; sd <= highest_socket_num; ++sd) {
                    if (!FD_ISSET(sd, &rdy_list))
                        continue;

                    if (sd == STDIN) {
                        char* cmd = (char*) calloc(CMD_SIZE, sizeof(char));

                        if (fgets(cmd, CMD_SIZE-1, stdin) == NULL)
                            exit(-1);

                        if (strcmp(cmd, "AUTHOR\n") == 0)
                            author_cmd();
                        else if (strcmp(cmd, "IP\n") == 0)
                            ip_cmd();
                        else if (strcmp(cmd, "PORT\n") == 0)
                            port_cmd();
                        else if (strcmp(cmd, "LIST\n") == 0)
                            list_cmd(&client_list);
                        else if (strcmp(cmd, "STATISTICS\n") == 0) {}

                        free(cmd);
                    }
                    else if (sd == listening_socket) {
                        socklen_t host_addr_size = sizeof(host_addr);
                        if ((con_sd = accept(sd, (struct sockaddr*) &host_addr, &host_addr_size)) < 0)
                            perror("error: server accept()");

                        FD_SET(con_sd, &master_list);
                        if (con_sd > highest_socket_num)
                            highest_socket_num = con_sd;

                        /* @TODO: SEND list of connected users to client */
                        if (host_addr.ss_family == AF_INET) {
                            struct sockaddr_in* host_addr_in = (struct sockaddr_in*) &host_addr;

                            char* hostname = (char*) malloc(32);
                            getnameinfo((struct sockaddr*) host_addr_in, sizeof(struct sockaddr_in), hostname, 32, NULL, 0, 0); 

                            struct host* new_host = (struct host*) calloc(1, sizeof(struct host));

                            if (inet_ntop(AF_INET, &host_addr_in->sin_addr, new_host->ip, 18) == NULL)
                                perror("error: server inet_ntop()");
                            memcpy(new_host->hostname, hostname, 32);
                            new_host->port = host_addr_in->sin_port;

                            list_insert(&client_list, new_host);

                            void* buf = calloc(BUFF_SIZE, sizeof(char));
                            *(int*) buf = LIST_CMD;
                            *(int*) (buf+4) = client_list.size;
                            int i = 8;
                            for (struct host* cur = client_list.head->next; cur != client_list.tail; cur = cur->next) {
                               memcpy(buf+i, cur, sizeof(struct host_serialized)); 
                               i += sizeof(struct host_serialized);
                            }
                            int bytes_send;
                            if ((bytes_send = send(con_sd, buf, BUFF_SIZE, 0)) == -1)
                                perror("error: server send()");
                            //printf("bytes send: %d\n", bytes_send);
                            //list_debug(&client_list);
                            free(buf);
                        }
                    }
                    else {
                        char* buf = (char*) calloc(BUFF_SIZE, sizeof(char));

                        if (recv(sd, buf, BUFF_SIZE, 0) <= 0) {
                            close(sd);
                            FD_CLR(sd, &master_list);
                        }
                        else {
                            /* @TODO: process client reqeust */
                        }
                    }
                }
            }
        }
    }
    else {
        printf("First argument must either be: c or s\n");
        exit(1);
    }

    return 0;
}

void get_ip() {
    if (my_ip != NULL)
        return;
    
    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo("8.8.8.8", "53", &hints, &res) != 0) {
        printf("Failed to obtain getaddrinfo() in get_ip()\n");
        exit(1);
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        printf("Failed to create socket() in get_ip()\n");
        exit(1);
    }

    if (connect(sd, res->ai_addr, res->ai_addrlen) != 0) {
        printf("Failed to connect() in get_ip()\n");
        exit(1);
    }

    if (getsockname(sd, res->ai_addr, &res->ai_addrlen) != 0) {
        printf("Failed to getsockname() in get_ip()\n");
        exit(1);
    }

    my_ip = (char*) malloc(16);
    struct sockaddr_in* sock_addr_in = (struct sockaddr_in*) res->ai_addr;
    if (inet_ntop(res->ai_family, &sock_addr_in->sin_addr, my_ip, 16) == NULL) {
        printf("Failed to inet_ntop() in get_ip()\n");
        exit(1);
    }
}

void author_cmd() {
    cse4589_print_and_log("[AUTHOR:SUCCESS]\n");
    cse4589_print_and_log("I, genjebek, have read and understood the course academic integrity policy.\n");
    cse4589_print_and_log("[AUTHOR:END]\n");
}

void exit_cmd() {
    exit(0);
}

void ip_cmd() {
    if (my_ip != NULL) {
        cse4589_print_and_log("[IP:SUCCESS]\n");
        cse4589_print_and_log("IP:%s\n", my_ip);
        cse4589_print_and_log("[IP:END]\n");
    }
    else {
        cse4589_print_and_log("[IP:ERROR]\n");
        cse4589_print_and_log("[IP:END]\n");
    }
}

void port_cmd() {
    cse4589_print_and_log("[PORT:SUCCESS]\n");
    cse4589_print_and_log("PORT:%d\n", atoi(my_port));
    cse4589_print_and_log("[PORT:END]\n"); 
}

int login_cmd(char* args) {
    char ip[17];
    char port[6];

    int i;
    for(i = 0; args[i] != ' ' && i < 16; ++i) {
        ip[i] = args[i];
    }
    ip[i] = '\0';

    args += i + 1;

    for(i = 0; args[i] != '\n' && i < 5; ++i) {
        if (args[i] < 48 || args[i] > 57) {
            login_error();
            return -1;
        }
        port[i] = args[i];
    }
    port[i] = '\0';

    /* ip validation */
    struct sockaddr_in sockaddr;
    if (inet_pton(AF_INET, ip, &(sockaddr.sin_addr)) != 1) {
        login_error();
        return -1;
    }
    
    /* port validation */
    if (args[i] != '\n' || (args[i] != '\n' && args[i] != ' ')) {
        login_error();
        return -1;
    }
    if (is_valid_port(port) == false) {
        login_error();
        return -1;
    }

    /* if both ip and port validated */
    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip, port, &hints, &res) != 0) {
        login_error();
        return -1;
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        login_error();
        return -1;
    }

    if (connect(sd, res->ai_addr, res->ai_addrlen) != 0) {
        login_error();
        return -1;
    }

    is_logged_in = true;
    cse4589_print_and_log("[LOGIN:SUCCESS]\n");
    cse4589_print_and_log("[LOGIN:END]\n");
    return sd;
}

void list_cmd(struct host_list* list) {
    cse4589_print_and_log("[LIST:SUCCESS]\n");
    struct host* cur = list->head->next;
    int i = 1;
    while (cur != list->tail) {
       cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i, cur->hostname, cur->ip, cur->port);
       ++i;
       cur = cur->next;
    }
    cse4589_print_and_log("[LIST:END]\n");
}

void login_error() {
    cse4589_print_and_log("[LOGIN:ERROR]\n");
    cse4589_print_and_log("[LOGIN:END]\n");
}

bool is_valid_port(char* port) {
    if (atoi(port) < 0 || atoi(port) > 65535)
        return false;
    return true;
}

void list_init(struct host_list* list) {
    struct host* head = (struct host*) calloc(1, sizeof(struct host));
    struct host* tail = (struct host*) calloc(1, sizeof(struct host));

    head->next = tail;
    tail->prev = head;

    head->is_head = true;
    tail->is_tail = true;

    list->head = head;
    list->tail = tail;
    list->size = 0;
}

void list_insert(struct host_list* list, struct host* new_host) {
    struct host* cur = list->head->next;
    while (cur != list->tail) {
        if (cur->port > new_host->port)
            break;
        cur = cur->next;
    }
    new_host->next = cur;
    new_host->prev = cur->prev;
    cur->prev->next = new_host;
    cur->prev = new_host;

    ++list->size;
}

void list_remove(struct host_list* list, struct host* host) {
    host->prev->next = host->next;
    host->next->prev = host->prev;
    free(host);
    --list->size;
}

void list_debug(struct host_list* list) {
    struct host* cur = list->head->next;
    int i = 0;
    printf("\nsize:%d\n", list->size);
    while (cur != list->tail) {
        printf("%d) hostname: %s ip: %s port: %d\n", i++, cur->hostname, cur->ip, cur->port);
        cur = cur->next;
    }
}

void list_free(struct host_list* list) {
    struct host* cur = list->head->next;
    while (cur != list->tail) {
        struct host* next = cur->next;
        free(cur);
        cur = next;
    }
    list->head->next = list->tail;
    list->tail->prev = list->head;
    list->size = 0;
}
