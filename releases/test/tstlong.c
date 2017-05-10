/*
 * File:   tstlong.c
 * Author: ljp
 *
 * Created on January 12, 2014, 2:06 PM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>


long
xatol(const char * const str) {
    char *endptr;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) || (errno != 0 && val == 0)) {
       val = 0 ;
    }

    if (endptr == str) {
       val = 0 ;
    }

    return val;
}

/*
 *
 */
int main(int argc, char** argv) {

    printf("devid=%ld\n",atol("3000000001"));

    return (EXIT_SUCCESS);
}

