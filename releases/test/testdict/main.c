
// We want the full POSIX and C99 standard
#define _GNU_SOURCE


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"


static size_t idx=0;

int
do_print(char *k, char *v) {
    
    char key[100];
    char val[100];
    
    snprintf(key,sizeof(key),"KEY_%04zu",idx);
    snprintf(val,sizeof(key),"VAL_%04zu",idx);
    idx++;
    if( strcmp(k,key) || strcmp(v,val) ) {
        printf("Failed comparison: (%s : %s)  !=  (%s : %s)\n",k,v,key,val);
        return 1;
    }
    //printf("%s : %s\n",k,v);
    return 0;
}

void
do_test(void) {
    
    dict_t d;
    
    d = new_dict();
    
    char key[100];
    char val[100];
    
    for( size_t i=0; i < 4000; i++) {
        snprintf(key,sizeof(key),"KEY_%04zu",i);
        snprintf(val,sizeof(key),"VAL_%04zu",i);
        if( -1 == add_dict(d,key,val) ) {
            printf("Error in add_dict()\n");
            return ;
        }
    }
    
    printf("idx=%zu, maxsize=%zu\n",d->idx,d->maxsize);
    
    map_dict(d,do_print);
    
    printf("\nChecking for the existance of keys:\n");
            
    printf("Key=KEY_0009 => %s\n",getval_dict(d,"KEY_0009"));
    printf("Key=KEY_1009 => %s\n",getval_dict(d,"KEY_1009"));
    printf("Key=KEY_3999 => %s\n",getval_dict(d,"KEY_3999"));
    printf("Key=KEY_4999 => %s\n",getval_dict(d,"KEY_4999"));
    
    printf("Test Done.\n");
}

int main(void) {
    
    do_test();
    
    return 0;
}




