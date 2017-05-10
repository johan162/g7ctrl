#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>


int
main(int argc, char **argv) {


  for(int i=0; i < 10; i++) {

    printf("[%02d%%]",10*i);
    fflush(stdout);
    sleep(2);
    printf("\033[2K\r");
    fflush(stdout);

  }

  exit(EXIT_SUCCESS);

}
