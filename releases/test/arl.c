/*
 * File:   arl.c
 * Author: ljp
 *
 * Created on January 6, 2014, 4:50 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

void
rl_linefinished(char *line) {
    printf("Line completed: \"%s\"\n",line);
}


/*
 *
 */
int main(int argc, char** argv) {

    int rc;
    fd_set read_fdset;
    struct timeval timeout;
    const mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

    int lfd = open("/tmp/rlog.log",O_CREAT|O_WRONLY|O_TRUNC,mode);

    rl_callback_handler_install("% ",rl_linefinished);

    do {
        FD_ZERO(&read_fdset);
        FD_SET(STDIN_FILENO, &read_fdset);

        timerclear(&timeout);
        timeout.tv_sec = 2;

        rc = select(STDIN_FILENO + 1, &read_fdset, NULL, NULL, &timeout);

        if( 0 == rc ) {

            write(lfd,"#\n",1);
            fsync(lfd);

        } else {
             rl_callback_read_char();
        }

    } while( 1 );

    exit(EXIT_SUCCESS);
}

