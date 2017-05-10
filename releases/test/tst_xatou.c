#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


main(void) {

  char *devid = "3000000001";
  unsigned di;
  char *endptr;

  di = strtol(devid,&endptr,10);

  printf("Devid=%s IS di=%ul\n",devid,di);

}
