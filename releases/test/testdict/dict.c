/* =========================================================================
 * File:        DICT.C
 * Description: Functions to do dictionary substitution in buffers and files
 * Author:      Johan Persson (johan162@gmail.com)
 *
 * Copyright (C) 2013-2015  Johan Persson
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>
 * =========================================================================
 */

// We want the full POSIX and C99 standard
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "dict.h"

#define MIN(a,b) (a) < (b) ? (a) : (b)

/**
 * Replace all occurrences of each key surrounded by "[]" with its value in
 * the buffer pointed to by buffer.
 * @param dict Dictionary to use
 * @param buffer Buffer to do replacement
 * @param maxlen Maximum size of buffer
 * @return 0 on success, -1 on failure
 */
int
replace_dict_in_buf(dict_t dict, char *buffer, size_t maxlen) {
    
    size_t const N = strlen(buffer) + MAX_KEYPAIR_VAL_SIZE*dict->idx;
    char match[256];
    char *wbuff = calloc(N, sizeof(char));
    char *pwbuff = wbuff;
    char *pbuff = buffer;
    size_t n = 0;

    while (n < N && *pbuff) {
        if (*pbuff == '[') {
            char *ppbuff = pbuff;
            size_t nn = 0;
            ++ppbuff;
            while (nn < sizeof (match) - 1 && *ppbuff && *ppbuff != ']') {
                match[nn++] = *ppbuff++;
            }
            match[nn] = '\0';
            if (*ppbuff == ']') {
                
                size_t i = 0;
                while (i < dict->idx && strcasecmp(match, dict->tuple[i].key)) {
                    i++;
                }
                if (i < dict->idx) {
                    // We got a match
                    nn = MIN(MAX_KEYPAIR_VAL_SIZE, strlen(dict->tuple[i].val));
                    size_t j = 0;
                    while (nn > 0) {
                        *pwbuff++ = dict->tuple[i].val[j++];
                        nn--;
                    }
                    pbuff = ++ppbuff;
                } else {
                    // No match
                    *pwbuff++ = *pbuff++;
                    n++;
                }

            } else {
                *pwbuff++ = *pbuff++;
                n++;
            }
        } else {
            *pwbuff++ = *pbuff++;
            n++;
        }
    }

    if (maxlen > strlen(wbuff)) {
        strcpy(buffer, wbuff);
        free(wbuff);
        return 0;
    } else {
        free(wbuff);
        return -1;
    }
}

/**
 * Read a template from a file and replace all keywords with the key values and store
 * the result in the buffer pointed to by buffer. It is the calling functions responsibility
 * to free the buffer after usage.
 * @param dict Dictionary to use
 * @param filename Template file to read
 * @param[out] buffer The resulting replaced text
 * @return 0 on success, -1 on failure
 */
int
replace_dict_in_file(dict_t dict, char *filename, char **buffer) {

    FILE *fp;
    *buffer = NULL;
    if ((fp = fopen(filename, "rb")) == NULL) {
        return -1;
    }
    int fd = fileno(fp);
    struct stat buf;
    fstat(fd, &buf);
    size_t const N = buf.st_size + MAX_KEYPAIR_VAL_SIZE * dict->idx + 1;
    *buffer = calloc(N, sizeof(char));
    size_t readsize = fread(*buffer, sizeof (char), buf.st_size, fp);
    if (readsize != (size_t) buf.st_size) {
        fclose(fp);
        free(*buffer);
        *buffer = NULL;
        return -1;
    }
    fclose(fp);
    return replace_dict_in_buf(dict, *buffer, N);
}

/**
 * Create a new list of key-pairs of the specified size
 * @return Return pointer to beginning of key-pair list
 */
dict_t
new_dict(void) {
    dict_t ptr = calloc(1, sizeof(struct dict));
    ptr->tuple = calloc(DICTIONARY_INIITIAL_SIZE, sizeof(struct dict_tuple));
    ptr->maxsize = DICTIONARY_INIITIAL_SIZE;
    return ptr;
}

/**
 * Add a key/val - pair to the list of pairs
 * @param dict Dictionary to add tuple to
 * @param key Key to add
 * @param val Val to add
 * @return 0 on success, -1 when list is full or key already exists
 */
int
add_dict(dict_t dict, char *key, char *val) {
    
    // First check that this key doesn't exist before
    if( getval_dict(dict,key) ) {
        return -1;
    }
    
    if (dict->idx >= dict->maxsize) {
        // Need to grow. We don't use realloc since we want the 
        // new memory to be zero initialized and this way is conceptually
        // simpler than doing a memset() with indexes.
        
        // Double the size each time
        size_t newsize=2*dict->maxsize;
        
        struct dict_tuple *newptr = calloc(newsize,sizeof(struct dict_tuple));
        if( NULL==newptr ) {
            return -1;
        }       
        memcpy(newptr,dict->tuple,dict->maxsize*sizeof(struct dict_tuple));
        free(dict->tuple);
        dict->tuple = newptr;
        dict->maxsize = newsize;
    } 
    
    dict->tuple[dict->idx].key = strdup(key);
    dict->tuple[dict->idx].val = strdup(val);
    dict->idx++;
    return 0;
    
}

/**
 * Free the dictionary and return all memory used
 * @param dict Dictionary to use
 * @return 0 on success, -1 if keys are an invalid pointer
 */
int
free_dict(dict_t dict) {
    if (dict == NULL)
        return -1;
    if( dict->tuple ) {
        for (size_t i = 0; i < dict->maxsize; ++i) {
            if (dict->tuple[i].key) {
                free(dict->tuple[i].key);
            }
            if (dict->tuple[i].val) {
                free(dict->tuple[i].val);
            }
        }
        free(dict->tuple);
    }
    free(dict);
    return 0;
}

/**
 * Map a specified function over all values in dict until either
 * all values has been mapped or the mapping function returns <> 0
 * 
 * @param dict Dictionary
 * @param f Function to map over dictionary
 * @return 0 on success, value returned from mapping function if <> 0
 */
int
map_dict(dict_t dict, dict_map_t f) {
    for(size_t i=0; i < dict->idx; i++) {
        if ( f(dict->tuple[i].key, dict->tuple[i].val) )
            return 1;;
    }
    return 0;
}

/**
 * @param dict Dictionary
 * @param key The key to search for
 * @return A string pointer to the value on success, NULL of the key doesn't exist
 */
char *
getval_dict(dict_t dict, char *key) {
    for(size_t i=0; i < dict->idx; i++) {
        if( 0==strcmp(key,dict->tuple[i].key) ) {
            return dict->tuple[i].val;
        }
    }
    return NULL;
}


/* EOF */
