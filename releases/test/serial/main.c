/* =========================================================================
 * File:        MAIN.C
* Description:  Testmodule for serial communication
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Ids$
 *
 * Copyright (C) 2013-2015  Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */


// We want the full POSIX and C99 standard
#define _GNU_SOURCE

// Standard UNIX includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "serial.h"


int
send_cmd(int sfd, char *cmd, size_t maxlen, char *res) {
    int ret = serial_write(sfd,strlen(cmd),cmd);

    if( -1 == ret ) {
        fprintf(stderr,"Cannot write to port");
        return -1;
    }

    ret = serial_read(sfd,maxlen,res);

    if( -1 == ret ) {
        fprintf(stderr,"Cannot read from port");
        return -1;
    }
    return 0;
}

int
main(void) {

    int sfd = serial_open(0,57600);
    if( sfd < 0 ) {
        fprintf(stderr,"Cannot open port");
        exit(1);
    }
    char buffer[32000];

    //    char *cmd1 = "$WP+GETLOCATION=0000,?\r\n";
    //char *cmd2 = "$WP+SETTZ=0000,+,01,00\n\n";
    //    char *cmd2 = "$WP+VER=0000\r\n";
    //    char *cmd3 = "$WP+COMMTYPE=0000,?\r\n";
    //    char *cmd4 = "$WP+TRACK=0000,?\r\n";
    // char *cmd5 = "$WP+SLEEP=0000,?\r\n";
char *cmd6 = "$WP+GETLOCATION=0000,?\r\n";
//char *cmd7 = "$WP+TRACK=0000,0\r\n";


    send_cmd(sfd, cmd6, 32000, buffer);
    printf("> %s",buffer);

    //    send_cmd(sfd, cmd7, 32000, buffer);
    //    printf("> %s",buffer);

    const size_t n=1024;
    char *l=calloc(n,sizeof(char));

    while(1) {
      getline(&l,&n,stdin);
      l[strlen(l)-2] = '\r';
      l[strlen(l)-1] = '\n';
      l[strlen(l)] = '\0';
      send_cmd(sfd, l , 32000, buffer);
      printf("> %s\n",buffer);
    }

    exit(0);

    //    send_cmd(sfd, cmd2, 32000, buffer);
    //    printf("DATA: %s",buffer);
    /*
    send_cmd(sfd, cmd2, 32000, buffer);
    printf("%s\n> %s",cmd2,buffer);

    send_cmd(sfd, cmd3, 32000, buffer);
    printf("%s\n> %s",cmd3,buffer);

    send_cmd(sfd, cmd4, 32000, buffer);
    printf("%s\n> %s",cmd4,buffer);

    send_cmd(sfd, cmd5, 32000, buffer);
    printf("%s\n> %s",cmd5,buffer);

    printf("Bye.\n");

    exit(0);
    */
}