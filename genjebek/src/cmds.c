#include "../include/cmds.h"
#include "../include/user.h"
#include "../include/logger.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdbool.h>

static char* my_ip;
static char* my_port;

char* get_my_ip() {
    return my_ip;
}
void set_my_ip(char* new_ip) {
    my_ip = new_ip;
}

char* get_my_port() {
    return my_port;
}
void set_my_port(char* new_port) {
    my_port = new_port;
}

void author_cmd() {
    cse4589_print_and_log("[AUTHOR:SUCCESS]\n");
    cse4589_print_and_log("I, genjebek, have read and understood the course academic integrity policy.\n");
    cse4589_print_and_log("[AUTHOR:END]\n");
}

void ip_cmd() {
    if (get_my_ip() != NULL) {
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
    if (get_my_port() != NULL) {
        cse4589_print_and_log("[PORT:SUCCESS]\n");
        cse4589_print_and_log("PORT:%d\n", atoi(my_port));
        cse4589_print_and_log("[PORT:END]\n");
    }
    else {
        cse4589_print_and_log("[PORT:ERROR]\n");
        cse4589_print_and_log("[PORT:END]\n"); 
    }
}

void list_cmd(struct user_list* list, bool is_logged_in, bool is_client) {
    if (is_client == true && is_logged_in == false) {
        cse4589_print_and_log("[LIST:ERROR]\n");
        cse4589_print_and_log("[LIST:END]\n"); 
        return;
    }

    cse4589_print_and_log("[LIST:SUCCESS]\n");

    struct user* cur = list->head->next;
    int i = 1;
    while (cur != list->tail) {
        if (is_client == false && cur->is_logged_in == false) {
            cur = cur->next;
            continue;
        }
       cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i, cur->hostname, cur->ip, cur->port);
       ++i;
       cur = cur->next;
    }

    cse4589_print_and_log("[LIST:END]\n");
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
