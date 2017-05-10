#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <time.h>

// At least 105 ms betwen each API call. This will result in
// just below 10 queries per second on average
#define GOOGLE_API_RLIMIT 105


static unsigned long last_api_call=0;
static unsigned long rlimit_ms=105;

int 
mtime(unsigned long *t) {
  struct timespec timespec;
#ifndef __APPLE__
  if( clock_gettime(CLOCK_MONOTONIC_RAW,&timespec) ) {
    return -1;
  }
#else 
  if( clock_gettime(CLOCK_MONOTONIC,&timespec) ) {
    return -1;
  }
#endif
  *t = (unsigned long)(timespec.tv_sec*1000+timespec.tv_nsec/1000000);
  return 0;
}


int
rate_limit_init(unsigned long limit) {
  rlimit_ms = limit;
  return mtime(&last_api_call);
}

void 
rate_limit(void) {

  unsigned long t1;
  mtime(&t1);
  const unsigned long diff = t1-last_api_call;
  if( diff < rlimit_ms ) {
    usleep((rlimit_ms-diff)*1000); 
  }
  (void)mtime(&last_api_call);
}


void 
api_call(void) {
  unsigned long t;
  mtime(&t);
  printf("# %lu ms (%lu)\n",t, t/1000);
}

void
test_rlimit(void) {
  /*
  if( rate_limit_init(101) ) {
    fprintf(stderr,"Cannot iniialize rate limit.");
    return;
    }^*/
  const int num=1000;
  time_t t1 = time(NULL);
  for(int i=0; i < num; ++i ) {
    rate_limit();
    api_call();
  }
  unsigned long diff = time(NULL)-t1;
  printf("Running time: %lu:%02lu\n",diff/60,diff%60);
  printf("Average: %.2f QPS\n",(float)num/diff);
}



int
main(int argc, char **argv) {

  struct timespec timespec;
  

  if( clock_gettime(CLOCK_MONOTONIC_RAW,&timespec) ) {
    fprintf(stderr, "Timer error ( %s : %d )\n",strerror(errno),errno);
    exit(EXIT_FAILURE);
  }

  printf("Time: %lu s : %lu ns [%lu ms]\n",
	 (unsigned long)timespec.tv_sec, 
	 (unsigned long)timespec.tv_nsec,
	 (unsigned long)(timespec.tv_sec*1000+timespec.tv_nsec/1000000) );

  test_rlimit();

  exit(EXIT_SUCCESS);

}


