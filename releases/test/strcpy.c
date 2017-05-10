#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
kalle(int *a) {
	*a = 99;
	return 0;
}

void
main(void) {

  char buf1[2],buf2[2];
  char buffer[16];

  buf1[0] = '1';
  buf1[1] = '2';

  buf2[0] = '3';
  buf2[1] = '\0';

  strcpy(buffer,"abcdefghijklmnopqrstuvwxyz");
  strncpy(buffer,buf1,1);

	kalle((int *)999999);

  printf("c1=%c, c2=%c, c3=%c\n\n",buffer[0],buffer[1],buffer[2]);



}
