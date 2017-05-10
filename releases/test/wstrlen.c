#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>


main() {

  char *s = "Åker\0\0\0\0\0\0\0\0";
  wchar_t d[128];

  mbstate_t t;
  memset (&t, '\0', sizeof (t));

  printf("strlen=%zu, mblen=%d, mbstowcs=%u\n",strlen(s),mblen(s,128),mbsrtowcs(d,&s,127,&t));

}
