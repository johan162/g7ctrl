/* =========================================================================
 * File:        RKEY.C
 * Description: Functions to do key/val pair substitution in buffers
 * Author:      Johan Persson (johan162@gmail.com)
 * SVN:         $Id: rkey.c 644 2015-01-10 10:18:27Z ljp $
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

#include "rkey.h"

#define MIN(a,b) (a) < (b) ? (a) : (b)

/**
 * Replace all occurrences of each key sourrounded by "[]" with its value in
 * the buffer pointed to by buffer.
 * @param buffer
 * @param maxlen
 * @param keys
 * @param nkeys
 * @return 0 on success, -1 on failure
 */
int
replace_keywords(char *buffer, size_t maxlen, struct keypairs keys[], size_t nkeys) {

    size_t const N = strlen(buffer) + MAX_KEYPAIR_VAL_SIZE*nkeys;
    char match[256];
    char *wbuff = calloc(1, N);
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
                while (i < nkeys && strcasecmp(match, keys[i].key)) {
                    i++;
                }
                if (i < nkeys) {
                    // We got a match
                    nn = MIN(MAX_KEYPAIR_VAL_SIZE, strlen(keys[i].val));
                    size_t j = 0;
                    while (nn > 0) {
                        *pwbuff++ = keys[i].val[j++];
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
 * @param filename Template file to read
 * @param[out] buffer The resulting replaced text
 * @param keys Keywords to replace
 * @param nkeys Number of keywords
 * @return 0 on success, -1 on failure
 */
int
replace_keywords_in_file(char *filename, char **buffer, struct keypairs keys[], size_t nkeys) {

    FILE *fp;
    *buffer = NULL;
    if ((fp = fopen(filename, "rb")) == NULL) {
        return -1;
    }
    int fd = fileno(fp);
    struct stat buf;
    fstat(fd, &buf);
    size_t const N = buf.st_size + MAX_KEYPAIR_VAL_SIZE * nkeys + 1;
    *buffer = calloc(1, N);
    size_t readsize = fread(*buffer, sizeof (char), buf.st_size, fp);
    if (readsize != (size_t) buf.st_size) {
        fclose(fp);
        free(*buffer);
        *buffer = NULL;
        return -1;
    }
    fclose(fp);
    return replace_keywords(*buffer, N, keys, nkeys);
}

/**
 * Create a new list of key-pairs of the specified size
 * @param maxsize Size of key-pairs
 * @return Return pointer to beginning of key-pair list
 */
struct keypairs *
new_keypairlist(size_t maxsize) {
    return calloc(maxsize, sizeof (struct keypairs));
}

/**
 * Add a key/val - pair to the list of pairs
 * @param keys Existing list
 * @param maxkeys Max size of list
 * @param key Key to add
 * @param val Val to add
 * @param idx Running index in list of pairs
 * @return 0 on success, -1 when list is full
 */
int
add_keypair(struct keypairs *keys, size_t maxkeys, char *key, char *val, size_t *idx) {
    if (*idx >= maxkeys) {
        return -1;
    } else {
        keys[*idx].key = strdup(key);
        keys[*idx].val = strdup(val);
        (*idx)++;
        return 0;
    }
}

/**
 * Free list of key/val-pairs previously created with new_keypairlist()
 * @param keys
 * @param maxkeys
 * @return 0 on success, -1 if keys are an invalid pointer
 */
int
free_keypairlist(struct keypairs *keys, size_t maxkeys) {
    if (keys == NULL)
        return -1;
    for (size_t i = 0; i < maxkeys; ++i) {
        if (keys[i].key) {
            free(keys[i].key);
        }
        if (keys[i].val) {
            free(keys[i].val);
        }
    }
    free(keys);
    return 0;
}

/* EOF */
