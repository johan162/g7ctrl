/* =========================================================================
 * File:        PCREDMALLOC.C
 * Description: Traced memory allocation/deallocation for PCRE library
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

#include <stdlib.h>
#include <syslog.h>
#include <errno.h>
#include <pthread.h>

#include "utils.h"
#include "pcredmalloc.h"
#include "logger.h"

/*
 * The following memory routines are just used to double check that
 * all the calls to PCRE regexp routines are matched with respect to
 * memory allocation.
 */

// struct _dpcre_mem_entry *pcre_mem_list = (void *) NULL;

int _dpcre_call_count = 0;
static pthread_mutex_t _dpcre_malloc_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Allocate the specified number of characters and return a pointer.
 * This will also increase the allocation count to know how many allocations
 * we have done. This can later be checked against the free() method to
 * see if we have forgotten to return memory somewhere.
 * @param size Number of chars to allocate.
 * @return Pointer to newly allocated memory
 */
void *
_dpcre_malloc(size_t size) {

    pthread_mutex_lock(&_dpcre_malloc_mutex);
    _dpcre_call_count++;
    pthread_mutex_unlock(&_dpcre_malloc_mutex);

#ifdef DETAILED_PCRE_DEBUGGING
    struct _dpcre_mem_entry *ent = calloc(1, sizeof (struct _dpcre_mem_entry));
    ent->size = size;
    ent->ptr = malloc(size);

    struct _dpcre_mem_entry *walk = pcre_mem_list;

    if (walk == NULL) {
        pcre_mem_list = ent;
    } else {
        while (walk->next) {
            walk = walk->next;
        }
        walk->next = ent;
    }
    logmsg(LOG_DEBUG, "PCRE MALLOC: %06d bytes", size);
    return ent->ptr;
#else
    return malloc(size);
#endif
}

/**
 * Free memory previous allocated by _dpcre_malloc()
 * @param ptr Pointer to memory area to free
 */
void
_dpcre_free(void *ptr) {

#ifdef DETAILED_PCRE_DEBUGGING
    struct _dpcre_mem_entry *walk = pcre_mem_list;
    struct _dpcre_mem_entry *prev = NULL;

    while (walk && walk->ptr != ptr) {
        prev = walk;
        walk = walk->next;
    }
    if (walk == NULL) {
        logmsg(LOG_CRIT, "FATAL: Trying to deallocat PCRE without previous allocation !");
    } else {
        if (prev == NULL) {
            // First entry in list
            pcre_mem_list = walk->next;
            //logmsg(LOG_DEBUG,"PCRE FREE: %06d bytes",walk->size);
            free(walk->ptr);
            free(walk);
        } else {
            prev->next = walk->next;
            //logmsg(LOG_DEBUG,"PCRE FREE: %06d bytes",walk->size);
            free(walk->ptr);
            free(walk);
        }
    }
#else
    free(ptr);
#endif

    pthread_mutex_lock(&_dpcre_malloc_mutex);
    _dpcre_call_count--;
    pthread_mutex_unlock(&_dpcre_malloc_mutex);
}

/**
 * Print a list of allocated memory
 * @param sockd Socket/filedescriptor to write to
 */
void
_dpcre_mem_list(int sockd) {

    _writef(sockd, "PCRE Lib allocation count: %02d\n", _dpcre_call_count);
#ifdef DETAILED_PCRE_DEBUGGING
    struct _dpcre_mem_entry *walk = pcre_mem_list;
    int n = 0;
    while (walk) {
        ++n;
        _writef(sockd, "  #%0002d: size = %06d bytes\n", n, walk->size);
        walk = walk->next;
    }
#endif
}

