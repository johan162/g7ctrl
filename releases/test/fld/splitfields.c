#define GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_RESP_FIELDS 12
#define MAX_FIELD_SIZE 32

struct splitfields {
	size_t nf;
	char fld[MAX_RESP_FIELDS][MAX_FIELD_SIZE];
};

size_t
strlen(char *buffer) {
  size_t n=0;
  while(*buffer++) ++n;
  return n;
}

int
xstrsplitfields(char *buffer, char divider, size_t maxf, struct splitfields *sfields) {

	//xstrtrim_crlf(buffer);
	sfields->nf = 0;
	size_t len = strlen(buffer);
	if( len >= 1024 || 0 == len ) return -1;

	char *bptr = buffer;
	char *ptr =NULL;
	while( len > 0 ) {
	  ptr = sfields->fld[sfields->nf];
	  while( *bptr != divider && len > 0 ) {
	    if( ' ' == *bptr ) *bptr++;
	    else *ptr++ = *bptr++;
	    --len;
	  }
	  if( *bptr == divider ) {
	    *ptr = '\0';
	    bptr++;
	    --len;
	  }
	  ++sfields->nf;
	}
	*ptr = '\0';
	return 0;
}

int
main(int argc, char *argv[]) {

	struct splitfields sf;
	if( argc == 2 ) {
	  int rc = xstrsplitfields(argv[1],',',12,&sf);
	  if( rc ) {
	    fprintf(stderr,"Invalid or empty input string (\"%s\")\n",argv[1]);
	  }
	  for(int i=0; i < sf.nf; i++) {
	    printf( "\"%s\"\n", sf.fld[i] );
	  }
	} else {
	  fprintf(stderr,"Usage: %s <arg>\n",basename(argv[0]));
	}
	return 0;

}