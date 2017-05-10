/*
 * File:   test_mloc.c
 * Author: ljp
 *
 * Created on December 11, 2013, 12:08 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/*
 *
 */
int main(int argc, char** argv) {
    char *buffer = calloc(1, 50 * 1024 * sizeof (char));

    if( argc > 1 ) {
        strcpy(buffer,argv[1]);
    } else {
        strcpy(buffer,"[3000000001,20131211002222,17.959445,59.366545,0,0,0,0,2,3.88V,0\r\n3000000001,20131211002422,17.959445,59.366545,0,0,0,0,2,3.88V,0]");
    }

    int cnt=0;
    if ('[' == *buffer) {
        char locBuff[512] = {'\0'};
        char *lptr = locBuff;
        char *bptr = buffer+1;

        do {
            size_t numFields=1;
            _Bool finished = 0;
            do {
                *lptr++ = *bptr++;
                finished = '\0' == *bptr ||
                    ('\r' == *bptr && '\n' == *(bptr + 1)) ||
                    ']' == *bptr;
                if ( ',' == *bptr ) numFields++;

            } while (!finished && numFields<12);
            *lptr = '\0';
            if( numFields == 11 )  {
                // We know have one row of location data that we can send to the
                // the database for storage
                printf("LINE %d: %s\n", cnt++, locBuff);
                if ('\r' == *bptr && '\n' == *(bptr + 1)) {
                    bptr += 2;
                }
            } else {
                printf("Was expecting 11 datafields found %d\n",numFields);
            }
            lptr = locBuff;

        } while (*bptr && ']' != *bptr);
        if( ']' != *bptr ) {
            printf("Was expecting ']' at end of data\n");
        }

    } else {
        // Truncate data for logging
        buffer[80] = '\0';
        printf("Expected '[' in the beginning of location data. \"%s\"\n", buffer);
    }
    printf("\n");
    return (EXIT_SUCCESS);
}

