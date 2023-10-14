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

#include "../include/global.h"
#include "../include/logger.h"

void author_cmd();
void exit_cmd();

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

    /* If client: */
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
        else if (strcmp(cmd, "EXIT\n") == 0) {
          exit_cmd();
        }

        //printf("msg: %s, size: %d\n", cmd, strlen(cmd));
      }
    }
    /* If server: */
    else if (argv[1][0] == 's') {

    }
    else {
      printf("First argument must either be: c or s\n");
      exit(1);
    }

	return 0;
}

void author_cmd() {
  cse4589_print_and_log("[AUTHOR:SUCCESS]\n");
  cse4589_print_and_log("I, genjebek, have read and understood the course academic integrity policy.\n");
  cse4589_print_and_log("[AUTHOR:END]\n");
}

void exit_cmd() {
  exit(0);
}
