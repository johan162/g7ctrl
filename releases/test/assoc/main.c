/* 
 * File:   main.c
 * Author: ljp
 *
 * Created on September 1, 2015, 8:55 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assocarray.h"

/*
 * 
 */
int main(int argc, char** argv) {

    struct assoc_array_t *a=assoc_new(8);
    
    char buff_name[16];
    char buff_val[16];
    for( size_t i=0; i<8; i++) {
        snprintf(buff_name,sizeof(buff_name),"name-%03zu",i);
        snprintf(buff_val,sizeof(buff_val),"val-%03zu",i);
        assoc_put(a,buff_name,buff_val);
    }
    
    const size_t BUFF_OUT_SIZE=10000;
    char *buff_out=calloc(BUFF_OUT_SIZE,sizeof(char));
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
    
    
    printf("Removing element 3 \n");
    assoc_del(a,"name-002");
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
    
    printf("Removing element 1 \n");
    assoc_del(a,"name-000");
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);

    printf("Removing last \n");
    assoc_del(a,"name-007");
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);

    printf("Adding xerxes\n");
    assoc_put(a,"xerxes","val-xerxes");
    
    printf("Adding aaron\n");
    assoc_put(a,"aaron","val-aaron");
    
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
    
    printf("Sorting\n");
    assoc_sort(a);
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
    
    printf("Updating aaoron\n");
    assoc_update(a,"aaron","new-aaron");
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
   
    printf("Updating xerxes\n");
    assoc_update(a,"xerxes","new-xerxes");
    assoc_to_json(a,buff_out,BUFF_OUT_SIZE);
    printf("%s\n",buff_out);
    
    
    assoc_destroy(a);
    a=NULL;
    
    return (EXIT_SUCCESS);
}

