
/*
 * run_test.c - Read a set of Speech Dispatcher commands and try them
 *
 * Copyright (C) 2001, 2002, 2003 Brailcom, o.p.s.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this package; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * $Id: run_test.c,v 1.14 2008-02-08 10:01:08 hanke Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
 
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "def.h"

#define FATAL(msg) { printf(msg"\n"); exit(1); }

int sockk;

#ifdef __SUNPRO_C
/* Added by Willie Walker - strcasestr is a gcc-ism
 */
char *strcasestr (const char *a, const char *b)
{
       size_t l;
       char f[3];

       snprintf (f, sizeof(f), "%c%c", tolower(*b), toupper(*b));
       for (l = strcspn(a, f); l != strlen(a); l += strcspn(a+l+1, f) + 1)
               if (strncasecmp(a+l, b, strlen(b)) == 0)
                       return (a + l);
       return NULL;
}
#endif /* __SUNPRO_C */

char*
send_data(int fd, char *message, int wfr)
{
	char *reply;
	int bytes;

	/* TODO: 1000?! */
	reply = (char*) malloc(sizeof(char) * 1000);
   
	/* write message to the socket */
	write(fd, message, strlen(message));

	/* read reply to the buffer */
	if (wfr == 1){
		bytes = read(fd, reply, 1000);
		/* print server reply to as a string */
		reply[bytes] = 0; 
	}else{
		return "";
	} 
            
	return reply;
}

void
wait_for(int fd, char* event)
{
    char * reply;
    int bytes;

    printf("       Waiting for: |%s|\n", event);
    reply = (char*) malloc(sizeof(char) * 1000);
    reply[0] = 0;
    while (0 == strcasestr(reply, event)) {
        bytes = read(fd, reply, 1000);
        if (bytes > 0) {
            reply[bytes] = 0;
            printf("       < %s\n", reply);
            fflush(NULL);
        }
    }
    free(reply);
    printf("       Continuing.\n", reply);
    fflush(NULL);
}

int 
init(char* client_name, char* conn_name)
{
  int sockfd;
  struct sockaddr_in address;
  char *env_port;
  int port;

  env_port = getenv("SPEECHD_PORT");
  if (env_port != NULL)
      port = strtol(env_port, NULL, 10);
  else
      port = SPEECHD_DEFAULT_PORT;

  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(port);
  address.sin_family = AF_INET;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  /* connect to server */
  if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
      return 0;

  return sockfd;
}

int
main(int argc, char* argv[])
{
    char* line;
    char* command;
    char* reply;
    int i;
    char *ret;
    FILE* test_file = NULL;
    int delays = 1;
    int indent = 0;
	
    if(argc < 2){
        printf("No test script specified!\n");
        exit(1);
    }
	
    if (!strcmp(argv[1],"stdin")){
        test_file = stdin;
    }else{
        test_file = fopen(argv[1], "r");
        if(test_file == NULL) FATAL("Test file doesn't exist");
    }	

    if(argc == 3){
        if(!strcmp(argv[2],"fast")) delays = 0;
        else{
            printf("Unrecognized parameter\n");
            exit(1);
        }
    }
	
    printf("Start of the test.\n");
    printf("==================\n\n");
	
    line = malloc(1024 * sizeof(char));
    reply = malloc(4096 * sizeof(char));

    sockk = init("run_test","user_test");
    if(sockk == 0) FATAL("Can't connect to Speech Dispatcher");

    assert(line != 0);

    while(1){
        ret = fgets(line, 1024, test_file);
        if (ret == NULL) break;
        if (strlen(line) <= 1){
            printf("\n");
            continue;
        }

        if (line[0] == '@'){
            command = (char*) strtok(line, "@\r\n");
            if (command == NULL)
                printf("\n");
            else
                printf("  %s\n", command);
            continue;
        }

        if (line[0] == '!'){
            command = (char*) strtok(line, "!\r\n");
            strcat(command,"\r\n");
            if (command == NULL) continue;

            printf("     >> %s", command);
            fflush(NULL);
            reply = send_data(sockk, command, 1);
            printf("     < %s", reply);
            fflush(NULL);
            continue;
        }
        
        if(line[0] == '.'){
            reply = send_data(sockk, "\r\n.\r\n", 1);
            printf("       < %s", reply);
            continue;
        }

        if(line[0] == '+'){
            command = (char*) strtok(&(line[1]), "+\r\n");
            wait_for(sockk, command);
            continue;
        }

        if(line[0] == '$'){
            if (delays){
                command = (char*) strtok(&(line[1]), "$\r\n");
                sleep(atoi(command));
            }
            continue;
        }

        if(line[0] == '^'){
            if (delays){
                command = (char*) strtok(&(line[1]), "$\r\n");
                usleep(atol(command));
            }
            continue;
        }

        if(line[0] == '~'){
            command = (char*) strtok(line, "~\r\n");
            indent = atoi(command);
            continue;
        }
				
		
        if(line[0] == '?'){
            getc(stdin);
            continue;
        }
        
        if(line[0] == '*'){
            system("clear");
            for (i=0; i<=indent - 1; i++){
                printf("\n");
            }				
            continue;
        }
		
        send_data(sockk, line, 0);            
        printf("     >> %s", line);
    }


    close(sockk);

    printf("\n==================\n");
    printf("End of the test.\n");
    exit(0);
}
