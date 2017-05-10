/* =========================================================================
 * File:        PCREDMALLOC.H
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


#ifndef PCREDMALLOC_H
#define	PCREDMALLOC_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * A linked list of memory allocation blocks. Used for debugging the
 * PCRE routines to check for double free or freeing non allocated memory.
 * This structure is populated by the _dpcre_malloc() routine and checked
 * whenever _dpcre_free() is called.
 */
struct _dpcre_mem_entry {
    void *ptr;
    size_t size;
    struct _dpcre_mem_entry *next;
};

void *
_dpcre_malloc(size_t size);

void
_dpcre_free(void *ptr);

void
_dpcre_mem_list(int sockd);


#ifdef	__cplusplus
}
#endif

#endif	/* PCREDMALLOC_H */

