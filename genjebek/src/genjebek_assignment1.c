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
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "../include/global.h"
#include "../include/logger.h"

#include "../include/user.h"
#include "../include/client.h"
#include "../include/server.h"
#include "../include/cmds.h"

static void init_ip();

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

    /* Gets and inits global variables for ip + port */
    init_ip();
    set_my_port(argv[2]);
    
    if (argv[1][0] == 'c') client_start();
    else if (argv[1][0] == 's') server_start(argv[2]);
    else {
        printf("First argument must either be: c or s\n");
        exit(1);
    }

    return 0;
}

static void init_ip() {
    if (get_my_ip() != NULL)
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

    set_my_ip((char*) malloc(16));
    struct sockaddr_in* sock_addr_in = (struct sockaddr_in*) res->ai_addr;
    if (inet_ntop(res->ai_family, &sock_addr_in->sin_addr, get_my_ip(), 16) == NULL) {
        printf("Failed to inet_ntop() in get_ip()\n");
        exit(1);
    }
}
