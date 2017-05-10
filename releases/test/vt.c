#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char*argv[]) {

  static char *bindirs[] = {"/usr/bin","/bin","/usr/local/bin","oll"};
  const size_t MAXBINDIRS = sizeof(bindirs)/sizeof(char *);
    
  printf("Size: %zu\n",MAXBINDIRS);

}
