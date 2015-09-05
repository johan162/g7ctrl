/* =========================================================================
 * File:        ASSOCARRAY.H
 * Description: A basic associate array that grows dynamically
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

#ifndef ASSOCARRAY_H
#define	ASSOCARRAY_H

#ifdef	__cplusplus
extern "C" {
#endif

struct assoc_pair {
    char *name;
    char *value;
};
    
typedef struct assoc_pair *assoc_pair_t;

struct assoc_array {
    assoc_pair_t a;
    size_t num_values;
    size_t max_values;
};

typedef struct assoc_array *assoc_array_t;
    

assoc_array_t
assoc_new(size_t initial_size);

char *
assoc_get(assoc_array_t a, char *name);

char *
assoc_get2(assoc_array_t a, char *name, char *notfound);

int
assoc_copy(assoc_array_t dest,assoc_array_t src);

int
assoc_add(assoc_array_t dest, assoc_array_t src);

int
assoc_intersection(assoc_array_t dest, assoc_array_t a1, assoc_array_t a2);

int
assoc_put(assoc_array_t a, char *name, char *value);

int
assoc_update(assoc_array_t a, char *name, char *value);

int
assoc_del(assoc_array_t a, char *name);

size_t 
assoc_len(assoc_array_t a);

size_t 
assoc_size(assoc_array_t a);

int 
assoc_destroy(assoc_array_t a);

int 
assoc_clear(assoc_array_t a);

int
assoc_export_to_json(assoc_array_t a, char *buff, size_t maxlen);

int
assoc_sort(assoc_array_t a);

int
assoc_import_from_json(assoc_array_t a, char *buf);


#ifdef	__cplusplus
}
#endif

#endif	/* ASSOCARRAY_H */

