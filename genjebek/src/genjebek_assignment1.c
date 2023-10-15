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

#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/logger.h"

void get_ip();
void author_cmd();
void exit_cmd();
void ip_cmd();
void port_cmd();

char* my_ip;
char* my_port;

/**
 * main function
 *
 * @param  argc Number of arguments
 * @param  argv The argument list
 * @return 0 EXIT_SUCCESS
 */
int main(int argc, char **argv)
{
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
      while (true) {
        printf("\n[PA1-Client@CSE489]$ ");
        fflush(stdout);

        char* cmd = (char*) malloc(127);
        if (fgets(cmd, 126, stdin) == NULL) {
          exit(-1);
        }

        if (strcmp(cmd, "AUTHOR\n") == 0) {
          author_cmd();
        }
        else if (strcmp(cmd, "IP\n") == 0) {
          ip_cmd();
        }
        else if (strcmp(cmd, "PORT\n") == 0) {
          port_cmd();
        }
        else if (strcmp(cmd, "LIST\n") == 0) {
          
        }
        else if (strcmp(cmd, "LOGIN\n") == 0) {
          
        }
        else if (strcmp(cmd, "REFRESH\n") == 0) {
          
        }
        else if (strcmp(cmd, "LOGOUT\n") == 0) {
          
        }
        else if (strcmp(cmd, "EXIT\n") == 0) {
          exit_cmd();
        }
      }
    }
    /* Server */
    else if (argv[1][0] == 's') {
      while (true) {
        printf("\n[PA1-Client@CSE489]$ ");
        fflush(stdout);

        char* cmd = (char*) malloc(127);
        if (fgets(cmd, 126, stdin) == NULL)
          exit(-1);

        if (strcmp(cmd, "AUTHOR\n") == 0)
          author_cmd();
        else if (strcmp(cmd, "IP\n") == 0)
          ip_cmd();
        else if (strcmp(cmd, "PORT\n") == 0)
          port_cmd();
        else if (strcmp(cmd, "LIST\n") == 0) {}
        else if (strcmp(cmd, "STATISTICS\n") == 0) {}
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
