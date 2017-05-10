#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include "utf8.h"

int
mbprintf(char *fmt, ...) {
  const int WBLEN = 20000;
  wchar_t *wbuff = calloc(WBLEN,sizeof(wchar_t));
  va_list ap;
  va_start(ap,fmt);

  const size_t wflen =  mbstowcs(NULL,fmt,0)+1;
  wchar_t wf[wflen];
  mbstowcs(wf, fmt, wflen);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"

  if( WBLEN==vswprintf(wbuff, WBLEN, wf, ap) ) {
     return -1;
  }

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

  const size_t mblen = wcstombs(NULL,wbuff,0)+1;
  char buff[mblen];
  wcstombs(buff,wbuff,mblen);

  free(wbuff);
  va_end(ap);
  printf("%s",buff);
  return 0;
}


int
main(int argc, char *argv[]) {

  setlocale(LC_ALL,"");

  char *s1="String without mb";
  char *s2="String with .ÅÄÖåäö. mb chars";


  mbprintf("%-40s|Olle\n",s2);
  printf("%-40s|Olle\n",s1);

  exit(0);


  printf("%-40s|Olle\n",s1);
  printf("%-40s|Olle\n",s2);
  printf("\n");
  u8_printf("%-40s|Olle\n",s1);
  u8_printf("%-40s|Olle\n",s2);
  printf("\n");

  //size_t mbstowcs(wchar_t *dest, const char *src, size_t n);
  wchar_t ws1[128],wbuff1[128];
  wchar_t ws2[128],wbuff2[128];
  char buff1[128],buff2[128];
  mbstowcs(ws1, s1, 127);
  mbstowcs(ws2, s2, 127);

  printf("Using wide chars\n");
  swprintf(wbuff1,128,L"%-40ls|Olle\n",ws1);
  swprintf(wbuff2,128,L"%-40ls|Olle\n",ws2);

  printf("After new conversion\n");
  wcstombs(buff1,wbuff1,sizeof(buff1));
  wcstombs(buff2,wbuff2,sizeof(buff2));
  printf("%s\n",buff1);
  printf("%s\n",buff2);

  printf("\n");
  printf("Trying mb with wprintf\n");
  wprintf(wbuff1,128,L"%-40s|Olle\n",s1);
  wprintf(wbuff2,128,L"%-40s|Olle\n",s2);


  printf("\n");

}
