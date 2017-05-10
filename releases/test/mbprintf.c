#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>

#include "utf8.h"

int
xmb_printf(char *fmt, ...) {
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

  va_end(ap);
  int rc = printf("%ls",wbuff);
  free(wbuff);
  return rc;
}


int
xmb_fprintf(FILE *fp,char *fmt, ...) {
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

  va_end(ap);
  int rc = fprintf(fp,"%ls",wbuff);
  free(wbuff);
  return rc;

}


int
main(int argc, char *argv[]) {
  char *s1="String without mb";
  char *s2="String with mb chars ÅÄÖåäö";


  setlocale(LC_ALL,"");

  printf("Using mbprintf() : \n");
  printf("------------------------\n");
  xmb_printf("%-35s| Column2\n",s2);
  printf("%-35s| Column2\n\n",s1);

  printf("Using normal printf() : \n");
  printf("------------------------\n");
  printf("%-35s| Column2\n",s2);
  printf("%-35s| Column2\n\n",s1);

  exit(EXIT_SUCCESS);

}
