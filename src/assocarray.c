/* =========================================================================
 * File:        ASSOCARRAY.C
 * Description: A basic associate array of strings that grows dynamically
 *              with the usual manipulation functions
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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "assocarray.h"
#include "xstr.h"


/**
 * Create and return a new associative array
 * @param initial_size Inital size of array
 * @return A pointer to a new associative array, NULL on error
 */
struct assoc_array_t *
assoc_new(size_t initial_size) {
    struct assoc_array_t *pa=calloc(1,sizeof(struct assoc_array_t));
    if( pa == NULL || initial_size < 8) {
        return NULL;
    }
    pa->a = calloc(initial_size, sizeof(struct assoc_pair_t));
    if( pa->a == NULL ) {
        free(pa);
        return NULL;
    }
    pa->max_values = initial_size;
    return pa;
}

/**
 * Get the value corresponding to the given name
 * @param a Associative array as returned by assoc_new()
 * @param name The name of the value
 * @return A pointer to the value, NULL of not found
 */
char *
assoc_get(struct assoc_array_t *a, char *name) {
    if( a==NULL || name == NULL || *name == '\0' || a->num_values == 0 )
        return NULL;
    
    size_t i=0;
    while( i<a->num_values && strcmp(name,a->a[i].name) ) {
        i++;
    }
    if( i>=a->num_values )
        return NULL;
    else
        return a->a[i].value;
}

/**
 * Return a value given a name and fallback to the value in "notfound"
 * if the named pair does not exists rather tha returning NULL
 * @param a Associative array as returned by assoc_new()
 * @param name The name of the value
 * @param notfound String to return if name is not found
 * @return Value of string in "Notfound"
 */
char *
assoc_get2(struct assoc_array_t *a, char *name, char *notfound) {
    char *ret = assoc_get(a, name);
    if( NULL == ret )
        return notfound;
    else
        return ret;
}


/**
 * Deep copy of two associative arrays
 * @param dest Destination
 * @param src Source
 * @return 0 on success, -1 if the destination array does not have enough space
 */
int
assoc_copy(struct assoc_array_t *dest,struct assoc_array_t *src) {
    if( dest->max_values < src->max_values )
        return -1;
    for(size_t i=0; i < dest->num_values ; i++) {
        free(dest->a[i].name);
        free(dest->a[i].value);
    }
    dest->num_values = src->num_values;
    for(size_t i=0; i < dest->num_values ; i++) {
        dest->a[i].name = strdup(src->a[i].name);
        dest->a[i].value = strdup(src->a[i].value);
        if( dest->a[i].name == NULL || dest->a[i].value == NULL ) {
            return -1;
        }
    }    
    return 0;
}

/**
 * Add a new name/value pair to the array
 * @param a Associative array as returned by assoc_new()
 * @param name Name
 * @param value String
 * @return 0 on success, -1 on failure, -2 if name exists
 */
int
assoc_put(struct assoc_array_t *a, char *name, char *value) {
    if( a==NULL || name == NULL || value == NULL || *name== '\0' )
        return -1;
    
    // Check that the name doesn't exist before
    if( assoc_get(a,name) ) {
        return -2;
    }
    
    if( a->num_values >= a->max_values ) {
        // We need to dynamically increase the size
        // Keep it simple and just double the size each time we hit the limit
        struct assoc_array_t *tmp = assoc_new(a->max_values * 2);
        if( NULL==tmp )
            return -1;
        assoc_copy(tmp, a);
        for(size_t i=0; i < a->num_values ; i++) {
            free(a->a[i].name);
            free(a->a[i].value);
        }        
        free(a->a);
        a->a = tmp->a;
        a->max_values = tmp->max_values;
        free(tmp);
    }
    a->a[a->num_values].name = strdup(name);
    a->a[a->num_values].value = strdup(value);
    if( a->a[a->num_values].name == NULL || a->a[a->num_values].value == NULL ) {
        return -1;
    }
    a->num_values++;
    return 0;
}

/**
 * Update an existing name with new value
 * @param a Associative array as returned by assoc_new()
 * @param name Existing name
 * @param value New value
 * @return 0 on success, -1 if value does not exists
 */
int
assoc_update(struct assoc_array_t *a, char *name, char *value) {
    if( NULL==assoc_get(a,name) ) {
        return -1;
    }
    size_t i=0;
    while( i < a->num_values && strcmp(a->a[i].name, name) ) {
        i++;        
    }  
    free(a->a[i].value);
    a->a[i].value = strdup(value);
    return 0;
}

/**
 * Delete the named value
 * @param a Associative array as returned by assoc_new()
 * @param name Name
 * @return 0 on success, -1 on failure. -2 if name doesn't exists
 */
int
assoc_del(struct assoc_array_t *a, char *name) {
    if( a==NULL || name == NULL || *name== '\0' )
        return -1;
    
    // Check that the name exists before
    if( a->num_values==0 || NULL==assoc_get(a,name) ) {
        return -2;
    }
    
    size_t i=0;
    while( i < a->num_values && strcmp(a->a[i].name, name) ) {
        i++;        
    }
    if( i >= a->num_values ) {
        return -1;
    }
    free(a->a[i].name);
    free(a->a[i].value);
    // Copy down all values
    for(size_t j=i; j < a->num_values-1; j++) {
        a->a[j].name = a->a[j+1].name;
        a->a[j].value = a->a[j+1].value;
    }
    a->a[a->num_values-1].name = NULL;
    a->a[a->num_values-1].value = NULL;
    a->num_values--;
    return 0;
}

/**
 * Return the current length of the associative array
 * @param a Associative array as returned by assoc_new()
 * @return The number of values in the array
 */
size_t 
assoc_len(struct assoc_array_t *a) {
    return a->num_values;
}

/**
 * Return the maximum size of the array
 * @param a Associative array as returned by assoc_new()
 * @return The current maximum size of array
 */
size_t 
assoc_size(struct assoc_array_t *a) {
    return a->max_values;
}

/**
 * Destroy the array (all memory is freed)
 * @param a
 * @return 0 on success, -1 on failure
 */
int 
assoc_destroy(struct assoc_array_t *a) {
    if( a==NULL )
        return -1;
    if( a->a ) {
        for(size_t i=0; i < a->num_values ; i++) {
            if( a->a[i].name ) free(a->a[i].name);
            if( a->a[i].value )free(a->a[i].value);
        }
        free(a->a);
    }
    free(a);
    return 0;
}

static int
qsort_compare(const void *e1, const void *e2) {
    return strcmp( ((struct assoc_pair_t *)e1)->name, ((struct assoc_pair_t *)e2)->name);
}

/**
 * Sort the array by name
 * @param a Associative array as returned by assoc_new()
 * @return 0 on success, -1 on failure
 */
int
assoc_sort(struct assoc_array_t *a) {
    if( a==NULL || a->num_values==0 )
        return -1;
    qsort(a->a, a->num_values, sizeof(struct assoc_pair_t), qsort_compare);
    return 0;
}

/**
 * Dump the array in a human readable format to the given string buffer
 * @param a Associative array as returned by assoc_new()
 * @param buff Output buffer
 * @param maxlen Maximum length
 * @return 0 on success, -1 on error
 */
int
assoc_tostring(struct assoc_array_t *a, char *buff, size_t maxlen) {
    if( a==NULL ) return -1;
    
    snprintf(buff, maxlen, "{\n  num_values:%04zu,\n  max_values:%04zu,\n  pairs : [\n",a->num_values, a->max_values);
    for(size_t i=0; i < a->num_values; i++) {
        xvstrncat(buff, maxlen, "    { \"%s\" : \"%s\" },\n", a->a[i].name, a->a[i].value);
    }
    xvstrncat(buff, maxlen, "  ]\n}\n");    
    return 0;
}