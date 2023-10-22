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
static void buffered_msg_response(struct user* user);
static void refresh_response(int type, struct user_list* user_list, int user_sd); 
static void send_response(void* in_buf, struct user_list* user_list, int user_sd); 
static void broadcast_response(void* in_buf, struct user_list* user_list, int user_sd); 
static void block_response(void* in_buf, struct user_list* list, int user_sd);
static void unblock_response(void* in_buf, struct user_list* list, int user_sd);
static void exit_response(struct user_list* user_list, int user_sd);

/* cmds */
static void stats_cmd(struct user_list* user_list);
static void blocked_cmd(char* cmd, struct user_list* user_list);

/* error functions */
static void blocked_error();

/* utilities */
static struct user* create_user(struct sockaddr_in* client_addr_in, int client_sd);
static char* get_status(bool is_logged_in);
static char* get_user_ip_by_socket(int user_sd);
static int get_user_port_by_socket(int user_sd);
static char* deserialize(void* in_buf, int metadata_offset);
static void* serialize(int ip_size, int msg_size, char* sender_ip, char* msg);
static void send_msg_to_user(struct user* target_user, void* out_buf, int out_buf_size, int ip_size, int msg_size);
static void block_unblock_success(int type, int user_sd);
static void block_unblock_failure(int type, int user_sd);

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
                    else if (strncmp(cmd, "BLOCKED ", 8) == 0) blocked_cmd(cmd+8, &user_list);

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
                            buffered_msg_response(old_user);
                        }
                        else {
                            struct user* new_user = create_user((struct sockaddr_in*) &client_addr, client_sd);
                            user_list_insert(&user_list, new_user);
                        }

                        refresh_response(TYPE_LOGIN, &user_list, client_sd); 
                    }
                }
                else {
                    char* buf = (char*) calloc(BUFF_SIZE, sizeof(char));

                    int recv_bytes;
                    if ((recv_bytes = recv(sd, buf, BUFF_SIZE, 0)) < 1) {
                        if (recv_bytes == 0) {
                            /* Find user by its socket and mark user as "logged_out" */
                            struct user* user = user_list_find_by_sd(&user_list, sd);
                            if (user != NULL)
                                user->is_logged_in = false;

                            close(sd);
                            FD_CLR(sd, &master_list);
                            continue;
                        }
                        else
                            perror("ERROR happened in receiving bytes");
                    }

                    int type = *(int*) buf;

                    if (type == TYPE_REFRESH) refresh_response(TYPE_REFRESH, &user_list, sd); 
                    else if (type == TYPE_SEND) send_response(buf+4, &user_list, sd);
                    else if (type == TYPE_BROADCAST) broadcast_response(buf+4, &user_list, sd);
                    else if (type == TYPE_BLOCK) block_response(buf+4, &user_list, sd);
                    else if (type == TYPE_UNBLOCK) unblock_response(buf+4, &user_list, sd);
                    else if (type == TYPE_EXIT) exit_response(&user_list, sd);

                    free(buf);
                }
            }
        }
    }
}

/* Server to client response functions */
static void refresh_response(int type, struct user_list* user_list, int user_sd) {
    int buf_size = 8 + sizeof(struct user_stripped) * user_list->size;
    void* buf = calloc(buf_size, sizeof(char));

    *(int*) buf = type;
    *(int*) (buf+4) = user_list->size;

    int i = 8;
    int actual_size = 0;
    for (struct user* cur = user_list->head->next; cur != user_list->tail; cur = cur->next) {
        if (cur->is_logged_in == false)
            continue;
       memcpy(buf+i, cur, sizeof(struct user_stripped)); 
       i += sizeof(struct user_stripped);
       ++actual_size;
    }
    int actual_buf_size = 8 + sizeof(struct user_stripped) * actual_size;

    int bytes_send;
    if ((bytes_send = send(user_sd, buf, actual_buf_size, 0)) == -1)
        perror("error: server send()");

    free(buf);
}

//static void login_response(int user_sd) {
//    void* out_buf = malloc(5);
//    *(int*) out_buf = TYPE_LOGIN;
//    *(char*) (out_buf+4) = 0; // success
//
//    int bytes_send;
//    if ((bytes_send = send(user_sd, out_buf, 5, 0)) == -1)
//        perror("error: server login_response()");
//
//    printf("login sent %d bytes\n", bytes_send);
//
//    free(out_buf);
//}

static void buffered_msg_response(struct user* user) {
    if (user->msg_buffer.size <= 0)
        return;

    void* out_buf = malloc(user->msg_buffer.buf_size);
    *(int*) out_buf = TYPE_SEND;
    *(int*) (out_buf+4) = user->msg_buffer.size;

    void* ptr = out_buf + 8;
    struct msg* cur = user->msg_buffer.head->next;
    while (cur != user->msg_buffer.tail) {
        memcpy(ptr, cur->data, cur->size);
        ptr += cur->size;
        cur = cur->next;
    }

    int bytes_send;
    if ((bytes_send = send(user->sd, out_buf, user->msg_buffer.buf_size, 0)) == -1)
        perror("error: server send()");
    user->num_msg_rcv += user->msg_buffer.size;

    free(out_buf);
    msg_buffer_free(&user->msg_buffer);
}

static void send_response(void* in_buf, struct user_list* user_list, int user_sd) {
    char* sender_ip = get_user_ip_by_socket(user_sd);

    int ip_size = strlen(sender_ip);
    int msg_size = *(int*) (in_buf+4);

    char* target_ip = deserialize(in_buf, 4); 
    char* msg = deserialize(in_buf + 4, ip_size);

    struct user* target_user = user_list_find_by_ip(user_list, target_ip);
    if (target_user == NULL) {
        cse4589_print_and_log("[RELAYED:ERROR]\n");
        cse4589_print_and_log("[RELAYED:END]\n");
        return;  
    }

    /* if sender is blocked by target, do NOT relay or buffer msg */
    if (user_blocked_list_find_by_ip(&target_user->blocked_list, sender_ip) != NULL) {
        return;
    }

    /* serialize data */
    int out_buf_size = 16 + ip_size + msg_size;
    void* out_buf = serialize(ip_size, msg_size, sender_ip, msg); 

    /* send or buffer msg for targer user */
    send_msg_to_user(target_user, out_buf, out_buf_size, ip_size, msg_size);

    ++user_list_find_by_ip(user_list, sender_ip)->num_msg_sent;

    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender_ip, target_ip, msg);
    cse4589_print_and_log("[RELAYED:END]\n");

    free(sender_ip);
    free(target_ip);
    free(msg);
}

static void broadcast_response(void* in_buf, struct user_list* user_list, int user_sd) {
    char* sender_ip = get_user_ip_by_socket(user_sd);
    char* msg = deserialize(in_buf, 0);

    int ip_size = strlen(sender_ip);
    int msg_size = *(int*) in_buf;

    struct user* target_user = user_list->head->next;
    while (target_user != user_list->tail) {
        /* do not send msg back to sender */
        if (strcmp(target_user->ip, sender_ip) == 0) {
            target_user = target_user->next;
            continue;
        }

        /* if cur user has blocked the sender -> do not send */
        if (user_blocked_list_find_by_ip(&target_user->blocked_list, sender_ip) != NULL) {
            target_user = target_user->next;
            continue;
        }

        /* serialize data */
        int out_buf_size = 16 + ip_size + msg_size;
        void* out_buf = serialize(ip_size, msg_size, sender_ip, msg); 

        /* send or buffer msg for targer user */
        send_msg_to_user(target_user, out_buf, out_buf_size, ip_size, msg_size);

        target_user = target_user->next;
    }

    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender_ip, "255.255.255.255", msg);
    cse4589_print_and_log("[RELAYED:END]\n");

    ++user_list_find_by_ip(user_list, sender_ip)->num_msg_sent;

    free(sender_ip);
    free(msg);
}

static void block_response(void* in_buf, struct user_list* user_list, int user_sd) {
    /* Retrieve info about user who sent BLOCK */
    char* this_user_ip = get_user_ip_by_socket(user_sd);
    struct user* this_user = user_list_find_by_ip(user_list, this_user_ip);

    /* Search for <client-ip> to be blocked */
    char* ip_to_block = deserialize(in_buf, 0);
    struct user* user_to_block = user_list_find_by_ip(user_list, ip_to_block);

    /* User to be blocked has EXITed the server. No further
       action is needed */
    if (user_to_block == NULL) {
        block_unblock_success(TYPE_BLOCK, user_sd);
        return;
    }

    struct user_blocked* found = user_blocked_list_find_by_ip(&this_user->blocked_list, ip_to_block);
    /* User to be blocked is already blocked -> error */
    if (found != NULL) {
        block_unblock_failure(TYPE_BLOCK, user_sd);
        return;
    }

    /* Add user to be blocked to blocked_list of this user */
    struct user_blocked* new_user_to_be_blocked = (struct user_blocked*) malloc(sizeof(struct user_blocked));
    new_user_to_be_blocked->user = user_to_block;
    user_blocked_list_insert(&this_user->blocked_list, new_user_to_be_blocked);

    block_unblock_success(TYPE_BLOCK, user_sd);

    free(this_user_ip);
    free(ip_to_block);
}

static void unblock_response(void* in_buf, struct user_list* user_list, int user_sd) {
    /* Retrieve info about user who sent BLOCK */
    char* this_user_ip = get_user_ip_by_socket(user_sd);
    struct user* this_user = user_list_find_by_ip(user_list, this_user_ip);

    /* Search for <client-ip> to be blocked */
    char* ip_to_unblock = deserialize(in_buf, 0);
    struct user* user_to_unblock = user_list_find_by_ip(user_list, ip_to_unblock);

    struct user_blocked* found = user_blocked_list_find_by_ip(&this_user->blocked_list, ip_to_unblock);
    /* User to be unblocked is not blocked by this user */
    if (found == NULL) {
        block_unblock_failure(TYPE_UNBLOCK, user_sd);
        return;
    }

    /* Remove user to be unblocked from this user's blocked_list */
    user_blocked_list_remove(&this_user->blocked_list, found);
    
    block_unblock_success(TYPE_UNBLOCK, user_sd);

    free(this_user_ip);
    free(ip_to_unblock);
}

static void exit_response(struct user_list* user_list, int user_sd) {
    char* ip = get_user_ip_by_socket(user_sd);
    int port = get_user_port_by_socket(user_sd);

    struct user* user = user_list_find_by_ip_and_port(user_list, ip, port);
    user_list_remove(user_list, user);
    free(ip);
}

static void block_unblock_success(int type, int user_sd) {
    void* out_buf = malloc(5);
    *(int*) out_buf = type;
    *(char*) (out_buf+4) = 0; //success

    int bytes_send;
    if ((bytes_send = send(user_sd, out_buf, 5, 0)) == -1)
        perror("error: server block/unblock()");
    free(out_buf);
}

static void block_unblock_failure(int type, int user_sd) {
    void* out_buf = malloc(5);
    *(int*) out_buf = type;
    *(char*) (out_buf+4) = 1; //success

    int bytes_send;
    if ((bytes_send = send(user_sd, out_buf, 5, 0)) == -1)
        perror("error: server block/unblock()");
    free(out_buf);
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
    user_blocked_list_init(&new_user->blocked_list);
    msg_buffer_init(&new_user->msg_buffer);

    return new_user;
}

static void stats_cmd(struct user_list* user_list) {
    cse4589_print_and_log("[STATISTICS:SUCCESS]\n");
    struct user* cur = user_list->head->next;
    int list_id = 1;
    while (cur != user_list->tail) {
        cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", list_id, cur->hostname, cur->num_msg_sent, cur->num_msg_rcv, get_status(cur->is_logged_in));
        cur = cur->next;
        ++list_id;
    }
    cse4589_print_and_log("[STATISTICS:END]\n");
}

static void blocked_cmd(char* cmd, struct user_list* user_list) {
    char ip[17];

    if (validate_ip(cmd, ip) < 0) {
        blocked_error();
        return;
    }

    struct user* user = user_list_find_by_ip(user_list, ip);
    if (user == NULL) {
        blocked_error();
        return;
    }

    cse4589_print_and_log("[BLOCKED:SUCCESS]\n");
    struct user_blocked* cur = user->blocked_list.head->next;
    int i = 1;
    while (cur != user->blocked_list.tail) {
        cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i, cur->user->hostname, cur->user->ip, cur->user->port);
        ++i;
        cur = cur->next;
    }
    cse4589_print_and_log("[BLOCKED:END]\n");
}

static void blocked_error() {
    cse4589_print_and_log("[BLOCKED:ERROR]\n");
    cse4589_print_and_log("[BLOCKED:END]\n");
}

static char* get_status(bool is_logged_in) {
    if (is_logged_in)
        return logged_in;
    return logged_out;
}

static char* get_user_ip_by_socket(int user_sd) {
    struct sockaddr_in user_addr_in;
    socklen_t user_addr_len = sizeof(struct sockaddr_in);

    if (getpeername(user_sd, (struct sockaddr*) &user_addr_in, &user_addr_len) == -1)
        perror("error: getpeername() in block_response");
    
    char* this_user_ip = (char*) malloc(18);
    if (inet_ntop(AF_INET, &user_addr_in.sin_addr, this_user_ip, 18) == NULL)
        perror("error: inet_ntop() in block_response");

    return this_user_ip;
}

static int get_user_port_by_socket(int user_sd) {
    struct sockaddr_in user_addr_in;
    socklen_t user_addr_len = sizeof(struct sockaddr_in);

    if (getpeername(user_sd, (struct sockaddr*) &user_addr_in, &user_addr_len) == -1)
        perror("error: getpeername() in block_response");
    

    return htons(user_addr_in.sin_port);
}

/* Deserializes according to my serialization format:
   |data size|data |
   |type:int |bytes| */
static char* deserialize(void* in_buf, int metadata_offset) {
    int data_size = *(int*) in_buf;
    char* data = (char*) malloc(data_size+1);

    memcpy(data, in_buf + 4 + metadata_offset, data_size);
    data[data_size] = '\0';

    return data;
}

static void* serialize(int ip_size, int msg_size, char* sender_ip, char* msg) {
    int out_buf_size = 16 + ip_size + msg_size;
    void* out_buf = calloc(out_buf_size, sizeof(char));
    *(int*) out_buf = TYPE_SEND;
    *(int*) (out_buf+4) = 1; // num of msg to send
    *(int*) (out_buf+8) = ip_size;
    *(int*) (out_buf+12) = msg_size;
    memcpy(out_buf+16, sender_ip, ip_size);
    memcpy(out_buf+16+ip_size, msg, msg_size);
    return out_buf;
}

static void send_msg_to_user(struct user* target_user, void* out_buf, int out_buf_size, int ip_size, int msg_size) {
    /* if target user == logged_in -> send msg without buffering */
    if (target_user->is_logged_in) {
        int bytes_send;
        if ((bytes_send = send(target_user->sd, out_buf, out_buf_size, 0)) == -1)
            perror("error: server send()");
        ++target_user->num_msg_rcv;
        free(out_buf);
    }
    else {
        struct msg* msg = (struct msg*) malloc(sizeof(struct msg)); 
        msg->data = out_buf+8;
        msg->size = 8 + ip_size + msg_size;
        //target_user->msg_buffer.buf_size += 8 + ip_size + msg_size;
        msg_buffer_insert(&target_user->msg_buffer, msg);
    }
}
