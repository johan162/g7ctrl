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


struct assoc_pair_t {
    char *name;
    char *value;
};

struct assoc_array_t {
    struct assoc_pair_t *a;
    size_t num_values;
    size_t max_values;
};

struct assoc_array_t *
assoc_new(size_t initial_size);

char *
assoc_get(struct assoc_array_t *a, char *name);

char *
assoc_get2(struct assoc_array_t *a, char *name, char *notfound);


int
assoc_copy(struct assoc_array_t *dest,struct assoc_array_t *src);

int
assoc_put(struct assoc_array_t *a, char *name, char *value);

int
assoc_update(struct assoc_array_t *a, char *name, char *value);

int
assoc_del(struct assoc_array_t *a, char *name);

size_t 
assoc_len(struct assoc_array_t *a);

size_t 
assoc_size(struct assoc_array_t *a);

int 
assoc_destroy(struct assoc_array_t *a);

int
assoc_to_json(struct assoc_array_t *a, char *buff, size_t maxlen);

int
assoc_sort(struct assoc_array_t *a);



#ifdef	__cplusplus
}
#endif

#endif	/* ASSOCARRAY_H */

