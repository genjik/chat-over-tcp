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

#include "../include/client.h"
#include "../include/user.h"
#include "../include/cmds.h"
#include "../include/logger.h"

static int login_cmd(char* args); 
static void refresh_cmd(int sd);
static void send_cmd(char* args, struct user_list* list, int sd);
static void broadcast_cmd(char* args, int sd);
static void block_cmd(char* args, struct user_list* list, int sd);
static void unblock_cmd(char* args, struct user_list* list, int sd);
static void exit_cmd(struct user_list* list, int sd);

static void login_error();
static void refresh_error();
static void send_error();
static void broadcast_error();
static void block_error();
static void unblock_error();

static int validate_ip(char* cmd, char* output_ip);
static int validate_port(char* cmd, char* output_port);
static int get_msg_size(char* cmd);

static bool is_logged_in = false;

void client_start() {
    struct user_list user_list;
    user_list_init(&user_list);

    fd_set master_list, rdy_list;
    FD_ZERO(&master_list);
    FD_ZERO(&rdy_list);
    FD_SET(STDIN, &master_list); 

    int fds_ready;
    int server_sd;
    int highest_socket_num = STDIN;

    while (true) {
        printf("\n[PA1-Client@CSE489]$ ");
        fflush(stdout);

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
                    else if (strcmp(cmd, "LIST\n") == 0) list_cmd(&user_list, is_logged_in, true);

                    else if (strncmp(cmd, "LOGIN ", 6) == 0) {
                        server_sd = login_cmd(cmd+6);
                        FD_SET(server_sd, &master_list);
                        if (server_sd > highest_socket_num)
                            highest_socket_num = server_sd;
                    }
                    else if (strncmp(cmd, "SEND ", 5) == 0) send_cmd(cmd+5, &user_list, server_sd);
                    else if (strncmp(cmd, "BROADCAST ", 10) == 0) broadcast_cmd(cmd+10, server_sd);
                    else if (strncmp(cmd, "BLOCK ", 6) == 0) block_cmd(cmd+6, &user_list, server_sd);
                    else if (strncmp(cmd, "UNBLOCK ", 8) == 0) unblock_cmd(cmd+8, &user_list, server_sd);
                    else if (strcmp(cmd, "REFRESH\n") == 0) refresh_cmd(server_sd);
                    else if (strcmp(cmd, "LOGOUT\n") == 0) {
                        close(server_sd);
                        FD_CLR(server_sd, &master_list);
                        is_logged_in = false;
                    }
                    else if (strcmp(cmd, "EXIT\n") == 0) exit_cmd(&user_list, server_sd);

                    free(cmd);
                }
                else if (sd == server_sd) {
                    char* buf = (char*) calloc(BUFF_SIZE, sizeof(char));
                    void* orig_ptr = buf;

                    if (recv(server_sd, buf, BUFF_SIZE, 0) <= 0) {
                        //printf("server closed connection\n");
                        close(server_sd);
                        is_logged_in = false;
                        FD_CLR(server_sd, &master_list);
                    }
                    else {
                        int type = *(int*) buf;

                        if (type == TYPE_REFRESH) {
                            int list_size = *(int*) (buf+4);
                            buf += 8;

                            user_list_free(&user_list);
                            for (int i = 0; i < list_size; ++i) {
                                struct user_stripped* host = (struct user_stripped*) buf; 
                                struct user* full_data = (struct user*) calloc(1, sizeof(struct user));
                                memcpy(full_data->hostname, host->hostname, 32);
                                memcpy(full_data->ip, host->ip, 18);
                                full_data->port = host->port;
                                user_list_insert(&user_list, full_data);
                                //printf("%d) ip: %s port: %d\n", i, host->ip, host->port);
                                buf += sizeof(struct user_stripped);
                            }
                            //user_list_debug(&user_list);
                        }
                    }
                    free(orig_ptr);
                }
            }
        }
    }
}

int login_cmd(char* args) {
    char ip[17];
    char port[6];

    int ptr = 0;
    if ((ptr = validate_ip(args, ip)) < 0) {
        login_error();
        return -1;
    }

    args += ptr + 1;
    
    if ((ptr = validate_port(args, port)) < 0) {
        login_error();
        return -1;
    }

    /* if both ip and port validated */
    struct addrinfo hints;
    struct addrinfo* res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, get_my_port(), &hints, &res) != 0) {
        login_error();
        return -1;
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        login_error();
        return -1;
    }

    int yes = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(sd, res->ai_addr, res->ai_addrlen) < 0) {
        login_error();
        return -1;
    }

    struct addrinfo* res2;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(ip, port, &hints, &res2) != 0) {
        login_error();
        return -1;
    }

    if (connect(sd, res2->ai_addr, res2->ai_addrlen) != 0) {
        login_error();
        return -1;
    }

    is_logged_in = true;
    cse4589_print_and_log("[LOGIN:SUCCESS]\n");
    cse4589_print_and_log("[LOGIN:END]\n");
    return sd;
}

void refresh_cmd(int sd) {
    if (is_logged_in == false) {
        refresh_error();
        return;
    }

    int size = 4;
    void* buf = malloc(size);
    *(int*) buf = TYPE_REFRESH;

    if (send(sd, buf, size, 0) == -1) {
        refresh_error();
        return;
    }

    cse4589_print_and_log("[REFRESH:SUCCESS]\n");
    cse4589_print_and_log("[REFRESH:END]\n");
}

void send_cmd(char* args, struct user_list* list, int sd) {
    if (is_logged_in == false) {
        send_error();
        return;
    }

    char ip[17];
    char msg[257];

    int ip_size = 0;
    if ((ip_size = validate_ip(args, ip)) < 0) {
        send_error();
        return;
    }

    if (user_list_find_by_ip(list, ip) == NULL) {
        send_error();
        return;
    }

    args += ip_size + 1; // skip space
    
    int msg_size = get_msg_size(args);
    for (int i = 0; i < msg_size; ++i) {
        msg[i] = args[i];
    }

    /* packing starts */
    int size = 12 + ip_size + msg_size;
    void* buf = malloc(size);
    *(int*) buf = TYPE_SEND;
    *(int*) (buf + 4) = ip_size;
    *(int*) (buf + 8) = msg_size;
    memcpy(buf + 12, ip, ip_size);
    memcpy(buf + 12 + ip_size, msg, msg_size);

    //printf("%d %d %d\n", *(int*) buf, *(int*) (buf+4), *(int*) (buf+8));
    //for (int i = 12; i < size; ++i) {
    //    printf("%c", *(char*) (buf+i));
    //}
    //printf("|\n");

    if (send(sd, buf, size, 0) == -1) {
        send_error();
        return;
    }

    cse4589_print_and_log("[SEND:SUCCESS]\n");
    cse4589_print_and_log("[SEND:END]\n");
}

void broadcast_cmd(char* args, int sd) {
    if (is_logged_in == false) {
        broadcast_error();
        return;
    }

    char msg[257];
    
    int msg_size = get_msg_size(args);
    for (int i = 0; i < msg_size; ++i) {
        msg[i] = args[i];
    }

    /* packing starts */
    int size = 8 + msg_size;
    void* buf = malloc(size);
    *(int*) buf = TYPE_BROADCAST;
    *(int*) (buf + 4) = msg_size;
    memcpy(buf + 8, msg, msg_size);

    //printf("%d %d\n", *(int*) buf, *(int*) (buf+4));
    //for (int i = 8; i < size; ++i) {
    //    printf("%c", *(char*) (buf+i));
    //}
    //printf("|\n");

    if (send(sd, buf, size, 0) == -1) {
        broadcast_error();
        return;
    }

    cse4589_print_and_log("[BROADCAST:SUCCESS]\n");
    cse4589_print_and_log("[BROADCAST:END]\n");
}

void block_cmd(char* args, struct user_list* list, int sd) {
    if (is_logged_in == false) {
        send_error();
        return;
    }

    char ip[17];

    int ip_size = 0;
    if ((ip_size = validate_ip(args, ip)) < 0) {
        printf("here1\n");
        block_error();
        return;
    }

    if (user_list_find_by_ip(list, ip) == NULL) {
        printf("here2\n");
        block_error();
        return;
    }

    /* packing starts */
    int size = 8 + ip_size;
    void* buf = malloc(size);
    *(int*) buf = TYPE_BLOCK;
    *(int*) (buf + 4) = ip_size;
    memcpy(buf + 8, ip, ip_size);

    //printf("%d %d\n", *(int*) buf, *(int*) (buf+4));
    //for (int i = 8; i < size; ++i) {
    //    printf("%c", *(char*) (buf+i));
    //}
    //printf("|\n");

    if (send(sd, buf, size, 0) == -1) {
        block_error();
        return;
    }

    //cse4589_print_and_log("[BLOCK:SUCCESS]\n");
    //cse4589_print_and_log("[BLOCK:END]\n");
}

void unblock_cmd(char* args, struct user_list* list, int sd) {
    if (is_logged_in == false) {
        send_error();
        return;
    }

    char ip[17];

    int ip_size = 0;
    if ((ip_size = validate_ip(args, ip)) < 0) {
        unblock_error();
        return;
    }

    if (user_list_find_by_ip(list, ip) == NULL) {
        unblock_error();
        return;
    }

    /* packing starts */
    int size = 8 + ip_size;
    void* buf = malloc(size);
    *(int*) buf = TYPE_UNBLOCK;
    *(int*) (buf + 4) = ip_size;
    memcpy(buf + 8, ip, ip_size);

    //printf("%d %d\n", *(int*) buf, *(int*) (buf+4));
    //for (int i = 8; i < size; ++i) {
    //    printf("%c", *(char*) (buf+i));
    //}
    //printf("|\n");

    if (send(sd, buf, size, 0) == -1) {
        unblock_error();
        return;
    }

    //cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
    //cse4589_print_and_log("[UNBLOCK:END]\n");
}

void exit_cmd(struct user_list* list, int sd) {
    if (is_logged_in == true) {
        struct user* cur = user_list_find_current_user(list);

        int size = 8 + sizeof(struct user_stripped);
        void* buf = malloc(size);

        *(int*) buf = TYPE_EXIT;
        memcpy(buf + 8, cur, sizeof(struct user_stripped)); 

        if (send(sd, buf, size, 0) == -1) {
            cse4589_print_and_log("[EXIT:ERROR]\n");
            cse4589_print_and_log("[EXIT:END]\n");
            return;
        }
    }

    exit(0);
}

static void refresh_error() {
    cse4589_print_and_log("[REFRESH:ERROR]\n");
    cse4589_print_and_log("[REFRESH:END]\n");
}
static void send_error() {
    cse4589_print_and_log("[SEND:ERROR]\n");
    cse4589_print_and_log("[SEND:END]\n");
}
static void broadcast_error() {
    cse4589_print_and_log("[BROADCAST:ERROR]\n");
    cse4589_print_and_log("[BROADCAST:END]\n");
}
static void block_error() {
    cse4589_print_and_log("[BLOCK:ERROR]\n");
    cse4589_print_and_log("[BLOCK:END]\n");
}
static void unblock_error() {
    cse4589_print_and_log("[UNBLOCK:ERROR]\n");
    cse4589_print_and_log("[UNBLOCK:END]\n");
}
static void login_error() {
    cse4589_print_and_log("[LOGIN:ERROR]\n");
    cse4589_print_and_log("[LOGIN:END]\n");
}

int validate_ip(char* cmd, char* output_ip) {
    /* ip extraction */
    int i;
    for(i = 0; cmd[i] != ' ' && cmd[i] != '\n' && i < 16; ++i) {
        output_ip[i] = cmd[i];
    }
    output_ip[i] = '\0';

    /* ip validation */
    struct sockaddr_in sockaddr;
    if (inet_pton(AF_INET, output_ip, &(sockaddr.sin_addr)) != 1)
        return -1;
    
    return i;
}

int validate_port(char* cmd, char* output_port) {
    /* port extraction */
    int i;
    for(i = 0; cmd[i] != '\n' && cmd[i] != ' ' && i < 5; ++i) {
        if (cmd[i] < 48 || cmd[i] > 57) {
            return -1;
        }
        output_port[i] = cmd[i];
    }
    output_port[i] = '\0';

    if (cmd[i] != '\n' && cmd[i] != ' ')
        return -1;

    /* port validation */
    if (atoi(output_port) < 0 || atoi(output_port) > 65535)
        return -1;

    return i;
}

int get_msg_size(char* cmd) {
    int last_index = 0;

    int i = 0;
    while (cmd[i] != '\0' && cmd[i] != '\n' && i < 256) {
        if (cmd[i] != ' ')
            last_index = i;
        ++i;
    }

    return last_index + 1; //exclusive
}
