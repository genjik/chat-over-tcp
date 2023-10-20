#ifndef CMDS
#define CMDS

#include <stdbool.h>
#include "user.h"

// longest cmd(10) + ip(16) + msg(256) + NL(1) = 283
#define CMD_SIZE 283
#define BUFF_SIZE 2048

#define STDIN 0

#define TYPE_REFRESH   100
#define TYPE_SEND      101
#define TYPE_BROADCAST 102
#define TYPE_BLOCK     103
#define TYPE_UNBLOCK   104
#define TYPE_EXIT      105

char* get_my_ip();
void set_my_ip(char* new_ip);

char* get_my_port();
void set_my_port(char* new_port);

void author_cmd();
void ip_cmd();
void port_cmd();
void list_cmd(struct user_list* list, bool is_logged_in, bool is_client); 

int validate_ip(char* cmd, char* output_ip);
int validate_port(char* cmd, char* output_port);

#endif
